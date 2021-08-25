#ifndef _MDSTAT_H
#define _MDSTAT_H

struct mdstat_ent *mdstat_read(int hold, int start);
void mdstat_close(void);
void free_mdstat(struct mdstat_ent *ms);
int mdstat_wait(int seconds);
void mdstat_wait_fd(int fd, const sigset_t *sigmask);
int mddev_busy(char *devnm);
struct mdstat_ent *mdstat_by_component(char *name);
struct mdstat_ent *mdstat_by_subdev(char *subdev, char *container);

#endif /* _MDSTAT_H */
