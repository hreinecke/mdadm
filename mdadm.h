/*
 * mdadm - manage Linux "md" devices aka RAID arrays.
 *
 * Copyright (C) 2001-2009 Neil Brown <neilb@suse.de>
 *
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    Author: Neil Brown
 *    Email: <neilb@suse.de>
 */

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

#define MdpMinorShift 6

#ifndef BLKGETSIZE64
#define BLKGETSIZE64 _IOR(0x12,114,size_t) /* return device size in bytes (u64 *arg) */
#endif

#ifdef __TINYC__
#undef minor
#undef major
#undef makedev
#define minor(x) ((x)&0xff)
#define major(x) (((x)>>8)&0xff)
#define makedev(M,m) (((M)<<8) | (m))
#endif

#if __GNUC__ < 3
struct stat64;
#endif

#ifdef __UCLIBC__
# include <features.h>
# ifndef __UCLIBC_HAS_LFS__
#  define lseek64 lseek
# endif
#endif

#define DEFAULT_CHUNK 512
#define DEFAULT_BITMAP_CHUNK 4096
#define DEFAULT_BITMAP_DELAY 5
#define DEFAULT_MAX_WRITE_BEHIND 256

/* When using "GET_DISK_INFO" it isn't certain how high
 * we need to check.  So we impose an absolute limit of
 * MAX_DISKS.  This needs to be much more than the largest
 * number of devices any metadata can support.  Currently
 * v1.x can support 1920
 */
#define MAX_DISKS	4096

/* MDMON_DIR is where pid and socket files used for communicating
 * with mdmon normally live.  Best is /var/run/mdadm as
 * mdmon is needed at early boot then it needs to write there prior
 * to /var/run being mounted read/write, and it also then needs to
 * persist beyond when /var/run is mounter read-only.  So, to be
 * safe, the default is somewhere that is read/write early in the
 * boot process and stays up as long as possible during shutdown.
 */
#ifndef MDMON_DIR
#define MDMON_DIR "/run/mdadm"
#endif /* MDMON_DIR */

#include	"md_u.h"
#include	"md_p.h"
#include	"msg.h"

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

#define	LEVEL_MULTIPATH		(-4)
#define	LEVEL_LINEAR		(-1)
#define	LEVEL_FAULTY		(-5)

/* kernel module doesn't know about these */
#define LEVEL_CONTAINER		(-100)
#define	LEVEL_UNSUPPORTED	(-200)

/* the kernel does know about this one ... */
#define	LEVEL_NONE		(-1000000)

/* for raid4/5/6 */
#define ALGORITHM_LEFT_ASYMMETRIC	0
#define ALGORITHM_RIGHT_ASYMMETRIC	1
#define ALGORITHM_LEFT_SYMMETRIC	2
#define ALGORITHM_RIGHT_SYMMETRIC	3

/* Define non-rotating (raid4) algorithms.  These allow
 * conversion of raid4 to raid5.
 */
#define ALGORITHM_PARITY_0		4 /* P or P,Q are initial devices */
#define ALGORITHM_PARITY_N		5 /* P or P,Q are final devices. */

/* DDF RAID6 layouts differ from md/raid6 layouts in two ways.
 * Firstly, the exact positioning of the parity block is slightly
 * different between the 'LEFT_*' modes of md and the "_N_*" modes
 * of DDF.
 * Secondly, or order of datablocks over which the Q syndrome is computed
 * is different.
 * Consequently we have different layouts for DDF/raid6 than md/raid6.
 * These layouts are from the DDFv1.2 spec.
 * Interestingly DDFv1.2-Errata-A does not specify N_CONTINUE but
 * leaves RLQ=3 as 'Vendor Specific'
 */

#define ALGORITHM_ROTATING_ZERO_RESTART	8 /* DDF PRL=6 RLQ=1 */
#define ALGORITHM_ROTATING_N_RESTART	9 /* DDF PRL=6 RLQ=2 */
#define ALGORITHM_ROTATING_N_CONTINUE	10 /*DDF PRL=6 RLQ=3 */

/* For every RAID5 algorithm we define a RAID6 algorithm
 * with exactly the same layout for data and parity, and
 * with the Q block always on the last device (N-1).
 * This allows trivial conversion from RAID5 to RAID6
 */
#define ALGORITHM_LEFT_ASYMMETRIC_6	16
#define ALGORITHM_RIGHT_ASYMMETRIC_6	17
#define ALGORITHM_LEFT_SYMMETRIC_6	18
#define ALGORITHM_RIGHT_SYMMETRIC_6	19
#define ALGORITHM_PARITY_0_6		20
#define ALGORITHM_PARITY_N_6		ALGORITHM_PARITY_N

#define RESYNC_NONE -1
#define RESYNC_DELAYED -2
#define RESYNC_PENDING -3
#define RESYNC_REMOTE  -4
#define RESYNC_UNKNOWN -5

