
CFLAGS= -g -Wall -Wextra 
OB=ob.o m.o
LDLIBS=
SRC= README makefile *.h *.c 
OP=op.o ops.o opst.o opar.o opdi.o optok.o opb.o opc.o opt.o \
    opm.o opf.o opx.o

test: m ob s st nm v gc ar di itp
	./ob && ./m && ./s && ./st && ./nm && \
	./v && ./gc && ./ar && ./di && ./itp

.o.c:

count:
	wc -l *.[ch]

clean:
	rm *.o *.exe g.mem l.mem x.mem

splint:
	splint +posixlib -boolops -predboolint +ignoresigns -type -nestcomment \
-noeffect \
-shiftimplementation -predboolothers -exportlocal -mustfreefresh ./*.c

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

nm:nm.c m.o ob.o s.o st.o gc.o itp.o $(OP)
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o v.o di.o gc.o      itp.o $(OP) f.o

v:v.c s.o m.o ob.o gc.o ar.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o     di.o gc.o nm.o itp.o $(OP) f.o

gc:gc.c s.o m.o ob.o gc.o ar.o st.o v.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o v.o di.o      nm.o itp.o $(OP) f.o

st:st.c m.o gc.o itp.o $(OP)
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o      v.o di.o gc.o nm.o itp.o $(OP) f.o

ar:ar.c s.o m.o ob.o gc.o st.o v.o gc.o itp.o nm.o $(OP)
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o st.o v.o di.o gc.o nm.o      itp.o $(OP) f.o

di:di.c s.o m.o ob.o gc.o ar.o st.o v.o gc.o itp.o nm.o $(OP)
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o v.o      gc.o nm.o itp.o $(OP) f.o

itp:itp.c s.o m.o ob.o gc.o ar.o st.o v.o gc.o nm.o di.o $(OP) f.o
	cc $(CFLAGS) -DTESTMODULE -o $@ $< m.o ob.o s.o ar.o st.o v.o di.o gc.o nm.o       $(OP) f.o
	

