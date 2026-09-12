/*
 * eval.c —— AST 树遍解释执行：让检查通过的程序"真的跑起来"
 *
 * lab3 的递归下降是"边分析边求值"——值是函数返回值，用完即丢；
 * lab5 先把程序物化成 AST，这里是**第二趟**遍历：对表达式求值、
 * 对语句按语义执行。两趟换来的是能力：lab6 将在两者之间插入"生成
 * 三地址码"这一趟，而本文件一行不用改（每个编译器后端目标码的
 * "参考实现"都长这样——先写个解释器定义清楚语义，再写代码生成
 * 与它对拍）。
 *
 * 【运行时环境 = 帧栈】每个进入的 { 块 } 对应一个 Frame，沿 parent
 * 链向外查找——结构和编译期的符号表作用域链一一对应，但身份不同：
 * 符号表在编译期回答"这个名字是什么"，环境在运行期回答"这个变量的
 * 值是多少"。这是 lab7 活动记录/栈帧的直接前身（届时帧变成内存里
 * 连续的栈帧，查找变成按偏移寻址）。
 *
 * 【为什么按 Sym* 而不是按名字查找】A_VAR 节点在编译期已挂上符号表
 * 条目（名字解析只做一次），不同作用域的同名变量是不同的 Sym——
 * 运行期比较指针即可，既不会查错，也不必做字符串比较。
 *
 * 【求值语义】与 lab3 的 rd_parser.c 逐字一致：
 *   int op int → int（/ 与 % 向零截断，C 语义；除数为零是运行时错误）
 *   任一 float → 提升为 double 运算（AST 上的 i2f 节点显式完成提升）
 *   比较 → 按 double 比较，结果 int 0/1
 *   print → 整数 %ld、浮点 %g（保证与 lab3 输出逐字节一致，可对拍）
 */
#include <stdio.h>
#include <stdlib.h>
#include "eval.h"

/* ---------------- 运行时值（lab3 同款） ---------------- */

typedef struct {
    int    isfloat;
    long   i;
    double d;
} Val;

/* ---------------- 环境帧栈 ---------------- */

typedef struct Entry {          /* 一个变量槽：Sym* + 当前值 */
    const Sym *sym;
    Val        val;
    struct Entry *next;
} Entry;

typedef struct Frame {          /* 一个块的作用域（链表实现；lab7 换成真栈帧） */
    struct Frame *parent;
    Entry *head;
} Frame;

static Frame *frame_push(Frame *parent)
{
    Frame *f = calloc(1, sizeof *f);
    f->parent = parent;
    return f;
}

static void frame_pop(Frame *f)
{
    Entry *e = f->head;
    while (e) { Entry *nx = e->next; free(e); e = nx; }
    free(f);
}

/* 沿帧链向外找 sym 对应的槽（父块/更外层块的变量也可见） */
static Entry *frame_lookup(Frame *env, const Sym *sym)
{
    for (Frame *f = env; f; f = f->parent)
        for (Entry *e = f->head; e; e = e->next)
            if (e->sym == sym) return e;
    return NULL;   /* 编译期已保证可见，走不到这里（防御性返回） */
}

/* 写入：槽已存在则更新（循环体反复执行同一 decl 时），否则新建 */
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

/* ---------------- 值运算（与 lab3 rd_parser.c 一致的语义） ---------------- */

static Val arith2(TokenType op, Val a, Val b, const AstNode *at)
{
    Val r = {0, 0, 0};
    if (a.isfloat || b.isfloat) {         /* 提升为 double（i2f 节点已完成转换） */
        double x = a.isfloat ? a.d : (double)a.i;
        double y = b.isfloat ? b.d : (double)b.i;
        r.isfloat = 1;
        switch (op) {
        case T_PLUS:  r.d = x + y; break;
        case T_MINUS: r.d = x - y; break;
        case T_STAR:  r.d = x * y; break;
        case T_SLASH: r.d = x / y; break; /* IEEE：除零得 inf/nan（与 lab3 一致） */
        default:      break;              /* % 含浮点已被编译期拒绝 */
        }
    } else {
        if ((op == T_SLASH || op == T_PERCENT) && b.i == 0) {
            fprintf(stderr, "line %d: runtime error: 整数%s除数为零\n",
                    at->line, op == T_SLASH ? "除法" : "取模");
            exit(1);                      /* 语义检查已通过，这是真正停机的原因 */
        }
        switch (op) {
        case T_PLUS:    r.i = a.i + b.i; break;
        case T_MINUS:   r.i = a.i - b.i; break;
        case T_STAR:    r.i = a.i * b.i; break;
        case T_SLASH:   r.i = a.i / b.i; break;   /* C 语义：向零截断 */
        case T_PERCENT: r.i = a.i % b.i; break;
        default:        break;
        }
    }
    return r;
}

