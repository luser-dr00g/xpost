#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "xpost_log.h"
#include "xpost_memory.h"
#include "xpost_main.h"

static int _xpost_init_count = 0;

int
xpost_init(void)
{
    if (++_xpost_init_count != 1)
        return _xpost_init_count;

    xpost_log_init();

    if (!xpost_memory_init())
        return --_xpost_init_count;

    return _xpost_init_count;
}

int
xpost_quit(void)
{
    if (_xpost_init_count <= 0)
    {
        XPOST_LOG_ERR("Init count not greater than 0 in shutdown.");
        return 0;
    }

    if (--_xpost_init_count != 0)
        return _xpost_init_count;

    return _xpost_init_count;
}
