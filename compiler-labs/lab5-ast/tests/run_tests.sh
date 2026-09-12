#!/usr/bin/env bash
# lab5 测试：求值对拍（含 lab3 同一测试集"第三跑"）+ AST/符号表 dump 对拍 + 类型错误拒绝
# 用法: bash tests/run_tests.sh   （在 lab5-ast 目录下执行）

set -u
cd "$(dirname "$0")/.."

BIN=./minicc5.exe
CASES=tests/cases
pass=0; fail=0

SRCS="src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c src/main.c"
if ! gcc -std=c11 -Wall -Wextra -O2 -o "$BIN" $SRCS 2> /tmp/lab5_cc.log; then
    echo "[FAIL] 编译失败"; cat /tmp/lab5_cc.log; exit 1
fi
if [ -s /tmp/lab5_cc.log ]; then
    echo "[FAIL] 编译有警告:"; cat /tmp/lab5_cc.log; exit 1
fi

check() { # check <名字> <实际文件> <期望文件>
    if diff -u <(tr -d '\r' < "$3") <(tr -d '\r' < "$2") > /tmp/lab5_diff.log 2>&1; then
        echo "[PASS] $1"; pass=$((pass+1))
    else
        echo "[FAIL] $1"; head -20 /tmp/lab5_diff.log; fail=$((fail+1))
    fi
}

# 1) expr.mc：lab3 的同一表达式集——递归下降直接求值(lab3)/SLR(lab4)/AST 解释执行(lab5)
#    三代分析器跑同一程序，print 输出必须逐字节一致
"$BIN" "$CASES/expr.mc" > /tmp/lab5_expr.out 2>/dev/null
check "表达式集(与 lab3 对拍,第三跑)" /tmp/lab5_expr.out "$CASES/expr.expected"

# 2) vars.mc：变量/类型提升/if-else/while/break-continue/块作用域遮蔽
if "$BIN" "$CASES/vars.mc" > /tmp/lab5_vars.out 2>/dev/null; then
    check "变量与作用域执行" /tmp/lab5_vars.out "$CASES/vars.expected"
else
    echo "[FAIL] 合法程序被拒绝"; fail=$((fail+1))
fi

# 3) AST dump 对拍：i2f 提升节点、块遮蔽、语句链都应在树上显式可见
"$BIN" -ast "$CASES/ast.mc" > /tmp/lab5_ast.out 2>/dev/null
check "AST dump(S-表达式)" /tmp/lab5_ast.out "$CASES/ast.expected"

# 4) 作用域树 dump 对拍：嵌套作用域 + 同名遮蔽 = 不同 scope 的两个符号
"$BIN" -symtab "$CASES/symtab.mc" > /tmp/lab5_sym.out 2>/dev/null
check "符号表作用域树" /tmp/lab5_sym.out "$CASES/symtab.expected"

# 5) 类型/语义错误：退出码 1，6 类错误消息各报一次，程序不执行（stdout 无输出）
"$BIN" "$CASES/typeerr.mc" > /tmp/lab5_tout.log 2> /tmp/lab5_terr.log
rc=$?
if [ $rc -ne 0 ] && [ ! -s /tmp/lab5_tout.log ] \
   && grep -q "重复声明" /tmp/lab5_terr.log \
   && grep -q "未声明" /tmp/lab5_terr.log \
   && grep -q "float 赋给 int" /tmp/lab5_terr.log \
   && grep -q "取模" /tmp/lab5_terr.log \
   && grep -q "在循环外" /tmp/lab5_terr.log \
   && grep -q "语法错误" /tmp/lab5_terr.log; then
    echo "[PASS] 6 类错误全部报出且不执行"; pass=$((pass+1))
else
    echo "[FAIL] 错误检查: rc=$rc"; cat /tmp/lab5_terr.log; fail=$((fail+1))
fi

# 5b) decl 作 if/while 非块体：语法层必须拒绝（放行会产出"编译可见、
#     运行未绑定"的分裂——审计修复回归）
"$BIN" "$CASES/declbody.mc" > /tmp/lab5_dbout.log 2> /tmp/lab5_dberr.log
rc=$?
if [ $rc -ne 0 ] && [ ! -s /tmp/lab5_dbout.log ] \
   && grep -q "声明不能直接作为" /tmp/lab5_dberr.log; then
    echo "[PASS] 声明作非块控制体被拒绝(declbody)"; pass=$((pass+1))
