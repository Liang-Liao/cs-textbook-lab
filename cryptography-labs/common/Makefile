# MSYS2 UCRT64 / mingw32-make (cmd.exe recipes)
CC      ?= gcc
AR      ?= ar
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS = -I.

TARGET = libcommon.a
OBJS   = crypto_common.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $(OBJS)
	-del /F /Q $(OBJS) 2>NUL

crypto_common.o : crypto_common.c crypto_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	-del /F /Q $(OBJS) $(TARGET) 2>NUL