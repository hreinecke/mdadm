#ifndef _MDADM_INTERNAL_H
#define _MDADM_INTERNAL

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

/* bitmap.c */
extern int bitmap_update_uuid(int fd, int *uuid, int swap);
/* calculate the size of the bitmap given the array size and bitmap chunksize */
static inline unsigned long long
bitmap_bits(unsigned long long array_size, unsigned long chunksize)
{
	return (array_size * 512 + chunksize - 1) / chunksize;
}

/* Grow.c */
extern char *locate_backup(char *name);
extern int restore_backup(struct supertype *st,
			  struct mdinfo *content,
			  int working_disks,
			  int spares,
			  char **backup_filep,
			  int verbose);
#endif /* _MDADM_INTERNAL */
