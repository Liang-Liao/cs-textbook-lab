/*
 * symtab.h —— 带作用域链的符号表（对照龙书 2.7 节"符号表"）
 *
 * lab1 的符号表只做"字符串驻留"（同一个词素只存一份），那只是符号表
 * 最初级的用法。真正的符号表要回答两个问题：
 *   1. 这个名字**是什么**？     —— 记录类型、声明位置（本 lab：int/float）
 *   2. 这个名字**在哪里可见**？ —— 作用域链（Scope 栈）
 *
 * 作用域链的做法（龙书 2.7 + 现代编译器的标准姿势）：
 *   - 每个 `{}` 块对应一个 Scope，内含一张哈希表；
 *   - Scope 通过 parent 指针连成链（进入块 push，离开块 pop）；
 *   - 查找名字时从最内层 Scope 沿链向外找，第一层命中即胜出——
 *     这就是 C 的"内层声明遮蔽外层同名"语义。
 *
 * 两个设计点（注释里反复出现，这里先说结论）：
 *   - pop 时不释放 Scope 结构，只改栈顶指针：`-symtab` 需要在整个程序
 *     分析完之后 dump 出**所有**出现过的作用域（含已退出的块）；
 *   - Sym 的生命周期 = 整个编译期（与 AST 一样长）：AST 的 A_VAR 节点
 *     直接挂 Sym* 指针，"名字解析"在编译期一次完成，运行期不再做
 *     字符串比较——这也是 lab7 活动记录里"变量槽位"思想的前身。
 */
#ifndef SYMTAB_H
#define SYMTAB_H

/* ---- 类型：lab5 类型检查的全部对象（int/float 两类，龙书 6.1 的最小版） ----
 * TY_ERR 不是真的类型，而是"这个子表达式已经检查出错了"的标记：
 * 检查器看到 TY_ERR 就不再对它重复报错（避免一个错误引来一串噪声）。 */
typedef enum { TY_INT, TY_FLOAT, TY_ERR } Type;

const char *type_name(Type t);   /* "int" / "float" / "err"（报错与 dump 用，ASCII） */

/* ---- 符号：一个被声明过的名字 ---- */
typedef struct Sym {
    const char *name;      /* 名字（指向词法器 malloc 的词素，编译期一直有效） */
    Type        type;      /* 声明的类型 */
    int         line;      /* 声明处行号（报错定位 / -symtab dump） */
    struct Sym *bucket_next; /* 同一哈希桶的碰撞链 */
    struct Sym *order_next;  /* 同一作用域内的声明顺序链（dump 按声明序打印） */
} Sym;

/* ---- 作用域：一个 {} 块（或全局层）拥有的那张符号表 ---- */
typedef struct Scope {
    struct Scope *parent;  /* 外层作用域（形成链；全局层为 NULL） */
    int           depth;   /* 嵌套深度（全局 0，进一层 +1；dump 缩进用） */
    int           id;      /* 创建序号（dump 显示 scope #N） */
    Sym          *buckets[64]; /* 哈希桶（变量名少，64 桶绰绰有余） */
    Sym          *first, *last; /* 本作用域按声明顺序串起的符号链 */
    struct Scope *created_next; /* 按"创建顺序"串起所有作用域（dump 用） */
} Scope;

void symtab_init(void);            /* 重置并创建全局作用域（每次编译调用一次） */
void scope_push(void);             /* 进入 { 块：压入新作用域 */
void scope_pop(void);              /* 离开 } 块：弹栈（结构保留，供 dump） */
int  scope_depth(void);            /* 当前作用域深度（报错信息用） */

/* 在**当前**作用域登记一个名字；同名已存在则返回 NULL（由调用方报
 * "重复声明"）。成功返回新建的 Sym——请把它挂到 AST 节点上。 */
Sym *sym_declare(const char *name, Type t, int line);

/* 从最内层作用域沿链向外查找；找不到返回 NULL（调用方报"未声明"）。 */
Sym *sym_lookup(const char *name);

/* 打印所有出现过的作用域及其中符号（-symtab 开关；按创建顺序+深度缩进） */
void symtab_dump(void);

#endif /* SYMTAB_H */
