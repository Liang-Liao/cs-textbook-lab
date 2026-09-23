# lab build: src + test (bin/test) + vendor
# 测试: ./bin/test [--list] [suite|suite/name ...]
# make check 跑全量；make check TEST_ARGS=linalg 可选算法

MSYS2_ROOT ?= C:/msys64
SHELL      := $(MSYS2_ROOT)/usr/bin/sh.exe
.SHELLFLAGS := -c
export PATH := $(MSYS2_ROOT)\ucrt64\bin;$(MSYS2_ROOT)\usr\bin;$(PATH)

CC       ?= gcc
CFLAGS   ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration
LAB_ROOT ?= ../..
CPPFLAGS += -Isrc -Ivendor -Itest
LDLIBS   += -lm
EXEEXT   ?=

BIN_DIR  := bin
TEST_BIN := $(BIN_DIR)/test$(EXEEXT)
TEST_ARGS ?=

SRC_SRC    ?= $(wildcard src/*.c)
TEST_SRC   ?= $(wildcard test/*.c)
VENDOR_SRC ?= $(wildcard vendor/*.c)

SRC_OBJS    := $(patsubst src/%.c,$(BIN_DIR)/src_%.o,$(SRC_SRC))
TEST_OBJS   := $(patsubst test/%.c,$(BIN_DIR)/test_%.o,$(TEST_SRC))
VENDOR_OBJS := $(patsubst vendor/%.c,$(BIN_DIR)/vendor_%.o,$(VENDOR_SRC))

.PHONY: all check clean test-list

all: $(TEST_BIN)

$(TEST_BIN): $(SRC_OBJS) $(TEST_OBJS) $(VENDOR_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC_OBJS) $(TEST_OBJS) $(VENDOR_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(BIN_DIR)/src_%.o: src/%.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BIN_DIR)/test_%.o: test/%.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BIN_DIR)/vendor_%.o: vendor/%.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# 全量: make check
# 单算法: make check TEST_ARGS=linalg
# 单场景: make check TEST_ARGS=linalg/lu_well_conditioned
check: $(TEST_BIN)
	./$(TEST_BIN) $(TEST_ARGS)

test-list: $(TEST_BIN)
	./$(TEST_BIN) --list

clean:
	rm -rf $(BIN_DIR)
