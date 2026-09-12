/*
 * main.c —— lab1 词法分析器驱动程序
 *
 * 用法:
 *   ./minilex.exe [选项] [源文件]     （缺省读 stdin）
 *   -s   打印完 token 流后附加打印符号表（标识符驻留结果）
 *
 * 输出格式（lab2 会用自动生成的 DFA 词法器对拍同样格式）:
 *   行:列<TAB>记号类别<TAB>词素
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "symtab.h"

/* 把整个文件读进内存（"现代做法"：一次 I/O，内存里随便游标移动）。
 * stdin 分支必须走增量 fread：管道不可 fseek，对它 SEEK_END 是未定义
 * 行为——旧实现靠 ftell 返回 -1 后 malloc(0)+巨量 fread "碰巧能用"，
 * 实际在 buf[got]='\0' 处越界写。与 lab8~10 的 read_file 同款写法。 */
static char *read_file(const char *path)
{
    FILE *f = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (!f) { fprintf(stderr, "error: 无法打开 %s\n", path); exit(1); }
    if (f == stdin) {
        size_t cap = 4096, len = 0;
        char *buf = malloc(cap);
        size_t got;
        while ((got = fread(buf + len, 1, cap - len - 1, f)) > 0) {
            len += got;
            if (cap - len < 2) { cap *= 2; buf = realloc(buf, cap); }
        }
        buf[len] = '\0';
        return buf;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    const char *path = "-";
    int show_symtab = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) show_symtab = 1;
        else path = argv[i];
    }

    Lexer lx;
    lexer_init(&lx, read_file(path));

    /* 主循环：不断取记号直到 EOF。
     * 注意 T_ERROR 记号也会打印出来（它们代表"这里有个坏东西"），
     * 这让错误恢复的过程可见——见 README。 */
    for (;;) {
        Token t = lexer_next(&lx);
        if (t.type == T_EOF) {
            printf("%d:%d\tT_EOF\t\n", t.line, t.col);
            break;
        }
        printf("%d:%d\t%-12s\t%s\n", t.line, t.col,
               token_name(t.type), t.lexeme);
    }

    if (show_symtab) symtab_print();
    return lexer_had_error() ? 1 : 0;
}
