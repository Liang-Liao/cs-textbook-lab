# Lab 10: C 与汇编互操作
# 分步生成 .o, 演示"编译-汇编-链接"三阶段 (覆盖标准 build 规则)
TARGET       = main
LAB_NO_BUILD = 1
include ../common/rules.mk

main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

asm_funcs.o: asm_funcs.s
	$(CC) $(CFLAGS) -c $< -o $@

build: $(EXE)

$(EXE): main.o asm_funcs.o
	$(CC) $(CFLAGS) -o $@ main.o asm_funcs.o
