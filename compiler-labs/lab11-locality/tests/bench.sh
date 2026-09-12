#!/usr/bin/env bash
# lab11 计时软验证（MODERN-NOTES 配套实验设计项），三组实验：
#   [1] -O 对比：LICM 外提 + 展开对循环体开销的削减（lab10 遗产，回归保留）；
#   [2] 步长实验：同一二维数组按行序 vs 按列序求和——行主序布局下
#       列序遍历每次跳跃一整行（256×8B=2KB），缓存行利用率崩塌；
#   [3] 循环次序实验：矩阵乘累加 i-j-k vs i-k-j——内层循环连续访问的
#       数组不同，ikj 让 B 的内层按下标 j 顺序滑动（局部性友好）。
# 注意：**软验证**——耗时受机器负载影响大，只看数量级差异，
# 不计入 run_tests.sh 通过标准；本脚本恒以 0 退出。
# 用法: bash tests/bench.sh   （在 lab11-locality 目录下执行）
set -u
cd "$(dirname "$0")/.."

BIN=./minicc11.exe
TMP=/tmp/lab11_bench
rm -rf "$TMP"; mkdir -p "$TMP"

SRCS="src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c src/ir.c src/gen.c src/irvm.c src/vm.c src/codegen.c src/opt.c src/main.c"
gcc -std=c11 -O2 -o "$BIN" $SRCS || { echo "[bench] minicc11 编译失败"; exit 1; }
gcc -std=c11 -O2 -c src/rt.c -o "$TMP/rt.o" || { echo "[bench] rt.c 编译失败"; exit 1; }

build_one() { # build_one <标签> <程序.mc> <额外flags...>
    local tag="$1" prog="$2"; shift 2
    "$BIN" "$@" "$prog" > "$TMP/$tag.s" 2>/dev/null \
        || { echo "[bench] IR 生成失败($tag)"; return 1; }
    gcc -std=c11 -O2 "$TMP/$tag.s" "$TMP/rt.o" -o "$TMP/$tag.exe" 2>/dev/null \
        || { echo "[bench] 链接失败($tag)"; return 1; }
}

measure() { # measure <标签> <次数>：连跑取平均（毫秒）
    local tag="$1" n="$2" t0 t1
    t0=$(date +%s%N)
    for r in $(seq 1 "$n"); do "$TMP/$tag.exe" > /dev/null || return 1; done
    t1=$(date +%s%N)
    echo $(( (t1 - t0) / (n * 1000000) ))
}

# ============ [1] -O：外提 + 展开（lab10 回归保留） ============
cat > "$TMP/bench1.mc" <<'EOF'
int g = 7;
int n = 5;
int sum = 0;
for (int i = 0; i < 4000000; i = i + 1)
    sum = sum + g * n;
print sum;
EOF

echo "===== [1] -O：不变量外提 + 展开 ×2 ====="
if build_one b1_noop "$TMP/bench1.mc" && build_one b1_opt "$TMP/bench1.mc" -O; then
    "$BIN" -O -ir "$TMP/bench1.mc" 2>&1 >/dev/null | grep '^\[opt\]'
    "$TMP/b1_noop.exe" > "$TMP/o1"; "$TMP/b1_opt.exe" > "$TMP/o2"
    if diff -q <(tr -d '\r' < "$TMP/o1") <(tr -d '\r' < "$TMP/o2") >/dev/null; then
        echo "语义一致: $(tr '\n' ' ' < "$TMP/o2")"
        echo "不带 -O : $(measure b1_noop 3) ms"
        echo "带 -O   : $(measure b1_opt 3) ms"
    else
        echo "不一致！优化改变了程序语义——bug，请跑 tests/run_tests.sh"
    fi
fi

# ============ [2] 步长：行序 vs 列序遍历 1024×1024（8MB 工作集 +
#              重复 5 轮放大信号，压过 Windows 的进程启动开销） ============
PRELUDE='int m[1024][1024];
int sum = 0;
for (int i = 0; i < 1024; i = i + 1)
    for (int j = 0; j < 1024; j = j + 1)
        m[i][j] = i + j;'
