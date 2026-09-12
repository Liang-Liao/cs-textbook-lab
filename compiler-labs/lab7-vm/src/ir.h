/*
 * ir.h —— 三地址码（四元式）的表示与打印（对照龙书 6.2 节）
 *
 * 【什么是三地址码】每条指令最多做**一次**运算：至多两个操作数 +
 * 一个结果。"地址"可以是变量名、编译器临时（t1, t2...）或立即数——
 * 统一用字符串表示，教学代码最直白。
 *
 * 【四元式】一条指令 = 四个栏位 (op, arg1, arg2, result)——本文件用
 * struct Quad 显式表达。龙书 6.2.2 的三地址指令全集在本 lab 的映射：
 *
 *   赋值        x = y              Q_ASSIGN
 *   二元运算    x = y + z          Q_BINOP（含算术与比较，bop 记运算符）
 *   一元运算    x = -y / x = !y    Q_NEG / Q_NOT
 *   类型转换    x = (float) y      Q_I2F（lab5 在 AST 上插的 i2f 落到这）
 *   数组读写    x = y[z] / y[z]=x  Q_LDX / Q_STX（6.4 的取址四元式）
 *   标号/跳转   L1: / goto L1      Q_LABEL / Q_GOTO
 *   条件跳转    if a < b goto L1   Q_IF_GOTO（比较直跳，6.6 的跳转代码）
 *               if_false x goto L1 Q_IFF_GOTO（数值真值：非零为真）
 *   过程调用    param x / call f,n Q_PARAM / Q_CALL（6.7）
 *   返回        return [x]         Q_RETURN
 *   打印        print x            Q_PRINT（MiniC 的外联输出）
 *
 * 【组织】每个函数一段（IrFunc），主代码（顶层语句）也是一段，名字
 * 固定为 "main"。段与段之间跳转不互通（VM 按函数为单位执行）。
 */
#ifndef IR_H
#define IR_H

#include "token.h"
#include "symtab.h"   /* Type：函数段的返回类型（return 零值的形状） */

typedef enum {
    Q_ASSIGN,
    Q_BINOP,
    Q_NEG,
    Q_NOT,
    Q_I2F,
    Q_LDX,          /* x = y[z]：数组读（y=数组 IR 名，z=下标地址） */
    Q_STX,          /* y[z] = x：数组写 */
    Q_LABEL,
    Q_GOTO,
    Q_IF_GOTO,
    Q_IFF_GOTO,
    Q_PARAM,
    Q_CALL,
    Q_RETURN,
    Q_PRINT
} QuadOp;

typedef struct {
    QuadOp     op;
    TokenType  bop;         /* Q_BINOP / Q_IF_GOTO 的运算符 */
    const char *x;          /* 结果地址（Q_CALL 的 NULL 表示 void 调用） */
    const char *y, *z;      /* 操作数地址（变量名/临时/立即数文本） */
    int        label;       /* Q_LABEL 定义的标号号；跳转指令的目标标号号
                             * （生成期为 -1 = 悬空待回填！） */
    int        argc;        /* Q_CALL 的实参个数 */
    int        line;        /* 源行号（运行时报错定位） */
} Quad;

typedef struct {
    const char  *name;      /* 函数名；主段为 "main" */
    Type         rettype;   /* 返回类型（无值 return 的零值形状） */
    const char  *params[8]; /* 形参的 IR 名（VM 按位置绑定用） */
    int          nparams;
    Quad        *q;
    int          n, cap;
} IrFunc;

/* ---- IR 名的静态属性表（gen 在翻译时顺手登记；名字全局唯一所以
 * 可以用平行表）。执行后端靠它们把"字符串地址"落实成具体存储： */
typedef struct { const char *name; Type ty; } NameType;   /* 标量名的类型 */
typedef struct {                                            /* 数组描述符 */
    const char *name;     /* 数组的 IR 名（基址） */
    int         len;      /* 元素个数（编译期常量） */
    Type        elem;     /* 元素类型（int/float） */
    int         is_global;/* 1 = 顶层声明（住全局数据区）；0 = 函数局部
                           * （每次调用在帧里开 len 格连续空间，递归安全） */
} ArrayDesc;

typedef struct {
    IrFunc funcs[128];      /* funcs[0] 恒为主段；其后按定义序 */
    int    nfuncs;
    NameType types[1024];   /* 每个 IR 名的静态类型（int/float） */
    int      ntypes;
    ArrayDesc arrays[256];  /* 每个数组的长度/元素类型/归属 */
    int       narrays;
    /* 系统临时名册（gen 的 newtemp 登记）：单一定义、无别名、经预分配
     * 保证不与用户变量冲突。优化器据此识别"哪些名字可安全删除"。 */
    const char *temps[2048];
    int         ntemps;
} IrProgram;

/* 人读文本（-ir 开关）：标号顶格、指令缩进两格，纯 ASCII 可对拍 */
void ir_print(const IrProgram *p);

#endif /* IR_H */
