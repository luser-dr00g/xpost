#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "osunix.h"

void echoon (FILE *f)
{
    struct termios ts;
    tcgetattr(fileno(f), &ts);
    ts.c_lflag |= ECHO;
    tcsetattr(fileno(f), TCSANOW, &ts);
}

void echooff (FILE *f)
{
    struct termios ts;
    tcgetattr(fileno(f), &ts);
    ts.c_lflag &= ~ECHO;
    tcsetattr(fileno(f), TCSANOW, &ts);
}

