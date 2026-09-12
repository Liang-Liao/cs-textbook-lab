#!/usr/bin/env bash
# lab9 测试：-O 优化管线的语义保真（七套旧用例 + 撞名回归，原生运行对拍）
#               + 四引擎 ×{-O} 对拍 + 双 IR dump 对拍 + 除零保真
# 用法: bash tests/run_tests.sh   （在 lab9-opt 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minicc9.exe
CASES=tests/cases
TMP=/tmp/lab9_work
rm -rf "$TMP"; mkdir -p "$TMP"
pass=0; fail=0

SRCS="src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c src/ir.c src/gen.c src/irvm.c src/vm.c src/codegen.c src/opt.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab9_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab9_cc.log; exit 1
fi
if [ -s /tmp/lab9_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab9_cc.log; exit 1
fi

gcc -std=c11 -O2 -c src/rt.c -o "$TMP/rt.o" || { echo "[FAIL] rt.c 编译失败"; exit 1; }

check() { # check <名字> <实际文件> <期望文件>
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab9_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; head -20 /tmp/lab9_diff.log; fail=$((fail+1))
    fi
}

# 带优化跑完整管线：<名>.mc → 优化 → .s → 链接 → 运行
native_O() { # native_O <名>
    "$BIN" -O "$CASES/$1.mc" > "$TMP/$1.s" 2>"$TMP/$1.generr" || return 1
    gcc "$TMP/$1.s" "$TMP/rt.o" -o "$TMP/$1.exe" 2>/dev/null || return 1
    "$TMP/$1.exe" > "$TMP/$1.out"
}

# 1) 七套旧用例 + collide + foldprec + foldcmp：带 -O 的机器码输出与期望
#    逐字节一致——旧用例期望原样沿用零改动（"优化不改变语义"的最强证明）；
#    foldprec 是浮点折叠精度回归（fmt_float %.17g），钉住 0.1+0.2 != 0.3；
#    foldcmp 钉住整型折叠比较按 long 精确比（>2^53 时 double 口径会反转）
for c in expr vars bool func ir arr vm collide foldprec foldcmp; do
    if native_O "$c"; then
        check "优化后执行 $c.mc(与 lab3~lab8 对拍)" "$TMP/$c.out" "$CASES/$c.expected"
    else
        echo "[FAIL] -O 管线 $c.mc 失败"; head -5 "$TMP/$c.generr"; fail=$((fail+1))
    fi
done

# 2) 双 IR dump：-ir 是原始形态、-O -ir 是优化后形态，各自与冻结的期望一致
"$BIN"    -ir "$CASES/opt.mc" > "$TMP/ir_raw.txt"   2>/dev/null
check "原始 IR dump(opt.mc)" "$TMP/ir_raw.txt" "$CASES/opt_ir.expected"
"$BIN" -O -ir "$CASES/opt.mc" > "$TMP/ir_opt.txt"   2>/dev/null
check "优化后 IR dump(opt.mc)" "$TMP/ir_opt.txt" "$CASES/opt_oir.expected"

# 3) 四引擎 ×{-O}：同一程序经优化后，x86-64/vm/irvm/eval 输出仍全部一致
allok=1
for c in expr vars bool func ir arr vm opt collide foldprec foldcmp; do
    native_O "$c" || { allok=0; echo "  -> $c.mc 原生失败"; continue; }
    "$BIN" -O -vm   "$CASES/$c.mc" > "$TMP/b.out" 2>/dev/null
    "$BIN" -O -irvm "$CASES/$c.mc" > "$TMP/i.out" 2>/dev/null
    "$BIN" -O -eval "$CASES/$c.mc" > "$TMP/e.out" 2>/dev/null
    diff <(tr -d '\r' < "$TMP/$c.out") <(tr -d '\r' < "$TMP/b.out") > /dev/null \
        && diff <(tr -d '\r' < "$TMP/$c.out") <(tr -d '\r' < "$TMP/i.out") > /dev/null \
        && diff <(tr -d '\r' < "$TMP/$c.out") <(tr -d '\r' < "$TMP/e.out") > /dev/null \
        || { allok=0; echo "  -> $c.mc 四引擎输出不一致"; }
done
if [ $allok -eq 1 ]; then
    echo "[PASS] 四引擎×{-O}对拍(x86-64 vs vm vs irvm vs eval, 11 个程序)"; pass=$((pass+1))
else
    echo "[FAIL] 四引擎对拍"; fail=$((fail+1))
fi

# 4) 除零保真：常量折叠不得吞掉运行时错误（print 1/0 在 -O 下仍停机）
printf 'print 1 / 0;\n' > "$TMP/divz.mc"
if "$BIN" -O "$TMP/divz.mc" > "$TMP/divz.s" 2>/dev/null \
   && gcc "$TMP/divz.s" "$TMP/rt.o" -o "$TMP/divz.exe" 2>/dev/null; then
    "$TMP/divz.exe" > "$TMP/dvz.out" 2> "$TMP/dvz.err"; rc=$?
    if [ $rc -ne 0 ] && [ ! -s "$TMP/dvz.out" ] && grep -q "除数为零" "$TMP/dvz.err"; then
        echo "[PASS] 除零保真：零除数不被折叠，运行时仍报错停机"; pass=$((pass+1))
    else
        echo "[FAIL] 除零保真: rc=$rc"; cat "$TMP/dvz.err"; fail=$((fail+1))
    fi
else
    echo "[FAIL] 除零用例生成/链接失败"; fail=$((fail+1))
fi

# 4b) 保守性清单三条（README 的语义承诺逐条钉住）
# a) 用户变量的死赋值即使无人读也不删——它可能藏着一次除零崩溃
printf 'int dead = 1 / 0;\nprint 5;\n' > /tmp/lab9_c1.mc
if "$BIN" -O /tmp/lab9_c1.mc > /tmp/lab9_c1.s 2>/dev/null \
   && gcc /tmp/lab9_c1.s "$TMP/rt.o" -o /tmp/lab9_c1.exe 2>/dev/null; then
    /tmp/lab9_c1.exe >/dev/null 2>/tmp/lab9_c1.err; rc=$?
    if [ $rc -ne 0 ] && grep -q "除数为零" /tmp/lab9_c1.err; then
        echo "[PASS] 保守性: 死赋值不删(除零崩溃仍在)"; pass=$((pass+1))
    else
        echo "[FAIL] 死赋值的除零被优化没: rc=$rc"; fail=$((fail+1))
    fi
else
    echo "[FAIL] c1 管线失败"; fail=$((fail+1))
fi

# b) STX 同块失效：两次同下标数组读之间夹一次写，第二次必须真读
#    （LDX 缓存若不被 STX 失效，会输出 2 而非 3）
printf 'int a[1];\nint t;\na[0] = 1;\nt = a[0];\na[0] = 2;\nprint t + a[0];\n' \
    > /tmp/lab9_c2.mc
"$BIN" -O /tmp/lab9_c2.mc > /tmp/lab9_c2.s 2>/dev/null \
    && gcc /tmp/lab9_c2.s "$TMP/rt.o" -o /tmp/lab9_c2.exe 2>/dev/null \
    && /tmp/lab9_c2.exe > /tmp/lab9_c2.out 2>/dev/null
if [ "$(tr -d ' \r\n' < /tmp/lab9_c2.out)" = "3" ]; then
    echo "[PASS] 保守性: 数组写使同块 LDX 缓存失效"; pass=$((pass+1))
else
    echo "[FAIL] STX 失效失效"; cat /tmp/lab9_c2.out 2>/dev/null; fail=$((fail+1))
fi

# c) CALL 清空全部缓存：被调方改全局，CALL 后的重读必须看到新值
printf 'int g = 1;\nint bump() { g = 9; return 0; }\nbump();\nprint g;\n' \
    > /tmp/lab9_c3.mc
"$BIN" -O /tmp/lab9_c3.mc > /tmp/lab9_c3.s 2>/dev/null \
    && gcc /tmp/lab9_c3.s "$TMP/rt.o" -o /tmp/lab9_c3.exe 2>/dev/null \
    && /tmp/lab9_c3.exe > /tmp/lab9_c3.out 2>/dev/null
if [ "$(tr -d ' \r\n' < /tmp/lab9_c3.out)" = "9" ]; then
    echo "[PASS] 保守性: CALL 后缓存重建(g=9)"; pass=$((pass+1))
else
    echo "[FAIL] CALL 缓存"; cat /tmp/lab9_c3.out 2>/dev/null; fail=$((fail+1))
fi

# 4c) 窥孔确实在干活：bool.mc 在 -O 下触发数十次窥孔改写
"$BIN" -O "$CASES/bool.mc" 2>&1 >/dev/null | grep -q "窥孔 [1-9]" \
    && { echo "[PASS] 窥孔 pass 有实际删除"; pass=$((pass+1)); } \
    || { echo "[FAIL] 窥孔计数为 0 或缺失"; fail=$((fail+1)); }

# 5) 类型/数组错误照旧拒绝（前端未动）
"$BIN" -O "$CASES/typeerr.mc" > /dev/null 2> "$TMP/terr.log"; rc=$?
if [ $rc -ne 0 ] && grep -q "隐式收窄" "$TMP/terr.log" && grep -q "语法错误" "$TMP/terr.log"; then
    echo "[PASS] 类型错误拒绝编译"; pass=$((pass+1))
else
    echo "[FAIL] 类型错误检查: rc=$rc"; fail=$((fail+1))
fi

# 6) 不带 -O 时与 lab8 行为一致（冒烟：expr 无 -O 原生也对）
"$BIN" "$CASES/expr.mc" > "$TMP/n.s" 2>/dev/null \
    && gcc "$TMP/n.s" "$TMP/rt.o" -o "$TMP/n.exe" \
    && "$TMP/n.exe" > "$TMP/n.out" \
    && diff -q <(tr -d '\r' < "$TMP/n.out") <(tr -d '\r' < "$CASES/expr.expected") >/dev/null
if [ $? -eq 0 ]; then
    echo "[PASS] 无 -O 与 lab8 行为一致"; pass=$((pass+1))
else
    echo "[FAIL] 无 -O 行为漂移"; fail=$((fail+1))
fi

# 7) 用法横幅（含 -O 说明）
if "$BIN" 2>&1 | grep -q "\-O"; then
    echo "[PASS] 用法横幅"; pass=$((pass+1))
else
    echo "[FAIL] 无参数未打印用法"; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "lab9: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
