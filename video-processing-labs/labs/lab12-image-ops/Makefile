CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Werror -Isrc
LDLIBS  := -lm

BUILD := out
BIN   := $(BUILD)/lab12
SRCS  := src/main.c src/ppm_io.c src/synth.c src/conv2d.c src/edge.c src/resize.c src/metrics.c
OBJS  := $(SRCS:src/%.c=$(BUILD)/%.o)

.PHONY: all test run clean

all: $(BIN)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDLIBS)

test: $(BIN)
	$(BIN) --selftest

run: $(BIN)
	$(BIN) --generate

clean:
	rm -rf $(BUILD)
