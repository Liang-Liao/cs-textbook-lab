#!/usr/bin/env bash
# lab12 测试：过程间分析四件套（-cg 调用图/内联/IPCP/mod-ref 放宽）+
#             二维数组四引擎对拍 + 依赖分析/迭代空间快照 + lab10/11 全量
#             回归（期望零改动）
# 用法: bash tests/run_tests.sh   （在 lab12-ipa 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minicc12.exe
CASES=tests/cases
TMP=/tmp/lab12_work
rm -rf "$TMP"; mkdir -p "$TMP"
pass=0; fail=0

SRCS="src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c src/ir.c src/gen.c src/irvm.c src/vm.c src/codegen.c src/opt.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab12_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab12_cc.log; exit 1
fi
if [ -s /tmp/lab12_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab12_cc.log; exit 1
fi

gcc -std=c11 -O2 -c src/rt.c -o "$TMP/rt.o" || { echo "[FAIL] rt.c 编译失败"; exit 1; }

check() { # check <名字> <实际文件> <期望文件>
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab12_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; head -20 /tmp/lab12_diff.log; fail=$((fail+1))
    fi
}
native_O() { # native_O <名>：<名>.mc → 优化 → .s → 链接 → 运行
    "$BIN" -O "$CASES/$1.mc" > "$TMP/$1.s" 2>"$TMP/$1.generr" || return 1
    gcc "$TMP/$1.s" "$TMP/rt.o" -o "$TMP/$1.exe" 2>/dev/null || return 1
    "$TMP/$1.exe" > "$TMP/$1.out"
}

# 1) 旧用例 + lab11 新用例 + lab12 新用例 + 审计修复回归：带 -O 的机器码
#    输出与期望逐字节一致。insline 钉住内联的 PARAM 即时快照语义（迟到
#    绑定会输出 86）；unrollinit/swap* 三负例/foldcmp 同 lab11 的口径
for c in expr vars bool func ir arr vm collide lics unroll licsunroll \
         mat dep swap swapbad space unrollnew unrolldenied \
         inline inline_rec ipconst pure cg \
         insline unrollinit foldcmp swapacc swapcall swapinit; do
    if native_O "$c"; then
        check "优化后执行 $c.mc(与前序 lab 对拍)" "$TMP/$c.out" "$CASES/$c.expected"
    else
        echo "[FAIL] -O 管线 $c.mc 失败"; head -5 "$TMP/$c.generr"; fail=$((fail+1))
    fi
done

# 2) 双 IR dump：lab10 三件套（外提/展开/组合回归零变化）
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

# 2b) lab11 变换的双 dump：
#     swap       —— 合法嵌套交换后 _oir 里外层测 j、内层测 i（结构证据）；
#     unrollnew  —— 无携带依赖的数组写体获得展开资格（体翻倍）；
#     unrolldenied —— 携带真依赖 dist=1，依赖门禁拒绝展开（体不翻倍）。
for c in swap unrollnew unrolldenied; do
    "$BIN"    -ir "$CASES/$c.mc" > "$TMP/${c}_ir.txt"   2>/dev/null
    "$BIN" -O -ir "$CASES/$c.mc" > "$TMP/${c}_oir.txt"  2>/dev/null
    check "原始 IR dump($c.mc)"   "$TMP/${c}_ir.txt"  "$CASES/${c}_ir.expected"
    check "优化后 IR dump($c.mc)" "$TMP/${c}_oir.txt" "$CASES/${c}_oir.expected"
done
grep -q "if j < 4 goto" "$TMP/swap_oir.txt" \
    && echo "[PASS] 交换生效(swap.mc 外层已改测 j)"; pass=$((pass+1)) \
    || { echo "[FAIL] 交换未生效"; fail=$((fail+1)); }

# 2c) lab12 双 dump：内联拼接形态 / IPCP 代入折叠 / mod/ref 放宽证据
for c in inline inline_rec ipconst pure; do
    "$BIN"    -ir "$CASES/$c.mc" > "$TMP/${c}_ir.txt"   2>/dev/null
    "$BIN" -O -ir "$CASES/$c.mc" > "$TMP/${c}_oir.txt"  2>/dev/null
    check "原始 IR dump($c.mc)"   "$TMP/${c}_ir.txt"  "$CASES/${c}_ir.expected"
    check "优化后 IR dump($c.mc)" "$TMP/${c}_oir.txt" "$CASES/${c}_oir.expected"
done

# 2d) lab12 结构断言（在自家冻结 dump 上 grep，证据要能自证）：
#     inline   —— 全部叶子调用被拼进调用点：优化后 IR 不再出现 call；
#     inline_rec —— 递归不在内联之列：call fact 必须原样保留；
#     pure     —— 纯 probe 因局部数组保持调用形态（call probe 存活），
#                 且体被展开 ×2（同一 param i 出现两轮步进对）。
if grep -q "call" "$TMP/inline_oir.txt"; then
    echo "[FAIL] 内联不彻底(inline_oir 残留 call)"; fail=$((fail+1))
else
    echo "[PASS] 叶子全内联(inline_oir 无 call)"; pass=$((pass+1))
fi
grep -q "call fact" "$TMP/inline_rec_oir.txt" \
    && { echo "[PASS] 递归不内联(inline_rec_oir 保留 call fact)"; pass=$((pass+1)); } \
    || { echo "[FAIL] 递归被误内联"; fail=$((fail+1)); }
grep -q "call probe" "$TMP/pure_oir.txt" \
    && { echo "[PASS] 局部数组门禁挡内联(pure_oir 保留 call probe)"; pass=$((pass+1)); } \
    || { echo "[FAIL] probe 应保持调用形态"; fail=$((fail+1)); }
[ "$(grep -c "call probe" "$TMP/pure_oir.txt")" -ge 2 ] \
    && { echo "[PASS] 含纯调用的循环体获得展开资格(probe 调用成对复制)"; pass=$((pass+1)); } \
    || { echo "[FAIL] 纯调用未解锁展开"; fail=$((fail+1)); }

# 2e) 审计修复回归：交换三门禁负例（与 lab11 同口径）——标量携带依赖/
#     体含 CALL/空 init 起点未知都必须拒绝（交换计数为 0）
for c in swapacc swapcall swapinit; do
    "$BIN" -O -ir "$CASES/$c.mc" > /dev/null 2> "$TMP/${c}_opt.txt"
    if grep -q "交换 0" "$TMP/${c}_opt.txt"; then
        echo "[PASS] 交换门禁拒绝($c.mc)"; pass=$((pass+1))
    else
        echo "[FAIL] 交换未被拒绝($c.mc)"; fail=$((fail+1))
    fi
done

# 3) 四引擎 ×{-O}：x86-64/vm/irvm/eval 输出全部一致（含二维数组语义）
allok=1
for c in expr vars bool func ir arr vm collide lics unroll licsunroll \
         mat dep swap swapbad unrollnew unrolldenied \
         inline inline_rec ipconst pure cg \
         insline unrollinit foldcmp swapacc swapcall swapinit; do
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
    echo "[PASS] 四引擎×{-O}对拍(x86-64 vs vm vs irvm vs eval, 27 个程序)"; pass=$((pass+1))
else
    echo "[FAIL] 四引擎对拍"; fail=$((fail+1))
fi

# 3b) 二维数组无 -O 与带 -O 同样四引擎一致（降糖不经优化也要对）
allok=1
for c in mat swap; do
    "$BIN" -vm   "$CASES/$c.mc" > "$TMP/b.out" 2>/dev/null
    "$BIN" -irvm "$CASES/$c.mc" > "$TMP/i.out" 2>/dev/null
    "$BIN" -eval "$CASES/$c.mc" > "$TMP/e.out" 2>/dev/null
    "$BIN" "$CASES/$c.mc" > "$TMP/n.s" 2>/dev/null \
        && gcc "$TMP/n.s" "$TMP/rt.o" -o "$TMP/n.exe" 2>/dev/null \
        && "$TMP/n.exe" > "$TMP/n.out" || { allok=0; continue; }
    for e in b i e; do
        diff <(tr -d '\r' < "$TMP/n.out") <(tr -d '\r' < "$TMP/$e.out") > /dev/null \
            || { allok=0; echo "  -> $c.mc 无-O 引擎($e)不一致"; }
    done
done
if [ $allok -eq 1 ]; then
    echo "[PASS] 二维数组无 -O 四引擎对拍(mat/swap)"; pass=$((pass+1))
else
    echo "[FAIL] 二维数组无 -O 对拍"; fail=$((fail+1))
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

# 5b) lab11 语义错误拒绝：维度不符/float 下标/总元素超限
"$BIN" -O "$CASES/materr.mc" > /dev/null 2> "$TMP/merr.log"; rc=$?
if [ $rc -ne 0 ] \
   && grep -q "需要两个下标" "$TMP/merr.log" \
   && grep -q "只能带一个下标" "$TMP/merr.log" \
   && grep -q "下标必须是 int" "$TMP/merr.log" \
   && grep -q "超过上限 1048576" "$TMP/merr.log"; then
    echo "[PASS] 二维数组语义错误拒绝(materr 四类)"; pass=$((pass+1))
else
    echo "[FAIL] materr 错误检查不全: rc=$rc"; fail=$((fail+1))
fi

# 6) 观测点快照：-deps 距离向量清单 / -space 迭代空间网格（stderr 冻结）
"$BIN" -deps  "$CASES/dep.mc"   2> "$TMP/dep_deps.txt"
check "-deps 距离向量快照(dep.mc)"   "$TMP/dep_deps.txt"   "$CASES/dep_deps.expected"
"$BIN" -space "$CASES/space.mc" 2> "$TMP/space_space.txt"
check "-space 网格快照(space.mc)"    "$TMP/space_space.txt" "$CASES/space_space.expected"

# 6b) lab12 观测点快照：-cg 调用图（stdout 冻结）+ [opt] 统计摘要
#     （stderr 快照——内联/IPCP 的量化证据，外提/展开计数即放宽生效的
#     直接证明）
"$BIN" -cg "$CASES/cg.mc" > "$TMP/cg_cg.txt" 2>/dev/null
check "-cg 调用图快照(cg.mc)"        "$TMP/cg_cg.txt"      "$CASES/cg_cg.expected"
"$BIN" -O -ir "$CASES/ipconst.mc" > /dev/null 2> "$TMP/ipconst_opt.txt"
check "[opt] 统计快照(ipconst.mc)"   "$TMP/ipconst_opt.txt" "$CASES/ipconst_opt.expected"
"$BIN" -O -ir "$CASES/pure.mc" > /dev/null 2> "$TMP/pure_opt.txt"
check "[opt] 统计快照(pure.mc)"      "$TMP/pure_opt.txt"    "$CASES/pure_opt.expected"

# 7) 横幅、-cg、-cfg、-trace 三冒烟
"$BIN" 2>&1 | grep -q "\-deps"         && echo "[PASS] 用法横幅"        && pass=$((pass+1)) || { echo "[FAIL] 横幅"; fail=$((fail+1)); }
"$BIN" 2>&1 | grep -q "\-cg"           && echo "[PASS] 用法横幅含 -cg"   && pass=$((pass+1)) || { echo "[FAIL] 横幅缺 -cg"; fail=$((fail+1)); }
"$BIN" -cg tests/cases/cg.mc | grep -q "递归" && echo "[PASS] -cg 递归环标注" && pass=$((pass+1)) || { echo "[FAIL] -cg 输出异常"; fail=$((fail+1)); }
"$BIN" -cfg tests/cases/lics.mc | grep -q "loop: head=" && echo "[PASS] -cfg 支配树与自然循环" && pass=$((pass+1)) || { echo "[FAIL] -cfg 输出异常"; fail=$((fail+1)); }
"$BIN" -O -vm -trace tests/cases/func.mc 2>&1 >/dev/null | grep -q "\[vm\] call fib" && echo "[PASS] -trace 帧快照" && pass=$((pass+1)) || { echo "[FAIL] -trace"; fail=$((fail+1)); }
"$BIN" -symtab tests/cases/mat.mc | grep -q "int\[3\]\[4\]" && echo "[PASS] -symtab 二维形状打印" && pass=$((pass+1)) || { echo "[FAIL] -symtab 二维形状"; fail=$((fail+1)); }

echo "----------------------------------------"
echo "lab12: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
