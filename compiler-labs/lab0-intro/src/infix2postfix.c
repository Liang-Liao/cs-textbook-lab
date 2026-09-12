/*
 * lab0-intro: 中缀表达式 -> 后缀表达式 翻译器
 * 对应龙书第 1~2 章的引入例子（编译器 = "扫描 + 翻译"的最小化身）
 *
 * 用法:
 *   ./infix2postfix.exe [选项] [文件]     (缺省从 stdin 读)
 *   -v   显示每一步的翻译过程（token / 运算符栈 / 输出）
 *   -e   翻译后再解释执行后缀式，打印结果
 *
 * 输入每行一个表达式，支持:
 *   整数/小数字面量、+ - * / % ( )
 *   运算符优先级与结合性同 C（全部左结合）
 *
 * ---------------------------------------------------------------
 * 为什么从"中缀转后缀"开始学编译？
 *
 *   中缀:  3 + 4 * 2        <-- 人类习惯，但需要括号和优先级规则
 *   后缀:  3 4 2 * +        <-- 机器友好：从左到右扫描，遇到运算符
 *                                就弹出栈顶两个操作数计算再压栈，
 *                                不需要括号，不需要优先级
 *
 * 这个两百行的小程序完整走了一遍编译器的核心循环:
 *   读入字符 -> 识别记号(token) -> 按规则改写(翻译) -> 输出
 * 后缀式也正是 lab7 栈式虚拟机指令形态的雏形。
 *
 * 注：这里实现的其实是"运算符优先分析"的栈式等价物（也称
 * shunting-yard 算法，Dijkstra 1961）。它是一个货真价实的语法
 * 分析器——只是它分析的文法（运算符优先文法）没有显式写出来。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------
 * 记号(token)：词法分析的基本单位。
 * 龙书第 3 章会正式定义；这里先用最简版本：数 或 运算符。
 * ------------------------------------------------------------------ */
typedef enum {
    TOK_NUM, /* 数字字面量（值存在 num 字段） */
    TOK_OP,  /* 运算符/括号（字符存在 op 字段） */
    TOK_END  /* 行结束 */
} TokKind;

typedef struct {
    TokKind kind;
    double num; /* TOK_NUM 时的数值 */
    char op;    /* TOK_OP  时的运算符字符 */
    int col;    /* 该记号在本行的起始列号（1 起），用于报错定位 */
} Token;

/* 命令行开关（简单起见用全局变量；工程上会封装成配置结构） */
static int eval_mode    = 0; /* -e：翻译后顺便求值 */
static int verbose_mode = 0; /* -v：打印翻译过程 */

/* ------------------------------------------------------------------
 * 运算符优先级：数字越大越优先。
 * '(' 特殊处理：它在栈中优先级最低（只有 ')' 能弹出它）。
 * ------------------------------------------------------------------ */
static int prec(char op)
{
    switch (op) {
    case '+': case '-': return 1;
    case '*': case '/': case '%': return 2;
    case '(': return 0; /* 在栈里谁都压不过它 */
    default: return -1;
    }
}

/* ------------------------------------------------------------------
 * 词法扫描：从 line[*col] 处扫描下一个记号（一个字符一个字符地看）
 *
 * 这就是龙书第 3 章"转移图"的雏形——手写的状态转移：
 *   见到数字/小数点 -> 连续吞掉所有数字和小数点 -> TOK_NUM
 *   见到运算符      -> 单字符即成 -> TOK_OP
 *   遇到其它字符    -> 报错（词法错误）
 * ------------------------------------------------------------------ */
