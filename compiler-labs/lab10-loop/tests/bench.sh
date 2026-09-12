#!/usr/bin/env bash
# lab10 计时软验证（MODERN-NOTES 配套实验设计项）：同一循环程序分别以
# 不带 -O / 带 -O 编译为原生代码，对比执行耗时——直观感受 LICM 外提
# （循环不变乘法只算一次）与展开 ×2（判断/步进开销减半）的效果。
# 注意：**软验证**——耗时受机器负载影响大，只看数量级差异，
# 不计入 run_tests.sh 通过标准；本脚本恒以 0 退出。
# 用法: bash tests/bench.sh   （在 lab10-loop 目录下执行）
set -u
cd "$(dirname "$0")/.."

BIN=./minicc10.exe
TMP=/tmp/lab10_bench
rm -rf "$TMP"; mkdir -p "$TMP"

SRCS="src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c src/ir.c src/gen.c src/irvm.c src/vm.c src/codegen.c src/opt.c src/main.c"
gcc -std=c11 -O2 -o "$BIN" $SRCS || { echo "[bench] minicc10 编译失败"; exit 1; }
gcc -std=c11 -O2 -c src/rt.c -o "$TMP/rt.o" || { echo "[bench] rt.c 编译失败"; exit 1; }

# 基准程序：偶数常量界的大计数循环 + 循环不变乘法（LICM/展开双重目标），
# 体刻意落在 lab10 的规范形态内：外提后体内只剩标量加法。
# 结果 35×4,000,000 = 140,000,000 落在 int 范围内，两版输出可逐字节比。
cat > "$TMP/bench.mc" <<'EOF'
int g = 7;
int n = 5;
int sum = 0;
for (int i = 0; i < 4000000; i = i + 1)
    sum = sum + g * n;
print sum;
EOF

build_one() { # build_one <标签> <额外flags...>
    local tag="$1"; shift
    "$BIN" "$@" "$TMP/bench.mc" > "$TMP/$tag.s" 2>/dev/null \
        || { echo "[bench] IR 生成失败($tag)"; return 1; }
    gcc -std=c11 -O2 "$TMP/$tag.s" "$TMP/rt.o" -o "$TMP/$tag.exe" 2>/dev/null \
        || { echo "[bench] 链接失败($tag)"; return 1; }
}

measure() { # measure <标签>：连跑 3 次取平均（毫秒）
    local t0 t1
    t0=$(date +%s%N)
    for r in 1 2 3; do "$TMP/$1.exe" > /dev/null || return 1; done
    t1=$(date +%s%N)
    echo $(( (t1 - t0) / 3000000 ))
}

build_one noopt  || exit 0
build_one opt -O || exit 0

echo "== -O 实际触发的优化 =="
"$BIN" -O -ir "$TMP/bench.mc" 2>&1 >/dev/null | grep '^\[opt\]'

echo "== 语义软检查（两版输出必须一致）=="
"$TMP/noopt.exe" > "$TMP/noopt.out"
"$TMP/opt.exe"   > "$TMP/opt.out"
if diff -q <(tr -d '\r' < "$TMP/noopt.out") <(tr -d '\r' < "$TMP/opt.out") >/dev/null; then
    echo "一致: $(tr '\n' ' ' < "$TMP/opt.out")"
else
    echo "不一致！优化改变了程序语义——这是 bug，请跑 tests/run_tests.sh 定位"
    exit 0
fi

echo "== 计时（3 次平均）=="
t_no=$(measure noopt) || { echo "[bench] 运行失败(noopt)"; exit 0; }
t_op=$(measure opt)   || { echo "[bench] 运行失败(opt)";   exit 0; }
echo "不带 -O : ${t_no} ms"
echo "带 -O   : ${t_op} ms"
exit 0
