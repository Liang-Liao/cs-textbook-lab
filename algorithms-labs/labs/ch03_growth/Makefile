# Chapter 03 lab: Growth of Functions.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch03_growth
BUILD_DIR := $(ROOT)/build/ch03_growth
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB) -lm

.PHONY: all test clean

all: $(BUILD_DIR)/growth_test $(BUILD_DIR)/demo_growth

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/growth.o: $(LAB_DIR)/growth.c $(LAB_DIR)/growth.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/growth_test: $(LAB_DIR)/growth_test.c $(BUILD_DIR)/growth.o $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD_DIR)/growth.o $(LDLIBS) -o $@

$(BUILD_DIR)/demo_growth: $(LAB_DIR)/demo_growth.c $(BUILD_DIR)/growth.o $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD_DIR)/growth.o $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/growth_test"
	@$(BUILD_DIR)/growth_test

clean:
	rm -rf $(BUILD_DIR)
