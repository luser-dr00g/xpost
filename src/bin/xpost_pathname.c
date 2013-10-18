#include "xpost_main.h"

int is_installed = 0;
char *exedir;

static
int checkexepath (char *exepath)
{
	char *slash;

    /* replace windows's \\  so we can compare paths */
    slash = exepath;
    while (*slash++) {
        if (*slash == '\\') *slash = '/';
    }

    /* global exedir is set in ps systemdict as /EXE_DIR */
	exedir = strdup(exepath);
	dirname(exedir);

#ifdef DEBUG_PATHNAME
	printf("exepath: %s\n", exepath);
	printf("dirname: %s\n", exedir);
	printf("PACKAGE_INSTALL_DIR: %s\n", PACKAGE_INSTALL_DIR);
#endif

    /* if path-to-executable is where it should be installed */
	is_installed = strstr(exepath, PACKAGE_INSTALL_DIR) != NULL;

#ifdef DEBUG_PATHNAME
	printf("is_installed: %d\n", is_installed);
#endif
	return 0;
}

static
char *appendtocwd (char *relpath)
{
	char buf[1024];
	if (getcwd(buf, sizeof buf) == NULL) {
		perror("getcwd() error");
		return NULL;
	}
	strcat(buf, "/");
	strcat(buf, relpath);
	return strdup(buf);
}

static
int searchpathforargv0(char *argv0)
{
    /*
       not implemented.
       This function may be necessary on some obscure unices.
       It is called if there is no other path information,
       ie. argv[0] is a bare name,
       and no /proc/???/exe links are present
    */
	return checkexepath(".");
}

static
int checkargv0 (char *argv0)
{
#ifdef HAVE_WIN32
	if (argv0[1] == ':'
			&& (argv0[2] == '/' || argv0[2] == '\\'))
#else
	if (argv0[0] == '/') /* absolute path */
#endif
		return checkexepath(argv0);
	else if (strchr(argv0, '/')) { /* relative path */
		char *tmp;
		int ret;
		tmp = appendtocwd(argv0);
		ret = checkexepath(tmp);
		free(tmp);
		return ret;
	} else /* no path info: search $PATH */
		return searchpathforargv0(argv0);
}

int xpost_is_installed (char *argv0)
{
	char buf[1024];
	ssize_t len;
    char *libsptr;

	printf("argv0: %s\n", argv0);

    /* hack for cygwin and mingw.
       there's this unfortunate ".libs" in there.
    */
    if ((libsptr = strstr(argv0, ".libs/"))) {
        printf("removing '.libs' from pathname\n");
        memmove(libsptr, libsptr+6, strlen(libsptr+6)+1);
        printf("argv0: %s\n", argv0);
        return checkargv0(argv0);
    }

#ifdef HAVE_WIN32
	return checkargv0(argv0);

	/*
	len = GetModuleFileName(NULL, buf, 1024);
	buf[len] = '\0';
	if (len == 0)
		return -1;
	else
		return checkexepath(buf);
	*/
#else

	if ((len = readlink("/proc/self/exe", buf, sizeof buf)) != -1) {
		buf[len] = '\0';
		//strcat(buf, basename(argv0)); //already there
	}

	if (len == -1)
		if ((len = readlink("/proc/curproc/exe", buf, sizeof buf)) != -1)
			buf[len] = '\0';

	if (len == -1)
		if ((len = readlink("/proc/self/path/a.out", buf, sizeof buf)) != -1)
			buf[len] = '\0';

	if (len == -1)
		return checkargv0(argv0);
	else
		return checkexepath(buf);
#endif
}