/* Sometimes the 'size' value passed needs to mean "Maximum".
 * In those cases with use MAX_SIZE
 */
#define MAX_SIZE	1

/* We want to use unsigned numbers for sector counts, but need
 * a value for 'invalid'.  Use '1'.
 */
#define INVALID_SECTORS 1
/* And another special number needed for --data_offset=variable */
#define VARIABLE_OFFSET 3

struct md_bb_entry {
	unsigned long long sector;
	int length;
};

struct md_bb {
	int supported;
	int count;
	struct md_bb_entry *entries;
};

/* general information that might be extracted from a superblock */
struct mdinfo {
	mdu_array_info_t	array;
	mdu_disk_info_t		disk;
	__u64			events;
	int			uuid[4];
	char			name[33];
	unsigned long long	data_offset;
	unsigned long long	new_data_offset;
	unsigned long long	component_size; /* same as array.size, except in
						 * sectors and up to 64bits.
						 */
	unsigned long long	custom_array_size; /* size for non-default sized
						    * arrays (in sectors)
						    */
#define NO_RESHAPE		0
#define VOLUME_RESHAPE		1
#define CONTAINER_RESHAPE	2
#define RESHAPE_NO_BACKUP	16 /* Mask 'or'ed in */
	int			reshape_active;
	unsigned long long	reshape_progress;
	int			recovery_blocked; /* for external metadata it
						   * indicates that there is
						   * reshape in progress in
						   * container,
						   * for native metadata it is
						   * reshape_active field mirror
						   */
	int journal_device_required;
	int journal_clean;

	enum {
		CONSISTENCY_POLICY_UNKNOWN,
		CONSISTENCY_POLICY_NONE,
		CONSISTENCY_POLICY_RESYNC,
		CONSISTENCY_POLICY_BITMAP,
		CONSISTENCY_POLICY_JOURNAL,
		CONSISTENCY_POLICY_PPL,
	} consistency_policy;

	/* During reshape we can sometimes change the data_offset to avoid
	 * over-writing still-valid data.  We need to know if there is space.
	 * So getinfo_super will fill in space_before and space_after in sectors.
	 * data_offset can be increased or decreased by this amount.
	 */
	unsigned long long	space_before, space_after;
	union {
		unsigned long long resync_start; /* per-array resync position */
		unsigned long long recovery_start; /* per-device rebuild position */
		#define MaxSector  (~0ULL) /* resync/recovery complete position */
	};
	long			bitmap_offset;	/* 0 == none, 1 == a file */
	unsigned int		ppl_size;
	int			ppl_offset;
	unsigned long long	ppl_sector;
	unsigned long		safe_mode_delay; /* ms delay to mark clean */
	int			new_level, delta_disks, new_layout, new_chunk;
	int			errors;
	unsigned long		cache_size; /* size of raid456 stripe cache*/
	int			mismatch_cnt;
	char			text_version[50];

	int container_member; /* for assembling external-metatdata arrays
			       * This is to be used internally by metadata
			       * handler only */
	int container_enough; /* flag external handlers can set to
			       * indicate that subarrays have not enough (-1),
			       * enough to start (0), or all expected disks (1) */
	char		sys_name[32];
	struct mdinfo *devs;
	struct mdinfo *next;

	/* Device info for mdmon: */
	int recovery_fd;
	int state_fd;
	int bb_fd;
	int ubb_fd;
	#define DS_FAULTY	1
	#define	DS_INSYNC	2
	#define	DS_WRITE_MOSTLY	4
	#define	DS_SPARE	8
	#define DS_BLOCKED	16
	#define	DS_REMOVE	1024
	#define	DS_UNBLOCK	2048
	int prev_state, curr_state, next_state;

	/* info read from sysfs */
	enum {
		ARRAY_CLEAR,
		ARRAY_INACTIVE,
		ARRAY_SUSPENDED,
		ARRAY_READONLY,
		ARRAY_READ_AUTO,
		ARRAY_CLEAN,
		ARRAY_ACTIVE,
		ARRAY_WRITE_PENDING,
		ARRAY_ACTIVE_IDLE,
		ARRAY_BROKEN,
		ARRAY_UNKNOWN_STATE,
	} array_state;
	struct md_bb bb;
};

struct spare_criteria {
	unsigned long long min_size;
	unsigned int sector_size;
};

enum prefix_standard {
	JEDEC,
	IEC
};

enum bitmap_update {
    NoUpdate,
    NameUpdate,
    NodeNumUpdate,
};

enum flag_mode {
	FlagDefault, FlagSet, FlagClear,
};

/* structures read from config file */
/* List of mddevice names and identifiers
 * Identifiers can be:
 *    uuid=128-hex-uuid
 *    super-minor=decimal-minor-number-from-superblock
 *    devices=comma,separated,list,of,device,names,with,wildcards
 *
 * If multiple fields are present, the intersection of all matching
 * devices is considered
 */
#define UnSet (0xfffe)
struct mddev_ident {
	char	*devname;

