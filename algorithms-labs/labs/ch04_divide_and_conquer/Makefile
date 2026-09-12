# Chapter 04 lab: Divide-and-Conquer.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch04_divide_and_conquer
BUILD_DIR := $(ROOT)/build/ch04_divide_and_conquer
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/max_subarray.o $(BUILD_DIR)/matrix_multiply.o
TESTS := $(BUILD_DIR)/max_subarray_test $(BUILD_DIR)/matrix_multiply_test
DEMOS := $(BUILD_DIR)/demo_divide_conquer

.PHONY: all test clean

all: $(TESTS) $(DEMOS)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/max_subarray_test: $(LAB_DIR)/max_subarray_test.c \
		$(BUILD_DIR)/max_subarray.o $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD_DIR)/max_subarray.o $(LDLIBS) -o $@

$(BUILD_DIR)/matrix_multiply_test: $(LAB_DIR)/matrix_multiply_test.c \
		$(BUILD_DIR)/matrix_multiply.o $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD_DIR)/matrix_multiply.o $(LDLIBS) -o $@

$(BUILD_DIR)/demo_divide_conquer: $(LAB_DIR)/demo_divide_conquer.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@set -e; for t in $(TESTS); do echo "== $$t"; "$$t"; done

clean:
	rm -rf $(BUILD_DIR)
