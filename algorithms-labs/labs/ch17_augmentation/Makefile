# Chapter 17 lab: Augmenting data structures.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch17_augmentation
BUILD_DIR := $(ROOT)/build/ch17_augmentation
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/order_stat_tree.o $(BUILD_DIR)/interval_tree.o

.PHONY: all test clean

all: $(BUILD_DIR)/augmentation_test $(BUILD_DIR)/demo_augmentation

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/augmentation_test: $(LAB_DIR)/augmentation_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_augmentation: $(LAB_DIR)/demo_augmentation.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/augmentation_test"
	@$(BUILD_DIR)/augmentation_test

clean:
	rm -rf $(BUILD_DIR)
