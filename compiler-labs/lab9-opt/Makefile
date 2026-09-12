# lab9: IR 优化器：常量折叠/传播、局部 CSE、活跃变量死代码删除、窥孔（龙书第 9 章）
# rt.c 是生成程序的运行时（不进 minicc9.exe，链接期与生成的 .s 一起编译）
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
SRCS    = src/lexer.c src/symtab.c src/ast.c src/parser.c src/eval.c \
          src/ir.c src/gen.c src/irvm.c src/vm.c src/codegen.c src/opt.c src/main.c
HDRS    = src/lexer.h src/token.h src/symtab.h src/ast.h src/parser.h \
          src/eval.h src/ir.h src/gen.h src/irvm.h src/vm.h src/codegen.h src/opt.h
TARGET  = minicc9.exe

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
