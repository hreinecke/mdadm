#ifndef _POLICY_H
#define _POLICY_H

extern char pol_act[], pol_domain[], pol_metadata[], pol_auto[];

/* iterate over the sublist starting at list, having the same
 * 'name' as 'list', and matching the given metadata (Where
 * NULL matches anything
 */
#define pol_for_each(item, list, _metadata)				\
	for (item = list;						\
	     item && item->name == list->name;				\
	     item = item->next)						\
		if (!(!_metadata || !item->metadata || _metadata == item->metadata)) \
			; else
/*
 * policy records read from mdadm are largely just name-value pairs.
 * The names are constants, not strdupped
 */
struct pol_rule {
	struct pol_rule *next;
	char *type;	/* rule_policy or rule_part */
	struct rule {
		struct rule *next;
		char *name;
		char *value;
		char *dups; /* duplicates of 'value' with a partNN appended */
	} *rule;
};

extern char rule_policy[], rule_part[];
extern char rule_path[], rule_type[];
extern char type_part[], type_disk[];

extern void policyline(char *line, char *type);
extern void policy_add(char *type, ...);
extern void policy_free(void);

extern struct dev_policy *path_policy(char **paths, char *type);
extern struct dev_policy *disk_policy(struct mdinfo *disk);
extern struct dev_policy *devid_policy(int devid);
extern void dev_policy_free(struct dev_policy *p);

//extern void pol_new(struct dev_policy **pol, char *name, char *val, char *metadata);
extern void pol_add(struct dev_policy **pol, char *name, char *val, char *metadata);
extern struct dev_policy *pol_find(struct dev_policy *pol, char *name);

enum policy_action {
	act_default,
	act_include,
	act_re_add,
	act_spare,	/* This only applies to bare devices */
	act_spare_same_slot, /* this allows non-bare devices,
			      * but only if recent removal */
	act_force_spare, /* this allow non-bare devices in any case */
	act_err
};

extern int policy_action_allows(struct dev_policy *plist, const char *metadata,
				enum policy_action want);
extern int disk_action_allows(struct mdinfo *disk, const char *metadata,
			      enum policy_action want);

struct domainlist {
	struct domainlist *next;
	const char *dom;
};

extern int domain_test(struct domainlist *dom, struct dev_policy *pol,
		       const char *metadata);
extern struct domainlist *domain_from_array(struct mdinfo *mdi,
					    const char *metadata);
extern void domainlist_add_dev(struct domainlist **dom, int devid,
			       const char *metadata);
extern void domain_free(struct domainlist *dl);
extern void domain_merge(struct domainlist **domp, struct dev_policy *pol,
			 const char *metadata);
void domain_add(struct domainlist **domp, char *domain);

extern void policy_save_path(char *id_path, struct map_ent *array);
extern int policy_check_path(struct mdinfo *disk, struct map_ent *array);

#define	INCR_NO		1
#define	INCR_UNSAFE	2
#define	INCR_ALREADY	4
#define	INCR_YES	8
extern struct mdinfo *container_choose_spares(struct supertype *st,
					      struct spare_criteria *criteria,
					      struct domainlist *domlist,
					      char *spare_group,
					      const char *metadata, int get_one);
#endif /* _POLICY_H */
