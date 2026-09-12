# Chapter 28 lab: Matrix operations.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch28_matrix_operations
BUILD_DIR := $(ROOT)/build/ch28_matrix_operations
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB) -lm

OBJS := $(BUILD_DIR)/matrix_ops.o

.PHONY: all test clean

all: $(BUILD_DIR)/matrix_ops_test $(BUILD_DIR)/demo_matrix_ops

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/matrix_ops_test: $(LAB_DIR)/matrix_ops_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_matrix_ops: $(LAB_DIR)/demo_matrix_ops.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/matrix_ops_test"
	@$(BUILD_DIR)/matrix_ops_test

clean:
	rm -rf $(BUILD_DIR)
