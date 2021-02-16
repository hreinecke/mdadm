/* We want to use unsigned numbers for sector counts, but need
 * a value for 'invalid'.  Use '1'.
 */
#define INVALID_SECTORS 1

/* Sometimes the 'size' value passed needs to mean "Maximum".
 * In those cases with use MAX_SIZE
 */
#define MAX_SIZE	1

/* And another special number needed for --data_offset=variable */
#define VARIABLE_OFFSET 3

/* Define PATH_MAX in case we don't use glibc or standard library does
 * not have PATH_MAX defined. Assume max path length is 4K characters.
 */
#define PATH_MAX	4096

#define	MD_MAJOR 9

#define	LEVEL_MULTIPATH		(-4)
#define	LEVEL_LINEAR		(-1)
#define	LEVEL_FAULTY		(-5)

/* kernel module doesn't know about these */
#define LEVEL_CONTAINER		(-100)
#define	LEVEL_UNSUPPORTED	(-200)

/* the kernel does know about this one ... */
#define	LEVEL_NONE		(-1000000)

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
	uint64_t	events;
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
#define MaxSector  (uint64_t)-1 /* resync/recovery complete position */
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

struct createinfo {
	int	uid;
	int	gid;
	int	autof;
	int	mode;
	int	symlinks;
	int	names;
	int	bblist;
	struct supertype *supertype;
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

struct map_ent {
	struct map_ent *next;
	char	devnm[32];
	char	metadata[20];
	int	uuid[4];
	int	bad;
	char	*path;
};

extern int open_dev(char *devnm);

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

/* lib.c */
extern int get_linux_version(void);
extern int is_near_layout_10(int layout);
extern unsigned long long parse_size(char *size);
extern long parse_num(char *num);
extern int parse_uuid(char *str, int uuid[4]);
extern void mdlib_set_name(const char *name);
extern const char *mdlib_get_name(void);
extern char *mdlib_get_version(void);

/* config.c */
extern int parse_auto(char *str, char *msg, int config);
extern struct mddev_ident *conf_get_ident(char *dev);
extern struct mddev_dev *conf_get_devs(void);
extern struct createinfo *conf_get_create_info(void);
extern void mdlib_set_conffile(char *file);
extern char *conf_get_homehost(int *require_homehostp);
extern char *conf_get_homecluster(void);
extern char *mdlib_get_conffile(void);

/* util.c */
extern void set_hooks(void);
extern int get_cluster_name(char **name);
extern int cluster_get_dlmlock(void);
extern int cluster_release_dlmlock(void);

extern int md_get_array_info(int fd, struct mdu_array_info_s *array);

struct supertype *mdadm_lookup_supertype(char *metadata);

/* mdadm functions */
extern int mdadm_manage_ro(char *devname, int fd, int readonly);
extern int mdadm_manage_run(char *devname, int fd, struct context *c);
extern int mdadm_manage_stop(char *devname, int fd, int quiet,
		       int will_retry);
extern int mdadm_manage_subdevs(char *devname, int fd,
			  struct mddev_dev *devlist, int verbose, int test,
			  char *update, int force);
extern int mdadm_set_action(char *dev, char *action);
extern int mdadm_stop_scan(int verbose);
extern int mdadm_autodetect(void);
extern int mdadm_grow_add_device(char *devname, int fd, char *newdev);
extern int mdadm_grow_addbitmap(char *devname, int fd,
			  struct context *c, struct shape *s);
extern int mdadm_grow_reshape(char *devname, int fd,
			struct mddev_dev *devlist,
			unsigned long long data_offset,
			struct context *c, struct shape *s);
extern int mdadm_grow_restart(struct supertype *st, struct mdinfo *info,
			int *fdlist, int cnt, char *backup_file, int verbose);
extern int mdadm_grow_continue(int mdfd, struct supertype *st,
			 struct mdinfo *info, char *backup_file,
			 int forked, int freeze_reshape);
extern int mdadm_grow_consistency_policy(char *devname, int fd,
				   struct context *c, struct shape *s);

extern int mdadm_grow_continue_command(char *devname, int fd,
				 char *backup_file, int verbose);

extern int mdadm_grow_set_size(int fd, unsigned long long array_size);

extern int mdadm_assemble(struct supertype *st, char *mddev,
			  struct mddev_ident *ident,
			  struct mddev_dev *devlist,
			  struct context *c);

extern int mdadm_scan_assemble(struct supertype *ss,
			       struct context *c,
			       struct mddev_ident *ident);

extern int mdadm_build(char *mddev, struct mddev_dev *devlist,
		       struct shape *s, struct context *c);

extern int mdadm_create(struct supertype *st, char *mddev,
			char *name, int *uuid,
			int subdevs, struct mddev_dev *devlist,
			struct shape *s,
			struct context *c,
			unsigned long long data_offset);

extern int mdadm_detail(char *dev, struct context *c);
extern int mdadm_detail_platform(struct supertype *st, int scan, int verbose,
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

extern int mdadm_kill(char *dev, struct supertype *st, int force, int verbose, int noexcl);
extern int mdadm_kill_subarray(char *dev, char *subarray, int verbose);
extern int mdadm_wait(char *dev);
extern int mdadm_wait_clean(char *dev, int verbose);

extern int mdadm_incremental(struct mddev_dev *devlist, struct context *c,
			     struct supertype *st);
extern int mdadm_update_subarray(char *dev, char *subarray, char *update,
				 struct mddev_ident *ident, int quiet);
extern void mdadm_rebuild_map(void);
extern int mdadm_incremental_scan(struct context *c, char *devnm);
extern int mdadm_incremental_remove(char *devname, char *path, int verbose);
extern int mdadm_examine_bitmap(char *filename, int brief, struct supertype *st);
extern int mdadm_dump_metadata(char *dev, char *dir, struct context *c,
			       struct supertype *st);
extern int mdadm_restore_metadata(char *dev, char *dir, struct context *c,
				  struct supertype *st, int only);
extern int mdadm_misc_scan(char devmode, struct context *c);

extern int mdadm_write_rules(char *rule_name);