	int	uuid_set;
	int	uuid[4];
	char	name[33];

	int super_minor;

	char	*devices;	/* comma separated list of device
				 * names with wild cards
				 */
	int	level;
	int raid_disks;
	int spare_disks;
	struct supertype *st;
	int	autof;		/* 1 for normal, 2 for partitioned */
	char	*spare_group;
	char	*bitmap_file;
	int	bitmap_fd;

	char	*container;	/* /dev/whatever name of container, or
				 * uuid of container.  You would expect
				 * this to be the 'devname' or UUID
				 * of some other entry.
				 */
	char	*member;	/* subarray within a container */

	struct mddev_ident *next;
	union {
		/* fields needed by different users of this structure */
		int assembled;	/* set when assembly succeeds */
	};
};

struct context {
	int	readonly;
	int	runstop;
	int	verbose;
	int	brief;
	int	no_devices;
	int	force;
	char	*homehost;
	int	require_homehost;
	char	*prefer;
	int	export;
	int	test;
	char	*subarray;
	char	*update;
	int	scan;
	int	SparcAdjust;
	int	autof;
	int	delay;
	int	freeze_reshape;
	char	*backup_file;
	int	invalid_backup;
	char	*action;
	int	nodes;
	char	*homecluster;
};

struct shape {
	int	raiddisks;
	int	sparedisks;
	int	journaldisks;
	int	level;
	int	layout;
	char	*layout_str;
	int	chunk;
	int	bitmap_chunk;
	char	*bitmap_file;
	int	assume_clean;
	int	write_behind;
	unsigned long long size;
	int	consistency_policy;
};

/* List of device names - wildcards expanded */
struct mddev_dev {
	char *devname;
	int disposition;	/* 'a' for add, 'r' for remove, 'f' for fail,
				 * 'A' for re_add.
				 * Not set for names read from .config
				 */
	enum flag_mode writemostly;
	enum flag_mode failfast;
	int used;		/* set when used */
	long long data_offset;
	struct mddev_dev *next;
};

typedef struct mapping {
	char *name;
	int num;
} mapping_t;

struct mdstat_ent {
	char		devnm[32];
	int		active;
	char		*level;
	char		*pattern; /* U for up, _ for down */
	int		percent; /* -1 if no resync */
	int		resync; /* 3 if check, 2 if reshape, 1 if resync, 0 if recovery */
	int		devcnt;
	int		raid_disks;
	char *		metadata_version;
	struct dev_member {
		char			*name;
		struct dev_member	*next;
	}		*members;
	struct mdstat_ent *next;
};

extern char *map_num(mapping_t *map, int num);

struct active_array;
struct metadata_update;

/* A superswitch provides entry point to a metadata handler.
 *
 * The superswitch primarily operates on some "metadata" that
 * is accessed via the 'supertype'.
 * This metadata has one of three possible sources.
 * 1/ It is read from a single device.  In this case it may not completely
 *    describe the array or arrays as some information might be on other
 *    devices.
 * 2/ It is read from all devices in a container.  In this case all
 *    information is present.
 * 3/ It is created by ->init_super / ->add_to_super.  In this case it will
 *    be complete once enough ->add_to_super calls have completed.
 *
 * When creating an array inside a container, the metadata will be
 * formed by a combination of 2 and 3.  The metadata or the array is read,
 * then new information is added.
 *
 * The metadata must sometimes have a concept of a 'current' array
 * and a 'current' device.
 * The 'current' array is set by init_super to be the newly created array,
 * or is set by super_by_fd when it finds it is looking at an array inside
 * a container.
 *
 * The 'current' device is either the device that the metadata was read from
 * in case 1, or the last device added by add_to_super in case 3.
 * Case 2 does not identify a 'current' device.
 */
