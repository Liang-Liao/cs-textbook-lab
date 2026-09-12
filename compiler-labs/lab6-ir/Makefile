# lab6: AST → 三地址码（四元式）：回填、布尔跳转代码、函数调用、IR 解释执行
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
SRCS    = src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c \
          src/ir.c src/gen.c src/irvm.c src/main.c
HDRS    = src/lexer.h src/token.h src/symtab.h src/ast.h src/parser.h \
          src/eval.h src/ir.h src/gen.h src/irvm.h
TARGET  = minicc6.exe

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
