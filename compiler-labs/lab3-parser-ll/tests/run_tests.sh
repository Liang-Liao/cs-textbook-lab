#!/usr/bin/env bash
# lab3 测试：求值对拍 + LL(1) 表/集对拍 + 错误恢复 + trace
# 用法: bash tests/run_tests.sh   （在 lab3-parser-ll 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minicc3.exe
CASES=tests/cases
pass=0; fail=0

SRCS="src/lexer.c src/grammar.c src/rd_parser.c src/ll_parser.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab3_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab3_cc.log; exit 1
fi
if [ -s /tmp/lab3_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab3_cc.log; exit 1
fi

check() { # check <名字> <实际文件> <期望文件>
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab3_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; head -20 /tmp/lab3_diff.log; fail=$((fail+1))
    fi
}

# 1) expr.mc：递归下降求值（20 个值）+ LL(1) 接受
"$BIN" "$CASES/expr.mc" > /tmp/lab3_expr.out 2>/dev/null
check "求值+接受" /tmp/lab3_expr.out "$CASES/expr.expected"

# 2) 两套分析器一致性：expr.mc 用 -ll 单独跑也应 accept
if "$BIN" -ll "$CASES/expr.mc" 2>/dev/null | grep -q "LL: accept"; then
    echo "[PASS] LL(1) 单独接受"; pass=$((pass+1))
else
    echo "[FAIL] LL(1) 拒绝了正确程序"; fail=$((fail+1))
fi

# 3) FIRST/FOLLOW 集对拍（手算过的经典结果）
"$BIN" -ff > /tmp/lab3_ff.out 2>/dev/null
check "FIRST/FOLLOW" /tmp/lab3_ff.out "$CASES/first_follow.expected"

# 4) LL(1) 分析表对拍 + 无冲突（退出码 0）
if "$BIN" -table > /tmp/lab3_table.out 2>/dev/null; then
    check "LL(1) 分析表" /tmp/lab3_table.out "$CASES/ll_table.expected"
else
    echo "[FAIL] 分析表构造报告了冲突"; fail=$((fail+1))
fi

# 5) 错误恢复：errors.mc 报错行覆盖第 3、4 行，退出码 1
"$BIN" "$CASES/errors.mc" > /dev/null 2>/tmp/lab3_err.log
rc=$?
if [ $rc -ne 0 ] && grep -q "line 3" /tmp/lab3_err.log && grep -q "line 4" /tmp/lab3_err.log; then
    echo "[PASS] panic 错误恢复(两处错误都报出)"; pass=$((pass+1))
else
    echo "[FAIL] 错误恢复: rc=$rc"; cat /tmp/lab3_err.log; fail=$((fail+1))
fi

# 5b) 整型取模的零除数：文案必须是"除数为零"——旧版只看运算符，
#     把 10 % 0 误报成"浮点数不能取模"
printf 'print 10 %% 0;\n' > /tmp/lab3_modz.mc
"$BIN" /tmp/lab3_modz.mc > /dev/null 2>/tmp/lab3_modz.log
if grep -q "除数为零" /tmp/lab3_modz.log \
   && ! grep -q "浮点数不能取模" /tmp/lab3_modz.log; then
    echo "[PASS] 取模零除数报错文案"; pass=$((pass+1))
else
    echo "[FAIL] 取模零除数文案异常"; cat /tmp/lab3_modz.log; fail=$((fail+1))
fi

# 5b) 恢复后照常分析：第 2 行(3)与第 5 行(9)的求值结果必须仍出现在
#     stdout——panic 恢复不是把后面的代码整个吞掉
"$BIN" "$CASES/errors.mc" > /tmp/lab3_rec.out 2>/dev/null
if [ "$(grep -cx 3 /tmp/lab3_rec.out)" -ge 1 ] && grep -qx 9 /tmp/lab3_rec.out; then
    echo "[PASS] 恢复后照常分析(3 与 9 均输出)"; pass=$((pass+1))
else
    echo "[FAIL] 恢复后丢输出"; cat /tmp/lab3_rec.out; fail=$((fail+1))
fi

# 5c) stdin '-' 路径（README 快速上手演示的管道用法）
printf 'print 6*7;\n' | "$BIN" -rd - > /tmp/lab3_stdin.out 2>/dev/null
if [ "$(cat /tmp/lab3_stdin.out)" = "42" ]; then
    echo "[PASS] stdin 管道读入"; pass=$((pass+1))
else
    echo "[FAIL] stdin 读入异常"; cat /tmp/lab3_stdin.out; fail=$((fail+1))
fi

# 6) trace 模式：能看到栈展开与匹配动作
if "$BIN" -ll -trace "$CASES/expr.mc" 2>/dev/null | grep -q "P0 : stmt_list -> stmt stmt_list"; then
    echo "[PASS] trace 可见最左推导"; pass=$((pass+1))
else
    echo "[FAIL] trace 输出异常"; fail=$((fail+1))
fi

# 6b) trace 可见记号匹配步（移进等价动作）
if "$BIN" -ll -trace "$CASES/expr.mc" 2>/dev/null | grep -q "匹配 print"; then
    echo "[PASS] trace 可见记号匹配"; pass=$((pass+1))
else
    echo "[FAIL] trace 缺少匹配动作"; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "lab3: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
