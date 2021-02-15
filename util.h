#ifndef _UTIL_H
#define _UTIL_H

/*
 * min()/max()/clamp() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#define min(x, y) ({                            \
	typeof(x) _min1 = (x);                  \
	typeof(y) _min2 = (y);                  \
	(void) (&_min1 == &_min2);              \
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({                            \
	typeof(x) _max1 = (x);                  \
	typeof(y) _max2 = (y);                  \
	(void) (&_max1 == &_max2);              \
	_max1 > _max2 ? _max1 : _max2; })

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

#ifdef NO_COROSYNC
#define CS_OK 1
typedef uint64_t cmap_handle_t;
#else
#include	<corosync/cmap.h>
#endif

#ifndef NO_DLM
#include	<libdlm.h>
#include	<errno.h>
#else
#define LKF_NOQUEUE	0x00000001
#define LKM_PWMODE	4
#define EUNLOCK		0x10002

typedef void *dlm_lshandle_t;

struct dlm_lksb {
	int sb_status;
	uint32_t sb_lkid;
	char sb_flags;
	char *sb_lvbptr;
};
#endif

struct cmap_hooks {
	void *cmap_handle;      /* corosync lib related */

	int (*initialize)(cmap_handle_t *handle);
	int (*get_string)(cmap_handle_t handle,
			  const char *string,
			  char **name);
	int (*finalize)(cmap_handle_t handle);
};

void set_cmap_hooks(void);

struct dlm_hooks {
	void *dlm_handle;	/* dlm lib related */

	dlm_lshandle_t (*create_lockspace)(const char *name,
					   unsigned int mode);
	dlm_lshandle_t (*open_lockspace)(const char *name);
	int (*release_lockspace)(const char *name, dlm_lshandle_t ls,
				 int force);
	int (*ls_lock)(dlm_lshandle_t lockspace, uint32_t mode,
		       struct dlm_lksb *lksb, uint32_t flags,
		       const void *name, unsigned int namelen,
		       uint32_t parent, void (*astaddr) (void *astarg),
		       void *astarg, void (*bastaddr) (void *astarg),
		       void *range);
	int (*ls_unlock_wait)(dlm_lshandle_t lockspace, uint32_t lkid,
			      uint32_t flags, struct dlm_lksb *lksb);
	int (*ls_get_fd)(dlm_lshandle_t ls);
	int (*dispatch)(int fd);
};

void set_dlm_hooks(void);
int dlm_funs_ready(void);

#endif /* _UTIL_H */
