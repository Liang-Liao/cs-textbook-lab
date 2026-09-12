# Chapter 10 lab: Elementary data structures.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch10_elementary_ds
BUILD_DIR := $(ROOT)/build/ch10_elementary_ds
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/stack.o $(BUILD_DIR)/queue.o \
        $(BUILD_DIR)/linked_list.o $(BUILD_DIR)/binary_tree.o

.PHONY: all test clean

all: $(BUILD_DIR)/elementary_ds_test $(BUILD_DIR)/demo_elementary_ds

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/elementary_ds_test: $(LAB_DIR)/elementary_ds_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_elementary_ds: $(LAB_DIR)/demo_elementary_ds.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/elementary_ds_test"
	@$(BUILD_DIR)/elementary_ds_test

clean:
	rm -rf $(BUILD_DIR)
