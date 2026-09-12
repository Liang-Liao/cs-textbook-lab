/*
 * parser.c —— 递归下降分析器 + 语法制导翻译（对照龙书 5.1~5.5、6.1、6.4 节）
 *
 * MiniC 文法（EBNF；比 lab5 多了函数/return/短路布尔/一维数组）：
 *
 *   program → { funcdef | stmt } EOF          ← 顶层混排：语句照常执行
 *   funcdef → (int|float|void) IDENT ( params ) { stmt_list }
 *   params  → ε | (int|float) IDENT { , (int|float) IDENT }
 *             （数组不作参数——没有指针退化，见下）
 *   decl    → (int|float) IDENT [ INT_LIT ] ;                  ← 数组声明
 *   stmt    → lab5 全部（decl/assign/print/expr/if/while/break/continue/
 *             block/;）| return [ expr ] ; | store
 *             | for ( init; cond; step ) stmt                     ← lab10
 *   for 的三个子句均可空（cond 空 = 恒真）；**纯语法糖**：直接拼装成
 *   既有 AST 组合 { init; while (cond) { body; step; } }，gen 与四个
 *   执行引擎零感知。限制：body 内暂不支持 continue（step 需复制到每个
 *   continue 点，留作练习）；break 语义天然正确。
 *   store   → IDENT [ expr ] = expr ;                          ← 数组写
 *   expr → or；or → and { '||' and }；and → equality { '&&' equality }
 *   equality/rel/add/term 与 lab5 相同（优先级同 C：|| < && < ==）
 *   factor → ... | ! factor | IDENT ( args ) | IDENT [ expr ]  ← 数组读
 *   args  → ε | expr { , expr }
 *
 * 数组的三条语言边界（都是设计决定，README 有展开）：
 *   1. 长度是编译期常量（INT_LIT），声明即零初始化；
 *   2. 不作函数参数/返回值——C 的"数组退化成指针"是另一层抽象，
 *      MiniC 的 IR 里没有指针，绕开它类型系统才干净；
 *   3. 下标必须是 int 表达式；运行时不查越界（与 C 一致）。
 *
 * 在 lab5 的"返回值=综合属性、参数=继承属性"骨架上新增两件事：
 *   1. 三记号前瞻：顶层要区分 `int f(` 与 `int x;`——类型、IDENT、'('
 *      三个记号都看完才能定夺（lab5 的双前瞻不够用了）；语句级区分
 *      `a[i] = e;`（store）与表达式语句也靠 nxt=='[' 这一记号前瞻；
 *   2. 继承属性多一个 in_func：return 只在函数内合法、且类型要与函数
 *      返回类型匹配——与 loop_depth（break/continue 检查）完全同机制。
 *
 * 函数的**先定义后用**是一遍编译的自然结果；函数名在解析函数体之前
 * 登记（递归可见）。要支持"先用后定义"得两遍扫描（先收齐所有签名），
 * 见 README 练习。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "lexer.h"

static Lexer lx;
static Token cur, nxt, nxt2;  /* 三记号前瞻：lab5 的双前瞻 + 1，
                               * 顶层 dispatch 要看"类型 IDENT (" */
static int   nerr;

static void advance(void)
{
    cur  = nxt;
    nxt  = nxt2;
    nxt2 = lexer_next_clean(&lx);
}

/* ---------------- 报错 ---------------- */

static void syn_error(const char *what)
{
    nerr++;
    fprintf(stderr, "line %d, col %d: error: 语法错误: 意外的 %s\n",
            cur.line, cur.col, what);
}

static void sem_error(const Token *t, const char *msg)
{
    nerr++;
    fprintf(stderr, "line %d, col %d: error: %s\n", t->line, t->col, msg);
}

/* 行内语义错误（手里只有 AST 节点时用节点行号定位） */
static void sem_error_line(int line, const char *msg)
{
    nerr++;
    fprintf(stderr, "line %d: error: %s\n", line, msg);
}

static void match(TokenType t)
{
    if (cur.type == t) advance();
    else {
        char msg[80];
        snprintf(msg, sizeof msg, "%s（期望 %s）",
                 cur.lexeme ? cur.lexeme : "?", token_name(t));
        syn_error(msg);
    }
}

static int is_compare(TokenType op)
{
    return op == T_LT || op == T_LE || op == T_GT ||
           op == T_GE || op == T_EQ || op == T_NEQ;
}

