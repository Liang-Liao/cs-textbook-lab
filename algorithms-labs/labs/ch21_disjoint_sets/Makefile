# Chapter 21 lab: Disjoint sets.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch21_disjoint_sets
BUILD_DIR := $(ROOT)/build/ch21_disjoint_sets
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/disjoint_set.o

.PHONY: all test clean

all: $(BUILD_DIR)/disjoint_set_test $(BUILD_DIR)/demo_disjoint_set

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/disjoint_set_test: $(LAB_DIR)/disjoint_set_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_disjoint_set: $(LAB_DIR)/demo_disjoint_set.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/disjoint_set_test"
	@$(BUILD_DIR)/disjoint_set_test

clean:
	rm -rf $(BUILD_DIR)
