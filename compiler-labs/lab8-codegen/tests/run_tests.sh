#!/usr/bin/env bash
# lab8 测试：七套用例的原生管线（minicc8 → .s → gcc 链接 rt.c → 运行）
#               + 四引擎对拍（x86-64 vs 栈式VM vs C递归irvm vs 树遍eval）
#               + 除零/无穷递归停机 + 类型错误拒绝
# 用法: bash tests/run_tests.sh   （在 lab8-codegen 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minicc8.exe
CASES=tests/cases
TMP=/tmp/lab8_work
rm -rf "$TMP"; mkdir -p "$TMP"
pass=0; fail=0

SRCS="src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c src/ir.c src/gen.c src/irvm.c src/vm.c src/codegen.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab8_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab8_cc.log; exit 1
fi
if [ -s /tmp/lab8_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab8_cc.log; exit 1
fi

# 运行时只编译一次，所有生成程序共用
if ! gcc -std=c11 -O2 -c src/rt.c -o "$TMP/rt.o" 2> /tmp/lab8_rt.log; then
    echo "[FAIL] 运行时编译失败"; cat /tmp/lab8_rt.log; exit 1
fi

check() { # check <名字> <实际文件> <期望文件>
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab8_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; head -20 /tmp/lab8_diff.log; fail=$((fail+1))
    fi
}

# 原生管线：<名>.mc → 汇编 → 链接 → 运行；任一步失败即 FAIL
native() { # native <名>  （stdout 落到 $TMP/<名>.out，成功返回 0）
    "$BIN" "$CASES/$1.mc" > "$TMP/$1.s" 2>"$TMP/$1.generr" || return 1
    gcc "$TMP/$1.s" "$TMP/rt.o" -o "$TMP/$1.exe" 2>"$TMP/$1.ccerr" || return 1
    "$TMP/$1.exe" > "$TMP/$1.out"
}

# 1) 七套用例第六跑：源码真的变成机器码后，输出仍与 lab3~lab7 逐字节一致
for c in expr vars bool func ir arr vm; do
    if native "$c"; then
        check "原生执行 $c.mc(机器码,与 lab3~lab7 对拍)" "$TMP/$c.out" "$CASES/$c.expected"
    else
        echo "[FAIL] 原生管线 $c.mc（生成/链接/运行某步失败）"
        head -5 "$TMP/$c.generr" "$TMP/$c.ccerr" 2>/dev/null
        fail=$((fail+1))
    fi
done

# 2) 四引擎对拍：同一程序 x86-64(默认) vs 栈式VM(-vm) vs C递归(-irvm)
#    vs 树遍(-eval)，输出必须一致——lab6 双引擎、lab7 三引擎的加厚版
allok=1
for c in expr vars bool func ir arr vm; do
    native "$c" || { allok=0; echo "  -> $c.mc 原生管线失败"; continue; }
    "$BIN" -vm   "$CASES/$c.mc" > "$TMP/vm.out"   2>/dev/null
    "$BIN" -irvm "$CASES/$c.mc" > "$TMP/irvm.out" 2>/dev/null
    "$BIN" -eval "$CASES/$c.mc" > "$TMP/ev.out"   2>/dev/null
    diff <(tr -d '\r' < "$TMP/$c.out") <(tr -d '\r' < "$TMP/vm.out")   > /dev/null \
        && diff <(tr -d '\r' < "$TMP/$c.out") <(tr -d '\r' < "$TMP/irvm.out") > /dev/null \
        && diff <(tr -d '\r' < "$TMP/$c.out") <(tr -d '\r' < "$TMP/ev.out")   > /dev/null \
        || { allok=0; echo "  -> $c.mc 四引擎输出不一致"; }
done
if [ $allok -eq 1 ]; then
    echo "[PASS] 四引擎对拍(x86-64 vs vm vs irvm vs eval, 7 个程序)"; pass=$((pass+1))
else
    echo "[FAIL] 四引擎对拍"; fail=$((fail+1))
fi

# 2b) ≥5 参调用：Win64 第 5+ 参走栈传递——后端最易错、此前从未被
#     任何用例执行过的路径；四引擎必须一致
printf 'int add7(int a, int b, int c, int d, int e, int f, int g) {\n    return a + b + c + d + e + f + g;\n}\nprint add7(1, 2, 3, 4, 5, 6, 7);\nprint add7(10, 20, 30, 40, 50, 60, 70);\n' > "$TMP/args7.mc"
a7ok=1
if "$BIN" "$TMP/args7.mc" > "$TMP/args7.s" 2>/dev/null \
   && gcc "$TMP/args7.s" "$TMP/rt.o" -o "$TMP/args7.exe" 2>/dev/null \
   && "$TMP/args7.exe" > "$TMP/args7.out"; then :; else a7ok=0; fi
"$BIN" -vm   "$TMP/args7.mc" > "$TMP/args7.vm.out"   2>/dev/null
"$BIN" -irvm "$TMP/args7.mc" > "$TMP/args7.irvm.out" 2>/dev/null
"$BIN" -eval "$TMP/args7.mc" > "$TMP/args7.ev.out"   2>/dev/null
for e in vm irvm ev; do
    diff -q <(tr -d '\r' < "$TMP/args7.out") \
            <(tr -d '\r' < "$TMP/args7.$e.out") >/dev/null || a7ok=0
done
[ "$(tr -d ' \r\n' < "$TMP/args7.out")" = "28280" ] || a7ok=0
if [ $a7ok -eq 1 ]; then
    echo "[PASS] 7 参调用(第 5+ 参走栈传递)四引擎一致"; pass=$((pass+1))
else
    echo "[FAIL] 多参调用路径"; head -3 "$TMP/args7.out"; fail=$((fail+1))
fi

# 2c) 浮点除零 → IEEE inf（%g 打印 "inf"/"-inf"），四引擎格式一致
printf 'print 1.0 / 0.0;\nprint -1.0 / 0.0;\n' > "$TMP/infd.mc"
iok=1
if "$BIN" "$TMP/infd.mc" > "$TMP/infd.s" 2>/dev/null \
   && gcc "$TMP/infd.s" "$TMP/rt.o" -o "$TMP/infd.exe" 2>/dev/null \
   && "$TMP/infd.exe" > "$TMP/infd.out"; then :; else iok=0; fi
"$BIN" -vm   "$TMP/infd.mc" > "$TMP/infd.vm.out"   2>/dev/null
"$BIN" -irvm "$TMP/infd.mc" > "$TMP/infd.irvm.out" 2>/dev/null
"$BIN" -eval "$TMP/infd.mc" > "$TMP/infd.ev.out"   2>/dev/null
for e in vm irvm ev; do
    diff -q <(tr -d '\r' < "$TMP/infd.out") \
            <(tr -d '\r' < "$TMP/infd.$e.out") >/dev/null || iok=0
done
grep -qx "inf" <(tr -d '\r' < "$TMP/infd.out") || iok=0
grep -qx -- "-inf" <(tr -d '\r' < "$TMP/infd.out") || iok=0
if [ $iok -eq 1 ]; then
    echo "[PASS] 浮点除零 inf 格式四引擎一致"; pass=$((pass+1))
else
    echo "[FAIL] 浮点除零 inf"; cat "$TMP/infd.out" 2>/dev/null; fail=$((fail+1))
fi

# 3) 类型错误：退出码非 0、九类错误照报、不生成可执行产物也不执行
"$BIN" "$CASES/typeerr.mc" > "$TMP/t.s" 2> "$TMP/terr.log"; rc=$?
if [ $rc -ne 0 ] && [ ! -s "$TMP/t.s" ] \
   && grep -q "重复声明 f" "$TMP/terr.log" \
   && grep -q "调用了未定义的函数" "$TMP/terr.log" \
   && grep -q "隐式收窄" "$TMP/terr.log" \
   && grep -q "语法错误" "$TMP/terr.log"; then
    echo "[PASS] 类型错误拒绝编译"; pass=$((pass+1))
else
    echo "[FAIL] 类型错误检查: rc=$rc"; cat "$TMP/terr.log"; fail=$((fail+1))
fi
"$BIN" "$CASES/arrerr.mc" > "$TMP/a.s" 2> "$TMP/aerr.log"; rc=$?
if [ $rc -ne 0 ] && grep -q "数组下标必须是 int" "$TMP/aerr.log" \
   && grep -q "长度必须为正" "$TMP/aerr.log"; then
    echo "[PASS] 数组类型错误拒绝编译"; pass=$((pass+1))
else
    echo "[FAIL] 数组类型错误检查: rc=$rc"; fail=$((fail+1))
fi

# 4) 运行时除零：机器码里 idiv 前的检查带行号进运行时助手并停机
printf 'print 1 / 0;\n' > "$TMP/divz.mc"
"$BIN" "$TMP/divz.mc" > "$TMP/divz.s" && gcc "$TMP/divz.s" "$TMP/rt.o" -o "$TMP/divz.exe" && \
    "$TMP/divz.exe" > "$TMP/dvz.out" 2> "$TMP/dvz.err"; rc=$?
if [ $rc -ne 0 ] && [ ! -s "$TMP/dvz.out" ] && grep -q "除数为零" "$TMP/dvz.err"; then
    echo "[PASS] 整数除零停机(机器码)"; pass=$((pass+1))
else
    echo "[FAIL] 除零检查: rc=$rc"; cat "$TMP/dvz.err"; fail=$((fail+1))
fi

# 5) 无穷递归：真调用栈爆掉——退出码非 0 且无输出（对比 lab7 的受控报错）
printf 'int loop(int n) { return loop(n + 1); }\nprint loop(0);\n' > "$TMP/ovf.mc"
"$BIN" "$TMP/ovf.mc" > "$TMP/ovf.s" && gcc "$TMP/ovf.s" "$TMP/rt.o" -o "$TMP/ovf.exe" && \
    "$TMP/ovf.exe" > "$TMP/ovf.out" 2>/dev/null; rc=$?
if [ $rc -ne 0 ] && [ ! -s "$TMP/ovf.out" ]; then
    echo "[PASS] 无穷递归在真栈上停机(崩溃退出码 $rc)"; pass=$((pass+1))
else
    echo "[FAIL] 无穷递归检查: rc=$rc"; fail=$((fail+1))
fi

# 6) 用法横幅（冒烟）+ -vm -trace 冒烟
if "$BIN" 2>&1 | grep -q "用法"; then
    echo "[PASS] 用法横幅"; pass=$((pass+1))
else
    echo "[FAIL] 无参数未打印用法"; fail=$((fail+1))
fi
if "$BIN" -vm -trace "$CASES/func.mc" 2>&1 >/dev/null | grep -q "\[vm\] call fact"; then
    echo "[PASS] -vm -trace 帧快照"; pass=$((pass+1))
else
    echo "[FAIL] -trace 输出异常"; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "lab8: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
