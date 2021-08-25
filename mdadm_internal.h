#ifndef _MDADM_INTERNAL_H
#define _MDADM_INTERNAL

/* values for 'trustworthy' */
#define	LOCAL	1
#define	LOCAL_ANY 10
#define	FOREIGN	2
#define	METADATA 3

/**
 * is_fd_valid() - check file descriptor.
 * @fd: file descriptor.
 *
 * The function checks if @fd is nonnegative integer and shall be used only
 * to verify open() result.
 */
static inline int is_fd_valid(int fd)
{
	return (fd > -1);
}

/**
 * is_level456() - check whether given level is between inclusive 4 and 6.
 * @level: level to check.
 *
 * Return: true if condition is met, false otherwise
 */
static inline bool is_level456(int level)
{
	return (level >= 4 && level <= 6);
}

/**
 * close_fd() - verify, close and unset file descriptor.
 * @fd: pointer to file descriptor.
 *
 * The function closes and invalidates file descriptor if appropriative. It
 * ignores incorrect file descriptor quitely to simplify error handling.
 */
static inline void close_fd(int *fd)
{
	if (is_fd_valid(*fd) && close(*fd) == 0)
		*fd = -1;
}

/**
 * signal_s() - Wrapper for sigaction() with signal()-like interface.
 * @sig: The signal to set the signal handler to.
 * @handler: The signal handler.
 *
 * Return: previous handler or SIG_ERR on failure.
 */
static inline sighandler_t signal_s(int sig, sighandler_t handler)
{
	struct sigaction new_act;
	struct sigaction old_act;

	new_act.sa_handler = handler;
	new_act.sa_flags = 0;

	if (sigaction(sig, &new_act, &old_act) == 0)
		return old_act.sa_handler;

	return SIG_ERR;
}

/* util.c */
int zero_disk_range(int fd, unsigned long long sector, size_t count);
int is_near_layout_10(int layout);
int parse_layout_10(char *layout);
int parse_layout_faulty(char *layout);
int check_ext2(int fd, char *name);
int check_reiser(int fd, char *name);
int check_raid(int fd, char *name);
int check_partitions(int fd, char *dname,
			    unsigned long long freesize,
			    unsigned long long size);
int fstat_is_blkdev(int fd, char *devname, dev_t *rdev);
int stat_is_blkdev(char *devname, dev_t *rdev);
int get_maj_min(char *dev, int *major, int *minor);
bool is_bit_set(int *val, unsigned char index);
int dev_open(char *dev, int flags);
void reopen_mddev(int mdfd);
int open_dev_flags(char *devnm, int flags);
int open_dev_excl(char *devnm);
int same_dev(char *one, char *two);
int compare_paths (char* path1,char* path2);
void enable_fds(int devices);
int continue_via_systemd(char *devnm, char *service_name);
unsigned long calc_csum(void *super, int bytes);

int enough(int level, int raid_disks, int layout, int clean,
		   char *avail);
int ask(char *mesg);

unsigned long long get_component_size(int fd);
void remove_partitions(int fd);
int test_partition(int fd);
int test_partition_from_id(dev_t id);
int get_data_disks(int level, int layout, int raid_disks);
unsigned long long calc_array_size(int level, int raid_disks, int layout,
				   int chunksize, unsigned long long devsize);
int flush_metadata_updates(struct supertype *st);
void append_metadata_update(struct supertype *st, void *buf, int len);
int add_disk(int mdfd, struct supertype *st,
		    struct mdinfo *sra, struct mdinfo *info);
int remove_disk(int mdfd, struct supertype *st,
		       struct mdinfo *sra, struct mdinfo *info);
int hot_remove_disk(int mdfd, unsigned long dev, int force);
int set_array_info(int mdfd, struct supertype *st, struct mdinfo *info);
int md_array_valid(int fd);
int md_array_active(int fd);
int md_array_is_active(struct mdinfo *info);
int md_get_array_info(int fd, struct mdu_array_info_s *array);
int md_set_array_info(int fd, struct mdu_array_info_s *array);
int md_get_disk_info(int fd, struct mdu_disk_info_s *disk);

unsigned long long min_recovery_start(struct mdinfo *array);

char *human_size(long long bytes);
char *human_size_brief(long long bytes, int prefix);
void print_r10_layout(int layout);

char *get_md_name(char *devnm);
void put_md_name(char *name);

int is_standard(char *dev, int *nump);
int is_subarray_active(char *subarray, char *devname);

int open_container(int fd);
int metadata_container_matches(char *metadata, char *devnm);
int metadata_subdev_matches(char *metadata, char *devnm);
int is_container_member(struct mdstat_ent *ent, char *devname);
int open_subarray(char *dev, char *subarray, struct supertype *st,
			 int quiet);
int check_env(char *name);
__u32 random32(void);
void random_uuid(__u8 *buf);

/* super1.c */
void *super1_make_v0(struct supertype *st, struct mdinfo *info,
		     mdp_super_t *sb0);

/* mdopen.c */
int create_mddev(char *dev, char *name, int autof, int trustworthy,
			char *chosen, int block_udev);

/* bitmap.c */
int bitmap_update_uuid(int fd, int *uuid, int swap);
int mdadm_is_bitmap_dirty(char *filename);
/* calculate the size of the bitmap given the array size and bitmap chunksize */
static inline unsigned long long
bitmap_bits(unsigned long long array_size, unsigned long chunksize)
{
	return (array_size * 512 + chunksize - 1) / chunksize;
}

/* Assemble.c */
int assemble_container_content(struct supertype *st, int mdfd,
				      struct mdinfo *content,
				      struct context *c,
				      char *chosen_name, int *result);

/* Grow.c */
char *locate_backup(char *name);
int restore_backup(struct supertype *st,
			  struct mdinfo *content,
			  int working_disks,
			  int spares,
			  char **backup_filep,
			  int verbose);
#endif /* _MDADM_INTERNAL */
