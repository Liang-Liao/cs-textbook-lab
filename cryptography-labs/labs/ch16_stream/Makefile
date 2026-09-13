# MSYS2 UCRT64 / mingw32-make (cmd.exe recipes)
CC       ?= gcc
CFLAGS   ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS  = -I../../common
LDLIBS    = -L../../common -lcommon -lbcrypt

TARGET = test_ch16
OBJS   = lfsr.o rc4.o test_ch16.o

.PHONY: all test clean

all: $(TARGET)
	-del /F /Q $(OBJS) 2>NUL

test: all
	.\$(TARGET).exe

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

lfsr.o : lfsr.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

rc4.o : rc4.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

test_ch16.o : test_ch16.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

clean:
	-del /F /Q $(TARGET).exe $(OBJS) 2>NUL