static Token next_token(const char *line, int *col, int lineno)
{
    Token t = {0};

    /* 跳过空白（只在本行内） */
    while (line[*col] && line[*col] != '\n' &&
           isspace((unsigned char)line[*col]))
        (*col)++;
    t.col = *col + 1; /* 记录起始列（1 起），供报错定位 */

    /* '#' 到行尾是注释（lab1 的 MiniC 会实现真正的行注释与块注释） */
    if (line[*col] == '#') {
        while (line[*col] && line[*col] != '\n') (*col)++;
        t.kind = TOK_END;
        return t;
    }

    if (line[*col] == '\0' || line[*col] == '\n') {
        t.kind = TOK_END;
        return t;
    }

    /* 数字（含小数）：连续吃 [0-9.]，交给 strtod 转成 double。
     * "最长匹配"原则在这里已经出现：能吃 '1' 就继续吃 '2'，
     * 于是 "123" 识别为一个数而不是三个。 */
    if (isdigit((unsigned char)line[*col]) ||
        (line[*col] == '.' && isdigit((unsigned char)line[*col + 1]))) {
        char *end;
        t.kind = TOK_NUM;
        t.num = strtod(line + *col, &end);
        *col = (int)(end - line);
        return t;
    }

    /* 运算符或括号 */
    if (strchr("+-*/%()", line[*col])) {
        t.kind = TOK_OP;
        t.op = line[*col];
        (*col)++;
        return t;
    }

    /* 非法字符：词法错误。真实编译器在此会尝试恢复（跳过该字符
     * 继续扫描），lab1 实现完整的错误恢复，这里直接终止。 */
    fprintf(stderr, "line %d, col %d: error: 非法字符 '%c'\n",
            lineno, t.col, line[*col]);
    exit(1);
}

/* ------------------------------------------------------------------
 * 翻译：中缀 -> 后缀（本程序的核心，一个隐式的语法分析器）
 *
 * 规则（对左结合运算符）：
 *   1. 读到数字          -> 直接输出
 *   2. 读到运算符 op     -> 把栈顶所有"优先级 >= op"的运算符弹出并
 *                           输出，然后 op 入栈
 *                           （等于也弹，保证 a-b-c 从左到右结合）
 *   3. 读到 '('          -> 入栈（当作屏障）
 *   4. 读到 ')'          -> 不断弹栈输出，直到弹出 '('（丢弃）
 *   5. 行结束            -> 弹空栈，剩余内容依次输出
 *
 * 翻译结束时，输出序列就是等价的后缀表达式。
 * ------------------------------------------------------------------ */
static char opstack[256]; /* 运算符栈 */
static int  sp = 0;

/* 往行输出缓冲追加一段。out 容量有限而输入行可达 4095 字节，
 * 不设防的 sprintf 会越界写——超限明确报错（fail-fast）。 */
static void push_out(char *out, size_t cap, int lineno, const char *frag)
{
    if (strlen(out) + strlen(frag) >= cap) {
        fprintf(stderr, "line %d: error: 表达式过长\n", lineno);
        exit(1);
    }
    strcat(out, frag);
}