extern struct superswitch {

	/* Used to report details of metadata read from a component
	 * device. ->load_super has been called.
	 */
	void (*examine_super)(struct supertype *st, char *homehost);
	void (*brief_examine_super)(struct supertype *st, int verbose);
	void (*brief_examine_subarrays)(struct supertype *st, int verbose);
	void (*export_examine_super)(struct supertype *st);
	int (*examine_badblocks)(struct supertype *st, int fd, char *devname);
	int (*copy_metadata)(struct supertype *st, int from, int to);

	/* Used to report details of an active array.
	 * ->load_super was possibly given a 'component' string.
	 */
	void (*detail_super)(struct supertype *st, char *homehost,
			     char *subarray);
	void (*brief_detail_super)(struct supertype *st, char *subarray);
	void (*export_detail_super)(struct supertype *st);

	/* Optional: platform hardware / firmware details */
	int (*detail_platform)(int verbose, int enumerate_only, char *controller_path);
	int (*export_detail_platform)(int verbose, char *controller_path);

	/* Used:
	 *   to get uuid to storing in bitmap metadata
	 *   and 'reshape' backup-data metadata
	 *   To see if a device is being re-added to an array it was part of.
	 */
	void (*uuid_from_super)(struct supertype *st, int uuid[4]);

	/* Extract generic details from metadata.  This could be details about
	 * the container, or about an individual array within the container.
	 * The determination is made either by:
	 *   load_super being given a 'component' string.
	 *   validate_geometry determining what to create.
	 * The info includes both array information and device information.
	 * The particular device should be:
	 *   The last device added by add_to_super
	 *   The device the metadata was loaded from by load_super
	 * If 'map' is present, then it is an array raid_disks long
	 * (raid_disk must already be set and correct) and it is filled
	 * with 1 for slots that are thought to be active and 0 for slots which
	 * appear to be failed/missing.
	 * *info is zeroed out before data is added.
	 */
	void (*getinfo_super)(struct supertype *st, struct mdinfo *info, char *map);
	struct mdinfo *(*getinfo_super_disks)(struct supertype *st);
	/* Check if the given metadata is flagged as belonging to "this"
	 * host.  0 for 'no', 1 for 'yes', -1 for "Don't record homehost"
	 */
	int (*match_home)(struct supertype *st, char *homehost);

	/* Make one of several generic modifications to metadata
	 * prior to assembly (or other times).
	 *   sparc2.2  - first bug in early 0.90 metadata
	 *   super-minor - change name of 0.90 metadata
	 *   summaries - 'correct' any redundant data
	 *   resync - mark array as dirty to trigger a resync.
	 *   uuid - set new uuid - only 0.90 or 1.x
	 *   name - change the name of the array (where supported)
	 *   homehost - change which host this array is tied to.
	 *   devicesize - If metadata is at start of device, change recorded
	 *               device size to match actual device size
	 *   byteorder - swap bytes for 0.90 metadata
	 *
	 *   force-one  - mark that device as uptodate, not old or failed.
	 *   force-array - mark array as clean if it would not otherwise
	 *               assemble
	 *   assemble   - not sure how this is different from force-one...
	 *   linear-grow-new - add a new device to a linear array, but don't
	 *                   change the size: so superblock still matches
	 *   linear-grow-update - now change the size of the array.
	 *   writemostly - set the WriteMostly1 bit in the superblock devflags
	 *   readwrite - clear the WriteMostly1 bit in the superblock devflags
	 *   failfast - set the FailFast1 bit in the superblock
	 *   nofailfast - clear the FailFast1 bit
	 *   no-bitmap - clear any record that a bitmap is present.
	 *   bbl       - add a bad-block-log if possible
	 *   no-bbl    - remove any bad-block-log is it is empty.
	 *   force-no-bbl - remove any bad-block-log even if empty.
	 *   revert-reshape - If a reshape is in progress, modify metadata so
	 *                    it will resume going in the opposite direction.
	 */
	int (*update_super)(struct supertype *st, struct mdinfo *info,
			    char *update,
			    char *devname, int verbose,
			    int uuid_set, char *homehost);

	/* Create new metadata for new array as described.  This could
	 * be a new container, or an array in a pre-existing container.
	 * Also used to zero metadata prior to writing it to invalidate old
	 * metadata.
	 */
	int (*init_super)(struct supertype *st, mdu_array_info_t *info,
			  struct shape *s, char *name,
			  char *homehost, int *uuid,
			  unsigned long long data_offset);

	/* update the metadata to include new device, either at create or
	 * when hot-adding a spare.
	 */
	int (*add_to_super)(struct supertype *st, mdu_disk_info_t *dinfo,
			    int fd, char *devname,
			    unsigned long long data_offset);
	/* update the metadata to delete a device,
	 * when hot-removing.
	 */
	int (*remove_from_super)(struct supertype *st, mdu_disk_info_t *dinfo);

	/* Write metadata to one device when fixing problems or adding
	 * a new device.
	 */
	int (*store_super)(struct supertype *st, int fd);

	/*  Write all metadata for this array.
	 */
	int (*write_init_super)(struct supertype *st);
	/* Check if metadata read from one device is compatible with an array,
	 * used when assembling an array, or pseudo-assembling was with
	 * "--examine --brief"
	 * If "st" has not yet been loaded the superblock from, "tst" is
	 * moved in, otherwise the superblock in 'st' is compared with
	 * 'tst'.
	 */
	int (*compare_super)(struct supertype *st, struct supertype *tst,
			     int verbose);
	/* Load metadata from a single device.  If 'devname' is not NULL
	 * print error messages as appropriate */
	int (*load_super)(struct supertype *st, int fd, char *devname);
	/* 'fd' is a 'container' md array - load array metadata from the
	 * whole container.
	 */
	int (*load_container)(struct supertype *st, int fd, char *devname);
	/* If 'arg' is a valid name of this metadata type, allocate and
	 * return a 'supertype' for the particular minor version */
	struct supertype * (*match_metadata_desc)(char *arg);
	/* If a device has the given size, and the data_offset has been
	 * requested - work out how much space is available for data.
	 * This involves adjusting for reserved space (e.g. bitmaps)
	 * and for any rounding.
	 * 'mdadm' only calls this for existing arrays where a possible
	 * spare is being added.  However some super-handlers call it
	 * internally from validate_geometry when creating an array.
	 */
	__u64 (*avail_size)(struct supertype *st, __u64 size,
			    unsigned long long data_offset);
	/*
	 * Return spare criteria for array:
	 * - minimum disk size can be used in array;
	 * - sector size can be used in array.
	 * Return values: 0 - for success and -EINVAL on error.
	 */
	int (*get_spare_criteria)(struct supertype *st,
				  struct spare_criteria *sc);
	/* Find somewhere to put a bitmap - possibly auto-size it - and
	 * update the metadata to record this.  The array may be newly
	 * created, in which case data_size may be updated, or it might
	 * already exist.  Metadata handler can know if init_super
	 * has been called, but not write_init_super.
	 *  0:     Success
	 * -Exxxx: On error
	 */
	int (*add_internal_bitmap)(struct supertype *st, int *chunkp,
				   int delay, int write_behind,
				   unsigned long long size, int may_change, int major);
	/* Perform additional setup required to activate a bitmap.
	 */
	int (*set_bitmap)(struct supertype *st, struct mdinfo *info);
	/* Seek 'fd' to start of write-intent-bitmap.  Must be an
	 * md-native format bitmap
	 */
	int (*locate_bitmap)(struct supertype *st, int fd, int node_num);
	/* if add_internal_bitmap succeeded for existing array, this
	 * writes it out.
	 */
	int (*write_bitmap)(struct supertype *st, int fd, enum bitmap_update update);
	/* Free the superblock and any other allocated data */
	void (*free_super)(struct supertype *st);

	/* validate_geometry is called with an st returned by
	 * match_metadata_desc.
	 * It should check that the geometry described is compatible with
	 * the metadata type.  It will be called repeatedly as devices
	 * added to validate changing size and new devices.  If there are
	 * inter-device dependencies, it should record sufficient details
	 * so these can be validated.
	 * Both 'size' and '*freesize' are in sectors.  chunk is KiB.
	 * Return value is:
	 *  1: everything is OK
	 *  0: not OK for some reason - if 'verbose', then error was reported.
	 * -1: st->sb was NULL, 'subdev' is a member of a container of this
	 *     type, but array is not acceptable for some reason
	 *     message was reported even if verbose is 0.
	 */
	int (*validate_geometry)(struct supertype *st, int level, int layout,
				 int raiddisks,
				 int *chunk, unsigned long long size,
				 unsigned long long data_offset,
				 char *subdev, unsigned long long *freesize,
				 int consistency_policy, int verbose);

	/* Return a linked list of 'mdinfo' structures for all arrays
	 * in the container.  For non-containers, it is like
	 * getinfo_super with an allocated mdinfo.*/
	struct mdinfo *(*container_content)(struct supertype *st, char *subarray);
	/* query the supertype for default geometry */
	void (*default_geometry)(struct supertype *st, int *level, int *layout, int *chunk); /* optional */
	/* Permit subarray's to be deleted from inactive containers */
	int (*kill_subarray)(struct supertype *st,
			     char *subarray_id); /* optional */
	/* Permit subarray's to be modified */
	int (*update_subarray)(struct supertype *st, char *subarray,
			       char *update, struct mddev_ident *ident); /* optional */
	/* Check if reshape is supported for this external format.
	 * st is obtained from super_by_fd() where st->subarray[0] is
	 * initialized to indicate if reshape is being performed at the
	 * container or subarray level
	 */
#define APPLY_METADATA_CHANGES		1
#define ROLLBACK_METADATA_CHANGES	0

	int (*reshape_super)(struct supertype *st,
			     unsigned long long size, int level,
			     int layout, int chunksize, int raid_disks,
			     int delta_disks, char *backup, char *dev,
			     int direction,
			     int verbose); /* optional */
	int (*manage_reshape)( /* optional */
		int afd, struct mdinfo *sra,
		struct supertype *st, unsigned long blocks,
		int *fds, unsigned long long *offsets,
		int dests, int *destfd, unsigned long long *destoffsets);

/* for mdmon */
	int (*open_new)(struct supertype *c, struct active_array *a,
			char *inst);

	/* Tell the metadata handler the current state of the array.
	 * This covers whether it is known to be consistent (no pending writes)
	 * and how far along a resync is known to have progressed
	 * (in a->resync_start).
	 * resync status is really irrelevant if the array is not consistent,
	 * but some metadata (DDF!) have a place to record the distinction.
	 * If 'consistent' is '2', then the array can mark it dirty if a
	 * resync/recovery/whatever is required, or leave it clean if not.
	 * Return value is 0 dirty (not consistent) and 1 if clean.
	 * it is only really important if consistent is passed in as '2'.
	 */
	int (*set_array_state)(struct active_array *a, int consistent);

	/* When the state of a device might have changed, we call set_disk to
	 * tell the metadata what the current state is.
	 * Typically this happens on spare->in_sync and (spare|in_sync)->faulty
	 * transitions.
	 * set_disk might be called when the state of the particular disk has
	 * not in fact changed.
	 */
	void (*set_disk)(struct active_array *a, int n, int state);
	void (*sync_metadata)(struct supertype *st);
	void (*process_update)(struct supertype *st,
			       struct metadata_update *update);
	/* Prepare updates allocates extra memory that might be
	 * needed.  If the update cannot be understood,  return 0.
	 */
	int (*prepare_update)(struct supertype *st,
			       struct metadata_update *update);

	/* activate_spare will check if the array is degraded and, if it
	 * is, try to find some spare space in the container.
	 * On success, it add appropriate updates (For process_update) to
	 * to the 'updates' list and returns a list of 'mdinfo' identifying
	 * the device, or devices as there might be multiple missing
	 * devices and multiple spares available.
	 */
	struct mdinfo *(*activate_spare)(struct active_array *a,
					 struct metadata_update **updates);
	/*
	 * Return statically allocated string that represents metadata specific
	 * controller domain of the disk. The domain is used in disk domain
	 * matching functions. Disks belong to the same domain if the they have
	 * the same domain from mdadm.conf and belong the same metadata domain.
	 * Returning NULL or not providing this handler means that metadata
	 * does not distinguish the differences between disks that belong to
	 * different controllers. They are in the domain specified by
	 * configuration file (mdadm.conf).
	 * In case when the metadata has the notion of domains based on disk
	 * it shall return NULL for disks that do not belong to the controller
	 * the supported domains. Such disks will form another domain and won't
	 * be mixed with supported ones.
	 */
	const char *(*get_disk_controller_domain)(const char *path);

	/* for external backup area */
	int (*recover_backup)(struct supertype *st, struct mdinfo *info);

	/* validate container after assemble */
	int (*validate_container)(struct mdinfo *info);

	/* write initial empty PPL on device */
	int (*write_init_ppl)(struct supertype *st, struct mdinfo *info, int fd);

	/* validate ppl before assemble */
	int (*validate_ppl)(struct supertype *st, struct mdinfo *info,
			    struct mdinfo *disk);

	/* records new bad block in metadata */
	int (*record_bad_block)(struct active_array *a, int n,
					unsigned long long sector, int length);

	/* clears bad block from metadata */
	int (*clear_bad_block)(struct active_array *a, int n,
					unsigned long long sector, int length);

	/* get list of bad blocks from metadata */
	struct md_bb *(*get_bad_blocks)(struct active_array *a, int n);

	int swapuuid; /* true if uuid is bigending rather than hostendian */
	int external;
	const char *name; /* canonical metadata name */
} *superlist[];

