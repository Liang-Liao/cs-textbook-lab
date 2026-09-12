# lab8: 目标代码生成：四元式 → x86-64 汇编（Windows ABI），经 gcc 链接成真 .exe
# rt.c 是生成程序的运行时（不进 minicc8.exe，链接期与生成的 .s 一起编译）
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
SRCS    = src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c \
          src/ir.c src/gen.c src/irvm.c src/vm.c src/codegen.c src/main.c
HDRS    = src/lexer.h src/token.h src/symtab.h src/ast.h src/parser.h \
          src/eval.h src/ir.h src/gen.h src/irvm.h src/vm.h src/codegen.h
TARGET  = minicc8.exe

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
