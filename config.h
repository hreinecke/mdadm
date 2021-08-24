#ifndef _CONFIG_H
#define _CONFIG_H

#include "mapfile.h"
#include "policy.h"

struct createinfo {
	int	uid;
	int	gid;
	int	autof;
	int	mode;
	int	names;
	int	bblist;
	struct supertype *supertype;
};

extern char *conf_get_mailaddr(void);
extern char *conf_get_mailfrom(void);
extern char *conf_get_program(void);
extern char *conf_get_homehost(int *require_homehostp);
extern char *conf_get_homecluster(void);

extern struct mddev_ident *conf_get_ident(char *dev);
extern struct mddev_dev *conf_get_devs(void);
extern int conf_test_dev(char *devname);
extern int conf_test_metadata(const char *version, struct dev_policy *pol, int is_homehost);
extern struct createinfo *conf_get_create_info(void);
extern void set_conffile(char *file);
extern int conf_get_monitor_delay(void);
extern char *conf_line(FILE *file);
extern char *conf_word(FILE *file, int allow_key);
extern int conf_name_is_free(char *name);
extern int conf_verify_devnames(struct mddev_ident *array_list);
extern int devname_matches(char *name, char *match);
extern struct mddev_ident *conf_match(struct supertype *st,
				      struct mdinfo *info,
				      char *devname,
				      int verbose, int *rvp);
extern void free_line(char *line);
extern int match_oneof(char *devices, char *devname);
extern int devname_matches(char *name, char *match);

#endif /* _CONFIG_H */