extern struct superswitch super0, super1;
extern struct superswitch super_imsm, super_ddf;
extern struct superswitch mbr, gpt;

struct metadata_update {
	int	len;
	char	*buf;
	void	*space; /* allocated space that monitor will use */
	void	**space_list; /* list of allocated spaces that monitor can
			       * use or that it returned.
			       */
	struct metadata_update *next;
};

/* A supertype holds a particular collection of metadata.
 * It identifies the metadata type by the superswitch, and the particular
 * sub-version of that metadata type.
 * metadata read in or created is stored in 'sb' and 'info'.
 * There are also fields used by mdmon to track containers.
q *
 * A supertype may refer to:
 *   Just an array, possibly in a container
 *   A container, not identifying any particular array
 *   Info read from just one device, not yet fully describing the array/container.
 *
 *
 * A supertype is created by:
 *   super_by_fd
 *   guess_super
 *   dup_super
 */
struct supertype {
	struct superswitch *ss;
	int minor_version;
	int max_devs;
	char container_devnm[32];    /* devnm of container */
	void *sb;
	void *info;
	void *other; /* Hack used to convert v0.90 to v1.0 */
	unsigned long long devsize;
	unsigned long long data_offset; /* used by v1.x only */
	int ignore_hw_compat; /* used to inform metadata handlers that it should ignore
				 HW/firmware related incompatability to load metadata.
				 Used when examining metadata to display content of disk
				 when user has no hw/firmare compatible system.
			      */
	struct metadata_update *updates;
	struct metadata_update **update_tail;

