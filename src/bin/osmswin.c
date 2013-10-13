#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "osmswin.h"

void echoon(FILE *f)
{
}

void echooff(FILE *f)
{
}

#ifdef __MINGW32__
int mkstemp(char *template)
{
    char *temp;

    temp = _mktemp(template);
    if (!temp)
        return -1;

    return _open(temp, _O_CREAT |  _O_TEMPORARY | _O_EXCL | _O_RDWR, _S_IREAD | _S_IWRITE);
}
#endif
