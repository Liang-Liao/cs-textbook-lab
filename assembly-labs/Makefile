# Makefile for x86-64 Assembly Labs
# 平台: Windows MSYS2 UCRT64 (make 在 MSYS2 的 /usr/bin 里)

# 全部 lab 目录
LABS := $(sort $(wildcard lab*))

# mingw32-make 默认用 cmd.exe 执行配方, 显式切到 sh
SHELL = sh

# 注意: 不要把 lab%_build 这类模式规则目标放进 .PHONY —
# .PHONY 目标会跳过隐式规则搜索, 导致配方永远不执行
.PHONY: all test clean

all: $(LABS:=_build)

# 单个 lab: make lab01_build / lab01_run / lab01_debug / lab01_test
lab%_build:
	@echo "=== Building Lab $* ==="
	@$(MAKE) -C lab$* build

lab%_run: lab%_build
	@$(MAKE) -C lab$* run

lab%_debug: lab%_build
	@$(MAKE) -C lab$* debug

lab%_test: lab%_build
	@$(MAKE) -C lab$* test

# 全部 lab 构建 + 自动验证 (与各 lab 的 expected.txt 比对)
test: $(LABS:=_test)

clean:
	@for lab in $(LABS); do \
		$(MAKE) -C $$lab clean 2>/dev/null; \
	done
	@echo "Cleaned all labs"
