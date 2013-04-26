//#define TESTMODULE

#include <stdbool.h>
#include <stdio.h>
#include "ob.h"

char *types[] = { TYPES(AS_STR) "invalid"};

integer isx(object o) {
	return !(o.tag &FLIT);
}

integer islist(object o) {
	return o.tag & FLIT;
}

integer type(object o) {
	return o.tag & TYPEMASK;
}

object cvx(object o){
	o.tag &= ~FLIT;
	return o;
}

object cvlit(object o) {
	o.tag |= FLIT;
	return o;
}


SINGLETONS(DEFINE_SINGLETON)

object consbool(bool b) {
	object o;
	o.int_.tag = booleantype;
	o.int_.pad = 0;
	o.int_.val = b;
	return o;
}

object consint(integer i){
	object o;
	o.int_.tag = integertype;
	o.int_.pad = 0;
	o.int_.val = i;
	return o;
}

object consreal(real r){
	object o;
	o.real_.tag = realtype;
	o.real_.pad = 0;
	o.real_.val = r;
	return o;
}

void dumpcompobject(object o){
	printf(" %u %u %u %u>",
			(unsigned int)o.comp_.tag,
			(unsigned int)o.comp_.sz,
			(unsigned int)o.comp_.ent,
			(unsigned int)o.comp_.off);
}

void dumpobject(object o){
	switch(type(o)) {
		default:
		case invalidtype: printf("<invalid object>"); break;

		case nulltype: printf("<null>"); break;
		case marktype: printf("<mark>"); break;
		case booleantype: printf("<boolean %s>", o.int_.val?"true":"false"); break;
		case integertype: printf("<integer %d>", (int)o.int_.val); break;
		case realtype: printf("<real %f>", (float)o.real_.val); break;

		case stringtype: printf("<string"); dumpcompobject(o); break;
		case arraytype: printf("<array"); dumpcompobject(o); break;
		case dicttype: printf("<dict"); dumpcompobject(o); break;

		case nametype: printf("<name %u>", (int)o.mark_.padw); break;
		case operatortype: printf("<operator %u>", (int)o.mark_.padw); break;
	}
}

#ifdef TESTMODULE
int main() {
	printf("\n^test ob module\n");
	object i = consint(5);
	object r = consreal(12.0);
	dumpobject(null);
	dumpobject(mark);
	dumpobject(i);
	dumpobject(r);
	puts("");
	return 0;
} 
#endif
