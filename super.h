#ifndef _SUPER_H
#define _SUPER_H

extern struct supertype *super_by_fd(int fd, char **subarray);
enum guess_types { guess_any, guess_array, guess_partitions };
extern struct supertype *guess_super_type(int fd, enum guess_types guess_type);

static inline struct supertype *guess_super(int fd) {
	return guess_super_type(fd, guess_any);
}
extern struct supertype *dup_super(struct supertype *st);
extern int get_dev_size(int fd, char *dname, unsigned long long *sizep);
extern int get_dev_sector_size(int fd, char *dname, unsigned int *sectsizep);
extern int must_be_container(int fd);
extern int dev_size_from_id(dev_t id, unsigned long long *size);
extern int dev_sector_size_from_id(dev_t id, unsigned int *size);
void wait_for(char *dev, int fd);

extern char *fname_from_uuid(struct supertype *st,
			     struct mdinfo *info, char *buf, char sep);

#endif /* _SUPER_H */