/* void 调用出现在"要值"的位置：报错并把类型改成 TY_ERR（防再级联）。
 * `f();` 表达式语句是唯一不查的地方——值被丢弃，void 合法。 */
static AstNode *check_valued(AstNode *e, int line)
{
    if (e->type == TY_VOID) {
        sem_error_line(line, "void 函数调用没有值，不能用在表达式里");
        e->type = TY_ERR;
    }
    return e;
}

/* ---------------- 类型检查（同 lab5 + void 传播） ---------------- */

static AstNode *make_binop(TokenType op, AstNode *l, AstNode *r, int line)
{
    /* void 调用做操作数：明确报错（不能静默改 TY_ERR——那会把错误
     * 吞掉，程序竟被判合法） */
    if (l->type == TY_VOID) {
        sem_error_line(line, "void 函数调用没有值，不能用在表达式里");
        l->type = TY_ERR;
    }
    if (r->type == TY_VOID) {
        sem_error_line(line, "void 函数调用没有值，不能用在表达式里");
        r->type = TY_ERR;
    }
    if (l->type == TY_ERR || r->type == TY_ERR)
        return ast_binop(op, l, r, TY_ERR, line);

    if (op == T_PERCENT && (l->type == TY_FLOAT || r->type == TY_FLOAT)) {
        sem_error_line(line, "浮点数不能取模");
        return ast_binop(op, l, r, TY_ERR, line);
    }

    if (l->type != r->type) {
        if (l->type == TY_INT) l = ast_i2f(l, line);
        else                   r = ast_i2f(r, line);
    }
    Type t = is_compare(op) ? TY_INT : l->type;
    return ast_binop(op, l, r, t, line);
}

/* 赋值/初始化/参数传递/return 共用：两侧类型要"合得上" */
static AstNode *coerce_assign(Type target, AstNode *e, int line)
{
    if (e->type == TY_VOID) {
        sem_error_line(line, "void 函数调用没有值，不能用在表达式里");
        e->type = TY_ERR;
        return e;
    }
    if (e->type == TY_ERR || e->type == target) return e;
    if (target == TY_FLOAT) return ast_i2f(e, line);

    char msg[96];
    snprintf(msg, sizeof msg, "不能把 float 赋给 %s（隐式收窄被禁止）",
             type_name(target));
    sem_error_line(line, msg);
    return e;
}

/* 数组下标必须是 int（6.4）：float 下标报错并置 TY_ERR 防级联 */
static AstNode *check_index(AstNode *idx, int line)
{
    if (idx->type == TY_FLOAT) {
        sem_error_line(line, "数组下标必须是 int");
        idx->type = TY_ERR;
    }
    return idx;
}

/* ---------------- 表达式 ---------------- */

static AstNode *parse_expr(void);

