/*
 * symtab.c —— 作用域链符号表的实现（对照龙书 2.7 节）
 *
 * 全部状态收在这一个文件里：当前作用域栈顶 + 所有创建过的 Scope 的
 * 顺序链。语法分析器只管在正确时机 push/pop（进/出块）与
 * declare/lookup（声明/引用），完全不用知道哈希细节。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

#define NBUCKETS 64   /* 哈希桶数（2 的幂，配合按位取模） */

static Scope *cur;                    /* 作用域栈顶（最内层） */
static Scope *created_head, *created_tail; /* 所有作用域的创建顺序链 */
static int    next_scope_id = 0;

const char *type_name(Type t)
{
    switch (t) {
    case TY_INT:   return "int";
    case TY_FLOAT: return "float";
    default:       return "err";
    }
}

/* BKDR 字符串哈希：乘 131 是教材上的经典常数，够用且可解释 */
static unsigned bucket_of(const char *s)
{
    unsigned h = 0;
    while (*s) h = h * 131 + (unsigned char)*s++;
    return h & (NBUCKETS - 1);
}

/* 新建空作用域并压栈；parent=NULL 时是程序最外层（全局层） */
static Scope *scope_new(Scope *parent)
{
    Scope *sc = calloc(1, sizeof *sc);
    sc->parent = parent;
    sc->depth  = parent ? parent->depth + 1 : 0;
    sc->id     = next_scope_id++;
    /* 串进"创建顺序"链（pop 不摘除——dump 要看全部作用域） */
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
    /* 只回退栈顶指针，不 free：Scope/Sym 的生命周期 = 整个编译期
     * （AST 节点还挂着 Sym*；-symtab 也还要打印已退出的作用域）。 */
    if (cur && cur->parent) cur = cur->parent;
}

int scope_depth(void) { return cur ? cur->depth : 0; }

Sym *sym_declare(const char *name, Type t, int line)
{
    /* 重复检查只看**当前**作用域：内层块里声明与外层同名的变量是
     * 合法的"遮蔽"（shadowing），不是错误——这是 C 的作用域规则。 */
    unsigned b = bucket_of(name);
    for (Sym *s = cur->buckets[b]; s; s = s->bucket_next)
        if (strcmp(s->name, name) == 0) return NULL;

    Sym *s = calloc(1, sizeof *s);
    s->name = name;
    s->type = t;
    s->line = line;
    s->bucket_next = cur->buckets[b];
    cur->buckets[b] = s;
    if (cur->last) cur->last->order_next = s;
    else           cur->first = s;
    cur->last = s;
    return s;
}

Sym *sym_lookup(const char *name)
{
    /* 沿作用域链从最内层向外找，第一层命中即胜出（遮蔽语义） */
    for (Scope *sc = cur; sc; sc = sc->parent) {
        for (Sym *s = sc->buckets[bucket_of(name)]; s; s = s->bucket_next)
            if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

void symtab_dump(void)
{
    for (Scope *sc = created_head; sc; sc = sc->created_next) {
        printf("scope #%d (depth %d)\n", sc->id, sc->depth);
        for (Sym *s = sc->first; s; s = s->order_next)
            printf("  %s : %s (line %d)\n", s->name, type_name(s->type), s->line);
    }
}
