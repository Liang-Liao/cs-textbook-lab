# Chapter 05 lab: Probabilistic Analysis and Randomized Algorithms.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch05_probabilistic
BUILD_DIR := $(ROOT)/build/ch05_probabilistic
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/random.o $(BUILD_DIR)/hiring.o

.PHONY: all test clean

all: $(BUILD_DIR)/probabilistic_test $(BUILD_DIR)/demo_probabilistic

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/probabilistic_test: $(LAB_DIR)/probabilistic_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_probabilistic: $(LAB_DIR)/demo_probabilistic.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/probabilistic_test"
	@$(BUILD_DIR)/probabilistic_test

clean:
	rm -rf $(BUILD_DIR)