static AstNode *parse_factor(void)
{
    int line = cur.line;
    if (cur.type == T_LPAREN) {
        advance();
        AstNode *e = parse_expr();
        match(T_RPAREN);
        return e;
    }
    if (cur.type == T_MINUS) {
        advance();
        return ast_neg(check_valued(parse_factor(), line), line);
    }
    if (cur.type == T_NOT) {          /* !e：值翻转，不涉及短路 */
        advance();
        return ast_not(check_valued(parse_factor(), line), line);
    }
    if (cur.type == T_INT_LIT) {
        AstNode *e = ast_int(cur.ival, line);
        advance();
        return e;
    }
    if (cur.type == T_FLOAT_LIT) {
        AstNode *e = ast_float(cur.dval, line);
        advance();
        return e;
    }
    if (cur.type == T_IDENT && nxt.type == T_LPAREN) {
        /* ---- 函数调用：IDENT ( args )（龙书 6.7） ---- */
        Token name_tok = cur;
        advance();                     /* IDENT */
        advance();                     /* '('   */

        Sym *sym = sym_lookup(name_tok.lexeme);
        if (!sym) {
            char msg[96];
            snprintf(msg, sizeof msg, "调用了未定义的函数 %s",
                     name_tok.lexeme);
            sem_error(&name_tok, msg);
        } else if (sym->kind != SYM_FUNC) {
            char msg[96];
            snprintf(msg, sizeof msg, "%s 不是函数", name_tok.lexeme);
            sem_error(&name_tok, msg);
            sym = NULL;                /* 当作没找到，走错误恢复 */
        }

        /* 解析实参（记录每个实参的行号，coerce 报错定位用） */
        AstNode *head = NULL, *tail = NULL;
        int nargs = 0;
        int arg_lines[MAX_PARAMS + 1];
        if (cur.type != T_RPAREN) {
            for (;;) {
                int aline = cur.line;
                AstNode *arg = check_valued(parse_expr(), aline);
                if (nargs <= MAX_PARAMS) arg_lines[nargs] = aline;
                nargs++;
                if (tail) tail->next = arg;
                else      head = arg;
                tail = arg;
                if (cur.type == T_COMMA) { advance(); continue; }
                break;
            }
        }
        match(T_RPAREN);

        if (!sym) return ast_err(line);           /* 未定义/非函数 */

        if (nargs > MAX_PARAMS) {
            char msg[80];
            snprintf(msg, sizeof msg, "实参太多（最多 %d 个）", MAX_PARAMS);
            sem_error(&name_tok, msg);
            return ast_err(line);
        }
        if (nargs != sym->nparams) {
            char msg[96];
            snprintf(msg, sizeof msg, "函数 %s 需要 %d 个实参，实给 %d 个",
                     sym->name, sym->nparams, nargs);
            sem_error(&name_tok, msg);
            return ast_err(line);
        }
        /* 逐参做"赋值式"类型检查：float 形参收 int 实参 → 插 i2f；
         * int 形参收 float 实参 → 报错（隐式收窄禁止，同赋值规则）。
         * coerce 可能给节点包一层 i2f（换新指针），用二级指针重写链。 */
        {
            AstNode **slot = &head;
            int i = 0;
            while (*slot) {
                *slot = coerce_assign(sym->ptypes[i], *slot, arg_lines[i]);
                slot = &(*slot)->next;
                i++;
            }
        }
        return ast_call(sym, head, line);
    }
    if (cur.type == T_IDENT) {
        Token name_tok = cur;
        Sym *sym = sym_lookup(name_tok.lexeme);
        if (!sym) {
            char msg[96];
            snprintf(msg, sizeof msg, "使用了未声明的变量 %s",
                     name_tok.lexeme);
            sem_error(&name_tok, msg);
            advance();
            return ast_err(line);
        }
        if (sym->kind == SYM_FUNC) {   /* f 不带括号：函数名不是值 */
            char msg[96];
            snprintf(msg, sizeof msg, "函数名 %s 不能当变量用",
                     name_tok.lexeme);
            sem_error(&name_tok, msg);
            advance();
            return ast_err(line);
        }
        if (nxt.type == T_LBRACKET) {  /* ---- a[i]：数组读（6.4） ---- */
            if (sym->arr_len == 0) {
                char msg[96];
                snprintf(msg, sizeof msg, "标量 %s 不能用下标访问",
                         name_tok.lexeme);
                sem_error(&name_tok, msg);
                advance();             /* 消耗掉 'IDENT [ expr ]'，防级联 */
                advance();
                (void)parse_expr();
                match(T_RBRACKET);
                return ast_err(line);
            }
            advance();                 /* IDENT */
            advance();                 /* '['   */
            int iline = cur.line;
            AstNode *idx = check_valued(parse_expr(), iline);
            match(T_RBRACKET);
            idx = check_index(idx, iline);
            return ast_index(sym, idx, line);
        }
        if (sym->arr_len > 0) {
            /* 数组名不带下标：MiniC 没有指针退化，数组本身不是值 */
            char msg[96];
            snprintf(msg, sizeof msg, "数组 %s 必须带下标使用",
                     name_tok.lexeme);
            sem_error(&name_tok, msg);
            advance();
            return ast_err(line);
        }
        AstNode *e = ast_var(sym, line);
        advance();
        return e;
    }
    syn_error(cur.lexeme ? cur.lexeme : "<EOF>");
    advance();
    return ast_err(line);
}

static AstNode *parse_term(void)
{
    AstNode *acc = parse_factor();
    while (cur.type == T_STAR || cur.type == T_SLASH || cur.type == T_PERCENT) {
        int line = cur.line;
        TokenType op = cur.type;
        advance();
        acc = make_binop(op, acc, parse_factor(), line);
    }
    return acc;
}

