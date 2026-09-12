/*
 * eval.c —— AST 树遍解释执行（lab5 的执行引擎，lab6 的参考实现）
 *
 * 【lab6 中的角色】代码生成（gen.c）和 IR 解释器（irvm.c）都可能出
 * bug；树遍解释器是"按 AST 直接照读语义"的最笨也最可信的实现——
 * run_tests.sh 让同一程序跑两种引擎并对拍输出。这正是工业界的做法：
 * 先写解释器定义清楚语义，再写编译路径与它对拍（V8 的 Sparkplug、
 * CPython 的 peephole 都这样验证）。
 *
 * 【lab6 带来的三个新语义】
 *   1. 短路求值：&&/|| 的右操作数可能**根本不求值**——eval 在树上
 *      天然做到（if 不成立就不递归），而 IR 要靠跳转代码实现（6.6）；
 *   2. 函数调用：实参在**调用方**环境求值，然后在新帧里绑定形参、
 *      执行函数体。C 递归当调用栈用——lab7 才把它显式化成栈帧；
 *   3. return：eval_stmt 多返回一个 EV_RETURN 信号（带返回值），
 *      沿语句结构向上传到函数调用边界——与 EV_BREAK/EV_CONTINUE
 *      同一套"控制流即返回值"的机制。
 *
 * 求值语义与 lab5 逐字一致（int/int 保 int、任一 float 提升、比较
 * 得 int 0/1、print 用 %ld/%g）。
 */
#include <stdio.h>
#include <stdlib.h>
#include "eval.h"

/* ---------------- 运行时值 ---------------- */

typedef struct {
    int    isfloat;
    long   i;
    double d;
} Val;

/* ---------------- 环境帧栈（同 lab5） ---------------- */

typedef struct Entry {
    const Sym *sym;
    Val        val;      /* 标量的值；数组的元素个数见 cells/len */
    Val       *cells;    /* 数组存储（calloc 分配 len 格，零初始化）；
                          * NULL = 标量。键是 Sym*——不同声明各占一格，
                          * 块级遮蔽天然正确 */
    int        len;
    struct Entry *next;
} Entry;

typedef struct Frame {
    struct Frame *parent;
    Entry *head;
} Frame;

static Frame *g_frame;   /* 全局帧：所有函数帧的词法父（MiniC 无嵌套函数） */

static Frame *frame_push(Frame *parent)
{
    Frame *f = calloc(1, sizeof *f);
    f->parent = parent;
    return f;
}

static void frame_pop(Frame *f)
{
    Entry *e = f->head;
    while (e) { Entry *nx = e->next; free(e->cells); free(e); e = nx; }
    free(f);
}

static Entry *frame_lookup(Frame *env, const Sym *sym)
{
    for (Frame *f = env; f; f = f->parent)
        for (Entry *e = f->head; e; e = e->next)
            if (e->sym == sym) return e;
    return NULL;
}

static void frame_bind(Frame *env, const Sym *sym, Val v)
{
    Entry *e = frame_lookup(env, sym);
    if (e) { e->val = v; return; }
    e = calloc(1, sizeof *e);
    e->sym = sym;
    e->val = v;
    e->next = env->head;
    env->head = e;
}

/* 数组声明：在当前帧开 len 格并零初始化（calloc）。每次进入作用域都
 * 新开一份——递归调用的各层激活各自拥有自己的数组，互不可见 */
static void frame_bind_array(Frame *env, const Sym *sym, int len)
{
    Entry *e = frame_lookup(env, sym);
    if (!e) {
        e = calloc(1, sizeof *e);
        e->sym = sym;
        e->next = env->head;
        env->head = e;
    }
    free(e->cells);                    /* 防御：重复绑定先释放旧存储 */
    e->cells = calloc((size_t)len, sizeof(Val));
    e->len   = len;
}

/* ---------------- 值运算（与 lab5 逐字一致） ---------------- */

