# Chapter 30 lab: Polynomials and the FFT.

ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
LAB_DIR := $(ROOT)/labs/ch30_polynomials_fft
BUILD_DIR := $(ROOT)/build/ch30_polynomials_fft
COMMON_LIB := $(ROOT)/build/libclrs.a
COMMON_INC := $(ROOT)/common/include

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -I$(COMMON_INC) -I$(LAB_DIR)
LDLIBS := $(COMMON_LIB) -lm

OBJS := $(BUILD_DIR)/poly_fft.o

.PHONY: all test clean

all: $(BUILD_DIR)/poly_fft_test $(BUILD_DIR)/demo_fft

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(LAB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/poly_fft_test: $(LAB_DIR)/poly_fft_test.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

$(BUILD_DIR)/demo_fft: $(LAB_DIR)/demo_fft.c $(OBJS) $(COMMON_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(OBJS) $(LDLIBS) -o $@

test: all
	@echo "== $(BUILD_DIR)/poly_fft_test"
	@$(BUILD_DIR)/poly_fft_test

clean:
	rm -rf $(BUILD_DIR)
