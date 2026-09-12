#!/usr/bin/env bash
# lab0 测试：正常用例对拍 + 错误用例检查退出码与报错信息
# 用法: bash tests/run_tests.sh   （在 lab0-intro 目录下执行）

set -u
cd "$(dirname "$0")/.."     # 进入 lab 根目录

BIN=./infix2postfix.exe
CASES=tests/cases

pass=0; fail=0

# 编译（零警告要求；警告也算失败）
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" src/infix2postfix.c 2> /tmp/lab0_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab0_cc.log; exit 1
fi
if [ -s /tmp/lab0_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab0_cc.log; exit 1
fi

check() { # check <名字> <实际> <期望>  （两侧都去掉 \r，Windows 文本模式兼容）
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab0_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; cat /tmp/lab0_diff.log; fail=$((fail+1))
    fi
}

# 1) 中缀 -> 后缀（纯翻译）
"$BIN" "$CASES/arith.in" > /tmp/lab0_postfix.out
check "后缀翻译" /tmp/lab0_postfix.out "$CASES/arith.postfix.expected"

# 2) 翻译 + 求值
"$BIN" -e "$CASES/arith.in" > /tmp/lab0_eval.out
check "翻译+求值" /tmp/lab0_eval.out "$CASES/arith.eval.expected"

# 3) 错误用例：第一条 (1+2 未闭合。程序在第一个错误处退出（lab0 尚无错误恢复）。
if "$BIN" "$CASES/errors.in" > /tmp/lab0_err.out 2>/tmp/lab0_err.log; then
    echo "[FAIL] 非法输入竟然被接受了"; fail=$((fail+1))
else
    if grep -q "括号未闭合" /tmp/lab0_err.log; then
        echo "[PASS] 括号未闭合报错"; pass=$((pass+1))
    else
        echo "[FAIL] 报错信息不对:"; cat /tmp/lab0_err.log; fail=$((fail+1))
    fi
fi

# 4) 取模零除数：与 '/' 同款守卫——干净报错退出，而不是 SIGFPE 崩溃。
#    （lab0 先打印后缀式再求值，故 stdout 有翻译行属正常）
printf '10 %% 0\n' > /tmp/lab0_moderr.in
"$BIN" -e /tmp/lab0_moderr.in > /tmp/lab0_moderr.out 2>/tmp/lab0_moderr.log; rc=$?
if [ $rc -ne 0 ] && grep -q "除数为零" /tmp/lab0_moderr.log; then
    echo "[PASS] 取模零除数报错"; pass=$((pass+1))
else
    echo "[FAIL] 取模零除数: rc=$rc"; cat /tmp/lab0_moderr.log; fail=$((fail+1))
fi

# 5) -v 过程显示：逐行展示 token/运算符栈/输出（README 快速上手演示的格式）
printf '3+4*2\n' | "$BIN" -v > /tmp/lab0_v.out
check "-v 过程显示" /tmp/lab0_v.out "$CASES/verbose.expected"

# 6) stdin 管道读入（缺省无参数即读 stdin，README 承诺）
echo "8-3-2" | "$BIN" -e > /tmp/lab0_pipe.out 2>&1
check "stdin 管道读入" /tmp/lab0_pipe.out "$CASES/pipe.expected"

# 7) 缺操作数：单独 '+' 报"操作数不足"而不是 vals[--vn] 下溢
printf '+\n' | "$BIN" -e > /dev/null 2>/tmp/lab0_opd.log; rc=$?
if [ $rc -ne 0 ] && grep -q "操作数不足" /tmp/lab0_opd.log; then
    echo "[PASS] 缺操作数报错"; pass=$((pass+1))
else
    echo "[FAIL] 缺操作数: rc=$rc"; cat /tmp/lab0_opd.log; fail=$((fail+1))
fi

# 8) 相邻数字（strtod 把 1.2.3 切成两个数）求值结束必须恰好剩一个结果
printf '1.2.3\n' | "$BIN" -e > /dev/null 2>/tmp/lab0_adj.log; rc=$?
if [ $rc -ne 0 ] && grep -q "操作数" /tmp/lab0_adj.log; then
    echo "[PASS] 相邻数字报错"; pass=$((pass+1))
else
    echo "[FAIL] 相邻数字: rc=$rc"; cat /tmp/lab0_adj.log; fail=$((fail+1))
fi

# 9) 容量守卫：括号过深 -> 运算符栈溢出；超长表达式 -> 表达式过长
{ printf '%0.s(' {1..300}; echo; } | "$BIN" -e >/dev/null 2>/tmp/lab0_deep.log; rc=$?
if [ $rc -ne 0 ] && grep -q "运算符栈溢出" /tmp/lab0_deep.log; then
    echo "[PASS] 括号过深报错"; pass=$((pass+1))
else
    echo "[FAIL] 括号过深: rc=$rc"; cat /tmp/lab0_deep.log; fail=$((fail+1))
fi
printf '1+%.0s' {1..800} | "$BIN" >/dev/null 2>/tmp/lab0_long.log; rc=$?
if [ $rc -ne 0 ] && grep -q "表达式过长" /tmp/lab0_long.log; then
    echo "[PASS] 超长表达式报错"; pass=$((pass+1))
else
    echo "[FAIL] 超长表达式: rc=$rc"; cat /tmp/lab0_long.log; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "lab0: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
