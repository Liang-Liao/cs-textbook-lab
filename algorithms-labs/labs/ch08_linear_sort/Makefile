# Chapter 08 lab: Sorting in Linear Time.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch08_linear_sort
BUILD_DIR := $(ROOT)/build/ch08_linear_sort
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB) -lm

OBJS := $(BUILD_DIR)/counting_sort.o $(BUILD_DIR)/radix_sort.o \
        $(BUILD_DIR)/bucket_sort.o

.PHONY: all test clean

all: $(BUILD_DIR)/linear_sort_test $(BUILD_DIR)/demo_linear_sort

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/linear_sort_test: $(LAB_DIR)/linear_sort_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_linear_sort: $(LAB_DIR)/demo_linear_sort.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/linear_sort_test"
	@$(BUILD_DIR)/linear_sort_test

clean:
	rm -rf $(BUILD_DIR)
