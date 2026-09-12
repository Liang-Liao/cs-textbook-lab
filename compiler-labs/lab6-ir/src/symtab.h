/*
 * symtab.h —— 带作用域链的符号表（对照龙书 2.7 节"符号表"）
 *
 * lab5 的符号表只记变量（名字→类型）；lab6 的语言有了函数，符号表要
 * 能回答第三类问题：
 *   1. 这个名字**是什么**？     —— 变量（类型）还是函数（签名）？
 *   2. 这个名字**在哪里可见**？ —— 作用域链（Scope 栈，同 lab5）
 *   3. **怎么调用它**？         —— 返回类型 + 参数个数 + 各参数类型
 *
 * 作用域链的机制与 lab5 完全一致：每个 {} 块一个 Scope、parent 连链、
 * 进块 push 出块 pop、查找沿链第一层命中即胜出（遮蔽语义）。函数体
 * 也是一个块作用域：参数登记在函数体作用域里，与局部变量同层——
 * 所以 `int f(int a) { int a; }` 是重复声明，与 C 一致。
 */
#ifndef SYMTAB_H
#define SYMTAB_H

/* ---- 类型（龙书 6.1 的最小版 + lab6 新增 void） ----
 * TY_VOID 只能做函数返回类型（void 变量在 parser 处被拒绝）；
 * TY_ERR 仍是"已检查出错误"的标记（防级联报错）。 */
typedef enum { TY_INT, TY_FLOAT, TY_VOID, TY_ERR } Type;

const char *type_name(Type t);   /* "int" / "float" / "void" / "err" */

/* ---- 符号种类：变量还是函数 ---- */
typedef enum { SYM_VAR, SYM_FUNC } SymKind;

#define MAX_PARAMS 8   /* MiniC 函数最多 8 个参数（教学编译器的合理上限） */

/* ---- 符号：一个被声明过的名字 ---- */
typedef struct Sym {
    SymKind     kind;      /* SYM_VAR / SYM_FUNC */
    const char *name;
    Type        type;      /* 变量类型 / 函数返回类型；数组 = 元素类型 */
    int         arr_len;   /* >0 = 一维数组（元素个数，编译期常量）；
                            * 0 = 标量（6.4 节数组翻译的静态信息来源） */
    int         line;      /* 声明处行号 */
    /* ---- SYM_FUNC 专属（签名：调用的类型检查全靠它） ---- */
    int         nparams;
    Type        ptypes[MAX_PARAMS];
    struct AstNode *fdef;  /* 函数定义的 AST 节点（parser 解析完后回填；
                            * 前向声明避免与 ast.h 循环包含） */
    const char *irname;    /* IR 层的唯一名（gen 首次访问该符号时分配）：
                            * 不同作用域的同名变量在扁平的 IR 名字空间里
                            * 会撞车——第二个 x 变成 x@2（见 gen.c） */
    /* ---- 通用链（哈希桶/声明序，同 lab5） ---- */
    struct Sym *bucket_next;
    struct Sym *order_next;
} Sym;

/* ---- 作用域：一个 {} 块（或全局层）拥有的那张符号表（同 lab5） ---- */
typedef struct Scope {
    struct Scope *parent;
    int           depth;
    int           id;
    Sym          *buckets[64];
    Sym          *first, *last;
    struct Scope *created_next;
} Scope;

void symtab_init(void);
void scope_push(void);
void scope_pop(void);
int  scope_depth(void);

/* 在当前作用域登记变量；同名已存在返回 NULL（调用方报"重复声明"）。 */
Sym *sym_declare(const char *name, Type t, int line);

/* 登记一维数组（6.4 节）：type 存元素类型，arr_len = 元素个数。
 * 长度必须是编译期常量（parser 保证），MiniC 不做指针退化——
 * 数组不能作函数参数/返回值，只能整体声明后按下标使用。 */
Sym *sym_declare_array(const char *name, Type elem, int len, int line);

/* 在当前作用域登记函数签名。注意与变量的登记时机相反：
 * 函数是**先登记、再解析函数体**——函数体内的递归调用（以及调用
 * "在本函数之前已定义"的其他函数）都因此可见；而变量是先解析初始
 * 化式再登记（防 `int x = x;` 自引用）。两种时机都是语言设计决定。 */
Sym *sym_declare_func(const char *name, Type ret, int nparams,
                      const Type *ptypes, int line);

/* 沿作用域链查找（遮蔽语义）；找不到返回 NULL。 */
Sym *sym_lookup(const char *name);

void symtab_dump(void);

#endif /* SYMTAB_H */
