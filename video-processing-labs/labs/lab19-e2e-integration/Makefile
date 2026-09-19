CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Werror -Isrc -pthread
LDLIBS  := -lm -pthread
BUILD   := out
BIN     := $(BUILD)/lab19
SRCS    := src/main.c src/pipeline.c src/wav_io.c src/adpcm.c src/agc.c src/ns_lite.c src/rtp_lite.c src/ppm_io.c src/vid_codec.c src/aec_lite.c src/vad_lite.c src/channel_lite.c src/avsync_lite.c
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
