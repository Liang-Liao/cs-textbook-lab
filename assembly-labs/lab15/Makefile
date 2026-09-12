# Lab 15: Shellcode 测试框架 (C 程序, 调试不用 layout asm)
TARGET      = loader
SRCS        = loader.c
LAB_NO_DEBUG = 1
include ../common/rules.mk

debug: $(EXE)
	gdb -q ./$(EXE) -ex "break main" -ex "run"
