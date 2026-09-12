#!/usr/bin/env bash
# lab4 测试：与 lab3 相同的表达式集对拍 + if/else + 悬空 else 冲突 + 错误拒绝
# 用法: bash tests/run_tests.sh   （在 lab4-parser-lr 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minicc4.exe
CASES=tests/cases
pass=0; fail=0

SRCS="src/lexer.c src/grammar.c src/slr.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab4_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab4_cc.log; exit 1
fi
if [ -s /tmp/lab4_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab4_cc.log; exit 1
fi

# 1) lab3 的表达式测试集：SLR 也应全部接受（左递归文法，无需消除左递归）
if "$BIN" "$CASES/expr.mc" 2>/dev/null | grep -q "SLR: accept"; then
    echo "[PASS] 表达式集(与 lab3 对拍)"; pass=$((pass+1))
else
    echo "[FAIL] 表达式集被拒绝"; fail=$((fail+1))
fi

# 2) if/else + 块语句程序
if "$BIN" "$CASES/ifelse.mc" 2>/dev/null | grep -q "SLR: accept"; then
    echo "[PASS] if/else 与块语句"; pass=$((pass+1))
else
    echo "[FAIL] if/else 程序被拒绝"; fail=$((fail+1))
fi

# 3) 恰好 1 个 shift/reduce 冲突，且在 ELSE 上，按移进消解
n=$("$BIN" "$CASES/expr.mc" 2>&1 >/dev/null | grep -c "SLR 冲突")
if [ "$n" -eq 1 ] && "$BIN" "$CASES/expr.mc" 2>&1 >/dev/null \
   | grep -q "T_KW_ELSE.*按惯例选择移进"; then
    echo "[PASS] 悬空 else 冲突被识别并按惯例消解(保持 shift)"; pass=$((pass+1))
else
    echo "[FAIL] 冲突数=$n（期望恰为 1、在 ELSE 上且消解为 shift）"; fail=$((fail+1))
fi

# 3b) 规范项目集族状态数：手工构造核实过的 52 个（防构造退化）
if "$BIN" "$CASES/expr.mc" 2>&1 >/dev/null | grep -q "\[stats\] states=52 "; then
    echo "[PASS] 项目集族 52 个状态"; pass=$((pass+1))
else
    echo "[FAIL] 状态数不是 52"; fail=$((fail+1))
fi

# 3c) reduce/reduce 计数分口径可见且为 0
"$BIN" "$CASES/expr.mc" 2>&1 >/dev/null | grep -q "reduce/reduce 冲突=0" \
    && { echo "[PASS] rr 冲突计数为 0"; pass=$((pass+1)); } \
    || { echo "[FAIL] 缺少 rr 冲突计数或非 0"; fail=$((fail+1)); }

# 4) 悬空 else 语义：移进优先 = else 绑定最近 if
#    trace 中必须先归约 P6(if..else stmt) 再归约 P5(if stmt)——顺序即绑定
if "$BIN" -trace "$CASES/ifelse.mc" 2>/dev/null | grep -A2 "reduce P6" | grep -q "reduce P5"; then
    echo "[PASS] else 绑定内层 if(trace 验证)"; pass=$((pass+1))
else
    echo "[FAIL] else 绑定顺序异常"; fail=$((fail+1))
fi

# 5) 语法错误拒绝且退出码 1
if "$BIN" "$CASES/errors.mc" >/dev/null 2>&1; then
    echo "[FAIL] 非法程序被接受"; fail=$((fail+1))
else
    echo "[PASS] 语法错误拒绝"; pass=$((pass+1))
fi

# 6) -states / -table 能跑（冒烟测试）
"$BIN" -states > /dev/null 2>&1 && "$BIN" -table > /dev/null 2>&1 \
    && { echo "[PASS] 项目集族/分析表输出"; pass=$((pass+1)); } \
    || { echo "[FAIL] -states/-table 崩溃"; fail=$((fail+1)); }

# 7) reduce/reduce 冲突按契约直接拒绝（slr.h 的承诺、README 练习 2）。
#    文法是编译期常量表，用 sed 注入一条 expr→INT_LIT（与 factor→INT_LIT
#    在同一状态各要归约、FOLLOW 相交 ⇒ rr）编一个变体二进制来验证。
mkdir -p /tmp/lab4_rr
sed '/\/\* P27 \*\//a\    { N(NT_EXPR),       { S(T_INT_LIT) }, 1 },   /* 注入：制造 rr 冲突 */' \
    src/grammar.c > /tmp/lab4_rr/grammar_rr.c
if gcc -std=c11 -Wall -Wextra -O2 -Isrc -o /tmp/lab4_rr/minicc4rr.exe \
      src/lexer.c /tmp/lab4_rr/grammar_rr.c src/slr.c src/main.c 2> /tmp/lab4_rr/cc.log; then
    if ! /tmp/lab4_rr/minicc4rr.exe "$CASES/expr.mc" >/dev/null 2> /tmp/lab4_rr/err.log \
       && grep -q "reduce/reduce 冲突无法用惯例消解" /tmp/lab4_rr/err.log; then
        echo "[PASS] reduce/reduce 冲突按契约直接拒绝"; pass=$((pass+1))
    else
        echo "[FAIL] rr 冲突未被拒绝"; cat /tmp/lab4_rr/err.log; fail=$((fail+1))
    fi
else
    echo "[FAIL] rr 变体编译失败"; cat /tmp/lab4_rr/cc.log; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "lab4: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
