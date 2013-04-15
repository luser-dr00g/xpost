CFLAGS= -g -Wall -Wextra
OB=ob.o v.o
LDLIBS=
SRC= README makefile *.h *.c 

v:v.c ob.h
	cc $(CFLAGS) -DTESTMODULE -o $@ $<
ob:ob.c ob.h
	cc $(CFLAGS) -DTESTMODULE -o $@ $<

test: v ob
	./ob && ./v
