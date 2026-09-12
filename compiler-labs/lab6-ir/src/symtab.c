/*
 * symtab.c —— 作用域链符号表的实现（对照龙书 2.7 节）
 *
 * 与 lab5 的差异只有三处：type_name 多了 "void"；Sym 带 kind 与函数
 * 签名；登记入口分成变量/函数两个（共用同一个查重路径——变量与函数
 * 在 MiniC 里共享一个名字空间，`int f; ` 和 `int f()` 打架算重复声明，
 * 与 C 相同）。其余（哈希、作用域链、dump）原样沿用 lab5。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

#define NBUCKETS 64

static Scope *cur;
static Scope *created_head, *created_tail;
static int    next_scope_id = 0;

const char *type_name(Type t)
{
    switch (t) {
    case TY_INT:   return "int";
    case TY_FLOAT: return "float";
    case TY_VOID:  return "void";
    default:       return "err";
    }
}

/* BKDR 字符串哈希（同 lab5） */
static unsigned bucket_of(const char *s)
{
    unsigned h = 0;
    while (*s) h = h * 131 + (unsigned char)*s++;
    return h & (NBUCKETS - 1);
}

static Scope *scope_new(Scope *parent)
{
    Scope *sc = calloc(1, sizeof *sc);
    sc->parent = parent;
    sc->depth  = parent ? parent->depth + 1 : 0;
    sc->id     = next_scope_id++;
    if (created_tail) created_tail->created_next = sc;
    else              created_head = sc;
    created_tail = sc;
    return sc;
}

void symtab_init(void)
{
    cur = NULL;
    created_head = created_tail = NULL;
    next_scope_id = 0;
    cur = scope_new(NULL);   /* 全局作用域（depth 0） */
}

void scope_push(void) { cur = scope_new(cur); }

void scope_pop(void)
{
    /* 只回退栈顶，不 free：Sym 的生命周期 = 整个编译期（AST 挂着
     * Sym*；-symtab 也还要打印已退出的作用域）——同 lab5。 */
    if (cur && cur->parent) cur = cur->parent;
}

int scope_depth(void) { return cur ? cur->depth : 0; }

/* 查重：当前作用域里已有同名符号（不分变量/函数——共享名字空间） */
static Sym *find_in_current(const char *name)
{
    for (Sym *s = cur->buckets[bucket_of(name)]; s; s = s->bucket_next)
        if (strcmp(s->name, name) == 0) return s;
    return NULL;
}

/* 登记的公共部分：查重 + 入桶 + 串声明序链 */
static Sym *sym_insert(const char *name, int line)
{
    if (find_in_current(name)) return NULL;
    Sym *s = calloc(1, sizeof *s);
    s->name = name;
    s->line = line;
    unsigned b = bucket_of(name);
    s->bucket_next = cur->buckets[b];
    cur->buckets[b] = s;
    if (cur->last) cur->last->order_next = s;
    else           cur->first = s;
    cur->last = s;
    return s;
}

Sym *sym_declare(const char *name, Type t, int line)
{
    Sym *s = sym_insert(name, line);
    if (!s) return NULL;
    s->kind = SYM_VAR;
    s->type = t;
    return s;
}

Sym *sym_declare_array(const char *name, Type elem, int len, int line)
{
    Sym *s = sym_insert(name, line);
    if (!s) return NULL;
    s->kind    = SYM_VAR;
    s->type    = elem;      /* type 字段存元素类型：a[i] 的类型查这里 */
    s->arr_len = len;
    return s;
}

Sym *sym_declare_func(const char *name, Type ret, int nparams,
                      const Type *ptypes, int line)
{
    Sym *s = sym_insert(name, line);
    if (!s) return NULL;
    s->kind    = SYM_FUNC;
    s->type    = ret;                 /* 返回类型 */
    s->nparams = nparams;
    for (int i = 0; i < nparams; i++) s->ptypes[i] = ptypes[i];
    return s;
}

Sym *sym_lookup(const char *name)
{
    for (Scope *sc = cur; sc; sc = sc->parent)
        for (Sym *s = sc->buckets[bucket_of(name)]; s; s = s->bucket_next)
            if (strcmp(s->name, name) == 0) return s;
    return NULL;
}

void symtab_dump(void)
{
    for (Scope *sc = created_head; sc; sc = sc->created_next) {
        printf("scope #%d (depth %d)\n", sc->id, sc->depth);
        for (Sym *s = sc->first; s; s = s->order_next) {
            if (s->kind == SYM_FUNC) {   /* 函数：返回类型 + 参数表 */
                printf("  %s : %s (", s->name, type_name(s->type));
                for (int i = 0; i < s->nparams; i++)
                    printf("%s%s", i ? ", " : "", type_name(s->ptypes[i]));
                printf(") (line %d)\n", s->line);
            } else {
                /* 数组打印成 int[8] 的形状（元素类型 + 长度） */
                if (s->arr_len > 0)
                    printf("  %s : %s[%d] (line %d)\n",
                           s->name, type_name(s->type), s->arr_len, s->line);
                else
                    printf("  %s : %s (line %d)\n",
                           s->name, type_name(s->type), s->line);
            }
        }
    }
}