static AstNode *parse_add(void)
{
    AstNode *acc = parse_term();
    while (cur.type == T_PLUS || cur.type == T_MINUS) {
        int line = cur.line;
        TokenType op = cur.type;
        advance();
        acc = make_binop(op, acc, parse_term(), line);
    }
    return acc;
}

static AstNode *parse_rel(void)
{
    AstNode *acc = parse_add();
    while (cur.type == T_LT || cur.type == T_LE ||
           cur.type == T_GT || cur.type == T_GE) {
        int line = cur.line;
        TokenType op = cur.type;
        advance();
        acc = make_binop(op, acc, parse_add(), line);
    }
    return acc;
}

static AstNode *parse_equality(void)
{
    AstNode *acc = parse_rel();
    while (cur.type == T_EQ || cur.type == T_NEQ) {
        int line = cur.line;
        TokenType op = cur.type;
        advance();
        acc = make_binop(op, acc, parse_rel(), line);
    }
    return acc;
}

/* and → equality { '&&' equality } —— 短路与（求值顺序在 IR 翻译里体现）。
 * void 检查只在**确实出现 &&** 时对两侧做——`banner();` 这种裸 void
 * 调用语句走到这里时还没有 &&，不能拦。 */
static AstNode *parse_and(void)
{
    AstNode *acc = parse_equality();
    while (cur.type == T_ANDAND) {
        int line = cur.line;
        advance();
        AstNode *l = check_valued(acc, line);
        AstNode *r = check_valued(parse_equality(), line);
        acc = ast_logic(A_AND, l, r, line);
    }
    return acc;
}

/* or → and { '||' and } —— 短路或；优先级最低（|| < && < ==，同 C） */
static AstNode *parse_or(void)
{
    AstNode *acc = parse_and();
    while (cur.type == T_OROR) {
        int line = cur.line;
        advance();
        AstNode *l = check_valued(acc, line);
        AstNode *r = check_valued(parse_and(), line);
        acc = ast_logic(A_OR, l, r, line);
    }
    return acc;
}

static AstNode *parse_expr(void) { return parse_or(); }

/* ---------------- 语句 ---------------- */

static AstNode *parse_stmt(int loop_depth, Sym *in_func);

static AstNode *parse_decl(void)
{
    Type t = cur.type == T_KW_INT ? TY_INT : TY_FLOAT;
    advance();

    if (cur.type != T_IDENT) {
        syn_error(cur.lexeme);
        while (cur.type != T_SEMI && cur.type != T_EOF) advance();
        if (cur.type == T_SEMI) advance();
        return NULL;
    }
    Token name_tok = cur;
    advance();

    /* ---- 数组后缀：int a[8];（长度 = 编译期整型字面量，6.4） ---- */
    int arr_len = 0;
    if (cur.type == T_LBRACKET) {
        advance();
        if (cur.type == T_INT_LIT) {
            arr_len = (int)cur.ival;
            advance();
            if (arr_len <= 0)
                sem_error(&name_tok, "数组长度必须为正");
        } else {
            sem_error(&cur, "数组长度必须是整型字面量");
        }
        match(T_RBRACKET);
    }

    AstNode *init = NULL;
    if (arr_len > 0 && cur.type == T_ASSIGN) {
        sem_error(&name_tok, "数组不能整体初始化，请逐元素赋值");
        advance();
        (void)parse_expr();            /* 解析后丢弃，防错误扩散 */
    } else if (cur.type == T_ASSIGN) {
        advance();
        init = check_valued(parse_expr(), name_tok.line);
    }
    match(T_SEMI);

    Sym *sym = arr_len > 0
             ? sym_declare_array(name_tok.lexeme, t, arr_len, name_tok.line)
             : sym_declare(name_tok.lexeme, t, name_tok.line);
    if (!sym) {
        char msg[96];
        snprintf(msg, sizeof msg, "重复声明 %s（当前作用域已有同名符号）",
                 name_tok.lexeme);
        sem_error(&name_tok, msg);
    }
    if (init && sym)
        init = coerce_assign(sym->type, init, name_tok.line);
    return ast_decl(sym, init, name_tok.line);
}

