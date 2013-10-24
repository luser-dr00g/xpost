#ifndef XPOST_F_H
#define XPOST_F_H

/*
   a filetype object uses .mark_.padw to store the ent
   for the FILE *
   */

Xpost_Object consfile(mfile *mem, /*@NULL@*/ FILE *fp);
Xpost_Object fileopen(mfile *mem, char *fn, char *mode);
FILE *filefile(mfile *mem, Xpost_Object f);
bool filestatus(mfile *mem, Xpost_Object f);
long filebytesavailable(mfile *mem, Xpost_Object f);
void fileclose(mfile *mem, Xpost_Object f);
Xpost_Object fileread(mfile *mem, Xpost_Object f);
void filewrite(mfile *mem, Xpost_Object f, Xpost_Object b);

#endif
