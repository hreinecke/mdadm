#ifndef _SYSFS_H
#define _SYSFS_H

/* If fd >= 0, get the array it is open on,
 * else use devnm.
 */
extern int sysfs_open(char *devnm, char *devname, char *attr);
extern int sysfs_init(struct mdinfo *mdi, int fd, char *devnm);
extern void sysfs_init_dev(struct mdinfo *mdi, dev_t devid);
extern void sysfs_free(struct mdinfo *sra);
extern struct mdinfo *sysfs_read(int fd, char *devnm, unsigned long options);
extern int sysfs_attr_match(const char *attr, const char *str);
extern int sysfs_match_word(const char *word, char **list);
extern int sysfs_set_str(struct mdinfo *sra, struct mdinfo *dev,
			 char *name, char *val);
extern int sysfs_set_num(struct mdinfo *sra, struct mdinfo *dev,
			 char *name, unsigned long long val);
extern int sysfs_set_num_signed(struct mdinfo *sra, struct mdinfo *dev,
				char *name, long long val);
extern int sysfs_uevent(struct mdinfo *sra, char *event);
extern int sysfs_get_fd(struct mdinfo *sra, struct mdinfo *dev,
			char *name);
extern int sysfs_fd_get_ll(int fd, unsigned long long *val);
extern int sysfs_get_ll(struct mdinfo *sra, struct mdinfo *dev,
			char *name, unsigned long long *val);
extern int sysfs_fd_get_two(int fd, unsigned long long *v1, unsigned long long *v2);
extern int sysfs_get_two(struct mdinfo *sra, struct mdinfo *dev,
			 char *name, unsigned long long *v1, unsigned long long *v2);
extern int sysfs_fd_get_str(int fd, char *val, int size);
extern int sysfs_attribute_available(struct mdinfo *sra, struct mdinfo *dev,
				     char *name);
extern int sysfs_get_str(struct mdinfo *sra, struct mdinfo *dev,
			 char *name, char *val, int size);
extern int sysfs_set_safemode(struct mdinfo *sra, unsigned long ms);
extern int sysfs_set_array(struct mdinfo *info, int vers);
extern int sysfs_add_disk(struct mdinfo *sra, struct mdinfo *sd, int resume);
extern int sysfs_disk_to_scsi_id(int fd, __u32 *id);
extern int sysfs_unique_holder(char *devnm, long rdev);
extern int sysfs_freeze_array(struct mdinfo *sra);
extern int sysfs_wait(int fd, int *msec);
extern int load_sys(char *path, char *buf, int len);

#endif /* _SYSFS_H */