static AstNode *parse_assign(int eat_semi)
{
    Token name_tok = cur;
    Sym *sym = sym_lookup(cur.lexeme);
    advance();                     /* IDENT */
    advance();                     /* '='   */

    AstNode *rhs = check_valued(parse_expr(), name_tok.line);
    if (eat_semi) match(T_SEMI);   /* for 的子句以 ')' 结尾，不吞分号 */

    if (!sym) {
        char msg[96];
        snprintf(msg, sizeof msg, "赋值给未声明的变量 %s", name_tok.lexeme);
        sem_error(&name_tok, msg);
        return ast_assign(NULL, rhs, name_tok.line);
    }
    if (sym->kind == SYM_FUNC) {
        char msg[96];
        snprintf(msg, sizeof msg, "不能给函数 %s 赋值", name_tok.lexeme);
        sem_error(&name_tok, msg);
        return ast_assign(NULL, rhs, name_tok.line);
    }
    rhs = coerce_assign(sym->type, rhs, name_tok.line);
    return ast_assign(sym, rhs, name_tok.line);
}

/* a[i] = e ; —— 数组元素赋值（6.4）。dispatch 保证进入时 cur=IDENT、
 * nxt='['。'=' 是强制的：`a[i];` 这种裸下标语句没有值效果，不合法，
 * 在 match(T_ASSIGN) 处报语法错误即可。 */
static AstNode *parse_store(int eat_semi)
{
    Token name_tok = cur;
    Sym *sym = sym_lookup(name_tok.lexeme);
    advance();                     /* IDENT */
    advance();                     /* '['   */

    int iline = cur.line;
    AstNode *idx = check_valued(parse_expr(), iline);
    match(T_RBRACKET);
    idx = check_index(idx, iline);
    match(T_ASSIGN);

    AstNode *rhs = check_valued(parse_expr(), name_tok.line);
    if (eat_semi) match(T_SEMI);

    if (!sym) {
        char msg[96];
        snprintf(msg, sizeof msg, "赋值给未声明的变量 %s", name_tok.lexeme);
        sem_error(&name_tok, msg);
        return ast_store(NULL, idx, rhs, name_tok.line);
    }
    if (sym->kind == SYM_FUNC || sym->arr_len == 0) {
        char msg[96];
        snprintf(msg, sizeof msg, "%s 不是可以下标赋值的数组",
                 name_tok.lexeme);
        sem_error(&name_tok, msg);
        return ast_store(sym->kind == SYM_FUNC ? NULL : sym,
                         idx, rhs, name_tok.line);
    }
    rhs = coerce_assign(sym->type, rhs, name_tok.line);  /* 元素类型 */
    return ast_store(sym, idx, rhs, name_tok.line);
}

/* ---- 辅助：子树内是否含 continue（不下潜嵌套循环——那里的
 * continue 归嵌套循环自己管）。用于 for 降糖的语义限制检查。 ---- */
static int has_continue_in(const AstNode *n)
{
    for (; n; n = n->next) {
        if (n->kind == A_CONTINUE) return 1;
        if (n->kind == A_WHILE || n->kind == A_FUNCDEF) continue;
        if (has_continue_in(n->a)) return 1;
        if (has_continue_in(n->b)) return 1;
        if (has_continue_in(n->c)) return 1;
    }
    return 0;
}

/* for (init; cond; step) stmt —— lab10 新增。**纯语法糖**：拼装成
 * 既有 AST 组合 { init; while (cond) { body; step; } }，gen 与四个
 * 执行引擎零感知。三个子句均可空（cond 空 = 恒真）。
 * 限制：body 内暂不支持 continue——降糖后 continue 的落点是 while
 * 循环头（会跳过 step），与 C 的 for 语义不符；支持它需要把 step 深
 * 拷贝到每个 continue 点，留作练习。break 的落点在循环外，语义天然
 * 正确。整个 for 包在自己的作用域：init 处声明的变量不出循环。 */
