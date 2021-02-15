#ifndef _UUID_H
#define _UUID_H

extern const int uuid_zero[4];
extern int same_uuid(int a[4], int b[4], int swapuuid);
extern void copy_uuid(void *a, int b[4], int swapuuid);
extern char *__fname_from_uuid(int id[4], int swap, char *buf, char sep);

#endif /* _UUID_H */
