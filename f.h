
object consfile(mfile *mem, FILE *fp);
object fileopen(mfile *mem, char *fn, char *mode); 
FILE *filefile(mfile *mem, object f);
bool filestatus(mfile *mem, object f);
long filebytesavailable(mfile *mem, object f);
void fileclose(mfile *mem, object f);
object fileread(mfile *mem, object f);
void filewrite(mfile *mem, object f, object b);


