# Lab 11: 文件 I/O
TARGET    = file_io
SRCS      = file_io.s
LAB_NO_RUN = 1
LAB_NO_CLEAN = 1
include ../common/rules.mk

# 运行后顺带展示写出的文件
run: $(EXE)
	./$(EXE)
	@echo "---"
	@cat output.txt 2>/dev/null || true

clean:
	rm -f $(EXE) *.o .test.out output.txt