	/* extra stuff used by mdmon */
	struct active_array *arrays;
	int sock; /* listen to external programs */
	char devnm[32]; /* e.g. md0.  This appears in metadata_version:
			 *  external:/md0/12
			 */
	int devcnt;
	int retry_soon;
	int nodes;
	char *cluster_name;

	struct mdinfo *devs;

};

extern int mdadm_manage_ro(char *devname, int fd, int readonly);
extern int mdadm_manage_run(char *devname, int fd, struct context *c);
extern int mdadm_manage_stop(char *devname, int fd, int quiet,
			     int will_retry);
extern int mdadm_manage_subdevs(char *devname, int fd,
				struct mddev_dev *devlist, int verbose,
				int test, char *update, int force);
extern int mdadm_autodetect(void);
extern int mdadm_grow_add_device(char *devname, int fd, char *newdev);
extern int mdadm_grow_add_bitmap(char *devname, int fd,
				struct context *c, struct shape *s);
extern int mdadm_grow_reshape(char *devname, int fd,
			      struct mddev_dev *devlist,
			      unsigned long long data_offset,
			      struct context *c, struct shape *s);
extern int mdadm_grow_restart(struct supertype *st, struct mdinfo *info,
			      int *fdlist, int cnt, char *backup_file,
			      int verbose);
