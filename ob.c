//#define TESTMODULE

#include <stdbool.h>
#include <stdio.h>
#include "ob.h"

char *types[] = { TYPES(AS_TYPE_STR) "invalid"};

int isx(object o) {
    return !(o.tag &FLIT);
}

int islit(object o) {
    return o.tag & FLIT;
}

int faccess(object o) {
    return (o.tag & FACCESS) >> FACCESSO;
}

object setfaccess(object o, int access) {
    o.tag &= ~FACCESS;
    o.tag |= (access << FACCESSO);
    return o;
}

int isreadable(object o) {
    if (type(o) == filetype) {
        return faccess(o) == readonly;
    } else {
        return faccess(o) >= readonly;
    }
}

int iswriteable(object o) {
    return faccess(o) == unlimited;
}

int type(object o) {
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

// null, mark
SINGLETONS(DEFINE_SINGLETON)

object consbool(bool b) {
    object o;
    o.int_.tag = booleantype | (unlimited << FACCESSO);
    o.int_.pad = 0;
    o.int_.val = b;
    return cvlit(o);
}

object consint(integer i){
    object o;
    o.int_.tag = integertype | (unlimited << FACCESSO);
    o.int_.pad = 0;
    o.int_.val = i;
    return cvlit(o);
}

object consreal(real r){
    object o;
    o.real_.tag = realtype | (unlimited << FACCESSO);
    o.real_.pad = 0;
    o.real_.val = r;
    return cvlit(o);
}

void dumpcompobject(object o){
    printf(" %c %u %u %u %u>",
            o.comp_.tag&FBANK? 'G': 'L',
            (unsigned int)o.comp_.tag,
            (unsigned int)o.comp_.sz,
            (unsigned int)o.comp_.ent,
            (unsigned int)o.comp_.off);
}

void dumpobject(object o){
    switch(type(o)) {
        default:
        case invalidtype: printf("<invalid object %04x %04x %04x %04x >",
                                  o.comp_.tag, o.comp_.sz, o.comp_.ent, o.comp_.off); break;

        case nulltype: printf("<null>"); break;
        case marktype: printf("<mark>"); break;
        case booleantype: printf("<boolean %s>", o.int_.val?"true":"false"); break;
        case integertype: printf("<integer %d>", (int)o.int_.val); break;
        case realtype: printf("<real %f>", (float)o.real_.val); break;

        case stringtype: printf("<string"); dumpcompobject(o); break;
        case arraytype: printf("<array"); dumpcompobject(o); break;
        case dicttype: printf("<dict"); dumpcompobject(o); break;

        case nametype: printf("<name %c %u %u %u>",
                               o.mark_.tag&FBANK? 'G':'L',
                               (int)o.mark_.tag,
                               (int)o.mark_.pad0,
                               (int)o.mark_.padw); break;
        case operatortype: printf("<operator %u>", (int)o.mark_.padw); break;
        case filetype: printf("<file>"); break;
        case savetype: printf("<save>"); break;
        case contexttype: printf("<context>"); break;
        case extendedtype: printf("<extended-real>"); break;
        case globtype: printf("<glob>"); break;
    }
}

#ifdef TESTMODULE
int main() {
    printf("\n^test ob module\n");
    object i = consint(5);
    object j = consint(3579);
    object q = consreal(12.0);
    object r = consreal(.0370);
    object b = consbool(true);
    object c = consbool(false);
    dumpobject(null);
    dumpobject(mark);
    dumpobject(i);
    dumpobject(j);
    dumpobject(q);
    dumpobject(r);
    dumpobject(b);
    dumpobject(c);
    puts("");
    return 0;
} 
#endif
