# lab10: 循环分析与优化：支配树/自然循环/LICM/计数循环展开（龙书第 10 章、9.6）
# rt.c 是生成程序的运行时（不进 minicc10.exe，链接期与生成的 .s 一起编译）
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
SRCS    = src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c \
          src/ir.c src/gen.c src/irvm.c src/vm.c src/codegen.c src/opt.c src/main.c
HDRS    = src/lexer.h src/token.h src/symtab.h src/ast.h src/parser.h \
          src/eval.h src/ir.h src/gen.h src/irvm.h src/vm.h src/codegen.h src/opt.h
TARGET  = minicc10.exe

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