static AstNode *parse_for(int loop_depth, Sym *in_func)
{
    int line = cur.line;
    advance();                                  /* for */
    match(T_LPAREN);

    scope_push();                               /* init 的声明限于本循环 */

    /* ---- init 子句：声明 / 标量赋值 / 数组元素写 / 空 ----
     * 声明式 init 由 parse_decl 自带吞分号；赋值/数组元素写两式以
     * eat_semi=0 复用语句版函数，分号必须在这里补吃——否则 cond 子句
     * 会把 init 的分号误当自己的终结符（cond 静默变恒真、step 报错）。 */
    AstNode *init = NULL;
    if (cur.type == T_SEMI)
        advance();
    else if (cur.type == T_KW_INT || cur.type == T_KW_FLOAT)
        init = parse_decl();                    /* 自带分号 */
    else if (cur.type == T_IDENT && nxt.type == T_ASSIGN) {
        init = parse_assign(0);
        match(T_SEMI);
    }
    else if (cur.type == T_IDENT && nxt.type == T_LBRACKET) {
        init = parse_store(0);
        match(T_SEMI);
    }
    else {
        syn_error(cur.lexeme ? cur.lexeme : "<EOF>");
        while (cur.type != T_SEMI && cur.type != T_EOF) advance();
        if (cur.type == T_SEMI) advance();
    }

    /* ---- cond 子句：可空 = 恒真 ---- */
    AstNode *cond = NULL;
    if (cur.type != T_SEMI)
        cond = check_valued(parse_expr(), line);
    match(T_SEMI);

    /* ---- step 子句：标量赋值 / 数组元素写 / 空（以 ')' 结尾） ---- */
    AstNode *step = NULL;
    if (cur.type != T_RPAREN) {
        if (cur.type == T_IDENT && nxt.type == T_ASSIGN)
            step = parse_assign(0);
        else if (cur.type == T_IDENT && nxt.type == T_LBRACKET)
            step = parse_store(0);
        else {
            syn_error(cur.lexeme ? cur.lexeme : "?");
            while (cur.type != T_RPAREN && cur.type != T_EOF) advance();
        }
    }
    match(T_RPAREN);

    /* ---- 循环体：loop_depth+1 使 break/continue 在语法层合法 ---- */
    AstNode *body = parse_stmt(loop_depth + 1, in_func);
    if (has_continue_in(body))
        sem_error_line(line, "for 循环体内的 continue 暂不支持（可改写为 while）");
    scope_pop();

    if (!cond) cond = ast_int(1, line);

    /* ---- 拼装：{ init; while (cond) { body; step; } } ----
     * body 与 step 串成语句链后**必须包进 A_BLOCK**：while 的体按
     * "单条语句"处理，链上的兄弟节点（step）不包块就永远不会执行。 */
    AstNode *inner_list = NULL, *inner_tail = NULL;
    if (body) { inner_list = body; inner_tail = body; }
    if (step) {
        if (inner_tail) inner_tail->next = step;
        else            inner_list = step;
        inner_tail = step;
    }
    AstNode *inner = inner_list ? ast_block(inner_list, line)
                                : ast_empty(line);
    AstNode *wh = ast_while(cond, inner, line);

    AstNode *head;
    if (init) { init->next = wh; head = init; }
    else      head = wh;
    return ast_block(head, line);
}

/* return [ expr ] ; —— 继承属性 in_func 决定合法性；
 * 规则：void 函数只许裸 return；非 void 函数 return 必须带值
 * （但函数体可以一条 return 都不写——落到函数尾返回零值）。 */
static AstNode *parse_return(Sym *in_func)
{
    Token kw = cur;
    int line = cur.line;
    advance();

    if (!in_func) {
        sem_error(&kw, "return 在函数外");
    } else if (in_func->type == TY_VOID) {
        if (cur.type != T_SEMI)
            sem_error(&kw, "void 函数不能 return 值");
    }
    AstNode *e = NULL;
    if (cur.type != T_SEMI) {
        e = check_valued(parse_expr(), line);
        if (in_func && in_func->type != TY_VOID)
            e = coerce_assign(in_func->type, e, line);
        /* void 函数带 return 值：上面已报错，表达式照常解析丢弃 */
    }
    match(T_SEMI);
    return ast_return(e, line);
}

static AstNode *parse_stmt_list(int loop_depth, Sym *in_func, int stop_rbrace)
{
    AstNode *head = NULL, *tail = NULL;
    while (cur.type != T_EOF &&
           !(stop_rbrace && cur.type == T_RBRACE)) {
        AstNode *s = parse_stmt(loop_depth, in_func);
        if (s) {
            if (tail) tail->next = s;
            else      head = s;
            tail = s;
        }
    }
    return head;
}