static Val compare(TokenType op, Val a, Val b)   /* 结果是 int 0/1（lab3 同款） */
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

/* ---------------- 表达式求值（深度优先 = 求综合属性的"第二遍"） ---------------- */

static Val eval_expr(const AstNode *e, Frame *env)
{
    Val v = {0, 0, 0};
    switch (e->kind) {
    case A_INT_LIT:   v.i = e->ival; break;
    case A_FLOAT_LIT: v.isfloat = 1; v.d = e->dval; break;
    case A_VAR: {
        Entry *en = frame_lookup(env, e->sym);
        if (!en) {
            /* 编译期名字解析已保证可见，走不到这里；防御性停机而非
             * 对 NULL 解引用。 */
            fprintf(stderr, "internal: 变量未绑定（编译器缺陷）\n");
            exit(1);
        }
        v = en->val;
        break;
    }
    case A_NEG: {
        Val a = eval_expr(e->a, env);
        if (a.isfloat) { v.isfloat = 1; v.d = -a.d; }
        else             v.i = -a.i;
        break;
    }
    case A_I2F: {                       /* 编译期插入的隐式提升，运行期照做 */
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
    default:
        break;   /* A_ERR 等不会出现在被执行的树上（nerr>0 时不会进入本文件） */
    }
    return v;
}

/* "非零为真"：C 风格条件（int/float 皆可，见 parser.c 的取舍说明） */
static int truthy(Val v) { return v.isfloat ? v.d != 0 : v.i != 0; }

/* ---------------- 语句执行：返回值是控制流信号 ----------------
 * break/continue 不用 longjmp，也不用标志变量全局散播——就让 eval_stmt
 * 返回一个枚举，由 while/block 消费。树遍解释器处理跳转最干净的办法。 */

enum { EV_OK = 0, EV_BREAK, EV_CONTINUE };

static int eval_stmt(const AstNode *s, Frame *env)
{
    switch (s->kind) {
    case A_DECL: {
        Val v = {0, 0, 0};
        if (s->a)  v = eval_expr(s->a, env);
        else       v.isfloat = s->sym->type == TY_FLOAT;  /* 未初始化取零值 */
        frame_bind(env, s->sym, v);
        return EV_OK;
    }
    case A_ASSIGN:
        frame_bind(env, s->sym, eval_expr(s->a, env));
        return EV_OK;
    case A_PRINT: {
        Val v = eval_expr(s->a, env);
        if (v.isfloat) printf("%g\n", v.d);   /* lab3 同款格式：跨 lab 对拍靠它 */
        else           printf("%ld\n", v.i);
        return EV_OK;
    }
    case A_EXPRSTMT:
        (void)eval_expr(s->a, env);           /* 求值后丢弃（副作用已发生） */
        return EV_OK;
    case A_IF:
        if (truthy(eval_expr(s->a, env))) return eval_stmt(s->b, env);
        if (s->c)                         return eval_stmt(s->c, env);
        return EV_OK;
    case A_WHILE:
        for (;;) {
            if (!truthy(eval_expr(s->a, env))) break;
            int r = eval_stmt(s->b, env);
            if (r == EV_BREAK)    break;      /* break：跳出循环 */
            /* EV_CONTINUE / EV_OK：都回到条件判断（continue 的语义） */
        }
        return EV_OK;
    case A_BLOCK: {
        Frame *inner = frame_push(env);       /* 进块：压帧（对应 scope_push） */
        int r = EV_OK;
        for (const AstNode *p = s->a; p; p = p->next) {
            r = eval_stmt(p, inner);
            if (r != EV_OK) break;            /* break/continue 穿块向上传 */
        }
        frame_pop(inner);                     /* 出块：弹帧，块内变量随之消亡 */
        return r;                             /* 把信号继续传给外层循环 */
    }
    case A_BREAK:    return EV_BREAK;
    case A_CONTINUE: return EV_CONTINUE;
    case A_EMPTY:    return EV_OK;
    default:         return EV_OK;
    }
}

/* ---------------- 入口 ---------------- */

int eval_run(const AstNode *prog)
{
    Frame *global = frame_push(NULL);    /* 对应全局作用域（scope #0） */
    for (const AstNode *p = prog->a; p; p = p->next)
        if (eval_stmt(p, global) != EV_OK)
            break;   /* 顶层出现 break/continue：编译期已拦（loop_depth=0），防御 */
    frame_pop(global);
    return 0;
}
