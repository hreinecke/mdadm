#ifndef _LIB_H
#define _LIB_H

#define HAVE_NFTW  we assume
#define HAVE_FTW

#ifdef __UCLIBC__
# include <features.h>
# ifndef __UCLIBC_HAS_LFS__
#  define lseek64 lseek
# endif
# ifndef  __UCLIBC_HAS_FTW__
#  undef HAVE_FTW
#  undef HAVE_NFTW
# endif
#endif

#ifdef __dietlibc__
# undef HAVE_NFTW
#endif

#if defined(__KLIBC__)
# undef HAVE_NFTW
# undef HAVE_FTW
#endif

#ifndef HAVE_NFTW
# define FTW_PHYS 1
# ifndef HAVE_FTW
  struct FTW {};
# endif
#endif

#ifdef HAVE_FTW
# include <ftw.h>
#endif

#define HAVE_NFTW  we assume
#define HAVE_FTW

#ifdef __UCLIBC__
# include <features.h>
# ifndef __UCLIBC_HAS_LFS__
#  define lseek64 lseek
# endif
# ifndef  __UCLIBC_HAS_FTW__
#  undef HAVE_FTW
#  undef HAVE_NFTW
# endif
#endif

#ifdef __dietlibc__
# undef HAVE_NFTW
#endif

#if defined(__KLIBC__)
# undef HAVE_NFTW
# undef HAVE_FTW
#endif

#ifndef HAVE_NFTW
# define FTW_PHYS 1
# ifndef HAVE_FTW
  struct FTW {};
# endif
#endif

#ifdef HAVE_FTW
# include <ftw.h>
#endif

int add_dev(const char *name, const struct stat *stb, int flag, struct FTW *s);
char *map_dev_preferred(int major, int minor, int create,
			       char *prefer);
static inline char *map_dev(int major, int minor, int create)
{
	return map_dev_preferred(major, minor, create, NULL);
}

/* Define PATH_MAX in case we don't use glibc or standard library does
 * not have PATH_MAX defined. Assume max path length is 4K characters.
 */
#ifndef PATH_MAX
#define PATH_MAX	4096
#endif

#define _ROUND_UP(val, base)	(((val) + (base) - 1) & ~(base - 1))
#define ROUND_UP(val, base)	_ROUND_UP(val, (typeof(val))(base))
#define ROUND_UP_PTR(ptr, base)	((typeof(ptr)) \
				 (ROUND_UP((unsigned long)(ptr), base)))

bool is_dev_alive(char *path);
char *devid2kname(dev_t devid);
char *devid2devnm(dev_t devid);
char *stat2devnm(struct stat *st);
char *stat2kname(struct stat *st);
char *fd2kname(int fd);
char *fd2devnm(int fd);
void print_quoted(char *str);
void print_escape(char *str);
int use_udev(void);
unsigned long GCD(unsigned long a, unsigned long b);
void udev_block(char *devnm);
void udev_unblock(void);

#endif /* _LIB_H */