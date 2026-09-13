# Top-level aggregator (MSYS2 UCRT64 + mingw32-make).
# test/clean go through scripts/*.sh via MSYS2 sh.exe

# 按环境变量 MSYS_SH 或 PATH 中的 sh 探测；也可显式传入完整路径，如：
# mingw32-make test MSYS_SH=C:/msys64/usr/bin/sh.exe
MSYS_SH ?= sh
MAKE    ?= mingw32-make

LABS := $(sort $(dir $(wildcard labs/ch*/Makefile)))

.PHONY: all test clean common $(LABS)

all: common $(LABS)

common:
	$(MAKE) -C common

test: common
	$(MSYS_SH) scripts/run_tests.sh

clean:
	$(MSYS_SH) scripts/clean_all.sh

$(LABS): common
	$(MAKE) -C $@
