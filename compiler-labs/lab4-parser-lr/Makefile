# lab4: LR(0)/SLR(1) 自底向上分析器
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
SRCS    = src/lexer.c src/grammar.c src/slr.c src/main.c
HDRS    = src/lexer.h src/token.h src/grammar.h src/slr.h
TARGET  = minicc4.exe

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
