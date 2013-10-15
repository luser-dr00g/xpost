#include "xpost_main.h"

int is_installed = 0;
char *exedir;

static
int checkexepath (char *exepath)
{
	exedir = strdup(exepath);
	dirname(exedir);
	printf("exepath: %s\n", exepath);
	printf("dirname: %s\n", exedir);
	printf("PACKAGE_INSTALL_DIR: %s\n", PACKAGE_INSTALL_DIR);
	is_installed = strstr(exepath, PACKAGE_INSTALL_DIR) != NULL;
	printf("is_installed: %d\n", is_installed);
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
	return checkexepath(".");
}

static
int checkargv0 (char *argv0)
{
	if (argv0[0] == '/') /* absolute path */
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
	printf("argv0: %s\n", argv0);
	//return checkargv0(argv0);

#ifdef HAVE_WIN32
	len = GetModuleFileName(NULL, buf, 1024);
	buf[len] = '\0';
	if (len == 0)
		return -1;
	else
		return checkexepath(buf);
#else

	if ((len = readlink("/proc/self/exe", buf, sizeof buf)) != -1) {
		buf[len] = '\0';
		strcat(buf, basename(argv0));
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

