#!/usr/bin/env bash
# lab1 测试：token 流对拍 + 符号表对拍 + 错误恢复检查
# 用法: bash tests/run_tests.sh   （在 lab1-lexer 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minilex.exe
CASES=tests/cases
pass=0; fail=0

if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" src/lexer.c src/symtab.c src/main.c 2> /tmp/lab1_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab1_cc.log; exit 1
fi
if [ -s /tmp/lab1_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab1_cc.log; exit 1
fi

check() { # check <名字> <实际文件> <期望文件>
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab1_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; cat /tmp/lab1_diff.log; fail=$((fail+1))
    fi
}

# 1) demo.mc 的 token 流（最长匹配、关键字、注释跳过……全在这里）
"$BIN" "$CASES/demo.mc" > /tmp/lab1_demo.out
check "demo token 流" /tmp/lab1_demo.out "$CASES/demo.expected"

# 2) -s 符号表（标识符去重驻留）
"$BIN" -s "$CASES/demo.mc" > /tmp/lab1_sym.out
check "符号表驻留" /tmp/lab1_sym.out "$CASES/demo_symtab.expected"

# 2b) stdin 缺省路径（README 承诺）：真管道喂入（cat |），token 流
#     必须与文件读入一致——旧实现对管道 fseek 是未定义行为
cat "$CASES/demo.mc" | "$BIN" > /tmp/lab1_stdin.out
check "stdin 管道读入" /tmp/lab1_stdin.out "$CASES/demo.expected"

# 3) 错误恢复：errors.mc 应报出 5 条错误并退出码 1；stdout 里 5 个坏
#    记号都要以 T_ERROR 出现在原位置（恢复≠吞掉）
"$BIN" "$CASES/errors.mc" > /tmp/lab1_err.out 2>/tmp/lab1_err.log
errcode=$?
nerr=$(grep -c "error:" /tmp/lab1_err.log)
ntok=$(grep -c "T_ERROR" /tmp/lab1_err.out)
if [ "$errcode" -ne 0 ] && [ "$nerr" -eq 5 ] && [ "$ntok" -eq 5 ]; then
    echo "[PASS] 错误恢复(共报 $nerr 条, stdout $ntok 个 T_ERROR, exit=$errcode)"; pass=$((pass+1))
else
    echo "[FAIL] 错误恢复: exit=$errcode, stderr $nerr 条(期望 5), stdout T_ERROR $ntok 个(期望 5)"
    cat /tmp/lab1_err.log; fail=$((fail+1))
fi

# 4) 全角标点（中文输入法常见事故）：stderr 与冻结期望全量对拍
#    （列号 = 坏字符起始列；曾 off-by-one 把 7/8/9 固化进期望文件）
"$BIN" "$CASES/fullwidth.mc" > /dev/null 2>/tmp/lab1_fw.log
check "全角标点 stderr 全量对拍" /tmp/lab1_fw.log "$CASES/fullwidth.stderr.expected"

# 4b) 数字规约边界：123abc（整数后跟字母）报错；
#     3.f 按规约 [0-9]+\.[0-9]* 与最长匹配切 FLOAT("3.")+IDENT(f)
printf 'print 123abc;\nint x = 3.f;\n' > /tmp/lab1_num.mc
"$BIN" /tmp/lab1_num.mc > /tmp/lab1_num.out 2>/tmp/lab1_num.err; rc=$?
# 类别列有 %-12s 尾部填充，先去掉空格/\r 再比较
istok() { awk -F'\t' -v l="$1" -v k="$2" -v x="$3" '
    { for (i = 1; i <= 3; i++) {
        gsub(/\r$/, "", $i); gsub(/^ +/, "", $i); gsub(/ +$/, "", $i) } }
    $1 == l && $2 == k && $3 == x { f = 1 }
    END { exit !f }' "$4"; }
if [ $rc -ne 0 ] \
   && grep -q "数字字面量后不能直接跟字母" /tmp/lab1_num.err \
   && istok "1:7" "T_ERROR" "<bad number>" /tmp/lab1_num.out \
   && istok "2:9" "T_FLOAT_LIT" "3." /tmp/lab1_num.out \
   && istok "2:11" "T_IDENT" "f" /tmp/lab1_num.out; then
    echo "[PASS] 数字规约边界(123abc 报错 / 3.f 切分)"; pass=$((pass+1))
else
    echo "[FAIL] 数字规约边界"; head -5 /tmp/lab1_num.err; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "lab1: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
