# Chapter 24 lab: Single-source shortest paths.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch24_shortest_paths
BUILD_DIR := $(ROOT)/build/ch24_shortest_paths
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/shortest_paths.o

.PHONY: all test clean

all: $(BUILD_DIR)/shortest_paths_test $(BUILD_DIR)/demo_shortest_paths

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shortest_paths_test: $(LAB_DIR)/shortest_paths_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_shortest_paths: $(LAB_DIR)/demo_shortest_paths.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/shortest_paths_test"
	@$(BUILD_DIR)/shortest_paths_test

clean:
	rm -rf $(BUILD_DIR)
