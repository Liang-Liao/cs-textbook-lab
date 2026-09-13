# MSYS2 UCRT64 / mingw32-make (cmd.exe recipes)
CC       ?= gcc
CFLAGS   ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS  = -I../../common
LDLIBS    = -L../../common -lcommon -lbcrypt

TARGET = test_ch11
OBJS   = gf2n.o numthy.o test_ch11.o

.PHONY: all test clean

all: $(TARGET)
	-del /F /Q $(OBJS) 2>NUL

test: all
	.\$(TARGET).exe

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

gf2n.o : gf2n.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

numthy.o : numthy.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

test_ch11.o : test_ch11.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

clean:
	-del /F /Q $(TARGET).exe $(OBJS) 2>NUL
