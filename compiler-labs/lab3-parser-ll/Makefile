# lab3: 递归下降 + LL(1) 预测分析器
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
SRCS    = src/lexer.c src/grammar.c src/rd_parser.c src/ll_parser.c src/main.c
HDRS    = src/lexer.h src/token.h src/grammar.h src/rd_parser.h src/ll_parser.h
TARGET  = minicc3.exe

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