extern int mdadm_grow_continue(int mdfd, struct supertype *st,
			       struct mdinfo *info, char *backup_file,
			       int forked, int freeze_reshape);
extern int mdadm_grow_consistency_policy(char *devname, int fd,
					 struct context *c, struct shape *s);

extern int mdadm_grow_continue_command(char *devname, int fd,
				       char *backup_file, int verbose);

extern int mdadm_assemble(struct supertype *st, char *mddev,
			  struct mddev_ident *ident,
			  struct mddev_dev *devlist,
			  struct context *c);

extern int mdadm_build(char *mddev, struct mddev_dev *devlist,
		       struct shape *s, struct context *c);

extern int mdadm_create(struct supertype *st, char *mddev,
			char *name, int *uuid,
			int subdevs, struct mddev_dev *devlist,
			struct shape *s,
			struct context *c,
			unsigned long long data_offset);

extern int mdadm_detail(char *dev, struct context *c);
extern int mdadm_detail_platform(struct superswitch *ss, int scan, int verbose,
				 int export, char *controller_path);
extern int mdadm_query(char *dev);
extern int mdadm_examine_badblocks(char *devname, int brief,
				  struct supertype *forcest);
extern int mdadm_examine(struct mddev_dev *devlist, struct context *c,
			 struct supertype *forcest);
extern int mdadm_monitor(struct mddev_dev *devlist,
			 char *mailaddr, char *alert_cmd,
			 struct context *c,
			 int daemonise, int oneshot,
			 int dosyslog, char *pidfile, int increments,
			 int share);

extern int mdadm_kill(char *dev, struct supertype *st, int force, int verbose,
		      int noexcl);
extern int mdadm_kill_subarray(char *dev, char *subarray, int verbose);
extern int mdadm_update_subarray(char *dev, char *subarray, char *update,
				 struct mddev_ident *ident, int quiet);
extern int mdadm_wait(char *dev);
extern int mdadm_wait_clean(char *dev, int verbose);
extern int mdadm_set_action(char *dev, char *action);

extern int mdadm_incremental(struct mddev_dev *devlist, struct context *c,
			     struct supertype *st);
extern void mdadm_rebuild_map(void);
extern int mdadm_incremental_scan(struct context *c, char *devnm);
extern int mdadm_incremental_remove(char *devname, char *path, int verbose);
extern int mdadm_create_bitmap(char *filename, int force, char uuid[16],
			       unsigned long chunksize,
			       unsigned long daemon_sleep,
			       unsigned long write_behind,
			       unsigned long long array_size,
			       int major);
extern int mdadm_examine_bitmap(char *filename, int brief,
				struct supertype *st);
extern int mdadm_is_bitmap_dirty(char *filename);
extern int mdadm_write_rules(char *rule_name);

/* Assemble.c */
int mdadm_scan_assemble(struct supertype *ss, struct context *c,
			struct mddev_ident *ident);

/* Detail.c */
int mdadm_misc_scan(char devmode, struct context *c);

/* Grow.c */
int mdadm_grow_set_size(int mdfd, unsigned long long array_size);

/* Manage.c */
int mdadm_set_action(char *dev, char *action);
int mdadm_stop_scan(int verbose);

extern struct supertype *mdadm_lookup_supertype(char *metadata);

extern char *mdlib_get_conffile(void);
extern void mdlib_set_conffile(char *file);
extern void mdlib_set_name(const char *name);
extern const char *mdlib_get_name(void);
extern const char *mdlib_get_version(void);

/* maps.c */
extern int mdadm_get_layout(int level, char *name);
extern int mdadm_default_layout(int level, int verbose);
extern int mdadm_personality_num(char *name);
extern char *mdadm_personality_name(int num);
extern int mdadm_consistency_policy_num(char *name);
extern char *mdadm_consistency_policy_name(int num);
extern char *mdadm_raid_layout_name(int level, int layout);
extern int mdadm_raid_layout_num(int level, char *layout);
extern int mdadm_faulty_layout(char *name);
extern int mdadm_array_state_num(char *name);
extern char *mdadm_array_state_name(int num);
extern char *mdadm_assemble_status(int status);

extern int mdadm_dump_metadata(char *dev, char *dir, struct context *c,
			       struct supertype *st);
