# Common library for CLRS labs.
# Produces build/libclrs.a relative to the repository root.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
COMMON_DIR := $(ROOT)/common
BUILD_DIR := $(ROOT)/build/common
LIB := $(ROOT)/build/libclrs.a

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_DIR)/include

SRCS := $(COMMON_DIR)/src/array.c \
        $(COMMON_DIR)/src/timing.c \
        $(COMMON_DIR)/src/test.c

OBJS := $(patsubst $(COMMON_DIR)/src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(LIB)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(COMMON_DIR)/src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS)
	@mkdir -p $(dir $(LIB))
	$(AR) rcs $@ $(OBJS)

clean:
	rm -rf $(BUILD_DIR) $(LIB)
