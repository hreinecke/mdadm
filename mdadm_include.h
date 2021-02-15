#define	_GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include	<unistd.h>
#ifdef __GLIBC__
extern __off64_t lseek64 __P ((int __fd, __off64_t __offset, int __whence));
#elif !defined(lseek64)
# if defined(__NO_STAT64) || __WORDSIZE != 32
# define lseek64 lseek
# endif
#endif

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<stdint.h>
#include	<stdlib.h>
#include	<time.h>
#include	<sys/time.h>
#include	<getopt.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<errno.h>
#include	<string.h>
#include	<syslog.h>
/* Newer glibc requires sys/sysmacros.h directly for makedev() */
#include	<sys/sysmacros.h>
#ifdef __dietlibc__
#include	<strings.h>
/* dietlibc has deprecated random and srandom!! */
#define random rand
#define srandom srand
#endif

#include	<linux/kdev_t.h>
/*#include	<linux/fs.h> */
#include	<sys/mount.h>
#include	<asm/types.h>
#include	<sys/ioctl.h>
