# MSYS2 UCRT64 / mingw32-make (cmd.exe recipes)
CC       ?= gcc
CFLAGS   ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
CPPFLAGS  = -I../../common
LDLIBS    = -L../../common -lcommon -lbcrypt

TARGET = test_ch18
OBJS   = md5.o sha1.o sha256.o test_ch18.o

.PHONY: all test clean

all: $(TARGET)
	-del /F /Q $(OBJS) 2>NUL

test: all
	.\$(TARGET).exe

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

md5.o : md5.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

sha1.o : sha1.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

sha256.o : sha256.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

test_ch18.o : test_ch18.c
	${CC} ${CFLAGS} ${CPPFLAGS} -c $< -o $@

clean:
	-del /F /Q $(TARGET).exe $(OBJS) 2>NUL