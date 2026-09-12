# Chapter 16 lab: Amortized analysis (dynamic tables).

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch16_amortized
BUILD_DIR := $(ROOT)/build/ch16_amortized
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/dynamic_table.o

.PHONY: all test clean

all: $(BUILD_DIR)/amortized_test $(BUILD_DIR)/demo_amortized

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/amortized_test: $(LAB_DIR)/amortized_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_amortized: $(LAB_DIR)/demo_amortized.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/amortized_test"
	@$(BUILD_DIR)/amortized_test

clean:
	rm -rf $(BUILD_DIR)