else
    echo "[FAIL] declbody 应报错拒绝: rc=$rc"; cat /tmp/lab5_dberr.log; fail=$((fail+1))
fi

# 6) -ast 是"只翻译不执行"：输出里不应出现任何 print 的求值结果
if ! "$BIN" -ast "$CASES/vars.mc" 2>/dev/null | grep -qx "22"; then
    echo "[PASS] -ast 只打印树不执行"; pass=$((pass+1))
else
    echo "[FAIL] -ast 模式竟执行了程序"; fail=$((fail+1))
fi

# 7) 无参数打印用法（冒烟）
if "$BIN" 2>&1 | grep -q "用法"; then
    echo "[PASS] 用法横幅"; pass=$((pass+1))
else
    echo "[FAIL] 无参数未打印用法"; fail=$((fail+1))
fi

# 8) 运行期除零与模零：编译通过，求值时停机报错——错误也是语义
dz_ok=1
for prog in 'print 1 / 0;' 'print 7 % 0;'; do
    printf '%s\n' "$prog" > /tmp/lab5_dz.mc
    "$BIN" /tmp/lab5_dz.mc > /dev/null 2>/tmp/lab5_dz.log; rc=$?
    if [ $rc -eq 0 ] || ! grep -q "除数为零" /tmp/lab5_dz.log; then dz_ok=0; fi
done
if [ $dz_ok -eq 1 ]; then
    echo "[PASS] 运行期除零/模零停机报错"; pass=$((pass+1))
else
    echo "[FAIL] 除零保真"; cat /tmp/lab5_dz.log; fail=$((fail+1))
fi

# 9) 负数取模：C 语义向零截断（-7 % 3 = -1，不是 +2）
printf 'print -7 %% 3;\n' > /tmp/lab5_mod.mc
"$BIN" /tmp/lab5_mod.mc > /tmp/lab5_mod.out 2>/dev/null
if [ "$(cat /tmp/lab5_mod.out)" = "-1" ]; then
    echo "[PASS] 负数取模向零截断"; pass=$((pass+1))
else
    echo "[FAIL] 取模方向异常"; cat /tmp/lab5_mod.out; fail=$((fail+1))
fi

# 10) float 作条件：非零即真（锁定现行为）
printf 'if (1.5) print 1; else print 2;\n' > /tmp/lab5_fcond.mc
"$BIN" /tmp/lab5_fcond.mc > /tmp/lab5_fcond.out 2>/dev/null
if [ "$(cat /tmp/lab5_fcond.out)" = "1" ]; then
    echo "[PASS] float 条件非零即真"; pass=$((pass+1))
else
    echo "[FAIL] float 条件行为漂移"; cat /tmp/lab5_fcond.out; fail=$((fail+1))
fi

# 11) 三层同名遮蔽：每层读到自己的 x（3/2/1）
printf 'int x = 1;\n{ int x = 2; { int x = 3; print x; } print x; }\nprint x;\n' \
    > /tmp/lab5_sh.mc
"$BIN" /tmp/lab5_sh.mc > /tmp/lab5_sh.out 2>/dev/null
if [ "$(tr -d ' \r\n' < /tmp/lab5_sh.out)" = "321" ]; then
    echo "[PASS] 三层遮蔽各读其层"; pass=$((pass+1))
else
    echo "[FAIL] 遮蔽链异常"; cat /tmp/lab5_sh.out; fail=$((fail+1))
fi

# 12) 有语义错误时 -ast/-symtab 的只翻译路径也必须拒绝输出
printf 'int x = ;\n' > /tmp/lab5_err.mc
ast_ok=1
"$BIN" -ast /tmp/lab5_err.mc >/tmp/lab5_ae.out 2>/dev/null && ast_ok=0
[ -s /tmp/lab5_ae.out ] && ast_ok=0
"$BIN" -symtab /tmp/lab5_err.mc >/tmp/lab5_se.out 2>/dev/null && ast_ok=0
[ -s /tmp/lab5_se.out ] && ast_ok=0
if [ $ast_ok -eq 1 ]; then
    echo "[PASS] 出错时 -ast/-symtab 拒绝输出"; pass=$((pass+1))
else
    echo "[FAIL] 出错后仍产出 dump"; fail=$((fail+1))
fi

echo "----------------------------------------"
echo "lab5: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
