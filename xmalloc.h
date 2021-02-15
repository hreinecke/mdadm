#ifndef _XMALLOC_H
#define _XMALLOC_H

void *xmalloc(size_t len);
void *xrealloc(void *ptr, size_t len);
void *xcalloc(size_t num, size_t size);
char *xstrdup(const char *str);

#endif /* _XMALLOC_H */
