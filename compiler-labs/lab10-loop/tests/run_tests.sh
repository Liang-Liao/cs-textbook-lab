#!/usr/bin/env bash
# lab10 测试：循环优化的语义保真（支配树/自然循环/LICM/展开 ×{-O}）
#               + 原生对拍 + 双 IR dump 对拍（含 LICM×展开组合回归）+ -cfg 冒烟
# 用法: bash tests/run_tests.sh   （在 lab10-loop 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minicc10.exe
CASES=tests/cases
TMP=/tmp/lab10_work
rm -rf "$TMP"; mkdir -p "$TMP"
pass=0; fail=0

SRCS="src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c src/ir.c src/gen.c src/irvm.c src/vm.c src/codegen.c src/opt.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab10_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab10_cc.log; exit 1
fi
if [ -s /tmp/lab10_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab10_cc.log; exit 1
fi

gcc -std=c11 -O2 -c src/rt.c -o "$TMP/rt.o" || { echo "[FAIL] rt.c 编译失败"; exit 1; }

check() { # check <名字> <实际文件> <期望文件>
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab10_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; head -20 /tmp/lab10_diff.log; fail=$((fail+1))
    fi
}
native_O() { # native_O <名>：<名>.mc → 优化 → .s → 链接 → 运行
    "$BIN" -O "$CASES/$1.mc" > "$TMP/$1.s" 2>"$TMP/$1.generr" || return 1
    gcc "$TMP/$1.s" "$TMP/rt.o" -o "$TMP/$1.exe" 2>/dev/null || return 1
    "$TMP/$1.exe" > "$TMP/$1.out"
}

# 1) 旧用例 + lics/unroll/licsunroll + unrollinit/foldcmp：带 -O 的机器码
#    输出与期望逐字节一致。unrollinit 钉住两处审计修复：for 赋值式 init
#    的解析、奇数起点循环被初值门禁拒绝展开（错误展开会把 28 算成 36）
for c in expr vars bool func ir arr vm collide lics unroll licsunroll \
         unrollinit foldcmp; do
    if native_O "$c"; then
        check "优化后执行 $c.mc(与 lab3~lab9 对拍)" "$TMP/$c.out" "$CASES/$c.expected"
    else
        echo "[FAIL] -O 管线 $c.mc 失败"; head -5 "$TMP/$c.generr"; fail=$((fail+1))
    fi
done

# 2) 双 IR dump：循环分析前后各自与冻结期望一致（lics=外提、unroll=展开、
#    licsunroll=外提后再展开的组合回归——克隆体必须共享外提临时原名）
"$BIN"    -ir tests/cases/lics.mc   > "$TMP/l_ir.txt"   2>/dev/null
"$BIN" -O -ir tests/cases/lics.mc   > "$TMP/l_oir.txt"  2>/dev/null
check "原始 IR dump(lics.mc)"   "$TMP/l_ir.txt"  "$CASES/lics_ir.expected"
check "优化后 IR dump(lics.mc)" "$TMP/l_oir.txt" "$CASES/lics_oir.expected"
"$BIN"    -ir tests/cases/unroll.mc > "$TMP/u_ir.txt"   2>/dev/null
"$BIN" -O -ir tests/cases/unroll.mc > "$TMP/u_oir.txt"  2>/dev/null
check "原始 IR dump(unroll.mc)"   "$TMP/u_ir.txt"  "$CASES/unroll_ir.expected"
check "优化后 IR dump(unroll.mc)" "$TMP/u_oir.txt" "$CASES/unroll_oir.expected"
"$BIN"    -ir tests/cases/licsunroll.mc > "$TMP/lu_ir.txt"   2>/dev/null
"$BIN" -O -ir tests/cases/licsunroll.mc > "$TMP/lu_oir.txt"  2>/dev/null
check "原始 IR dump(licsunroll.mc)"   "$TMP/lu_ir.txt"  "$CASES/licsunroll_ir.expected"
check "优化后 IR dump(licsunroll.mc)" "$TMP/lu_oir.txt" "$CASES/licsunroll_oir.expected"

# 2b) 初值门禁的双向证据：合法环（零起点偶数趟）仍要展开——统计里展开
#     计数 ≥1；奇数起点被拒的一侧由 §1 的 s=28 输出对拍钉住
"$BIN" -O -ir "$CASES/unrollinit.mc" > /dev/null 2> "$TMP/ui_opt.txt"
if grep -qE "展开 [1-9]" "$TMP/ui_opt.txt"; then
    echo "[PASS] 合法环仍展开(unrollinit 展开计数 ≥1)"; pass=$((pass+1))
else
    echo "[FAIL] 合法环未展开(初值门禁误杀)"; fail=$((fail+1))
fi

# 3) 四引擎 ×{-O}：同一程序经循环优化后，x86-64/vm/irvm/eval 输出全部一致
allok=1
for c in expr vars bool func ir arr vm collide lics unroll licsunroll \
         unrollinit foldcmp; do
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
    echo "[PASS] 四引擎×{-O}对拍(x86-64 vs vm vs irvm vs eval, 13 个程序)"; pass=$((pass+1))
else
    echo "[FAIL] 四引擎对拍"; fail=$((fail+1))
fi

# 4) 除零保真：循环内的常量零除数也不许折叠进外提（print 1/0 在环里仍停机）
printf 'int i = 0;\nwhile (i < 3) { print 1 / 0; i = i + 1; }\n' > "$TMP/divz.mc"
if "$BIN" -O "$TMP/divz.mc" > "$TMP/divz.s" 2>/dev/null \
   && gcc "$TMP/divz.s" "$TMP/rt.o" -o "$TMP/divz.exe" 2>/dev/null; then
    "$TMP/divz.exe" > "$TMP/dvz.out" 2> "$TMP/dvz.err"; rc=$?
    if [ $rc -ne 0 ] && grep -q "除数为零" "$TMP/dvz.err"; then
        echo "[PASS] 循环内除零保真（不因外提而丢失）"; pass=$((pass+1))
    else
        echo "[FAIL] 循环内除零: rc=$rc"; fail=$((fail+1))
    fi
else
    echo "[FAIL] 除零用例生成失败"; fail=$((fail+1))
fi

# 5) 类型错误照旧拒绝；无 -O 行为与 lab8 一致（冒烟）
"$BIN" -O "$CASES/typeerr.mc" > /dev/null 2> "$TMP/terr.log"; rc=$?
if [ $rc -ne 0 ] && grep -q "隐式收窄" "$TMP/terr.log"; then
    echo "[PASS] 类型错误拒绝编译"; pass=$((pass+1))
else
    echo "[FAIL] 类型错误检查: rc=$rc"; fail=$((fail+1))
fi
"$BIN" "$CASES/expr.mc" > "$TMP/n.s" 2>/dev/null \
    && gcc "$TMP/n.s" "$TMP/rt.o" -o "$TMP/n.exe" \
    && "$TMP/n.exe" > "$TMP/n.out" \
    && diff -q <(tr -d '\r' < "$TMP/n.out") <(tr -d '\r' < "$CASES/expr.expected") >/dev/null
if [ $? -eq 0 ]; then
    echo "[PASS] 无 -O 行为一致"; pass=$((pass+1))
else
    echo "[FAIL] 无 -O 行为漂移"; fail=$((fail+1))
fi

# 6) 横幅、-cfg、-trace 三冒烟
"$BIN" 2>&1 | grep -q "\-cfg"          && echo "[PASS] 用法横幅"        && pass=$((pass+1)) || { echo "[FAIL] 横幅"; fail=$((fail+1)); }
"$BIN" -cfg tests/cases/lics.mc | grep -q "loop: head=" && echo "[PASS] -cfg 支配树与自然循环" && pass=$((pass+1)) || { echo "[FAIL] -cfg 输出异常"; fail=$((fail+1)); }
"$BIN" -O -vm -trace tests/cases/func.mc 2>&1 >/dev/null | grep -q "\[vm\] call fib" && echo "[PASS] -trace 帧快照" && pass=$((pass+1)) || { echo "[FAIL] -trace"; fail=$((fail+1)); }

echo "----------------------------------------"
echo "lab10: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
