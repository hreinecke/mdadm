#ifndef _MDADM_INTERNAL_H
#define _MDADM_INTERNAL

/* values for 'trustworthy' */
#define	LOCAL	1
#define	LOCAL_ANY 10
#define	FOREIGN	2
#define	METADATA 3

/* util.c */
extern int zero_disk_range(int fd, unsigned long long sector, size_t count);
extern int is_near_layout_10(int layout);
extern int parse_layout_10(char *layout);
extern int parse_layout_faulty(char *layout);
extern int check_ext2(int fd, char *name);
extern int check_reiser(int fd, char *name);
extern int check_raid(int fd, char *name);
extern int check_partitions(int fd, char *dname,
			    unsigned long long freesize,
			    unsigned long long size);
extern int fstat_is_blkdev(int fd, char *devname, dev_t *rdev);
extern int stat_is_blkdev(char *devname, dev_t *rdev);
extern int get_maj_min(char *dev, int *major, int *minor);
extern int dev_open(char *dev, int flags);
extern void reopen_mddev(int mdfd);
extern int open_dev_flags(char *devnm, int flags);
extern int open_dev_excl(char *devnm);
extern int same_dev(char *one, char *two);
extern int compare_paths (char* path1,char* path2);
extern void enable_fds(int devices);
extern int continue_via_systemd(char *devnm, char *service_name);
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
extern int add_disk(int mdfd, struct supertype *st,
		    struct mdinfo *sra, struct mdinfo *info);
extern int remove_disk(int mdfd, struct supertype *st,
		       struct mdinfo *sra, struct mdinfo *info);
extern int hot_remove_disk(int mdfd, unsigned long dev, int force);
extern int set_array_info(int mdfd, struct supertype *st, struct mdinfo *info);
int md_array_valid(int fd);
int md_array_active(int fd);
int md_array_is_active(struct mdinfo *info);
int md_get_disk_info(int fd, struct mdu_disk_info_s *disk);

unsigned long long min_recovery_start(struct mdinfo *array);

extern char *human_size(long long bytes);
extern char *human_size_brief(long long bytes, int prefix);
extern void print_r10_layout(int layout);

extern char *get_md_name(char *devnm);
extern void put_md_name(char *name);

extern int is_standard(char *dev, int *nump);
extern int is_subarray_active(char *subarray, char *devname);

extern int open_container(int fd);
extern int metadata_container_matches(char *metadata, char *devnm);
extern int metadata_subdev_matches(char *metadata, char *devnm);
extern int is_container_member(struct mdstat_ent *ent, char *devname);
extern int open_subarray(char *dev, char *subarray, struct supertype *st,
			 int quiet);
extern int check_env(char *name);
extern __u32 random32(void);
extern void random_uuid(__u8 *buf);

/* super1.c */
void *super1_make_v0(struct supertype *st, struct mdinfo *info,
		     mdp_super_t *sb0);

/* mdopen.c */
extern int create_mddev(char *dev, char *name, int autof, int trustworthy,
			char *chosen, int block_udev);

/* bitmap.c */
extern int bitmap_update_uuid(int fd, int *uuid, int swap);
extern int mdadm_is_bitmap_dirty(char *filename);
/* calculate the size of the bitmap given the array size and bitmap chunksize */
static inline unsigned long long
bitmap_bits(unsigned long long array_size, unsigned long chunksize)
{
	return (array_size * 512 + chunksize - 1) / chunksize;
}

/* Assemble.c */
extern int assemble_container_content(struct supertype *st, int mdfd,
				      struct mdinfo *content,
				      struct context *c,
				      char *chosen_name, int *result);

/* Grow.c */
extern char *locate_backup(char *name);
extern int restore_backup(struct supertype *st,
			  struct mdinfo *content,
			  int working_disks,
			  int spares,
			  char **backup_filep,
			  int verbose);
#endif /* _MDADM_INTERNAL */
