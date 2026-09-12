# Chapter 19 lab: Fibonacci heaps.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch19_fibonacci_heaps
BUILD_DIR := $(ROOT)/build/ch19_fibonacci_heaps
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/fib_heap.o

.PHONY: all test clean

all: $(BUILD_DIR)/fib_heap_test $(BUILD_DIR)/demo_fib_heap

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fib_heap_test: $(LAB_DIR)/fib_heap_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_fib_heap: $(LAB_DIR)/demo_fib_heap.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/fib_heap_test"
	@$(BUILD_DIR)/fib_heap_test

clean:
	rm -rf $(BUILD_DIR)
