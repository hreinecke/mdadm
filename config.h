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

char *conf_get_mailaddr(void);
char *conf_get_mailfrom(void);
char *conf_get_program(void);
char *conf_get_homehost(int *require_homehostp);
char *conf_get_homecluster(void);

struct mddev_ident *conf_get_ident(char *dev);
struct mddev_dev *conf_get_devs(void);
int conf_test_dev(char *devname);
int conf_test_metadata(const char *version, struct dev_policy *pol, int is_homehost);
struct createinfo *conf_get_create_info(void);
void set_conffile(char *file);
int conf_get_monitor_delay(void);
char *conf_line(FILE *file);
char *conf_word(FILE *file, int allow_key);
int conf_name_is_free(char *name);
int conf_verify_devnames(struct mddev_ident *array_list);
int devname_matches(char *name, char *match);
struct mddev_ident *conf_match(struct supertype *st,
				      struct mdinfo *info,
				      char *devname,
				      int verbose, int *rvp);
void free_line(char *line);
int match_oneof(char *devices, char *devname);
int devname_matches(char *name, char *match);

#endif /* _CONFIG_H */
