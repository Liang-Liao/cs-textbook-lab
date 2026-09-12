#!/usr/bin/env bash
# lab2 测试：正则匹配用例 + DOT 稳定性 + mini-flex 与 lab1 对拍
# 用法: bash tests/run_tests.sh   （在 lab2-regex 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minire.exe
CASES=tests/cases
pass=0; fail=0

SRCS="src/re_parse.c src/nfa.c src/dfa.c src/scan.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab2_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab2_cc.log; exit 1
fi
if [ -s /tmp/lab2_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab2_cc.log; exit 1
fi

ok() { echo "[PASS] $1"; pass=$((pass+1)); }
bad() { echo "[FAIL] $1"; fail=$((fail+1)); }

# 1) 逐条匹配用例（re_cases.txt: 正则<TAB>串<TAB>期望）
ncase=0
while IFS=$'\t' read -r re s expect; do
    case "$re" in ''|'#'*) continue ;; esac
    [ "$s" = "<EMPTY>" ] && s=""      # 空串用占位符表示（read 会折叠空字段）
    got=$("$BIN" "$re" "$s" 2>/dev/null | awk '{print $NF}')
    ncase=$((ncase+1))
    if [ "$got" = "$expect" ]; then pass=$((pass+1))
    else bad "匹配: /$re/ \"$s\" 期望 $expect 得到 $got"; fi
done < "$CASES/re_cases.txt"
if [ "$ncase" -gt 0 ] && [ "$fail" -eq 0 ]; then
    ok "匹配用例共 $ncase 条"
else
    bad "匹配用例共 $ncase 条，存在失败"
fi

# 1b) 非法正则负例：解析器必须报错拒绝（曾对类内孤立 '\' 越界读）
for badre in '(' '[a' '[a-' '[z-a]' 'a\'; do
    if "$BIN" "$badre" x >/dev/null 2>/tmp/lab2_bad_re.log; then
        bad "非法正则 /$badre/ 未被拒绝"
    elif ! grep -Eqi "error|错误" /tmp/lab2_bad_re.log; then
        bad "非法正则 /$badre/ 无报错信息"
    else
        ok "非法正则拒绝: /$badre/"
    fi
done

# 2) 龙书经典例子 (a|b)*abb 的状态数：NFA=14 DFA=5 min=4
stats=$("$BIN" '(a|b)*abb' abb 2>&1 >/dev/null | grep stats)
echo "$stats" | grep -q "NFA states=14  DFA states=5  minimized=4" \
    && ok "(a|b)*abb 状态数与龙书图 3.34/3.9 一致 [$stats]" \
    || bad "状态数不对: $stats"

# 2b) 最小化正确性回归 a(a|b)*abb：真最小 DFA 是 5 态（P1≡P3 应合并）。
#     曾有实现按"与组代表不同即各自成新组"细化，把等价状态永久拆散成 6 态。
stats=$("$BIN" 'a(a|b)*abb' aabb xabb ababb 2>&1 >/dev/null | grep stats)
echo "$stats" | grep -q "NFA states=16  DFA states=6  minimized=5" \
    && ok "a(a|b)*abb 最小化为 5 态 [$stats]" \
    || bad "最小化过分裂: $stats"

# 3) DOT 输出确定性对拍
"$BIN" -dot '(a|b)*abb' 2>/dev/null > /tmp/lab2_dot.out
if diff -u <(tr -d '\r' < "$CASES/abb_dot.expected") \
            <(tr -d '\r' < /tmp/lab2_dot.out) > /tmp/lab2_dotdiff.log 2>&1; then
    ok "DOT 输出稳定"
else
    bad "DOT 输出变化"; cat /tmp/lab2_dotdiff.log
fi

# 4) mini-flex 对拍 lab1 手写词法器（同一 demo.mc，输出逐字节一致）
"$BIN" -scan "$CASES/minic.rules" "$CASES/demo.mc" > /tmp/lab2_scan.out 2>/dev/null
if diff -u <(tr -d '\r' < "$CASES/scan.expected") \
            <(tr -d '\r' < /tmp/lab2_scan.out) > /tmp/lab2_scandiff.log 2>&1; then
    ok "mini-flex 与 lab1 手写词法器输出完全一致"
else
    bad "对拍不一致"; head -20 /tmp/lab2_scandiff.log
fi

# 5) 扫描错误：@ 无法匹配任何规则 -> 报错且退出码 1
if "$BIN" -scan "$CASES/minic.rules" "$CASES/bad.mc" > /dev/null 2>/tmp/lab2_bad.log; then
    bad "非法字符未被报告"
else
    grep -q "error" /tmp/lab2_bad.log && ok "非法字符报错" || { bad "报错缺失"; cat /tmp/lab2_bad.log; }
fi

echo "----------------------------------------"
echo "lab2: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
