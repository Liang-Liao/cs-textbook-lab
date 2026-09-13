# Shared lab build fragment. Include from labs/labNN-*/Makefile
# Set before include:
#   LAB_NAME   — binary name
# Optional:
#   COMMON_DIR — default ../../common
#   LAB_SRCS   — default $(wildcard src/*.c)
#   COMMON_SRCS — default $(COMMON_DIR)/*.c

# lab.mk lives at <root>/make/lab.mk
CGL_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

CC      ?= gcc
ifeq ($(origin CC),default)
  CC := gcc
endif
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O0 -g
LDFLAGS ?= -lm

COMMON_DIR  ?= $(CGL_ROOT)/common
OUT_DIR     ?= out
BIN_DIR     ?= $(OUT_DIR)/bin

ifeq ($(LAB_SRCS),)
  LAB_SRCS := $(wildcard src/*.c)
endif
ifeq ($(COMMON_SRCS),)
  COMMON_SRCS := $(wildcard $(COMMON_DIR)/*.c)
endif

CFLAGS += -I$(COMMON_DIR)

LAB_OBJS    := $(patsubst src/%.c,$(OUT_DIR)/lab_%.o,$(LAB_SRCS))
COMMON_OBJS := $(patsubst $(COMMON_DIR)/%.c,$(OUT_DIR)/common_%.o,$(COMMON_SRCS))
OBJS        := $(LAB_OBJS) $(COMMON_OBJS)
BIN         := $(BIN_DIR)/$(LAB_NAME)

.PHONY: all clean release

all: $(BIN)

# Target-specific CFLAGS replaces the global value for this target and all
# prerequisites, so it must restate -I$(COMMON_DIR) (added below) or every
# lab compile loses the include path.
release: CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -I$(COMMON_DIR)
release: clean all

$(BIN): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OUT_DIR)/lab_%.o: src/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_DIR)/common_%.o: $(COMMON_DIR)/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Portable mkdir/rm. OS=Windows_NT is also set inside MSYS2/Git Bash shells,
# where sh.exe on PATH executes recipes; MSYSTEM is only set by those shells,
# so use it to pick POSIX vs cmd syntax.
ifeq ($(OS),Windows_NT)
ifneq ($(MSYSTEM),)
USE_POSIX := 1
endif
else
USE_POSIX := 1
endif

ifeq ($(USE_POSIX),1)
$(OUT_DIR) $(BIN_DIR):
	@mkdir -p $@
clean:
	@rm -rf $(OUT_DIR)
else
$(OUT_DIR) $(BIN_DIR):
	@if not exist $(subst /,\,$@) mkdir $(subst /,\,$@)
clean:
	@if exist $(subst /,\,$(OUT_DIR)) rmdir /s /q $(subst /,\,$(OUT_DIR))
endif