extern int mdadm_restore_metadata(char *dev, char *dir, struct context *c,
				  struct supertype *st, int only);

int md_array_valid(int fd);
int md_array_active(int fd);
int md_array_is_active(struct mdinfo *info);
int md_get_array_info(int fd, struct mdu_array_info_s *array);
int md_set_array_info(int fd, struct mdu_array_info_s *array);
int md_get_disk_info(int fd, struct mdu_disk_info_s *disk);
extern int get_linux_version(void);
extern int mdadm_version(char *version);
extern void mdlib_manage_fork_fds(int close_all);
extern unsigned long long parse_size(char *size);
extern int parse_uuid(char *str, int uuid[4]);
extern long parse_num(char *num);
extern int parse_cluster_confirm_arg(char *inp, char **devname, int *slot);

extern int is_standard(char *dev, int *nump);
extern int open_dev(char *devnm);

extern int conf_parse_auto(char *str, char *msg, int config);
extern char *conf_get_mailaddr(void);
extern char *conf_get_mailfrom(void);
extern char *conf_get_program(void);
extern char *conf_get_homehost(int *require_homehostp);
extern char *conf_get_homecluster(void);

extern unsigned long calc_csum(void *super, int bytes);
extern int enough(int level, int raid_disks, int layout, int clean,
		   char *avail);
extern int ask(char *mesg);
extern unsigned long long get_component_size(int fd);
extern void remove_partitions(int fd);
extern int test_partition(int fd);
extern int test_partition_from_id(dev_t id);
extern int get_data_disks(int level, int layout, int raid_disks);
extern unsigned long long calc_array_size(int level, int raid_disks, int layout,
				   int chunksize, unsigned long long devsize);
extern int flush_metadata_updates(struct supertype *st);
extern void append_metadata_update(struct supertype *st, void *buf, int len);
extern int assemble_container_content(struct supertype *st, int mdfd,
				      struct mdinfo *content,
				      struct context *c,
				      char *chosen_name, int *result);

extern int move_spare(char *from_devname, char *to_devname, dev_t devid);
extern int add_disk(int mdfd, struct supertype *st,
		    struct mdinfo *sra, struct mdinfo *info);
extern int remove_disk(int mdfd, struct supertype *st,
		       struct mdinfo *sra, struct mdinfo *info);
extern int hot_remove_disk(int mdfd, unsigned long dev, int force);
extern int sys_hot_remove_disk(int statefd, int force);
extern int set_array_info(int mdfd, struct supertype *st, struct mdinfo *info);
unsigned long long min_recovery_start(struct mdinfo *array);

extern char *human_size(long long bytes);
extern char *human_size_brief(long long bytes, int prefix);
extern void print_r10_layout(int layout);

extern char *find_free_devnm(int use_partitions);

extern void put_md_name(char *name);
extern dev_t devnm2devid(char *devnm);
extern char *get_md_name(char *devnm);

extern char DefaultConfFile[];

extern int create_mddev(char *dev, char *name, int autof, int trustworthy,
			char *chosen, int block_udev);
/* values for 'trustworthy' */
#define	LOCAL	1
#define	LOCAL_ANY 10
#define	FOREIGN	2
#define	METADATA 3
extern int open_mddev(char *dev, int report_errors);
extern int open_container(int fd);
extern int metadata_container_matches(char *metadata, char *devnm);
extern int metadata_subdev_matches(char *metadata, char *devnm);
extern int is_container_member(struct mdstat_ent *ent, char *devname);
extern int is_subarray_active(char *subarray, char *devname);
extern int open_subarray(char *dev, char *subarray, struct supertype *st, int quiet);
extern struct superswitch *version_to_superswitch(char *vers);

extern int mdmon_running(char *devnm);
extern int mdmon_pid(char *devnm);
extern int check_env(char *name);
extern __u32 random32(void);
extern void random_uuid(__u8 *buf);
extern int start_mdmon(char *devnm);

void *super1_make_v0(struct supertype *st, struct mdinfo *info, mdp_super_t *sb0);

/* lib.c */
extern char *stat2devnm(struct stat *st);
extern void udev_block(char *devnm);
extern void udev_unblock(void);

/* util.c */
extern void set_hooks(void);
extern int get_cluster_name(char **name);
extern int cluster_get_dlmlock(void);
extern int cluster_release_dlmlock(void);

static inline int is_subarray(char *vers)
{
	/* The version string for a 'subarray' (an array in a container)
	 * is
	 *    /containername/componentname    for normal read-write arrays
	 *    -containername/componentname    for arrays which mdmon must not
	 *				      reconfigure.  They might be read-only
	 *				      or might be undergoing reshape etc.
	 * containername is e.g. md0, md_d1
	 * componentname is dependant on the metadata. e.g. '1' 'S1' ...
	 */
	return (*vers == '/' || *vers == '-');
}

static inline char *to_subarray(struct mdstat_ent *ent, char *container)
{
	return &ent->metadata_version[10+strlen(container)+1];
}

