# Chapter 11 lab: Hash tables.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch11_hash_tables
BUILD_DIR := $(ROOT)/build/ch11_hash_tables
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB) -lm

OBJS := $(BUILD_DIR)/direct_address.o $(BUILD_DIR)/hash_chain.o \
        $(BUILD_DIR)/hash_open_address.o

.PHONY: all test clean

all: $(BUILD_DIR)/hash_tables_test $(BUILD_DIR)/demo_hash_tables

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hash_tables_test: $(LAB_DIR)/hash_tables_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_hash_tables: $(LAB_DIR)/demo_hash_tables.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/hash_tables_test"
	@$(BUILD_DIR)/hash_tables_test

clean:
	rm -rf $(BUILD_DIR)
