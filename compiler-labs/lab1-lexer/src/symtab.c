#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

/* 符号表条目 */
typedef struct Entry {
    char          *text;      /* 标识符词素的副本（NUL 结尾） */
    int            first_line;/* 首次出现的行号 */
    struct Entry  *next_hash; /* 同一个桶里的下一条（哈希冲突链） */
    struct Entry  *next_all;  /* 全局插入顺序链（用于按序打印） */
} Entry;

#define NBUCKETS 257  /* 桶数取个素数，散列更均匀（小表足够） */

static Entry *buckets[NBUCKETS];
static Entry *head_all, *tail_all; /* 插入顺序链表 */

/*
 * FNV-1a 字符串哈希（工业界常用的简短哈希，sizeof(size_t) 无关紧要，
 * 这里用 32 位变体）。龙书 2.7 节只说"用哈希表"，具体哈希函数任选。
 */
static unsigned fnv1a(const char *s, int len)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= (unsigned char)s[i];
        h *= 16777619u;
    }
    return h;
}

const char *symtab_intern(const char *start, int len, int line)
{
    unsigned b = fnv1a(start, len) % NBUCKETS;

    /* 先查：同词素是否已经驻留过（链地址法处理冲突） */
    for (Entry *e = buckets[b]; e; e = e->next_hash)
        if ((int)strlen(e->text) == len && strncmp(e->text, start, len) == 0)
            return e->text;

    /* 没有则新建 */
    Entry *e = malloc(sizeof *e);
    e->text = malloc((size_t)len + 1);
    memcpy(e->text, start, len);
    e->text[len] = '\0';
    e->first_line = line;
    e->next_hash = buckets[b];
    buckets[b] = e;
    e->next_all = NULL;
    if (tail_all) tail_all->next_all = e; else head_all = e;
    tail_all = e;
    return e->text;
}

void symtab_print(void)
{
    printf("== symbol table (identifier interned, in insertion order) ==\n");
    for (Entry *e = head_all; e; e = e->next_all)
        printf("  %-16s first seen at line %d\n", e->text, e->first_line);
}