static void translate(const char *line, int lineno, int verbose)
{
    int col = 0;
    Token t = next_token(line, &col, lineno);
    if (t.kind == TOK_END) return; /* 空行跳过 */

    sp = 0;
    char out[2048] = ""; /* 本行的后缀输出（字符串形式，便于打印） */
    char frag[32];

    for (;;) {
        if (verbose) {
            /* 把当前 token 显示成短字符串，方便人眼对齐看 */
            char tokbuf[32];
            if (t.kind == TOK_NUM)      snprintf(tokbuf, sizeof tokbuf, "%g", t.num);
            else if (t.kind == TOK_END) strcpy(tokbuf, "END");
            else                        snprintf(tokbuf, sizeof tokbuf, "'%c'", t.op);
            printf("  token=%-6s | 运算符栈=[%.*s] | 输出=%s\n",
                   tokbuf, sp, opstack, out);
        }

        if (t.kind == TOK_END) {
            /* 行结束：弹出所有剩余运算符。此时若栈里还有 '(' 说明
             * 括号不闭合——这就是"语法错误"。 */
            while (sp > 0) {
                char op = opstack[--sp];
                if (op == '(') {
                    fprintf(stderr, "line %d: error: 括号未闭合\n", lineno);
                    exit(1);
                }
                snprintf(frag, sizeof frag, "%c ", op);
                push_out(out, sizeof out, lineno, frag);
            }
            break;
        }

        if (t.kind == TOK_NUM) {
            snprintf(frag, sizeof frag, "%g ", t.num);
            push_out(out, sizeof out, lineno, frag);
        } else if (t.op == '(') {
            if (sp >= (int)sizeof opstack) {
                fprintf(stderr, "line %d: error: 运算符栈溢出\n", lineno);
                exit(1);
            }
            opstack[sp++] = '(';
        } else if (t.op == ')') {
            /* 弹到 '(' 为止；栈空了还没见到 '(' -> 括号不匹配 */
            int found = 0;
            while (sp > 0) {
                char op = opstack[--sp];
                if (op == '(') { found = 1; break; }
                snprintf(frag, sizeof frag, "%c ", op);
                push_out(out, sizeof out, lineno, frag);
            }
            if (!found) {
                fprintf(stderr, "line %d, col %d: error: 右括号多余\n",
                        lineno, t.col);
                exit(1);
            }
        } else {
            /* 普通运算符：弹掉栈顶优先级 >= 自己的（左结合的关键：
             * "等于"也弹。如果改成 > 就变成右结合，练习 2 会用到） */
            while (sp > 0 && prec(opstack[sp - 1]) >= prec(t.op)) {
                char op = opstack[--sp];
                snprintf(frag, sizeof frag, "%c ", op);
                push_out(out, sizeof out, lineno, frag);
            }
            if (sp >= (int)sizeof opstack) {
                fprintf(stderr, "line %d: error: 运算符栈溢出\n", lineno);
                exit(1);
            }
            opstack[sp++] = t.op;
        }
        t = next_token(line, &col, lineno);
    }

    printf("%s\n", out);

    /* 可选：解释执行后缀式（一遍扫描 + 一个数栈）。
     * "从左到右扫描、遇运算符弹两个数算完压回去"——这正是
     * lab7 栈式虚拟机执行指令的样子，提前感受一下。 */
    if (eval_mode) {
        double vals[256];
        int vn = 0;
        const char *p = out;
        while (*p) {
            if (isdigit((unsigned char)*p)) {
                char *end;
                if (vn >= (int)(sizeof vals / sizeof vals[0])) {
                    fprintf(stderr, "line %d: error: 操作数过多\n", lineno);
                    exit(1);
                }
                vals[vn++] = strtod(p, &end);
                p = end;
            } else if (strchr("+-*/%", *p)) {
                /* 弹两个数前必须查够不够——"+" 单独成行这类缺操作数
                 * 输入若不设防，vals[--vn] 会下溢越界读。 */
                if (vn < 2) {
                    fprintf(stderr, "line %d: error: 操作数不足\n", lineno);
                    exit(1);
                }
                double b = vals[--vn], a = vals[--vn], r = 0;
                switch (*p) {
                case '+': r = a + b; break;
                case '-': r = a - b; break;
                case '*': r = a * b; break;
                case '/':
                    if (b == 0) {
                        fprintf(stderr, "line %d: error: 除数为零\n", lineno);
                        exit(1);
                    }
                    r = a / b;
                    break;
                case '%':
                    if (b == 0) {
                        /* 与 '/' 同款守卫：整数取模的零除数是硬件异常
                         * （SIGFPE），不能放过去——错误也是语义。 */
                        fprintf(stderr, "line %d: error: 除数为零\n", lineno);
                        exit(1);
                    }
                    r = (double)((long long)a % (long long)b); break;
                }
                vals[vn++] = r;
                p++;
            } else {
                p++;
            }
        }
        /* 求值结束必须恰好剩一个结果：多出来说明数字相邻（如 1.2.3
         * 被切成两个数）或表达式残缺——都是语法错误，不能拿
         * vals[vn-1] 碰运气。 */
        if (vn != 1) {
            fprintf(stderr, "line %d: error: 操作数不足\n", lineno);
            exit(1);
        }
        printf("  = %g\n", vals[0]);
    }
}

/* 逐行读入；lab0 的约定是"一行 = 一个完整的翻译单元"。 */
int main(int argc, char **argv)
{
    const char *path = NULL;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-e") == 0) eval_mode = 1;
        else if (strcmp(argv[i], "-v") == 0) verbose_mode = 1;
        else    path = argv[i];
    }

    FILE *f = path ? fopen(path, "r") : stdin;
    if (!f) {
        fprintf(stderr, "error: 无法打开文件 %s\n", path);
        return 1;
    }

    char line[4096];
    int lineno = 0;
    while (fgets(line, sizeof line, f)) {
        translate(line, ++lineno, verbose_mode);
    }
    if (f != stdin) fclose(f);
    return 0;
}
