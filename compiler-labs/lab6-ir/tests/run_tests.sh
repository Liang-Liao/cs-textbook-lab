#!/usr/bin/env bash
# lab6 测试：求值对拍（expr/vars 迎来第四跑）+ 短路/函数/数组 + IR dump 对拍
#               + 双引擎对拍（IR 执行 vs 树遍执行）+ 类型错误拒绝
# 用法: bash tests/run_tests.sh   （在 lab6-ir 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minicc6.exe
CASES=tests/cases
pass=0; fail=0

SRCS="src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c src/ir.c src/gen.c src/irvm.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab6_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab6_cc.log; exit 1
fi
if [ -s /tmp/lab6_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab6_cc.log; exit 1
fi

check() { # check <名字> <实际文件> <期望文件>
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab6_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; head -20 /tmp/lab6_diff.log; fail=$((fail+1))
    fi
}

# 1) expr.mc / vars.mc：lab3/lab5 的同一测试集第四跑——IR 执行输出逐字节一致
"$BIN" "$CASES/expr.mc" > /tmp/lab6_expr.out 2>/dev/null
check "表达式集(与 lab3/lab5 对拍,第四跑)" /tmp/lab6_expr.out "$CASES/expr.expected"
"$BIN" "$CASES/vars.mc" > /tmp/lab6_vars.out 2>/dev/null
check "变量与作用域集(第四跑)" /tmp/lab6_vars.out "$CASES/vars.expected"

# 2) bool.mc：短路求值（1/0 被跳过不死机）+ 优先级
"$BIN" "$CASES/bool.mc" > /tmp/lab6_bool.out 2>/dev/null
check "短路布尔执行" /tmp/lab6_bool.out "$CASES/bool.expected"

# 3) func.mc：递归/参数提升/void 副作用/全局读写/嵌套递归
"$BIN" "$CASES/func.mc" > /tmp/lab6_func.out 2>/dev/null
check "函数与递归执行" /tmp/lab6_func.out "$CASES/func.expected"

# 3b) arr.mc：一维数组（6.4）——零初始化/表达式下标/浮点数组/递归各拥其帧
"$BIN" "$CASES/arr.mc" > /tmp/lab6_arr.out 2>/dev/null
check "数组执行(6.4)" /tmp/lab6_arr.out "$CASES/arr.expected"

# 4) ir.mc 执行 + IR dump 对拍（回填结果：名字唯一化 main_x@1、跳转齐全）
"$BIN" "$CASES/ir.mc" > /tmp/lab6_ir.out 2>/dev/null
check "遮蔽/循环/调用执行" /tmp/lab6_ir.out "$CASES/ir.expected"
"$BIN" -ir "$CASES/ir.mc" > /tmp/lab6_irdump.out 2>/dev/null
check "四元式 dump(回填+名字唯一化)" /tmp/lab6_irdump.out "$CASES/ir_dump.expected"

# 5) 双引擎对拍：同一程序 IR 执行 与 树遍执行 输出必须一致
#    （gen/irvm 的真 bug——比较走错运算、全局帧丢失、短路失效——都是它抓的）
allok=1
for c in expr vars bool func ir arr arrloop; do
    "$BIN" "$CASES/$c.mc" > /tmp/lab6_a.out 2>/dev/null
    "$BIN" -eval "$CASES/$c.mc" > /tmp/lab6_b.out 2>/dev/null
    diff <(tr -d '\r' < /tmp/lab6_a.out) <(tr -d '\r' < /tmp/lab6_b.out) > /dev/null \
        || { allok=0; echo "  -> $c.mc 两引擎输出不一致"; }
done
if [ $allok -eq 1 ]; then
    echo "[PASS] 双引擎对拍(IR vs 树遍, 7 个程序)"; pass=$((pass+1))
else
    echo "[FAIL] 双引擎对拍"; fail=$((fail+1))
fi

# 5b) 数组的类型错误：五类各报一条、退出码非 0、不执行
"$BIN" "$CASES/arrerr.mc" > /tmp/lab6_aout.log 2> /tmp/lab6_aerr.log
rc=$?
if [ $rc -ne 0 ] && [ ! -s /tmp/lab6_aout.log ] \
   && grep -q "必须带下标" /tmp/lab6_aerr.log \
   && grep -q "不能用下标访问" /tmp/lab6_aerr.log \
   && grep -q "数组下标必须是 int" /tmp/lab6_aerr.log \
   && grep -q "数组长度必须为正" /tmp/lab6_aerr.log \
   && grep -q "不能整体初始化" /tmp/lab6_aerr.log; then
    echo "[PASS] 数组类型错误全部报出且不执行"; pass=$((pass+1))
else
    echo "[FAIL] 数组错误检查: rc=$rc"; cat /tmp/lab6_aerr.log; fail=$((fail+1))
fi

# 5b2) main 是保留名：用户函数不能叫 main——顶层段固定占名，否则
#      irvm 调 main() 命中顶层段而非函数体（静默错语义）
"$BIN" "$CASES/mainname.mc" > /tmp/lab6_mout.log 2> /tmp/lab6_merr.log
rc=$?
if [ $rc -ne 0 ] && [ ! -s /tmp/lab6_mout.log ] \
   && grep -q "main 是顶层段的保留名" /tmp/lab6_merr.log; then
    echo "[PASS] main 保留名拒绝且不执行"; pass=$((pass+1))
else
    echo "[FAIL] main 保留名检查: rc=$rc"; cat /tmp/lab6_merr.log; fail=$((fail+1))
fi

# 6) 类型/语义错误：退出码 1、9 类错误各报出、不执行（stdout 无输出）
"$BIN" "$CASES/typeerr.mc" > /tmp/lab6_tout.log 2> /tmp/lab6_terr.log
rc=$?
if [ $rc -ne 0 ] && [ ! -s /tmp/lab6_tout.log ] \
   && grep -q "重复声明 f" /tmp/lab6_terr.log \
   && grep -q "重复声明 g" /tmp/lab6_terr.log \
   && grep -q "调用了未定义的函数" /tmp/lab6_terr.log \
   && grep -q "需要 2 个实参" /tmp/lab6_terr.log \
   && grep -q "隐式收窄" /tmp/lab6_terr.log \
   && grep -c "void 函数调用没有值" /tmp/lab6_terr.log | grep -qx "2" \
   && grep -q "return 在函数外" /tmp/lab6_terr.log \
   && grep -q "语法错误" /tmp/lab6_terr.log; then
    echo "[PASS] 9 类错误全部报出且不执行"; pass=$((pass+1))
else
    echo "[FAIL] 错误检查: rc=$rc"; cat /tmp/lab6_terr.log; fail=$((fail+1))
fi

# 6b) decl 作 if/while 非块体：语法层必须拒绝（放行会让 -eval 空指针
#     解引用、IRVM 静默读 0——审计修复回归）
"$BIN" "$CASES/declbody.mc" > /tmp/lab6_dbout.log 2> /tmp/lab6_dberr.log
rc=$?
if [ $rc -ne 0 ] && [ ! -s /tmp/lab6_dbout.log ] \
   && grep -q "声明不能直接作为" /tmp/lab6_dberr.log; then
    echo "[PASS] 声明作非块控制体被拒绝(declbody)"; pass=$((pass+1))
else
    echo "[FAIL] declbody 应报错拒绝: rc=$rc"; cat /tmp/lab6_dberr.log; fail=$((fail+1))
fi

# 7) 运行时除零仍是停机错误（编译期拦不住的那类）
printf 'print 1 / 0;\n' > /tmp/lab6_divz.mc
if ! "$BIN" /tmp/lab6_divz.mc > /dev/null 2>&1; then
    echo "[PASS] 整数除零停机"; pass=$((pass+1))
else
    echo "[FAIL] 除零未被拦截"; fail=$((fail+1))
fi

# 8) 用法横幅（冒烟）
if "$BIN" 2>&1 | grep -q "用法"; then
    echo "[PASS] 用法横幅"; pass=$((pass+1))
else
    echo "[FAIL] 无参数未打印用法"; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "lab6: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
