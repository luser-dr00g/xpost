
#include "xpost_main.h"


int main(int argc, char *argv[])
{
    printf("xpost_main\n");
	xpost_is_installed(argv[0]);

    createitp();

    runitp();

    destroyitp();
	free(exedir);

    return 0;
}

