/*
 * gen.c —— AST → 三地址码翻译器（本 lab 主文件；龙书 6.3/6.4/6.6/6.7）
 *
 * 三个核心机制：
 *
 * 1.【回填 backpatching】（6.6.2）——生成跳转时目标常常还不知道
 *   （比如 `a && b`：a 为真时该跳去哪？要等 b 生成完才确定）。
 *   做法：先 emit 一条目标悬空（label=-1）的跳转，记下它的下标
 *   （makelist）；等目标确定了再统一填写（backpatch）。四件套：
 *      makelist(i)  merge(l1,l2)  backpatch(l, L)  nextquad
 *   全部只有十几行——教科书上"高级"的技术，实现起来就这么点。
 *
 * 2.【两套布尔翻译】（6.6.1/6.6.5）
 *   - 条件位置（if/while 的条件）走**跳转代码**：gen_cond 返回
 *     (truelist, falselist)——真/假时该跳去哪的悬空链；
 *   - 值位置（`print a && b`）走**跳转代码定值到临时**：用回填生成
 *     "真→t=1 goto end；假→t=0；end:" 的菱形。
 *   同一棵 && 子树在两种位置生成完全不同的代码——但语义一致，
 *   irvm 与 eval 的对拍验证这一点。
 *
 * 3.【名字唯一化】AST 上靠 Sym* 区分不同作用域的同名变量；IR 是扁平
 *   名字空间，gen 给每个 Sym 分配唯一 IR 名（第二个 x → x@1）。
 *   这是"高层表示→低层表示"必然要做的降级，LLVM IR 的 %"x" 重命名、
 *   SSA 的 %1 %2 重命名是同一件事的更彻底版本。
 *
 * break/continue 的翻译也用回填：循环体内 break 生成悬空 goto 挂到
 * breaks 链，循环出口标号确定后统一回填——继承属性（LoopCtx）下传
 * 收集，与 lab5 parser 的 loop_depth 机制一脉相承。
 *
 * 4.【数组翻译】（6.4）——a[i] 发一条 Q_LDX、a[i] = e 发一条 Q_STX，
 *   "基址 + 下标×宽度"的具体计算交给执行后端：IR 层只表达"按下标
 *   读写"这个动作，数组的长度/元素类型/全局还是局部记在
 *   IrProgram.arrays[] 描述符里（每个 IR 名的静态类型同样登记进
 *   types[] 表——名字全局唯一，两张平表都可行）。MiniC 的数组不作
 *   函数参数、没有指针退化，IR 里因此不需要指针这种类型。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen.h"

static IrProgram P;          /* 翻译结果（静态单例：每次编译调用一次） */
static IrFunc  *cur;         /* 当前正在生成的段 */
static int      ntemp, nlabel;
static int      src_line;    /* 当前源行号（塞进每条 Quad 供运行时报错） */

/* ================= 0. 小工具 ================= */