static Val arith2(TokenType op, Val a, Val b, const AstNode *at)
{
    Val r = {0, 0, 0};
    if (a.isfloat || b.isfloat) {
        double x = a.isfloat ? a.d : (double)a.i;
        double y = b.isfloat ? b.d : (double)b.i;
        r.isfloat = 1;
        switch (op) {
        case T_PLUS:  r.d = x + y; break;
        case T_MINUS: r.d = x - y; break;
        case T_STAR:  r.d = x * y; break;
        case T_SLASH: r.d = x / y; break;
        default:      break;
        }
    } else {
        if ((op == T_SLASH || op == T_PERCENT) && b.i == 0) {
            fprintf(stderr, "line %d: runtime error: 整数%s除数为零\n",
                    at->line, op == T_SLASH ? "除法" : "取模");
            exit(1);
        }
        switch (op) {
        case T_PLUS:    r.i = a.i + b.i; break;
        case T_MINUS:   r.i = a.i - b.i; break;
        case T_STAR:    r.i = a.i * b.i; break;
        case T_SLASH:   r.i = a.i / b.i; break;
        case T_PERCENT: r.i = a.i % b.i; break;
        default:        break;
        }
    }
    return r;
}

static Val compare(TokenType op, Val a, Val b)
{
    double x = a.isfloat ? a.d : (double)a.i;
    double y = b.isfloat ? b.d : (double)b.i;
    Val r = {0, 0, 0};
    switch (op) {
    case T_LT:  r.i = x <  y; break;
    case T_LE:  r.i = x <= y; break;
    case T_GT:  r.i = x >  y; break;
    case T_GE:  r.i = x >= y; break;
    case T_EQ:  r.i = x == y; break;
    case T_NEQ: r.i = x != y; break;
    default:    break;
    }
    return r;
}

static int truthy(Val v) { return v.isfloat ? v.d != 0 : v.i != 0; }

/* ---------------- 语句执行的控制流信号 ----------------
 * break/continue/return 都不用 longjmp——让 eval_stmt 返回一个枚举，
 * 由 while/block/函数调用边界各自消费。树遍解释器最干净的做法。 */

enum { EV_OK = 0, EV_BREAK, EV_CONTINUE, EV_RETURN_SOON };

/* ---------------- 函数调用（6.7 的运行期语义） ---------------- */

static Val eval_expr(const AstNode *e, Frame *env);
static int  eval_stmt(const AstNode *s, Frame *env, Val *ret);

/* 调用：实参在调用方环境求值（值传递），新帧绑定形参后执行函数体。
 * 用 C 递归当调用栈——lab7 会把这一层显式化成活动记录。 */
static Val eval_call(const Sym *fn, const AstNode *args, Frame *caller)
{
    const AstNode *fd = fn->fdef;
    Frame *f = frame_push(g_frame);   /* 函数帧的词法父 = 全局帧 */

    /* 逐参绑定：实参表达式的类型已与形参对齐（parser 在实参上包了
     * i2f），直接求值进槽位即可 */
    const AstNode *p = args;
    for (int i = 0; i < fn->nparams; i++) {
        Val v = {0, 0, 0};
        if (p) { v = eval_expr(p, caller); p = p->next; }
        frame_bind(f, fd->psym[i], v);
    }

    Val ret = {0, 0, 0};
    if (fn->type == TY_FLOAT) ret.isfloat = 1;   /* 无 return 落到尾：零值 */
    for (const AstNode *s = fd->a->a; s; s = s->next)
        if (eval_stmt(s, f, &ret) == EV_RETURN_SOON) break;

    frame_pop(f);
    return ret;
}

/* ---------------- 表达式求值 ---------------- */

static Val eval_expr(const AstNode *e, Frame *env)
{
    Val v = {0, 0, 0};
    switch (e->kind) {
    case A_INT_LIT:   v.i = e->ival; break;
    case A_FLOAT_LIT: v.isfloat = 1; v.d = e->dval; break;
    case A_VAR: {
        Entry *en = frame_lookup(env, e->sym);
        if (!en) {
            /* 编译期名字解析已保证可见（decl 作受控语句已在语法层拒绝）；
             * 防御性停机而非对 NULL 解引用。 */
            fprintf(stderr, "internal: 变量未绑定（编译器缺陷）\n");
            exit(1);
        }
        v = en->val;
        break;
    }
    case A_INDEX: {                      /* a[i]：沿帧链找数组，按下标取元素 */
        Entry *en = frame_lookup(env, e->sym);
        Val iv = eval_expr(e->a, env);
        if (en && en->cells) v = en->cells[iv.i];   /* 越界不查（同 C） */
        break;
    }
    case A_NEG: {
        Val a = eval_expr(e->a, env);
        if (a.isfloat) { v.isfloat = 1; v.d = -a.d; }
        else             v.i = -a.i;
        break;
    }
    case A_NOT: {                        /* !e：真值翻转，结果 int */
        Val a = eval_expr(e->a, env);
        v.i = !truthy(a);
        break;
    }
    case A_AND: {                        /* 短路与：左假 → 右不求值 */
        Val a = eval_expr(e->a, env);
        if (!truthy(a)) { v.i = 0; break; }
        v.i = truthy(eval_expr(e->b, env));
        break;
    }
    case A_OR: {                         /* 短路或：左真 → 右不求值 */
        Val a = eval_expr(e->a, env);
        if (truthy(a)) { v.i = 1; break; }
        v.i = truthy(eval_expr(e->b, env));
        break;
    }
    case A_I2F: {
        Val a = eval_expr(e->a, env);
        v.isfloat = 1;
        v.d = (double)a.i;
        break;
    }
    case A_BINOP: {
        Val a = eval_expr(e->a, env);
        Val b = eval_expr(e->b, env);
        switch (e->op) {
        case T_LT: case T_LE: case T_GT: case T_GE:
        case T_EQ: case T_NEQ:
            v = compare(e->op, a, b); break;
        default:
            v = arith2(e->op, a, b, e); break;
        }
        break;
    }
    case A_CALL:
        v = eval_call(e->sym, e->a, env);
        break;
    default:
        break;
    }
    return v;
}

