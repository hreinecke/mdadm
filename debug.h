#ifndef _DEBUG_H
#define _DEBUG_H

#include <assert.h>
#include <stdarg.h>

#ifdef DEBUG
#define dprintf(fmt, arg...) \
	fprintf(stderr, "%s: %s: "fmt, mdlib_get_name(), __func__, ##arg)
#define dprintf_cont(fmt, arg...) \
	fprintf(stderr, fmt, ##arg)
#else
#define dprintf(fmt, arg...) \
        ({ if (0) fprintf(stderr, "%s: %s: " fmt, mdlib_get_name(), __func__, ##arg); 0; })
#define dprintf_cont(fmt, arg...) \
        ({ if (0) fprintf(stderr, fmt, ##arg); 0; })
#endif

static inline int xasprintf(char **strp, const char *fmt, ...) {
	va_list ap;
	int ret;
	va_start(ap, fmt);
	ret = vasprintf(strp, fmt, ap);
	va_end(ap);
	assert(ret >= 0);
	return ret;
}

#ifdef DEBUG
#define pr_err(fmt, args...) fprintf(stderr, "%s: %s: "fmt, mdlib_get_name(), __func__, ##args)
#else
#define pr_err(fmt, args...) fprintf(stderr, "%s: "fmt, mdlib_get_name(), ##args)
#endif
#define cont_err(fmt ...) fprintf(stderr, "       " fmt)

#endif /* _DEBUG_H */
