#ifndef _MDADM_INTERNAL_H
#define _MDADM_INTERNAL

/* util.c */
extern int zero_disk_range(int fd, unsigned long long sector, size_t count);
extern int is_near_layout_10(int layout);

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
