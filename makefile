CFLAGS= -g -Wall -Wextra
OB=ob.o m.o
LDLIBS=
SRC= README makefile *.h *.c 

count:
	wc -l *.[ch]

m:m.c ob.h
	cc $(CFLAGS) -DTESTMODULE -o $@ $<
ob:ob.c ob.h
	cc $(CFLAGS) -DTESTMODULE -o $@ $<
s:s.c m.o ob.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o
nm:nm.c m.o ob.o s.o st.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o st.o
v:v.c s.o m.o ob.o gc.o ar.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o gc.o ar.o
gc:gc.c s.o m.o ob.o gc.o ar.o st.o v.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o v.o
di:di.c s.o m.o ob.o gc.o ar.o st.o v.o gc.o itp.o nm.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o v.o gc.o itp.o nm.o
	

test: m ob s nm v gc di
	./ob && ./m && ./s && ./nm && ./v && ./gc && ./di
