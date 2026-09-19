CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Werror -Isrc
LDLIBS  := -lm
BUILD   := out
BIN     := $(BUILD)/lab06
SRCS    := src/main.c src/fft.c src/noise.c src/vad.c src/lpc.c src/pgm_io.c src/wav_io.c
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o)

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