/* ---------------- 语句执行 ---------------- */

static int eval_stmt(const AstNode *s, Frame *env, Val *ret)
{
    switch (s->kind) {
    case A_DECL: {
        if (s->sym->arr_len > 0) {        /* 数组声明：开 len 格，零初始化 */
            frame_bind_array(env, s->sym, s->sym->arr_len);
            return EV_OK;
        }
        Val v = {0, 0, 0};
        if (s->a)  v = eval_expr(s->a, env);
        else       v.isfloat = s->sym->type == TY_FLOAT;
        frame_bind(env, s->sym, v);
        return EV_OK;
    }
    case A_ASSIGN:
        frame_bind(env, s->sym, eval_expr(s->a, env));
        return EV_OK;
    case A_STORE: {                       /* a[i] = e：先下标后右值，顺序与 IR 一致 */
        Entry *en = frame_lookup(env, s->sym);
        Val iv = eval_expr(s->a, env);
        Val rv = eval_expr(s->b, env);
        if (en && en->cells) en->cells[iv.i] = rv;  /* 越界不查（同 C） */
        return EV_OK;
    }
    case A_PRINT: {
        Val v = eval_expr(s->a, env);
        if (v.isfloat) printf("%g\n", v.d);
        else           printf("%ld\n", v.i);
        return EV_OK;
    }
    case A_EXPRSTMT:
        (void)eval_expr(s->a, env);     /* 值丢弃（void 调用的唯一位置） */
        return EV_OK;
    case A_IF:
        if (truthy(eval_expr(s->a, env))) return eval_stmt(s->b, env, ret);
        if (s->c)                         return eval_stmt(s->c, env, ret);
        return EV_OK;
    case A_WHILE:
        for (;;) {
            if (!truthy(eval_expr(s->a, env))) break;
            int r = eval_stmt(s->b, env, ret);
            if (r == EV_BREAK)        break;
            if (r == EV_RETURN_SOON)  return EV_RETURN_SOON;  /* return 穿循环 */
        }
        return EV_OK;
    case A_BLOCK: {
        Frame *inner = frame_push(env);
        int r = EV_OK;
        for (const AstNode *p = s->a; p; p = p->next) {
            r = eval_stmt(p, inner, ret);
            if (r != EV_OK) break;     /* break/continue/return 都向上传 */
        }
        frame_pop(inner);
        return r;
    }
    case A_BREAK:    return EV_BREAK;
    case A_CONTINUE: return EV_CONTINUE;
    case A_EMPTY:    return EV_OK;
    case A_RETURN:
        if (s->a) *ret = eval_expr(s->a, env);
        return EV_RETURN_SOON;
    case A_FUNCDEF:
        return EV_OK;                  /* 定义不是动作：顶层扫描时跳过 */
    default:         return EV_OK;
    }
}

/* ---------------- 入口 ---------------- */

int eval_run(const AstNode *prog)
{
    g_frame = frame_push(NULL);
    Val dummy = {0, 0, 0};
    for (const AstNode *p = prog->a; p; p = p->next) {
        if (p->kind == A_FUNCDEF) continue;   /* 函数定义只登记不执行 */
        if (eval_stmt(p, g_frame, &dummy) != EV_OK)
            break;   /* 顶层 break/continue：编译期已拦（防御） */
    }
    frame_pop(g_frame);
    g_frame = NULL;
    return 0;
}
