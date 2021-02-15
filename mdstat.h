#ifndef _MDSTAT_H
#define _MDSTAT_H

extern struct mdstat_ent *mdstat_read(int hold, int start);
extern void mdstat_close(void);
extern void free_mdstat(struct mdstat_ent *ms);
extern int mdstat_wait(int seconds);
extern void mdstat_wait_fd(int fd, const sigset_t *sigmask);
extern int mddev_busy(char *devnm);
extern struct mdstat_ent *mdstat_by_component(char *name);
extern struct mdstat_ent *mdstat_by_subdev(char *subdev, char *container);

#endif /* _MDSTAT_H */
