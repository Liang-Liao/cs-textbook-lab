CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Werror -Isrc -pthread
LDLIBS  := -lm -lws2_32 -pthread
BUILD   := out
BIN     := $(BUILD)/lab18
SRCS    := src/main.c src/rtp.c src/udp.c
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
