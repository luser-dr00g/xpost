CFLAGS= -g -Wall -Wextra
OB=ob.o m.o
LDLIBS=
SRC= README makefile *.h *.c 

count:
	wc -l *.[ch]

clean:
	rm *.o *.exe

splint:
	splint +posixlib -boolops -predboolint +ignoresigns \
	-type -shiftimplementation -predboolothers -exportlocal ./*.c

m.ps:m.pic
	pic m.pic|groff > m.ps
m.eps:m.ps
	ps2eps m.ps
m.png:m.eps
	convert m.eps m.png
s.ps:s.pic
	pic s.pic|groff > s.ps
s.eps:s.ps
	ps2eps s.ps
s.png:s.eps
	convert s.eps s.png

m:m.c ob.h
	cc $(CFLAGS) -DTESTMODULE -o $@ $<
ob:ob.c ob.h
	cc $(CFLAGS) -DTESTMODULE -o $@ $<
s:s.c m.o ob.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o
nm:nm.c m.o ob.o s.o st.o gc.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o st.o gc.o
v:v.c s.o m.o ob.o gc.o ar.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o gc.o ar.o
gc:gc.c s.o m.o ob.o gc.o ar.o st.o v.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o v.o
ar:ar.c s.o m.o ob.o gc.o st.o v.o gc.o itp.o nm.o op.o ops.o di.o opst.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o st.o v.o gc.o itp.o nm.o op.o ops.o di.o opst.o
di:di.c s.o m.o ob.o gc.o ar.o st.o v.o gc.o itp.o nm.o op.o ops.o opst.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o v.o gc.o itp.o nm.o op.o ops.o opst.o
itp:itp.c s.o m.o ob.o gc.o ar.o st.o v.o gc.o nm.o di.o op.o ops.o opst.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o v.o gc.o nm.o di.o op.o ops.o opst.o
	

test: m ob s nm v gc ar di itp
	./ob && ./m && ./s && ./nm && ./v && ./gc && ./ar && ./di && ./itp
