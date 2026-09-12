# Chapter 26 lab: Maximum flow.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch26_maxflow
BUILD_DIR := $(ROOT)/build/ch26_maxflow
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB)

OBJS := $(BUILD_DIR)/maxflow.o

.PHONY: all test clean

all: $(BUILD_DIR)/maxflow_test $(BUILD_DIR)/demo_maxflow

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/maxflow_test: $(LAB_DIR)/maxflow_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_maxflow: $(LAB_DIR)/demo_maxflow.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/maxflow_test"
	@$(BUILD_DIR)/maxflow_test

clean:
	rm -rf $(BUILD_DIR)
