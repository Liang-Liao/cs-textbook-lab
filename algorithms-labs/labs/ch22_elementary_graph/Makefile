# Chapter 22 lab: Elementary graph algorithms.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch22_elementary_graph
BUILD_DIR := $(ROOT)/build/ch22_elementary_graph
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/graph.o

.PHONY: all test clean

all: $(BUILD_DIR)/graph_test $(BUILD_DIR)/demo_graph $(BUILD_DIR)/bench_bfs

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/graph_test: $(LAB_DIR)/graph_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_graph: $(LAB_DIR)/demo_graph.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/bench_bfs: $(LAB_DIR)/bench_bfs.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/graph_test"
	@$(BUILD_DIR)/graph_test

clean:
	rm -rf $(BUILD_DIR)