/* 声明只能出现在块里。作为 if/while 的直接执行体时，名字在编译期就登记
 * 进外层作用域、运行期却要执行到才绑定——条件为假即得"编译可见、运行
 * 未绑定"的分裂局面（-eval 会空指针解引用，IRVM 静默读 0，两引擎对拍
 * 直接分裂）。C99 同样不允许声明作受控语句，这里对齐；for 不需要此门禁
 * ——parse_for 自带 scope_push 包裹，声明落在循环自己的作用域里。 */
static AstNode *no_decl_body(AstNode *s, int line)
{
    if (s && s->kind == A_DECL)
        sem_error_line(line, "声明不能直接作为 if/while 的执行体——请用 { } 包住");
    return s;
}

static AstNode *parse_stmt(int loop_depth, Sym *in_func)
{
    int line = cur.line;
    switch (cur.type) {
    case T_KW_INT: case T_KW_FLOAT:
        return parse_decl();

    case T_KW_VOID:
        sem_error(&cur, "void 只能做函数返回类型（不存在 void 变量）");
        while (cur.type != T_SEMI && cur.type != T_EOF) advance();
        if (cur.type == T_SEMI) advance();
        return NULL;

    case T_KW_RETURN:
        return parse_return(in_func);

    case T_KW_PRINT: {
        advance();
        AstNode *e = check_valued(parse_expr(), line);
        match(T_SEMI);
        return ast_print(e, line);
    }

    case T_KW_IF: {
        advance();
        match(T_LPAREN);
        AstNode *cond = check_valued(parse_expr(), line);
        match(T_RPAREN);
        AstNode *th = no_decl_body(parse_stmt(loop_depth, in_func), line);
        AstNode *el = NULL;
        if (cur.type == T_KW_ELSE) {
            advance();
            el = no_decl_body(parse_stmt(loop_depth, in_func), line);
        }
        return ast_if(cond, th, el, line);
    }

    case T_KW_WHILE: {
        advance();
        match(T_LPAREN);
        AstNode *cond = check_valued(parse_expr(), line);
        match(T_RPAREN);
        AstNode *body = no_decl_body(parse_stmt(loop_depth + 1, in_func), line);
        return ast_while(cond, body, line);
    }

    case T_KW_FOR:
        return parse_for(loop_depth, in_func);

    case T_KW_BREAK: case T_KW_CONTINUE: {
        const char *what = cur.type == T_KW_BREAK ? "break" : "continue";
        AstKind k = cur.type == T_KW_BREAK ? A_BREAK : A_CONTINUE;
        if (loop_depth == 0) {
            char msg[64];
            snprintf(msg, sizeof msg, "%s 在循环外", what);
            sem_error(&cur, msg);
        }
        advance();
        match(T_SEMI);
        return k == A_BREAK ? ast_break(line) : ast_continue(line);
    }

    case T_LBRACE: {
        advance();
        scope_push();
        AstNode *body = parse_stmt_list(loop_depth, in_func, 1);
        scope_pop();
        match(T_RBRACE);
        return ast_block(body, line);
    }

    case T_SEMI:
        advance();
        return ast_empty(line);

    default:
        if (cur.type == T_IDENT && nxt.type == T_ASSIGN)
            return parse_assign(1);
        if (cur.type == T_IDENT && nxt.type == T_LBRACKET)
            return parse_store(1);
        {   /* 裸表达式语句：expr ;（含 `f();`——void 调用唯一合法位置） */
            AstNode *e = parse_expr();
            match(T_SEMI);
            return ast_exprstmt(e, line);
        }
    }
}

/* ---- 函数定义：(int|float|void) IDENT ( params ) { stmt_list } ----
 * 进入本函数时 cur=类型关键字。签名**先登记再解析体**（递归可见）。 */
