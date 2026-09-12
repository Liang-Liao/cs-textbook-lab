# Chapter 06 lab: Heapsort.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch06_heapsort
BUILD_DIR := $(ROOT)/build/ch06_heapsort
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/heapsort.o $(BUILD_DIR)/priority_queue.o
TESTS := $(BUILD_DIR)/heapsort_test $(BUILD_DIR)/priority_queue_test

.PHONY: all test clean

all: $(TESTS) $(BUILD_DIR)/demo_heapsort

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/heapsort_test: $(LAB_DIR)/heapsort_test.c $(BUILD_DIR)/heapsort.o $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD_DIR)/heapsort.o $(LDLIBS) -o $@

$(BUILD_DIR)/priority_queue_test: $(LAB_DIR)/priority_queue_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_heapsort: $(LAB_DIR)/demo_heapsort.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@set -e; for t in $(TESTS); do echo "== $$t"; "$$t"; done

clean:
	rm -rf $(BUILD_DIR)
