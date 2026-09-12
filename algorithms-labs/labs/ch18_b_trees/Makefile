# Chapter 18 lab: B-trees.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch18_b_trees
BUILD_DIR := $(ROOT)/build/ch18_b_trees
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/btree.o

.PHONY: all test clean

all: $(BUILD_DIR)/btree_test $(BUILD_DIR)/demo_btree

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/btree_test: $(LAB_DIR)/btree_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_btree: $(LAB_DIR)/demo_btree.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/btree_test"
	@$(BUILD_DIR)/btree_test

clean:
	rm -rf $(BUILD_DIR)