static AstNode *parse_funcdef(void)
{
    Type ret = cur.type == T_KW_INT   ? TY_INT
             : cur.type == T_KW_FLOAT ? TY_FLOAT : TY_VOID;
    advance();                          /* 类型关键字 */

    if (cur.type != T_IDENT) {
        syn_error(cur.lexeme);
        while (cur.type != T_SEMI && cur.type != T_EOF) advance();
        if (cur.type == T_SEMI) advance();
        return NULL;
    }
    Token name_tok = cur;
    advance();                          /* IDENT */
    match(T_LPAREN);                    /* 由 dispatch 保证存在，仍稳妥匹配 */

    /* 参数表：收集类型与名字（参数名稍后登记进函数体作用域） */
    Type ptypes[MAX_PARAMS];
    const char *pnames[MAX_PARAMS];
    int nparams = 0;
    if (cur.type != T_RPAREN) {
        for (;;) {
            if ((cur.type == T_KW_INT || cur.type == T_KW_FLOAT) &&
                nxt.type == T_IDENT && nparams < MAX_PARAMS) {
                ptypes[nparams] = cur.type == T_KW_INT ? TY_INT : TY_FLOAT;
                pnames[nparams] = nxt.lexeme;
                nparams++;
                advance();              /* 类型 */
                advance();              /* 名字 */
            } else if (nparams >= MAX_PARAMS) {
                char msg[80];
                snprintf(msg, sizeof msg, "形参太多（最多 %d 个）", MAX_PARAMS);
                sem_error(&cur, msg);
            } else {
                syn_error(cur.lexeme);
                break;
            }
            if (cur.type == T_COMMA) { advance(); continue; }
            break;
        }
    }
    match(T_RPAREN);
    match(T_LBRACE);

    /* 签名登记（时机在解析函数体之前——见 symtab.h 的对照说明）。
     * 重名失败时改用**已存在的那个符号**当 in_func：函数体照常解析，
     * 重复声明只报这一处错，不级联出"return 在函数外"的噪声。 */
    if (strcmp(name_tok.lexeme, "main") == 0) {
        /* 顶层段固定叫 main（见 gen 的入口段命名）：用户函数再叫
         * main 就重名了，irvm 调 main() 会命中顶层段而非函数体，
         * 静默错语义——按保留名拒绝。（自 lab6 回移植） */
        char msg[96];
        snprintf(msg, sizeof msg,
                 "main 是顶层段的保留名，函数不能命名为 main");
        sem_error(&name_tok, msg);
    }
    Sym *sym = sym_declare_func(name_tok.lexeme, ret, nparams, ptypes,
                                name_tok.line);
    if (!sym) {
        char msg[96];
        snprintf(msg, sizeof msg, "重复声明 %s（当前作用域已有同名符号）",
                 name_tok.lexeme);
        sem_error(&name_tok, msg);
        sym = sym_lookup(name_tok.lexeme);   /* 沿用已存在符号（哪怕是
                                              * 变量）：只报重复这一处错 */
    }

    /* 函数体作用域：参数与局部变量同层（与 C 一致） */
    scope_push();
    Sym *psyms[MAX_PARAMS];
    for (int i = 0; i < nparams; i++) {
        psyms[i] = sym_declare(pnames[i], ptypes[i], name_tok.line);
        if (!psyms[i]) {
            char msg[96];
            snprintf(msg, sizeof msg, "重复声明形参 %s", pnames[i]);
            sem_error(&name_tok, msg);
        }
    }
    AstNode *body = parse_stmt_list(0, sym, 1);
    scope_pop();
    match(T_RBRACE);

    if (!sym) return NULL;
    return ast_funcdef(sym, ast_block(body, name_tok.line),
                       psyms, nparams, name_tok.line);
}

/* ---------------- 入口 ---------------- */

AstNode *parse_program(const char *src, int *nerr_out)
{
    lexer_init(&lx, src);
    symtab_init();
    nerr = 0;
    cur  = lexer_next_clean(&lx);
    nxt  = lexer_next_clean(&lx);
    nxt2 = lexer_next_clean(&lx);

    AstNode *head = NULL, *tail = NULL;
    for (;;) {
        while (cur.type != T_EOF) {
            /* 顶层 dispatch：类型 IDENT ( 三连 → 函数定义，否则语句。
             * void 打头的只可能是函数（parse_stmt 会拒绝 void 变量） */
            int func_like =
                (cur.type == T_KW_INT  || cur.type == T_KW_FLOAT ||
                 cur.type == T_KW_VOID) &&
                nxt.type == T_IDENT && nxt2.type == T_LPAREN;
            AstNode *s = func_like ? parse_funcdef()
                                   : parse_stmt(0, NULL);
            if (s) {
                if (tail) tail->next = s;
                else      head = s;
                tail = s;
            }
        }
        if (cur.type == T_EOF) break;
        syn_error("}");
        advance();
    }

    if (lexer_had_error()) nerr++;
    *nerr_out = nerr;
    return ast_prog(head);
}
