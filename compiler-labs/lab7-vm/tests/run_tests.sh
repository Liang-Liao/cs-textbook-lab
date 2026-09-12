#!/usr/bin/env bash
# lab7 测试：旧五集第五跑（VM 输出与 lab3~lab6 逐字节一致）
#               + 三引擎对拍（栈式VM vs C递归irvm vs 树遍eval）
#               + 栈溢出拦截 + -trace 冒烟 + 类型错误拒绝
# 用法: bash tests/run_tests.sh   （在 lab7-vm 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minicc7.exe
CASES=tests/cases
pass=0; fail=0

SRCS="src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c src/ir.c src/gen.c src/irvm.c src/vm.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab7_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab7_cc.log; exit 1
fi
if [ -s /tmp/lab7_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab7_cc.log; exit 1
fi

check() { # check <名字> <实际文件> <期望文件>
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab7_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; head -20 /tmp/lab7_diff.log; fail=$((fail+1))
    fi
}

# 1) expr/vars/bool/func/ir/arr 六套：同一测试集第五跑——
#    换了执行引擎，输出必须仍逐字节一致（期望文件原样沿用 lab6 的）
for c in expr vars bool func ir arr; do
    "$BIN" "$CASES/$c.mc" > /tmp/lab7_$c.out 2>/dev/null
    check "执行 $c.mc(与 lab3~lab6 对拍,第五跑)" /tmp/lab7_$c.out "$CASES/$c.expected"
done

# 2) vm.mc：显式活动记录的专属用例（递归中同名多份、按值传递、全局/局部）
"$BIN" "$CASES/vm.mc" > /tmp/lab7_vm.out 2>/dev/null
check "栈帧语义(vm.mc: 递归帧独立/按值/全局)" /tmp/lab7_vm.out "$CASES/vm.expected"

# 2b) arr.mc 在栈帧语义下的第二跑：数组占帧内连续格、递归各拥其帧
"$BIN" "$CASES/arr.mc" > /tmp/lab7_arrvm.out 2>/dev/null
if diff <(tr -d '\r' < /tmp/lab7_arrvm.out) <(tr -d '\r' < "$CASES/arr.expected") > /dev/null; then
    echo "[PASS] 数组的帧布局(连续槽位,与 lab6 对拍)"; pass=$((pass+1))
else
    echo "[FAIL] 数组帧布局"; fail=$((fail+1))
fi

# 3) -ir dump 对拍不变（gen 一字未动，回填结果照旧）
"$BIN" -ir "$CASES/ir.mc" > /tmp/lab7_irdump.out 2>/dev/null
check "四元式 dump(回填+名字唯一化)" /tmp/lab7_irdump.out "$CASES/ir_dump.expected"

# 4) 三引擎对拍：栈式 VM(默认) vs C 递归解释器(-irvm) vs 树遍(-eval)
#    同一程序三种执行方式输出必须一致（lab6 双引擎对拍的加厚版）
allok=1
for c in expr vars bool func ir arr vm; do
    "$BIN"          "$CASES/$c.mc" > /tmp/lab7_a.out 2>/dev/null
    "$BIN" -irvm    "$CASES/$c.mc" > /tmp/lab7_b.out 2>/dev/null
    "$BIN" -eval    "$CASES/$c.mc" > /tmp/lab7_c.out 2>/dev/null
    diff <(tr -d '\r' < /tmp/lab7_a.out) <(tr -d '\r' < /tmp/lab7_b.out) > /dev/null \
        && diff <(tr -d '\r' < /tmp/lab7_a.out) <(tr -d '\r' < /tmp/lab7_c.out) > /dev/null \
        || { allok=0; echo "  -> $c.mc 三引擎输出不一致"; }
done
if [ $allok -eq 1 ]; then
    echo "[PASS] 三引擎对拍(VM vs irvm vs eval, 7 个程序)"; pass=$((pass+1))
else
    echo "[FAIL] 三引擎对拍"; fail=$((fail+1))
fi

# 5) 类型/语义错误：退出码 1、九类错误各报出、不执行（stdout 无输出）
"$BIN" "$CASES/typeerr.mc" > /tmp/lab7_tout.log 2> /tmp/lab7_terr.log
rc=$?
if [ $rc -ne 0 ] && [ ! -s /tmp/lab7_tout.log ] \
   && grep -q "重复声明 f" /tmp/lab7_terr.log \
   && grep -q "重复声明 g" /tmp/lab7_terr.log \
   && grep -q "调用了未定义的函数" /tmp/lab7_terr.log \
   && grep -q "需要 2 个实参" /tmp/lab7_terr.log \
   && grep -q "隐式收窄" /tmp/lab7_terr.log \
   && grep -c "void 函数调用没有值" /tmp/lab7_terr.log | grep -qx "2" \
   && grep -q "return 在函数外" /tmp/lab7_terr.log \
   && grep -q "语法错误" /tmp/lab7_terr.log; then
    echo "[PASS] 9 类错误全部报出且不执行"; pass=$((pass+1))
else
    echo "[FAIL] 错误检查: rc=$rc"; cat /tmp/lab7_terr.log; fail=$((fail+1))
fi

# 6) 运行时错误依旧停机：除零（编译期拦不住的那类）
printf 'print 1 / 0;\n' > /tmp/lab7_divz.mc
if ! "$BIN" /tmp/lab7_divz.mc > /dev/null 2>&1; then
    echo "[PASS] 整数除零停机"; pass=$((pass+1))
else
    echo "[FAIL] 除零未被拦截"; fail=$((fail+1))
fi

# 7) 无穷递归 → 栈溢出被拦下（lab6 没有的运行时保障：真调用栈会爆掉，
#    VM 的显式栈在 STACK_MAX 处报中文错误并退出非零）
printf 'int loop(int n) { return loop(n + 1); }\nprint loop(0);\n' > /tmp/lab7_ovf.mc
"$BIN" /tmp/lab7_ovf.mc > /tmp/lab7_vout.log 2> /tmp/lab7_verr.log
rc=$?
if [ $rc -ne 0 ] && [ ! -s /tmp/lab7_vout.log ] \
   && grep -q "栈溢出" /tmp/lab7_verr.log; then
    echo "[PASS] 无穷递归触发栈溢出且停机"; pass=$((pass+1))
else
    echo "[FAIL] 栈溢出检查: rc=$rc"; head -3 /tmp/lab7_verr.log; fail=$((fail+1))
fi

# 8) -trace 冒烟：stderr 出现 call/ret 的活动记录快照，stdout 不受影响
"$BIN" -trace "$CASES/vm.mc" > /tmp/lab7_trout.log 2> /tmp/lab7_trerr.log
if grep -q "\[vm\] call down" /tmp/lab7_trerr.log \
   && grep -q "\[vm\] ret  down" /tmp/lab7_trerr.log \
   && diff <(tr -d '\r' < /tmp/lab7_trout.log) \
           <(tr -d '\r' < "$CASES/vm.expected") > /dev/null; then
    echo "[PASS] -trace 帧快照(stderr)且 stdout 照常对拍"; pass=$((pass+1))
else
    echo "[FAIL] -trace 输出异常"; head -5 /tmp/lab7_trerr.log; fail=$((fail+1))
fi

# 8b) 帧快照数值断言：fp/sp 的具体值也要对（README 节选即冻结样本）——
#     防止有人改坏帧布局后 trace 仍"看起来在打印"
if grep -q "\[vm\] call down      深1   fp=4     sp=14    实参(n=4) 局部6格" /tmp/lab7_trerr.log \
   && grep -q "回到 down:10  sp=47" /tmp/lab7_trerr.log; then
    echo "[PASS] 帧快照 fp/sp 数值精确匹配"; pass=$((pass+1))
else
    echo "[FAIL] fp/sp 数值漂移（帧布局变了？）"; fail=$((fail+1))
fi

# 8c) int 边界值比较：三方引擎与语义一致（compare 已按整型精确比较）
printf 'int x = 2147483647;\nprint x > 0;\nprint x + 1 < 0;\nprint x == 2147483647;\n' \
    > /tmp/lab7_intedge.mc
"$BIN"          /tmp/lab7_intedge.mc > /tmp/lab7_ie_a.out 2>/dev/null
"$BIN" -irvm    /tmp/lab7_intedge.mc > /tmp/lab7_ie_b.out 2>/dev/null
"$BIN" -eval    /tmp/lab7_intedge.mc > /tmp/lab7_ie_c.out 2>/dev/null
if diff -q <(tr -d '\r' < /tmp/lab7_ie_a.out) <(tr -d '\r' < /tmp/lab7_ie_b.out) >/dev/null \
   && diff -q <(tr -d '\r' < /tmp/lab7_ie_a.out) <(tr -d '\r' < /tmp/lab7_ie_c.out) >/dev/null \
   && [ "$(cat /tmp/lab7_ie_a.out | tr -d ' \r\n')" = "111" ]; then
    echo "[PASS] int 边界比较三引擎一致(含 32 位回绕)"; pass=$((pass+1))
else
    echo "[FAIL] int 边界比较分歧"; head -3 /tmp/lab7_ie_a.out; fail=$((fail+1))
fi

# 9) 用法横幅（冒烟）
if "$BIN" 2>&1 | grep -q "用法"; then
    echo "[PASS] 用法横幅"; pass=$((pass+1))
else
    echo "[FAIL] 无参数未打印用法"; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "lab7: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
