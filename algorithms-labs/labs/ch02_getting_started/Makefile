# Chapter 02 lab: Getting Started (insertion sort, merge sort).

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch02_getting_started
BUILD_DIR := $(ROOT)/build/ch02_getting_started
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

ALGOS := insertion_sort merge_sort
TESTS := $(addprefix $(BUILD_DIR)/,$(addsuffix _test,$(ALGOS)))
DEMOS := $(BUILD_DIR)/demo_sort $(BUILD_DIR)/bench_sort
OBJS := $(BUILD_DIR)/insertion_sort.o $(BUILD_DIR)/merge_sort.o

.PHONY: all test clean

all: $(TESTS) $(DEMOS)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/insertion_sort.o: $(LAB_DIR)/insertion_sort.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/merge_sort.o: $(LAB_DIR)/merge_sort.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/insertion_sort_test: $(LAB_DIR)/insertion_sort_test.c \
		$(BUILD_DIR)/insertion_sort.o $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD_DIR)/insertion_sort.o $(LDLIBS) -o $@

$(BUILD_DIR)/merge_sort_test: $(LAB_DIR)/merge_sort_test.c \
		$(BUILD_DIR)/merge_sort.o $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD_DIR)/merge_sort.o $(LDLIBS) -o $@

$(BUILD_DIR)/demo_sort: $(LAB_DIR)/demo_sort.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/bench_sort: $(LAB_DIR)/bench_sort.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@set -e; for t in $(TESTS); do echo "== $$t"; "$$t"; done

clean:
	rm -rf $(BUILD_DIR)
