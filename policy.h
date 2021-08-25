#ifndef _POLICY_H
#define _POLICY_H

/* FAILED_SLOTS is where to save files storing recent removal of array
 * member in order to allow future reuse of disk inserted in the same
 * slot for array recovery
 */
#ifndef FAILED_SLOTS_DIR
#define FAILED_SLOTS_DIR "/run/mdadm/failed-slots"
#endif /* FAILED_SLOTS */

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

void policyline(char *line, char *type);
void policy_add(char *type, ...);
void policy_free(void);

/*
 * Data structures for policy management.
 * Each device can have a policy structure that lists
 * various name/value pairs each possibly with a metadata associated.
 * The policy list is sorted by name/value/metadata
 */
struct dev_policy {
	struct dev_policy *next;
	char *name;	/* None of these strings are allocated.  They are
			 * all just references to strings which are known
			 * to exist elsewhere.
			 * name and metadata can be compared by address equality.
			 */
	const char *metadata;
	const char *value;
};

struct dev_policy *path_policy(char **paths, char *type);
struct dev_policy *disk_policy(struct mdinfo *disk);
struct dev_policy *devid_policy(int devid);
void dev_policy_free(struct dev_policy *p);

//extern void pol_new(struct dev_policy **pol, char *name, char *val, char *metadata);
void pol_add(struct dev_policy **pol, char *name, char *val, char *metadata);
struct dev_policy *pol_find(struct dev_policy *pol, char *name);

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

int policy_action_allows(struct dev_policy *plist, const char *metadata,
			 enum policy_action want);
int disk_action_allows(struct mdinfo *disk, const char *metadata,
		       enum policy_action want);

struct domainlist {
	struct domainlist *next;
	const char *dom;
};

int domain_test(struct domainlist *dom, struct dev_policy *pol,
		const char *metadata);
struct domainlist *domain_from_array(struct mdinfo *mdi,
					    const char *metadata);
void domainlist_add_dev(struct domainlist **dom, int devid,
			       const char *metadata);
void domain_free(struct domainlist *dl);
void domain_merge(struct domainlist **domp, struct dev_policy *pol,
		  const char *metadata);
void domain_add(struct domainlist **domp, char *domain);

void policy_save_path(char *id_path, struct map_ent *array);
int policy_check_path(struct mdinfo *disk, struct map_ent *array);

#define	INCR_NO		1
#define	INCR_UNSAFE	2
#define	INCR_ALREADY	4
#define	INCR_YES	8
struct mdinfo *container_choose_spares(struct supertype *st,
				       struct spare_criteria *criteria,
				       struct domainlist *domlist,
				       char *spare_group,
				       const char *metadata, int get_one);
#endif /* _POLICY_H */
