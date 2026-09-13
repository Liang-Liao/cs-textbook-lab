# MSYS2 UCRT64 / mingw32-make (cmd.exe recipes)
CC       ?= gcc
CFLAGS   ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS  = -I../../common
LDLIBS    = -L../../common -lcommon -lbcrypt

TARGET = test_ch04
OBJS   = secret_share.o ticket.o test_ch04.o

.PHONY: all test clean

all: $(TARGET)
	-del /F /Q $(OBJS) 2>NUL

test: all
	.\$(TARGET).exe

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

secret_share.o : secret_share.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

ticket.o : ticket.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

test_ch04.o : test_ch04.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

clean:
	-del /F /Q $(TARGET).exe $(OBJS) 2>NUL