static char *str_dup(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

/* 每函数的四元式数组扩容 */
static Quad *emit(QuadOp op)
{
    if (cur->n == cur->cap) {
        cur->cap = cur->cap ? cur->cap * 2 : 64;
        cur->q = realloc(cur->q, (size_t)cur->cap * sizeof *cur->q);
    }
    Quad *q = &cur->q[cur->n++];
    memset(q, 0, sizeof *q);
    q->op    = op;
    q->label = -1;           /* -1 = 悬空（跳转待回填 / 非跳转不用） */
    q->line  = src_line;
    return q;
}

static int last_quad_idx(void) { return cur->n - 1; }   /* nextquad 的用途 */

static int name_taken(const char *s);   /* 定义在 SEEN 表之后 */

/* 生成临时名：避开已被用户变量占用的名字（gen_program 开头的预分配
 * 会把所有变量 IR 名登记进 SEEN，这里查得到），并登记进 P.temps 名
 * 册——临时是"单一定义、无别名"的代词，优化器靠这本名册识别它们。 */
static const char *newtemp(void)
{
    char b[16];
    do {
        snprintf(b, sizeof b, "t%d", ++ntemp);
    } while (name_taken(b));
    char *r = str_dup(b);
    if (P.ntemps < 2048) P.temps[P.ntemps++] = r;
    return r;
}

/* 在当前位置放一个新标号（"接下来生成的指令从这里开始"） */
static int place_label(void)
{
    int L = ++nlabel;
    Quad *q = emit(Q_LABEL);
    q->label = L;
    return L;
}

static void emit_assign(const char *x, const char *y)
{
    Quad *q = emit(Q_ASSIGN);
    q->x = x;
    q->y = y;
}

/* ---- 名字唯一化：Sym → IR 名（同名第二次出现起加 @N 后缀） ---- */

static struct { const char *name; int count; } SEEN[512];
static int nseen;

static const char *irname(Sym *s)
{
    if (s->irname) return s->irname;
    int cnt = 0;
    int i;
    for (i = 0; i < nseen; i++)
        if (strcmp(SEEN[i].name, s->name) == 0) break;
    if (i < nseen) cnt = ++SEEN[i].count;
    else {
        if (nseen >= (int)(sizeof SEEN / sizeof SEEN[0])) {
            fprintf(stderr, "internal error: 不同名字数超过 %d\n", nseen);
            exit(2);
        }
        SEEN[nseen].name = s->name; SEEN[nseen].count = 0; nseen++;
    }

    if (cnt == 0) s->irname = s->name;           /* 第一个同名者直接用原名 */
    else {
        char b[96];
        snprintf(b, sizeof b, "%s@%d", s->name, cnt);
        s->irname = str_dup(b);
    }
    return s->irname;
}

/* 名字是否已被占用（所有变量 IR 名经预分配都在 SEEN 里） */
static int name_taken(const char *s)
{
    for (int i = 0; i < nseen; i++)
        if (strcmp(SEEN[i].name, s) == 0) return 1;
    return 0;
}

/* ---- IR 名的静态属性登记（进 IrProgram 的平行表，供执行后端查） ----
 * 名字全局唯一（irname 保证），所以"名字→类型"可以用一张平表。
 * 标量的类型在分配 IR 名时顺手记下；临时变量的类型在创建处记。 */

static void note_type(const char *name, Type ty)
{
    for (int i = 0; i < P.ntypes; i++)
        if (strcmp(P.types[i].name, name) == 0) return;   /* 已登记 */
    if (P.ntypes < 1024) {
        P.types[P.ntypes].name = name;
        P.types[P.ntypes].ty   = ty;
        P.ntypes++;
    }
}

/* 带类型登记的 irname：所有"变量变成 IR 名"的地方都走这里 */
static const char *irname_typed(Sym *s)
{
    const char *n = irname(s);
    note_type(n, s->type);
    return n;
}

/* 数组描述符：长度/元素类型/归属段（顶层声明 is_global=1）。执行后端
 * 据此在全局区或帧里开 len 格连续空间，LDX/STX 按基址+下标×宽度寻址 */
static void note_array(const char *name, int len, Type elem, int is_global)
{
    if (P.narrays < 256) {
        P.arrays[P.narrays].name      = name;
        P.arrays[P.narrays].len       = len;
        P.arrays[P.narrays].elem      = elem;
        P.arrays[P.narrays].is_global = is_global;
        P.narrays++;
    }
    note_type(name, elem);          /* a[i] 表达式的类型查这张表得到 */
}

/* ================= 1. 回填四件套（6.6.2） ================= */

typedef struct { int idx[256]; int n; } QL;   /* 悬空跳转的下标链 */

static QL mklist(int i)
{
    QL l; l.n = 0;
    l.idx[l.n++] = i;
    return l;
}

static QL merge(QL a, QL b)
{
    /* 静默截断会把未回填的悬空跳转丢掉——虽有末尾自检兜底，也不该
     * 走到那一步才发现：超限直接报错。 */
    if (a.n + b.n > (int)(sizeof a.idx / sizeof a.idx[0])) {
        fprintf(stderr, "internal error: 回填链超长（>%d）\n",
                (int)(sizeof a.idx / sizeof a.idx[0]));
        exit(2);
    }
    for (int i = 0; i < b.n; i++) a.idx[a.n++] = b.idx[i];
    return a;
}

static void backpatch(QL l, int label)
{
    for (int i = 0; i < l.n; i++)
        cur->q[l.idx[i]].label = label;
}

/* ================= 2. 表达式 → 值代码（6.4） =================
 * 返回"地址"：变量名 / 临时名 / 立即数文本。 */

typedef struct { QL t, f; } CF;   /* 跳转代码的真/假出口悬空链 */
static CF gen_cond(const AstNode *e);   /* 前向声明 */

/* 浮点立即数文本：保证含 '.'（VM 靠它分辨 int/float；%g 的 2.0 是 "2"） */
static const char *float_text(double d)
{
    char b[64];
    snprintf(b, sizeof b, "%g", d);
    if (!strpbrk(b, ".e")) strcat(b, ".0");
    return str_dup(b);
}

static const char *gen_expr(const AstNode *e)
{
    src_line = e->line;
    switch (e->kind) {
    case A_INT_LIT: {
        char b[32];
        snprintf(b, sizeof b, "%ld", e->ival);
        return str_dup(b);
    }
    case A_FLOAT_LIT:
        return float_text(e->dval);
    case A_VAR:
        return irname_typed(e->sym);
    case A_INDEX: {
        /* a[i]（6.4）：下标求值成地址，发一条取址四元式读元素。
         * 基名与下标都是普通"地址"；数组的存储布局由执行后端按
         * arrays[] 描述符落实——IR 层只表达"按下标读"这个动作。 */
        const char *base = irname_typed(e->sym);
        const char *idx  = gen_expr(e->a);
        const char *t    = newtemp();
        note_type(t, e->type);                 /* 元素类型 */
        Quad *q = emit(Q_LDX);
        q->x = t; q->y = base; q->z = idx;
        return t;
    }
    case A_NEG: case A_NOT: case A_I2F: {
        const char *a = gen_expr(e->a);
        const char *t = newtemp();
        note_type(t, e->type);
        Quad *q = emit(e->kind == A_NEG ? Q_NEG
                     : e->kind == A_NOT ? Q_NOT : Q_I2F);
        q->x = t; q->y = a;
        return t;
    }
    case A_BINOP: {
        const char *a = gen_expr(e->a);
        const char *b = gen_expr(e->b);
        const char *t = newtemp();
        note_type(t, e->type);
        Quad *q = emit(Q_BINOP);
        q->bop = e->op; q->x = t; q->y = a; q->z = b;
        return t;
    }
    case A_AND: case A_OR: {
        /* 值上下文的布尔：跳转代码定值到临时（6.6.5 的菱形）
         *     ...gen_cond 的悬空跳转...
         *     L1: t = 1 ; goto Lend
         *     L2: t = 0
         *     Lend:
         */
        const char *t = newtemp();
        note_type(t, TY_INT);                  /* 菱形定出的总是 int 0/1 */
        CF c = gen_cond(e);
        backpatch(c.t, place_label());
        emit_assign(t, "1");
        Quad *j = emit(Q_GOTO); (void)j;      /* 跳过 t=0 的分支 */
        QL jend = mklist(last_quad_idx());
        backpatch(c.f, place_label());
        emit_assign(t, "0");
        backpatch(jend, place_label());
        return t;
    }
    case A_CALL: {
        /* 先求值所有实参并逐个 param（求值在 param 之前！），再 call */
        int n = 0;
        for (const AstNode *p = e->a; p; p = p->next) {
            const char *a = gen_expr(p);
            Quad *q = emit(Q_PARAM);
            q->y = a;
            n++;
        }
        Quad *q = emit(Q_CALL);
        q->y = e->sym->name;          /* 函数名全局唯一，直接用 */
        q->argc = n;
        if (e->type == TY_VOID) return NULL;   /* void 调用无值 */
        q->x = newtemp();
        note_type(q->x, e->type);              /* 结果临时类型 = 返回类型 */
        return q->x;
    }
    default:
        return "0";                   /* A_ERR 等：不会被执行（防御） */
    }
}

/* ================= 3. 条件 → 跳转代码 + 回填（6.6.4） =================
 * 返回 (truelist, falselist)：条件为真/为假时应跳去的悬空指令链。 */

static int is_compare(TokenType op)
{
    return op == T_LT || op == T_LE || op == T_GT ||
           op == T_GE || op == T_EQ || op == T_NEQ;
}

static CF gen_cond(const AstNode *e)
{
    CF r; r.t.n = r.f.n = 0;
    switch (e->kind) {
    case A_BINOP:
        if (is_compare(e->op)) {
            /* 比较直跳：省一个临时（龙书 6.6 的 if a<b goto L） */
            const char *a = gen_expr(e->a);
            const char *b = gen_expr(e->b);
            Quad *q = emit(Q_IF_GOTO);
            q->bop = e->op; q->y = a; q->z = b;
            r.t = mklist(last_quad_idx());
            Quad *g = emit(Q_GOTO);
            (void)g;
            r.f = mklist(last_quad_idx());
            return r;
        }
        break;                        /* 非比较运算 → 数值真值 */
    case A_AND: {                     /* B1 && B2：B1 真才轮到 B2 */
        CF c1 = gen_cond(e->a);
        backpatch(c1.t, place_label());   /* B1 的真出口 = B2 的开始 */
        CF c2 = gen_cond(e->b);
        r.t = c2.t;
        r.f = merge(c1.f, c2.f);          /* 任一为假 → 整体假 */
        return r;
    }
    case A_OR: {                      /* B1 || B2：B1 假才轮到 B2 */
        CF c1 = gen_cond(e->a);
        backpatch(c1.f, place_label());   /* B1 的假出口 = B2 的开始 */
        CF c2 = gen_cond(e->b);
        r.t = merge(c1.t, c2.t);          /* 任一为真 → 整体真 */
        r.f = c2.f;
        return r;
    }
    case A_NOT: {                     /* !B：真假出口互换 */
        CF c = gen_cond(e->a);
        r.t = c.f;
        r.f = c.t;
        return r;
    }
    default:
        break;
    }
    /* 数值真值（含调用、字面量、算术结果）：非零为真。
     * 真假两个出口都要发显式跳转——只发"假跳转、真落空"对 && 碰巧
     * 正确（B1 真时落空恰好是 B2 的开头），但对 || 是错的：B1 真
     * 时该跳过 B2，落空却直接掉进 B2 的代码（双实现对拍抓出的
     * bug：`1 || 1/0` 把除法执行了）。两条跳转与比较路径完全对称。
     * "跳转目标恰是下一条指令"的多余 goto 留给窥孔优化（练习）。 */
    const char *a = gen_expr(e);
    Quad *q = emit(Q_IFF_GOTO);
    q->y = a;
    r.f = mklist(last_quad_idx());
    Quad *g = emit(Q_GOTO); (void)g;
    r.t = mklist(last_quad_idx());
    return r;
}

/* ================= 4. 语句（6.3 声明 / 6.6 控制流 / 6.7 调用） ================= */

typedef struct { QL breaks, continues; int inloop; } LoopCtx;

static void gen_stmt(const AstNode *s, LoopCtx *ctx)
{
    src_line = s->line;
    switch (s->kind) {
    case A_DECL:
        if (s->sym->arr_len > 0) {
            /* 数组声明**不发指令**：存储由执行后端按 arrays[] 描述符
             * 预留——全局区或帧内连续 len 格；"新空间必为零值"是三个
             * 执行器共同的不变量，声明即零初始化由此自然成立。
             * **例外：循环体内声明的数组必须逐元素显式清零**——irvm
             * 按名字惰性分配只做一次，不发指令就会把上一轮的残值带
             * 进下一轮（eval 是逐块清零，两引擎会分歧）。 */
            note_array(irname_typed(s->sym), s->sym->arr_len,
                       s->sym->type, cur == &P.funcs[0]);
            if (ctx->inloop) {
                char idx[16];
                for (int j = 0; j < s->sym->arr_len; j++) {
                    Quad *q = emit(Q_STX);
                    q->x = "0";                        /* 元素清零 */
                    q->y = irname_typed(s->sym);       /* 数组基名 */
                    snprintf(idx, sizeof idx, "%d", j);
                    q->z = str_dup(idx);               /* 立即数下标 */
                }
            }
            break;
        }
        {   /* 标量声明 = 给 IR 名赋初值（零或 init） */
            const char *v = irname_typed(s->sym);
            emit_assign(v, s->a ? gen_expr(s->a) : "0");
        }
        break;
    case A_ASSIGN:
        emit_assign(irname_typed(s->sym), gen_expr(s->a));
        break;
    case A_STORE: {                   /* a[i] = e（6.4）：下标与右值都先求值 */
        const char *idx = gen_expr(s->a);
        const char *val = gen_expr(s->b);
        Quad *q = emit(Q_STX);
        q->x = val; q->y = irname_typed(s->sym); q->z = idx;
        break;
    }
    case A_PRINT: {
        const char *a = gen_expr(s->a);
        Quad *q = emit(Q_PRINT);
        q->y = a;
        break;
    }
    case A_EXPRSTMT:
        (void)gen_expr(s->a);         /* 值丢弃（void 调用唯一合法位置） */
        break;
    case A_IF: {
        CF c = gen_cond(s->a);
        backpatch(c.t, place_label());        /* Lthen:（真出口） */
        gen_stmt(s->b, ctx);
        if (s->c) {
            Quad *j = emit(Q_GOTO); (void)j;  /* then 结束跳过 else */
            QL jend = mklist(last_quad_idx());
            backpatch(c.f, place_label());    /* Lelse: */
            gen_stmt(s->c, ctx);
            backpatch(jend, place_label());   /* Lend: */
        } else {
            backpatch(c.f, place_label());    /* 无 else：假出口 = 结束 */
        }
        break;
    }
    case A_WHILE: {
        int Lbegin = place_label();
        CF c = gen_cond(s->a);
        backpatch(c.t, place_label());        /* Lbody: */
        LoopCtx inner = {.inloop = 1};   /* break/continue 属最近循环；
                                          * inloop 供循环体数组清零判定 */
        gen_stmt(s->b, &inner);
        { Quad *q = emit(Q_GOTO); q->label = Lbegin; }
        int Lend = place_label();
        backpatch(c.f, Lend);                 /* 条件假 → 出口 */
        backpatch(inner.breaks, Lend);        /* break → 出口 */
        backpatch(inner.continues, Lbegin);   /* continue → 条件处 */
        break;
    }
    case A_BLOCK:                     /* IR 无作用域：块直接展开 */
        for (const AstNode *p = s->a; p; p = p->next)
            gen_stmt(p, ctx);
        break;
    case A_BREAK: {                   /* 悬空 goto 挂链，出口确定后回填 */
        Quad *q = emit(Q_GOTO); (void)q;
        ctx->breaks = merge(ctx->breaks, mklist(last_quad_idx()));
        break;
    }
    case A_CONTINUE: {
        Quad *q = emit(Q_GOTO); (void)q;
        ctx->continues = merge(ctx->continues, mklist(last_quad_idx()));
        break;
    }
    case A_RETURN: {
        const char *a = s->a ? gen_expr(s->a) : NULL;
        Quad *q = emit(Q_RETURN);
        q->y = a;
        break;
    }
    case A_EMPTY:
    case A_FUNCDEF:                   /* 定义不是动作（顶层已处理） */
    default:
        break;
    }
}

/* ================= 5. 函数段与入口 ================= */

static IrFunc *add_func(const char *name, Type ret)
{
    if (P.nfuncs >= (int)(sizeof P.funcs / sizeof P.funcs[0])) {
        fprintf(stderr, "error: 函数数超过上限 %d\n",
                (int)(sizeof P.funcs / sizeof P.funcs[0]));
        exit(2);
    }
    IrFunc *f = &P.funcs[P.nfuncs++];
    memset(f, 0, sizeof *f);
    f->name = name;
    f->rettype = ret;
    return f;
}

static void gen_funcdef(const AstNode *fd)
{
    IrFunc *save = cur;
    cur = add_func(fd->sym->name, fd->sym->type);
    for (int i = 0; i < fd->sym->nparams; i++)
        /* 形参只能是标量（数组不作参数——6.4 的语言边界） */
        cur->params[cur->nparams++] = irname_typed(fd->psym[i]);

    LoopCtx ctx = {0};
    for (const AstNode *p = fd->a->a; p; p = p->next)
        gen_stmt(p, &ctx);
    /* 落到函数尾的隐式 return（返回类型对应的零值）——VM 的停止点 */
    if (cur->n == 0 || cur->q[cur->n - 1].op != Q_RETURN)
        emit(Q_RETURN);
    cur = save;
}

/* ---- 预分配：先把整棵树里所有变量符号的 IR 名（连类型）定下来 ----
 * 没有这一步，用户写 `int t1` 会和第一个临时撞成同一个名字——
 * irname 首现保原名、newtemp 从 t1 起计数，两套命名互不知情。
 * 先走一遍树登记好全部变量名，newtemp 的 name_taken 就能避开。 */
static void prename(const AstNode *n)
{
    for (; n; n = n->next) {
        if (n->kind == A_FUNCDEF) {           /* 形参是变量，先登记 */
            for (int i = 0; i < n->sym->nparams; i++)
                if (n->psym[i]) irname_typed(n->psym[i]);
            prename(n->a);                    /* 函数体 */
            continue;
        }
        if (n->sym && (n->kind == A_VAR || n->kind == A_INDEX ||
                       n->kind == A_DECL || n->kind == A_ASSIGN ||
                       n->kind == A_STORE))
            irname_typed(n->sym);
        prename(n->a);
        prename(n->b);
        prename(n->c);
    }
}

IrProgram *gen_program(const AstNode *prog)
{
    memset(&P, 0, sizeof P);
    ntemp = nlabel = 0;
    nseen = 0;
    src_line = 1;
    prename(prog);            /* 预分配必须先于任何 newtemp */

    cur = add_func("main", TY_VOID);  /* funcs[0] 恒为主段（顶层代码） */
    for (const AstNode *p = prog->a; p; p = p->next) {
        if (p->kind == A_FUNCDEF) gen_funcdef(p);
        else {
            LoopCtx ctx = {0};
            gen_stmt(p, &ctx);
        }
    }
    if (cur->n == 0 || cur->q[cur->n - 1].op != Q_RETURN)
        emit(Q_RETURN);

    /* 自检：所有跳转都已回填（悬空 = gen 的 bug，宁可在这里炸） */
    for (int f = 0; f < P.nfuncs; f++)
        for (int i = 0; i < P.funcs[f].n; i++) {
            Quad *q = &P.funcs[f].q[i];
            if ((q->op == Q_GOTO || q->op == Q_IF_GOTO ||
                 q->op == Q_IFF_GOTO) && q->label < 0) {
                fprintf(stderr, "internal error: 回填遗漏（func %s quad %d）\n",
                        P.funcs[f].name, i);
                exit(2);
            }
        }
    return &P;
}