printf '%s\nfor (int r = 0; r < 5; r = r + 1) {\n    sum = 0;\n    for (int i = 0; i < 1024; i = i + 1)\n        for (int j = 0; j < 1024; j = j + 1)\n            sum = sum + m[i][j];\n}\nprint sum;\n' "$PRELUDE" > "$TMP/bench_row.mc"
printf '%s\nfor (int r = 0; r < 5; r = r + 1) {\n    sum = 0;\n    for (int j = 0; j < 1024; j = j + 1)\n        for (int i = 0; i < 1024; i = i + 1)\n            sum = sum + m[i][j];\n}\nprint sum;\n' "$PRELUDE" > "$TMP/bench_col.mc"

echo ""
echo "===== [2] 步长实验：行主序下 行序 vs 列序求和（1024×1024） ====="
if build_one b2_row "$TMP/bench_row.mc" && build_one b2_col "$TMP/bench_col.mc"; then
    "$TMP/b2_row.exe" > "$TMP/o1"; "$TMP/b2_col.exe" > "$TMP/o2"
    if diff -q <(tr -d '\r' < "$TMP/o1") <(tr -d '\r' < "$TMP/o2") >/dev/null; then
        echo "两版输出一致: $(tr '\n' ' ' < "$TMP/o2")"
        t_r=$(measure b2_row 5); t_c=$(measure b2_col 5)
        echo "行序(相邻 j 连续): ${t_r} ms"
        echo "列序(每次跳一行): ${t_c} ms"
    else
        echo "不一致！请跑 tests/run_tests.sh 定位"
    fi
fi

# ============ [3] 循环次序：i-j-k vs i-k-j（256×256 矩阵乘累加和） ============
cat > "$TMP/bench_ijk.mc" <<'EOF'
int a[256][256];
int b[256][256];
for (int i = 0; i < 256; i = i + 1)
    for (int j = 0; j < 256; j = j + 1) {
        a[i][j] = i % 8;
        b[i][j] = j % 8;
    }
int s = 0;
for (int i = 0; i < 256; i = i + 1)
    for (int j = 0; j < 256; j = j + 1) {
        int acc = 0;
        for (int k = 0; k < 256; k = k + 1)
            acc = acc + a[i][k] * b[k][j];
        s = s + acc;
    }
print s;
EOF
cat > "$TMP/bench_ikj.mc" <<'EOF'
int a[256][256];
int b[256][256];
for (int i = 0; i < 256; i = i + 1)
    for (int j = 0; j < 256; j = j + 1) {
        a[i][j] = i % 8;
        b[i][j] = j % 8;
    }
int s = 0;
for (int i = 0; i < 256; i = i + 1)
    for (int k = 0; k < 256; k = k + 1) {
        int r = a[i][k];
        for (int j = 0; j < 256; j = j + 1)
            s = s + r * b[k][j];
    }
print s;
EOF

echo ""
echo "===== [3] 循环次序实验：256×256 整数矩阵乘累加和 ====="
if build_one b3_ijk "$TMP/bench_ijk.mc" && build_one b3_ikj "$TMP/bench_ikj.mc"; then
    "$TMP/b3_ijk.exe" > "$TMP/o1"; "$TMP/b3_ikj.exe" > "$TMP/o2"
    if diff -q <(tr -d '\r' < "$TMP/o1") <(tr -d '\r' < "$TMP/o2") >/dev/null; then
        echo "两种次序结果一致: $(tr '\n' ' ' < "$TMP/o2")"
        t_i=$(measure b3_ijk 5); t_k=$(measure b3_ikj 5)
        echo "i-j-k(b 内层随 k 跳行): ${t_i} ms"
        echo "i-k-j(b 内层沿 j 连续): ${t_k} ms"
    else
        echo "不一致！请跑 tests/run_tests.sh 定位"
    fi
fi

echo ""
echo "(软验证结束，恒以 0 退出)"
exit 0
