# common/rules.mk — 各 lab 共享的构建规则 (Windows MSYS2 UCRT64)
#
# 用法 (lab 目录下的 Makefile 只需 3 行):
#   TARGET = hello
#   SRCS   = hello.s
#   include ../common/rules.mk
#
# 需要自定义某个目标时 (lab10 分步编译, lab13 计时输出...):
#   在 include 之前设 LAB_NO_BUILD / LAB_NO_RUN / LAB_NO_DEBUG /
#   LAB_NO_TEST / LAB_NO_CLEAN = 1 关掉标准规则, 再写自己的规则,
#   避免目标重定义警告。

CC     = gcc
CFLAGS = -g -Wa,-I../common

# mingw32-make 默认用 cmd.exe 执行配方, 显式切到 sh (UCRT64 终端必带)
SHELL  = sh
# 统一用 .exe 后缀命名产物, 避免 mingw gcc 自动加 .exe 导致 make 依赖失效
EXE = $(TARGET).exe

ifeq ($(LAB_NO_BUILD),)
.PHONY: build
build: $(EXE)

$(EXE): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@
endif

ifeq ($(LAB_NO_RUN),)
.PHONY: run
run: $(EXE)
	./$(EXE)
endif

ifeq ($(LAB_NO_DEBUG),)
.PHONY: debug
debug: $(EXE)
	gdb -q ./$(EXE) -ex "layout asm" -ex "break main" -ex "run"
endif

# make test: 运行程序, 与 expected.txt 逐字比对
# 输出不确定的 lab (如 lab13 计时) 设 LAB_NO_TEST=1 并写自己的 test 目标
ifeq ($(LAB_NO_TEST),)
.PHONY: test
test: $(EXE)
	@./$(EXE) > .test.out 2>&1
	@if diff -q expected.txt .test.out >/dev/null 2>&1; then \
		echo "PASS $(TARGET)"; \
	else \
		echo "FAIL $(TARGET)"; \
		diff expected.txt .test.out || true; \
		rm -f .test.out; exit 1; \
	fi
	@rm -f .test.out
endif

ifeq ($(LAB_NO_CLEAN),)
.PHONY: clean
clean:
	rm -f $(EXE) *.o .test.out
endif
