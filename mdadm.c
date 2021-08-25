/*
 * mdadm - manage Linux "md" devices aka RAID arrays.
 *
 * Copyright (C) 2001-2013 Neil Brown <neilb@suse.de>
 *
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    Author: Neil Brown
 *    Email: <neilb@suse.de>
 *
 *    Additions for bitmap and write-behind RAID options, Copyright (C) 2003-2004,
 *    Paul Clements, SteelEye Technology, Inc.
 */

#include "mdadm.h"
#include "mdadm_exec.h"
#include "debug.h"
#include "mdstat.h"
#include "sysfs.h"
#include <ctype.h>

#define SYSLOG_FACILITY LOG_DAEMON

static int misc_list(struct mddev_dev *devlist,
		     struct mddev_ident *ident,
		     char *dump_directory,
		     struct supertype *ss, struct context *c);
static const char mdadm_name[] = "mdadm";

mapping_t modes[] = {
	{ "assemble", ASSEMBLE},
	{ "build", BUILD},
	{ "create", CREATE},
	{ "manage", MANAGE},
	{ "misc", MISC},
	{ "monitor", MONITOR},
	{ "grow", GROW},
	{ "incremental", INCREMENTAL},
	{ "auto-detect", AUTODETECT},
	{ NULL, UnSet }
};

char *get_mode_name(int num)
{
	mapping_t *map = modes;
	while (map->name) {
		if (map->num == num)
			return map->name;
		map++;
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	int mode = 0;
	int opt;
	int option_index;
	int rv;

	unsigned long long array_size = 0;
	struct mddev_ident ident;
	char *configfile = NULL;
	int devmode = 0;
	int bitmap_fd = -1;
	struct mddev_dev *devlist = NULL;
	struct mddev_dev **devlistend = & devlist;
	struct mddev_dev *dv;
	int devs_found = 0;
	int grow_continue = 0;
	/* autof indicates whether and how to create device node.
	 * bottom 3 bits are style.  Rest (when shifted) are number of parts
	 * 0  - unset
	 * 1  - don't create (no)
	 * 2  - if is_standard, then create (yes)
	 * 3  - create as 'md' - reject is_standard mdp (md)
	 * 4  - create as 'mdp' - reject is_standard md (mdp)
	 * 5  - default to md if not is_standard (md in config file)
	 * 6  - default to mdp if not is_standard (part, or mdp in config file)
	 */
	struct context c = {
		.require_homehost = 1,
	};
	struct shape s = {
		.journaldisks	= 0,
		.level		= UnSet,
		.layout		= UnSet,
		.bitmap_chunk	= UnSet,
		.consistency_policy	= CONSISTENCY_POLICY_UNKNOWN,
		.data_offset = INVALID_SECTORS,
	};

	char *mailaddr = NULL;
	char *program = NULL;
	int increments = 20;
	int daemonise = 0;
	char *pidfile = NULL;
	int oneshot = 0;
	int spare_sharing = 1;
	struct supertype *ss = NULL;
	enum flag_mode writemostly = FlagDefault;
	enum flag_mode failfast = FlagDefault;
	char *shortopt = short_options;
	int dosyslog = 0;
	int rebuild_map = 0;
	char *remove_path = NULL;
	char *udev_filename = NULL;
	char *dump_directory = NULL;

	int print_help = 0;
	FILE *outf;

	int mdfd = -1;
	int locked = 0;

	mdlib_set_name(mdadm_name);

	srandom(time(0) ^ getpid());

	ident.uuid_set = 0;
	ident.level = UnSet;
	ident.raid_disks = UnSet;
	ident.super_minor = UnSet;
	ident.devices = 0;
	ident.spare_group = NULL;
	ident.autof = 0;
	ident.st = NULL;
	ident.bitmap_fd = -1;
	ident.bitmap_file = NULL;
	ident.name[0] = 0;
	ident.container = NULL;
	ident.member = NULL;

	if (get_linux_version() < 2006015) {
		pr_err("This version of mdadm does not support kernels older than 2.6.15\n");
		exit(1);
	}

	while ((option_index = -1),
	       (opt = getopt_long(argc, argv, shortopt, long_options,
				  &option_index)) != -1) {
		int newmode = mode;
		/* firstly, some mode-independent options */
		switch(opt) {
		case HelpOptions:
			print_help = 2;
			continue;
		case 'h':
			print_help = 1;
			continue;

		case 'V':
			fputs(mdlib_get_version(), stderr);
			exit(0);

		case 'v': c.verbose++;
			continue;

		case 'q': c.verbose--;
			continue;

		case 'b':
			if (mode == ASSEMBLE || mode == BUILD ||
			    mode == CREATE || mode == GROW ||
			    mode == INCREMENTAL || mode == MANAGE)
				break; /* b means bitmap */
		case Brief:
			c.brief = 1;
			continue;

		case NoDevices:
			c.no_devices = 1;
			continue;

		case 'Y': c.export++;
			continue;

		case HomeHost:
			if (strcasecmp(optarg, "<ignore>") == 0)
				c.require_homehost = 0;
			else
				c.homehost = optarg;
			continue;

		case OffRootOpt:
			/* Silently ignore old option */
			continue;

		case Prefer:
			if (c.prefer)
				free(c.prefer);
			if (asprintf(&c.prefer, "/%s/", optarg) <= 0)
				c.prefer = NULL;
			continue;

		case ':':
		case '?':
			fputs(Usage, stderr);
			exit(2);
		}
		/* second, figure out the mode.
		 * Some options force the mode.  Others
		 * set the mode if it isn't already
		 */

		switch(opt) {
		case ManageOpt:
			newmode = MANAGE;
			shortopt = short_bitmap_options;
			break;
		case 'a':
		case Add:
		case AddSpare:
		case AddJournal:
		case 'r':
		case Remove:
		case Replace:
		case With:
		case 'f':
		case Fail:
		case ReAdd: /* re-add */
		case ClusterConfirm:
			if (!mode) {
				newmode = MANAGE;
				shortopt = short_bitmap_options;
			}
			break;

		case 'A': newmode = ASSEMBLE;
			shortopt = short_bitmap_auto_options;
			break;
		case 'B': newmode = BUILD;
			shortopt = short_bitmap_auto_options;
			break;
		case 'C': newmode = CREATE;
			shortopt = short_bitmap_auto_options;
			break;
		case 'F': newmode = MONITOR;
			shortopt = short_monitor_options;
			break;
		case 'G': newmode = GROW;
			shortopt = short_bitmap_options;
			break;
		case 'I': newmode = INCREMENTAL;
			shortopt = short_bitmap_auto_options;
			break;
		case AutoDetect:
			newmode = AUTODETECT;
			break;

		case MiscOpt:
		case 'D':
		case 'E':
		case 'X':
		case 'Q':
		case ExamineBB:
		case Dump:
		case Restore:
		case Action:
			newmode = MISC;
			break;

		case 'R':
		case 'S':
		case 'o':
		case 'w':
		case 'W':
		case WaitOpt:
		case Waitclean:
		case DetailPlatform:
		case KillSubarray:
		case UpdateSubarray:
		case UdevRules:
		case KillOpt:
			if (!mode)
				newmode = MISC;
			break;

		case NoSharing:
			newmode = MONITOR;
			break;
		}
		if (mode && newmode == mode) {
			/* everybody happy ! */
		} else if (mode && newmode != mode) {
			/* not allowed.. */
			pr_err("");
			if (option_index >= 0)
				fprintf(stderr, "--%s", long_options[option_index].name);
			else
				fprintf(stderr, "-%c", opt);
			fprintf(stderr, " would set mdadm mode to \"%s\", but it is already set to \"%s\".\n",
				get_mode_name(newmode),
				get_mode_name(mode));
			exit(2);
		} else if (!mode && newmode) {
			mode = newmode;
			if (mode == MISC && devs_found) {
				pr_err("No action given for %s in --misc mode\n",
					devlist->devname);
				cont_err("Action options must come before device names\n");
				exit(2);
			}
		} else {
			/* special case of -c --help */
			if ((opt == 'c' || opt == ConfigFile) &&
			    (strncmp(optarg, "--h", 3) == 0 ||
			     strncmp(optarg, "-h", 2) == 0)) {
				fputs(Help_config, stdout);
				exit(0);
			}

			/* If first option is a device, don't force the mode yet */
			if (opt == 1) {
				if (devs_found == 0) {
					dv = malloc(sizeof(*dv));
					dv->devname = optarg;
					dv->disposition = devmode;
					dv->writemostly = writemostly;
					dv->failfast = failfast;
					dv->used = 0;
					dv->next = NULL;
					*devlistend = dv;
					devlistend = &dv->next;

					devs_found++;
					continue;
				}
				/* No mode yet, and this is the second device ... */
				pr_err("An option must be given to set the mode before a second device\n"
					"       (%s) is listed\n", optarg);
				exit(2);
			}
			if (option_index >= 0)
				pr_err("--%s", long_options[option_index].name);
			else
				pr_err("-%c", opt);
			fprintf(stderr, " does not set the mode, and so cannot be the first option.\n");
			exit(2);
		}

		/* if we just set the mode, then done */
		switch(opt) {
		case ManageOpt:
		case MiscOpt:
		case 'A':
		case 'B':
		case 'C':
		case 'F':
		case 'G':
		case 'I':
		case AutoDetect:
			continue;
		}
		if (opt == 1) {
			/* an undecorated option - must be a device name.
			 */

			if (devs_found > 0 && devmode == DetailPlatform) {
				pr_err("controller may only be specified once. %s ignored\n",
						optarg);
				continue;
			}

			if (devs_found > 0 && mode == MANAGE && !devmode) {
				pr_err("Must give one of -a/-r/-f for subsequent devices at %s\n", optarg);
				exit(2);
			}
			if (devs_found > 0 && mode == GROW && !devmode) {
				pr_err("Must give -a/--add for devices to add: %s\n", optarg);
				exit(2);
			}
			dv = malloc(sizeof(*dv));
			dv->devname = optarg;
			dv->disposition = devmode;
			dv->writemostly = writemostly;
			dv->failfast = failfast;
			dv->used = 0;
			dv->next = NULL;
			*devlistend = dv;
			devlistend = &dv->next;

			devs_found++;
			continue;
		}

		/* We've got a mode, and opt is now something else which
		 * could depend on the mode */
#define O(a,b) ((a<<16)|b)
		switch (O(mode,opt)) {
		case O(GROW,'c'):
		case O(GROW,ChunkSize):
		case O(CREATE,'c'):
		case O(CREATE,ChunkSize):
		case O(BUILD,'c'): /* chunk or rounding */
		case O(BUILD,ChunkSize): /* chunk or rounding */
			if (s.chunk) {
				pr_err("chunk/rounding may only be specified once. Second value is %s.\n", optarg);
				exit(2);
			}
			s.chunk = mdadm_parse_size(optarg);
			if (s.chunk == INVALID_SECTORS ||
			    s.chunk < 8 || (s.chunk&1)) {
				pr_err("invalid chunk/rounding value: %s\n",
					optarg);
				exit(2);
			}
			/* Convert sectors to K */
			s.chunk /= 2;
			continue;

		case O(INCREMENTAL, 'e'):
		case O(CREATE,'e'):
		case O(ASSEMBLE,'e'):
		case O(MISC,'e'): /* set metadata (superblock) information */
			if (ss) {
				pr_err("metadata information already given\n");
				exit(2);
			}
			ss = mdadm_lookup_supertype(optarg);
			if (!ss) {
				pr_err("unrecognised metadata identifier: %s\n", optarg);
				exit(2);
			}
			continue;

		case O(MANAGE,'W'):
		case O(MANAGE,WriteMostly):
		case O(BUILD,'W'):
		case O(BUILD,WriteMostly):
		case O(CREATE,'W'):
		case O(CREATE,WriteMostly):
			/* set write-mostly for following devices */
			writemostly = FlagSet;
			continue;

		case O(MANAGE,'w'):
			/* clear write-mostly for following devices */
			writemostly = FlagClear;
			continue;

		case O(MANAGE,FailFast):
		case O(CREATE,FailFast):
			failfast = FlagSet;
			continue;
		case O(MANAGE,NoFailFast):
			failfast = FlagClear;
			continue;

		case O(GROW,'z'):
		case O(CREATE,'z'):
		case O(BUILD,'z'): /* size */
			if (s.size > 0) {
				pr_err("size may only be specified once. Second value is %s.\n", optarg);
				exit(2);
			}
			if (strcmp(optarg, "max") == 0)
				s.size = MAX_SIZE;
			else {
				s.size = mdadm_parse_size(optarg);
				if (s.size == INVALID_SECTORS || s.size < 8) {
					pr_err("invalid size: %s\n", optarg);
					exit(2);
				}
				/* convert sectors to K */
				s.size /= 2;
			}
			continue;

		case O(GROW,'Z'): /* array size */
			if (array_size > 0) {
				pr_err("array-size may only be specified once. Second value is %s.\n", optarg);
				exit(2);
			}
			if (strcmp(optarg, "max") == 0)
				array_size = MAX_SIZE;
			else {
				array_size = mdadm_parse_size(optarg);
				if (array_size == 0 ||
				    array_size == INVALID_SECTORS) {
					pr_err("invalid array size: %s\n",
						optarg);
					exit(2);
				}
			}
			continue;

		case O(CREATE,DataOffset):
		case O(GROW,DataOffset):
			if (s.data_offset != INVALID_SECTORS) {
				pr_err("data-offset may only be specified one. Second value is %s.\n", optarg);
				exit(2);
			}
			if (mode == CREATE && strcmp(optarg, "variable") == 0)
				s.data_offset = VARIABLE_OFFSET;
			else
				s.data_offset = mdadm_parse_size(optarg);
			if (s.data_offset == INVALID_SECTORS) {
				pr_err("invalid data-offset: %s\n",
					optarg);
				exit(2);
			}
			continue;

		case O(GROW,'l'):
		case O(CREATE,'l'):
		case O(BUILD,'l'): /* set raid level*/
			if (s.level != UnSet) {
				pr_err("raid level may only be set once.  Second value is %s.\n", optarg);
				exit(2);
			}
			s.level = mdadm_personality_num(optarg);
			if (s.level == UnSet) {
				pr_err("invalid raid level: %s\n",
					optarg);
				exit(2);
			}
			if (s.level != 0 && s.level != LEVEL_LINEAR &&
			    s.level != 1 && s.level != LEVEL_MULTIPATH &&
			    s.level != LEVEL_FAULTY && s.level != 10 &&
			    mode == BUILD) {
				pr_err("Raid level %s not permitted with --build.\n",
					optarg);
				exit(2);
			}
			if (s.sparedisks > 0 && s.level < 1 && s.level >= -1) {
				pr_err("raid level %s is incompatible with spare-devices setting.\n",
					optarg);
				exit(2);
			}
			ident.level = s.level;
			continue;

		case O(GROW, 'p'): /* new layout */
		case O(GROW, Layout):
			if (s.layout_str) {
				pr_err("layout may only be sent once.  Second value was %s\n", optarg);
				exit(2);
			}
			s.layout_str = optarg;
			/* 'Grow' will parse the value */
			continue;

		case O(CREATE,'p'): /* raid5 layout */
		case O(CREATE,Layout):
		case O(BUILD,'p'): /* faulty layout */
		case O(BUILD,Layout):
			if (s.layout != UnSet) {
				pr_err("layout may only be sent once.  Second value was %s\n", optarg);
				exit(2);
			}
			s.layout = mdadm_get_layout(s.level, optarg);
			if (s.layout < -1)
				exit(2);
			continue;

		case O(CREATE,AssumeClean):
		case O(BUILD,AssumeClean): /* assume clean */
		case O(GROW,AssumeClean):
			s.assume_clean = 1;
			continue;

		case O(GROW,'n'):
		case O(CREATE,'n'):
		case O(BUILD,'n'): /* number of raid disks */
			if (s.raiddisks) {
				pr_err("raid-devices set twice: %d and %s\n",
					s.raiddisks, optarg);
				exit(2);
			}
			s.raiddisks = mdadm_parse_num(optarg);
			if (s.raiddisks <= 0) {
				pr_err("invalid number of raid devices: %s\n",
					optarg);
				exit(2);
			}
			ident.raid_disks = s.raiddisks;
			continue;
		case O(ASSEMBLE, Nodes):
		case O(GROW, Nodes):
		case O(CREATE, Nodes):
			c.nodes = mdadm_parse_num(optarg);
			if (c.nodes < 2) {
				pr_err("clustered array needs two nodes at least: %s\n",
					optarg);
				exit(2);
			}
			continue;
		case O(CREATE, ClusterName):
		case O(ASSEMBLE, ClusterName):
			c.homecluster = optarg;
			if (strlen(c.homecluster) > 64) {
				pr_err("Cluster name too big.\n");
				exit(2);
			}
			continue;
		case O(CREATE,'x'): /* number of spare (eXtra) disks */
			if (s.sparedisks) {
				pr_err("spare-devices set twice: %d and %s\n",
				       s.sparedisks, optarg);
				exit(2);
			}
			if (s.level != UnSet && s.level <= 0 && s.level >= -1) {
				pr_err("spare-devices setting is incompatible with raid level %d\n",
					s.level);
				exit(2);
			}
			s.sparedisks = mdadm_parse_num(optarg);
			if (s.sparedisks < 0) {
				pr_err("invalid number of spare-devices: %s\n",
					optarg);
				exit(2);
			}
			continue;

		case O(CREATE,'a'):
		case O(CREATE,Auto):
		case O(BUILD,'a'):
		case O(BUILD,Auto):
		case O(INCREMENTAL,'a'):
		case O(INCREMENTAL,Auto):
		case O(ASSEMBLE,'a'):
		case O(ASSEMBLE,Auto): /* auto-creation of device node */
			c.autof = mdadm_parse_auto(optarg, "--auto flag", 0);
			continue;
		case O(BUILD,'f'): /* force honouring '-n 1' */
		case O(BUILD,Force): /* force honouring '-n 1' */
		case O(GROW,'f'): /* ditto */
		case O(GROW,Force): /* ditto */
		case O(CREATE,'f'): /* force honouring of device list */
		case O(CREATE,Force): /* force honouring of device list */
		case O(ASSEMBLE,'f'): /* force assembly */
		case O(ASSEMBLE,Force): /* force assembly */
		case O(MISC,'f'): /* force zero */
		case O(MISC,Force): /* force zero */
		case O(MANAGE,Force): /* add device which is too large */
			c.force = 1;
			continue;
			/* now for the Assemble options */
		case O(ASSEMBLE, FreezeReshape):   /* Freeze reshape during
						    * initrd phase */
		case O(INCREMENTAL, FreezeReshape):
			c.freeze_reshape = 1;
			continue;
		case O(CREATE,'u'): /* uuid of array */
		case O(ASSEMBLE,'u'): /* uuid of array */
			if (ident.uuid_set) {
				pr_err("uuid cannot be set twice.  Second value %s.\n", optarg);
				exit(2);
			}
			if (mdadm_parse_uuid(optarg, ident.uuid))
				ident.uuid_set = 1;
			else {
				pr_err("Bad uuid: %s\n", optarg);
				exit(2);
			}
			continue;

		case O(CREATE,'N'):
		case O(ASSEMBLE,'N'):
		case O(MISC,'N'):
			if (ident.name[0]) {
				pr_err("name cannot be set twice.   Second value %s.\n", optarg);
				exit(2);
			}
			if (mode == MISC && !c.subarray) {
				pr_err("-N/--name only valid with --update-subarray in misc mode\n");
				exit(2);
			}
			if (strlen(optarg) > 32) {
				pr_err("name '%s' is too long, 32 chars max.\n",
					optarg);
				exit(2);
			}
			strcpy(ident.name, optarg);
			continue;

		case O(ASSEMBLE,'m'): /* super-minor for array */
		case O(ASSEMBLE,SuperMinor):
			if (ident.super_minor != UnSet) {
				pr_err("super-minor cannot be set twice.  Second value: %s.\n", optarg);
				exit(2);
			}
			if (strcmp(optarg, "dev") == 0)
				ident.super_minor = -2;
			else {
				ident.super_minor = mdadm_parse_num(optarg);
				if (ident.super_minor < 0) {
					pr_err("Bad super-minor number: %s.\n", optarg);
					exit(2);
				}
			}
			continue;

		case O(ASSEMBLE,'o'):
		case O(MANAGE,'o'):
		case O(CREATE,'o'):
			c.readonly = 1;
			continue;

		case O(ASSEMBLE,'U'): /* update the superblock */
		case O(MISC,'U'):
			if (c.update) {
				pr_err("Can only update one aspect of superblock, both %s and %s given.\n",
					c.update, optarg);
				exit(2);
			}
			if (mode == MISC && !c.subarray) {
				pr_err("Only subarrays can be updated in misc mode\n");
				exit(2);
			}
			c.update = optarg;
			if (strcmp(c.update, "sparc2.2") == 0)
				continue;
			if (strcmp(c.update, "super-minor") == 0)
				continue;
			if (strcmp(c.update, "summaries") == 0)
				continue;
			if (strcmp(c.update, "resync") == 0)
				continue;
			if (strcmp(c.update, "uuid") == 0)
				continue;
			if (strcmp(c.update, "name") == 0)
				continue;
			if (strcmp(c.update, "homehost") == 0)
				continue;
			if (strcmp(c.update, "home-cluster") == 0)
				continue;
			if (strcmp(c.update, "nodes") == 0)
				continue;
			if (strcmp(c.update, "devicesize") == 0)
				continue;
			if (strcmp(c.update, "bitmap") == 0)
				continue;
			if (strcmp(c.update, "no-bitmap") == 0)
				continue;
			if (strcmp(c.update, "bbl") == 0)
				continue;
			if (strcmp(c.update, "no-bbl") == 0)
				continue;
			if (strcmp(c.update, "force-no-bbl") == 0)
				continue;
			if (strcmp(c.update, "ppl") == 0)
				continue;
			if (strcmp(c.update, "no-ppl") == 0)
				continue;
			if (strcmp(c.update, "metadata") == 0)
				continue;
			if (strcmp(c.update, "revert-reshape") == 0)
				continue;
			if (strcmp(c.update, "layout-original") == 0 ||
			    strcmp(c.update, "layout-alternate") == 0 ||
			    strcmp(c.update, "layout-unspecified") == 0)
				continue;
			if (strcmp(c.update, "byteorder") == 0) {
				if (ss) {
					pr_err("must not set metadata type with --update=byteorder.\n");
					exit(2);
				}
				ss = mdadm_lookup_supertype("0.swap");
				if (!ss) {
					pr_err("INTERNAL ERROR cannot find 0.swap\n");
					exit(2);
				}

				continue;
			}
			if (strcmp(c.update,"?") == 0 ||
			    strcmp(c.update, "help") == 0) {
				outf = stdout;
				fprintf(outf, "%s: ", mdlib_get_name());
			} else {
				outf = stderr;
				fprintf(outf,
					"%s: '--update=%s' is invalid.  ",
					mdlib_get_name(), c.update);
			}
			fprintf(outf, "Valid --update options are:\n"
		"     'sparc2.2', 'super-minor', 'uuid', 'name', 'nodes', 'resync',\n"
		"     'summaries', 'homehost', 'home-cluster', 'byteorder', 'devicesize',\n"
		"     'bitmap', 'no-bitmap', 'metadata', 'revert-reshape'\n"
		"     'bbl', 'no-bbl', 'force-no-bbl', 'ppl', 'no-ppl'\n"
		"     'layout-original', 'layout-alternate', 'layout-unspecified'\n"
				);
			exit(outf == stdout ? 0 : 2);

		case O(MANAGE,'U'):
			/* update=devicesize is allowed with --re-add */
			if (devmode != 'A') {
				pr_err("--update in Manage mode only allowed with --re-add.\n");
				exit(1);
			}
			if (c.update) {
				pr_err("Can only update one aspect of superblock, both %s and %s given.\n",
					c.update, optarg);
				exit(2);
			}
			c.update = optarg;
			if (strcmp(c.update, "devicesize") != 0 &&
			    strcmp(c.update, "bbl") != 0 &&
			    strcmp(c.update, "force-no-bbl") != 0 &&
			    strcmp(c.update, "no-bbl") != 0) {
				pr_err("only 'devicesize', 'bbl', 'no-bbl', and 'force-no-bbl' can be updated with --re-add\n");
				exit(2);
			}
			continue;

		case O(INCREMENTAL,NoDegraded):
			pr_err("--no-degraded is deprecated in Incremental mode\n");
		case O(ASSEMBLE,NoDegraded): /* --no-degraded */
			c.runstop = -1; /* --stop isn't allowed for --assemble,
					 * so we overload slightly */
			continue;

		case O(ASSEMBLE,'c'):
		case O(ASSEMBLE,ConfigFile):
		case O(INCREMENTAL, 'c'):
		case O(INCREMENTAL, ConfigFile):
		case O(MISC, 'c'):
		case O(MISC, ConfigFile):
		case O(MONITOR,'c'):
		case O(MONITOR,ConfigFile):
		case O(CREATE,ConfigFile):
			if (configfile) {
				pr_err("configfile cannot be set twice.  Second value is %s.\n", optarg);
				exit(2);
			}
			configfile = optarg;
			mdlib_set_conffile(configfile);
			/* FIXME possibly check that config file exists.  Even parse it */
			continue;
		case O(ASSEMBLE,'s'): /* scan */
		case O(MISC,'s'):
		case O(MONITOR,'s'):
		case O(INCREMENTAL,'s'):
			c.scan = 1;
			continue;

		case O(MONITOR,'m'): /* mail address */
		case O(MONITOR,EMail):
			if (mailaddr)
				pr_err("only specify one mailaddress. %s ignored.\n",
					optarg);
			else
				mailaddr = optarg;
			continue;

		case O(MONITOR,'p'): /* alert program */
		case O(MONITOR,ProgramOpt): /* alert program */
			if (program)
				pr_err("only specify one alter program. %s ignored.\n",
					optarg);
			else
				program = optarg;
			continue;

		case O(MONITOR,'r'): /* rebuild increments */
		case O(MONITOR,Increment):
			increments = mdadm_parse_num(optarg);
			if (increments > 99 || increments < 1) {
				pr_err("please specify positive integer between 1 and 99 as rebuild increments.\n");
				exit(2);
			}
			continue;

		case O(MONITOR,'d'): /* delay in seconds */
		case O(GROW, 'd'):
		case O(BUILD,'d'): /* delay for bitmap updates */
		case O(CREATE,'d'):
			if (c.delay)
				pr_err("only specify delay once. %s ignored.\n", optarg);
			else {
				c.delay = mdadm_parse_num(optarg);
				if (c.delay < 1) {
					pr_err("invalid delay: %s\n",
						optarg);
					exit(2);
				}
			}
			continue;
		case O(MONITOR,'f'): /* daemonise */
		case O(MONITOR,Fork):
			daemonise = 1;
			continue;
		case O(MONITOR,'i'): /* pid */
			if (pidfile)
				pr_err("only specify one pid file. %s ignored.\n",
					optarg);
			else
				pidfile = optarg;
			continue;
		case O(MONITOR,'1'): /* oneshot */
			oneshot = 1;
			spare_sharing = 0;
			continue;
		case O(MONITOR,'t'): /* test */
			c.test = 1;
			continue;
		case O(MONITOR,'y'): /* log messages to syslog */
			openlog("mdadm", LOG_PID, SYSLOG_FACILITY);
			dosyslog = 1;
			continue;
		case O(MONITOR, NoSharing):
			spare_sharing = 0;
			continue;

			/* now the general management options.  Some are applicable
			 * to other modes. None have arguments.
			 */
		case O(GROW,'a'):
		case O(GROW,Add):
		case O(MANAGE,'a'):
		case O(MANAGE,Add): /* add a drive */
			devmode = 'a';
			continue;
		case O(MANAGE,AddSpare): /* add drive - never re-add */
			devmode = 'S';
			continue;
		case O(MANAGE,AddJournal): /* add journal */
			if (s.journaldisks && (s.level < 4 || s.level > 6)) {
				pr_err("--add-journal is only supported for RAID level 4/5/6.\n");
				exit(2);
			}
			devmode = 'j';
			continue;
		case O(MANAGE,ReAdd):
			devmode = 'A';
			continue;
		case O(MANAGE,'r'): /* remove a drive */
		case O(MANAGE,Remove):
			devmode = 'r';
			continue;
		case O(MANAGE,'f'): /* set faulty */
		case O(MANAGE,Fail):
		case O(INCREMENTAL,'f'):
		case O(INCREMENTAL,Remove):
		case O(INCREMENTAL,Fail): /* r for incremental is taken, use f
					   * even though we will both fail and
					   * remove the device */
			devmode = 'f';
			continue;
		case O(MANAGE, ClusterConfirm):
			devmode = 'c';
			continue;
		case O(MANAGE,Replace):
			/* Mark these devices for replacement */
			devmode = 'R';
			continue;
		case O(MANAGE,With):
			/* These are the replacements to use */
			if (devmode != 'R') {
				pr_err("--with must follow --replace\n");
				exit(2);
			}
			devmode = 'W';
			continue;
		case O(INCREMENTAL,'R'):
		case O(MANAGE,'R'):
		case O(ASSEMBLE,'R'):
		case O(BUILD,'R'):
		case O(CREATE,'R'): /* Run the array */
			if (c.runstop < 0) {
				pr_err("Cannot both Stop and Run an array\n");
				exit(2);
			}
			c.runstop = 1;
			continue;
		case O(MANAGE,'S'):
			if (c.runstop > 0) {
				pr_err("Cannot both Run and Stop an array\n");
				exit(2);
			}
			c.runstop = -1;
			continue;
		case O(MANAGE,'t'):
			c.test = 1;
			continue;

		case O(MISC,'Q'):
		case O(MISC,'D'):
		case O(MISC,'E'):
		case O(MISC,KillOpt):
		case O(MISC,'R'):
		case O(MISC,'S'):
		case O(MISC,'X'):
		case O(MISC, ExamineBB):
		case O(MISC,'o'):
		case O(MISC,'w'):
		case O(MISC,'W'):
		case O(MISC, WaitOpt):
		case O(MISC, Waitclean):
		case O(MISC, DetailPlatform):
		case O(MISC, KillSubarray):
		case O(MISC, UpdateSubarray):
		case O(MISC, Dump):
		case O(MISC, Restore):
		case O(MISC ,Action):
			if (opt == KillSubarray || opt == UpdateSubarray) {
				if (c.subarray) {
					pr_err("subarray can only be specified once\n");
					exit(2);
				}
				c.subarray = optarg;
			}
			if (opt == Action) {
				if (c.action) {
					pr_err("Only one --action can be specified\n");
					exit(2);
				}
				if (strcmp(optarg, "idle") == 0 ||
				    strcmp(optarg, "frozen") == 0 ||
				    strcmp(optarg, "check") == 0 ||
				    strcmp(optarg, "repair") == 0)
					c.action = optarg;
				else {
					pr_err("action must be one of idle, frozen, check, repair\n");
					exit(2);
				}
			}
			if (devmode && devmode != opt &&
			    (devmode == 'E' ||
			     (opt == 'E' && devmode != 'Q'))) {
				pr_err("--examine/-E cannot be given with ");
				if (devmode == 'E') {
					if (option_index >= 0)
						fprintf(stderr, "--%s\n",
							long_options[option_index].name);
					else
						fprintf(stderr, "-%c\n", opt);
				} else if (isalpha(devmode))
					fprintf(stderr, "-%c\n", devmode);
				else
					fprintf(stderr, "previous option\n");
				exit(2);
			}
			devmode = opt;
			if (opt == Dump || opt == Restore) {
				if (dump_directory != NULL) {
					pr_err("dump/restore directory specified twice: %s and %s\n",
					       dump_directory, optarg);
					exit(2);
				}
				dump_directory = optarg;
			}
			continue;
		case O(MISC, UdevRules):
			if (devmode && devmode != opt) {
				pr_err("--udev-rules must be the only option.\n");
			} else {
				if (udev_filename)
					pr_err("only specify one udev rule filename. %s ignored.\n",
						optarg);
				else
					udev_filename = optarg;
			}
			devmode = opt;
			continue;
		case O(MISC,'t'):
			c.test = 1;
			continue;

		case O(MISC, Sparc22):
			if (devmode != 'E') {
				pr_err("--sparc2.2 only allowed with --examine\n");
				exit(2);
			}
			c.SparcAdjust = 1;
			continue;

		case O(ASSEMBLE,'b'): /* here we simply set the bitmap file */
		case O(ASSEMBLE,Bitmap):
			if (!optarg) {
				pr_err("bitmap file needed with -b in --assemble mode\n");
				exit(2);
			}
			if (strcmp(optarg, "internal") == 0 ||
			    strcmp(optarg, "clustered") == 0) {
				pr_err("no need to specify --bitmap when assembling"
					" arrays with internal or clustered bitmap\n");
				continue;
			}
			bitmap_fd = open(optarg, O_RDWR);
			if (!*optarg || bitmap_fd < 0) {
				pr_err("cannot open bitmap file %s: %s\n", optarg, strerror(errno));
				exit(2);
			}
			ident.bitmap_fd = bitmap_fd; /* for Assemble */
			continue;

		case O(ASSEMBLE, BackupFile):
		case O(GROW, BackupFile):
			/* Specify a file into which grow might place a backup,
			 * or from which assemble might recover a backup
			 */
			if (c.backup_file) {
				pr_err("backup file already specified, rejecting %s\n", optarg);
				exit(2);
			}
			c.backup_file = optarg;
			continue;

		case O(GROW, Continue):
			/* Continue interrupted grow
			 */
			grow_continue = 1;
			continue;
		case O(ASSEMBLE, InvalidBackup):
			/* Acknowledge that the backupfile is invalid, but ask
			 * to continue anyway
			 */
			c.invalid_backup = 1;
			continue;

		case O(BUILD,'b'):
		case O(BUILD,Bitmap):
		case O(CREATE,'b'):
		case O(CREATE,Bitmap): /* here we create the bitmap */
		case O(GROW,'b'):
		case O(GROW,Bitmap):
			if (s.bitmap_file) {
				pr_err("bitmap cannot be set twice. Second value: %s.\n", optarg);
				exit(2);
			}
			if (strcmp(optarg, "internal") == 0 ||
			    strcmp(optarg, "none") == 0 ||
			    strchr(optarg, '/') != NULL) {
				s.bitmap_file = optarg;
				continue;
			}
			if (strcmp(optarg, "clustered") == 0) {
				s.bitmap_file = optarg;
				/* Set the default number of cluster nodes
				 * to 4 if not already set by user
				 */
				if (c.nodes < 1)
					c.nodes = 4;
				continue;
			}
			/* probable typo */
			pr_err("bitmap file must contain a '/', or be 'internal', or be 'clustered', or 'none'\n"
				"       not '%s'\n", optarg);
			exit(2);

		case O(GROW,BitmapChunk):
		case O(BUILD,BitmapChunk):
		case O(CREATE,BitmapChunk): /* bitmap chunksize */
			s.bitmap_chunk = mdadm_parse_size(optarg);
			if (s.bitmap_chunk == 0 ||
			    s.bitmap_chunk == INVALID_SECTORS ||
			    s.bitmap_chunk & (s.bitmap_chunk - 1)) {
				pr_err("invalid bitmap chunksize: %s\n",
				       optarg);
				exit(2);
			}
			s.bitmap_chunk = s.bitmap_chunk * 512;
			continue;

		case O(GROW, WriteBehind):
		case O(BUILD, WriteBehind):
		case O(CREATE, WriteBehind):
			s.write_behind = DEFAULT_MAX_WRITE_BEHIND;
			if (optarg) {
				s.write_behind = mdadm_parse_num(optarg);
				if (s.write_behind < 0 ||
				    s.write_behind > 16383) {
					pr_err("Invalid value for maximum outstanding write-behind writes: %s.\n\tMust be between 0 and 16383.\n", optarg);
					exit(2);
				}
			}
			continue;
		case O(INCREMENTAL, 'r'):
		case O(INCREMENTAL, RebuildMapOpt):
			rebuild_map = 1;
			continue;
		case O(INCREMENTAL, IncrementalPath):
			remove_path = optarg;
			continue;
		case O(CREATE, WriteJournal):
			if (s.journaldisks) {
				pr_err("Please specify only one journal device for the array.\n");
				pr_err("Ignoring --write-journal %s...\n", optarg);
				continue;
			}
			dv = malloc(sizeof(*dv));
			dv->devname = optarg;
			dv->disposition = 'j';  /* WriteJournal */
			dv->used = 0;
			dv->next = NULL;
			*devlistend = dv;
			devlistend = &dv->next;
			devs_found++;

			s.journaldisks = 1;
			continue;
		case O(CREATE, 'k'):
		case O(GROW, 'k'):
			s.consistency_policy = mdadm_consistency_policy_num(optarg);
			if (s.consistency_policy < CONSISTENCY_POLICY_RESYNC) {
				pr_err("Invalid consistency policy: %s\n",
				       optarg);
				exit(2);
			}
			continue;
		}
		/* We have now processed all the valid options. Anything else is
		 * an error
		 */
		if (option_index > 0)
			pr_err(":option --%s not valid in %s mode\n",
				long_options[option_index].name,
				get_mode_name(mode));
		else
			pr_err("option -%c not valid in %s mode\n",
				opt, get_mode_name(mode));
		exit(2);

	}

	if (print_help) {
		char *help_text;
		if (print_help == 2)
			help_text = OptionHelp;
		else
			help_text = mode_help[mode];
		if (help_text == NULL)
			help_text = Help;
		fputs(help_text,stdout);
		exit(0);
	}

	if (s.journaldisks) {
		if (s.level < 4 || s.level > 6) {
			pr_err("--write-journal is only supported for RAID level 4/5/6.\n");
			exit(2);
		}
		if (s.consistency_policy != CONSISTENCY_POLICY_UNKNOWN &&
		    s.consistency_policy != CONSISTENCY_POLICY_JOURNAL) {
			pr_err("--write-journal is not supported with consistency policy: %s\n",
			       mdadm_consistency_policy_name(s.consistency_policy));
			exit(2);
		}
	}

	if (mode == CREATE &&
	    s.consistency_policy != CONSISTENCY_POLICY_UNKNOWN) {
		if (s.level <= 0) {
			pr_err("--consistency-policy not meaningful with level %s.\n",
			       mdadm_personality_name(s.level));
			exit(2);
		} else if (s.consistency_policy == CONSISTENCY_POLICY_JOURNAL &&
			   !s.journaldisks) {
			pr_err("--write-journal is required for consistency policy: %s\n",
			       mdadm_consistency_policy_name(s.consistency_policy));
			exit(2);
		} else if (s.consistency_policy == CONSISTENCY_POLICY_PPL &&
			   s.level != 5) {
			pr_err("PPL consistency policy is only supported for RAID level 5.\n");
			exit(2);
		} else if (s.consistency_policy == CONSISTENCY_POLICY_BITMAP &&
			   (!s.bitmap_file ||
			    strcmp(s.bitmap_file, "none") == 0)) {
			pr_err("--bitmap is required for consistency policy: %s\n",
			       mdadm_consistency_policy_name(s.consistency_policy));
			exit(2);
		} else if (s.bitmap_file &&
			   strcmp(s.bitmap_file, "none") != 0 &&
			   s.consistency_policy != CONSISTENCY_POLICY_BITMAP &&
			   s.consistency_policy != CONSISTENCY_POLICY_JOURNAL) {
			pr_err("--bitmap is not compatible with consistency policy: %s\n",
			       mdadm_consistency_policy_name(s.consistency_policy));
			exit(2);
		}
	}

	if (!mode && devs_found) {
		mode = MISC;
		devmode = 'Q';
		if (devlist->disposition == 0)
			devlist->disposition = devmode;
	}
	if (!mode) {
		fputs(Usage, stderr);
		exit(2);
	}

	/* Ok, got the option parsing out of the way
	 * hopefully it's mostly right but there might be some stuff
	 * missing
	 *
	 * That is mostly checked in the per-mode stuff but...
	 *
	 * For @,B,C and A without -s, the first device listed must be
	 * an md device.  We check that here and open it.
	 */

	if (mode == MANAGE || mode == BUILD || mode == CREATE ||
	    mode == GROW || (mode == ASSEMBLE && ! c.scan)) {
		struct stat stb;
		int ret;

		if (devs_found < 1) {
			pr_err("an md device must be given in this mode\n");
			exit(2);
		}
		if ((int)ident.super_minor == -2 && c.autof) {
			pr_err("--super-minor=dev is incompatible with --auto\n");
			exit(2);
		}
		if (mode == MANAGE || mode == GROW) {
			mdfd = mdadm_open_mddev(devlist->devname, 1);
			if (mdfd < 0)
				exit(1);

			ret = fstat(mdfd, &stb);
			if (ret) {
				pr_err("fstat failed on %s.\n", devlist->devname);
				exit(1);
			}
		} else {
			char *bname = basename(devlist->devname);

			if (strlen(bname) > MD_NAME_MAX) {
				pr_err("Name %s is too long.\n", devlist->devname);
				exit(1);
			}

			ret = stat(devlist->devname, &stb);
			if (ident.super_minor == -2 && ret != 0) {
				pr_err("--super-minor=dev given, and listed device %s doesn't exist.\n",
				       devlist->devname);
				exit(1);
			}

			if (!ret && !mdlib_stat_is_md_dev(&stb)) {
				pr_err("device %s exists but is not an md array.\n", devlist->devname);
				exit(1);
			}
		}
		if (ident.super_minor == -2)
			ident.super_minor = minor(stb.st_rdev);
	}

	if (s.raiddisks) {
		if (s.raiddisks == 1 &&  !c.force && s.level != LEVEL_FAULTY) {
			pr_err("'1' is an unusual number of drives for an array, so it is probably\n"
				"     a mistake.  If you really mean it you will need to specify --force before\n"
				"     setting the number of drives.\n");
			exit(2);
		}
	}

	rv = 0;

	mdlib_set_homehost(&c);

	mdlib_set_hooks(); /* set hooks from libs */

	if (c.homecluster == NULL && (c.nodes > 0)) {
		rv = mdlib_set_homecluster(&c);
		if (rv) {
			pr_err("The md can't get cluster name\n");
			exit(1);
		}
	}

	if (c.update && strcmp(c.update, "nodes") == 0 && c.nodes == 0) {
		pr_err("Please specify nodes number with --nodes\n");
		exit(1);
	}

	if (c.backup_file && s.data_offset != INVALID_SECTORS) {
		pr_err("--backup-file and --data-offset are incompatible\n");
		exit(2);
	}

	if ((mode == MISC && devmode == 'E') ||
	    (mode == MONITOR && spare_sharing == 0))
		/* Anyone may try this */;
	else if (geteuid() != 0) {
		pr_err("must be super-user to perform this action\n");
		exit(1);
	}

	ident.autof = c.autof;

	if (c.scan && c.verbose < 2)
		/* --scan implied --brief unless -vv */
		c.brief = 1;

	if (mode == CREATE) {
		if (s.bitmap_file && strcmp(s.bitmap_file, "clustered") == 0) {
			locked = mdlib_cluster_get_dlmlock();
			if (locked != 1)
				exit(1);
		}
	} else if (mode == MANAGE || mode == GROW || mode == INCREMENTAL) {
		if (!mdadm_cluster_is_locked(mdfd, devmode))
			exit(1);
	}

	switch(mode) {
	case MANAGE:
		/* readonly, add/remove, readwrite, runstop */
		if (c.readonly > 0)
			rv = mdadm_manage_ro(devlist->devname, mdfd,
					     c.readonly);
		if (!rv && devs_found>1)
			rv = mdadm_manage_subdevs(devlist->devname, mdfd,
						  devlist->next, c.verbose,
						  c.test, c.update, c.force);
		if (!rv && c.readonly < 0)
			rv = mdadm_manage_ro(devlist->devname, mdfd, c.readonly);
		if (!rv && c.runstop > 0)
			rv = mdadm_manage_run(devlist->devname, mdfd, &c);
		if (!rv && c.runstop < 0)
			rv = mdadm_manage_stop(devlist->devname, mdfd, c.verbose, 0);
		break;
	case ASSEMBLE:
		if (!c.scan && c.runstop == -1) {
			pr_err("--no-degraded not meaningful without a --scan assembly.\n");
			exit(1);
		} else if (devs_found == 1 && ident.uuid_set == 0 &&
			   ident.super_minor == UnSet && ident.name[0] == 0 &&
			   !c.scan) {
			/* Only a device has been given, so get details from config file */
			rv |= mdadm_assemble(ss, devlist->devname, NULL,
					     NULL, &c);
		} else if (!c.scan)
			rv = mdadm_assemble(ss, devlist->devname, &ident,
				      devlist->next, &c);
		else if (devs_found > 0) {
			if (c.update && devs_found > 1) {
				pr_err("can only update a single array at a time\n");
				exit(1);
			}
			if (c.backup_file && devs_found > 1) {
				pr_err("can only assemble a single array when providing a backup file.\n");
				exit(1);
			}
			for (dv = devlist; dv; dv = dv->next)
				rv |= mdadm_assemble(ss, dv->devname, NULL,
					       NULL, &c);
		} else {
			if (c.update) {
				pr_err("--update not meaningful with a --scan assembly.\n");
				exit(1);
			}
			if (c.backup_file) {
				pr_err("--backup_file not meaningful with a --scan assembly.\n");
				exit(1);
			}
			rv = mdadm_scan_assemble(ss, &c, &ident);
		}

		break;
	case BUILD:
		if (c.delay == 0)
			c.delay = DEFAULT_BITMAP_DELAY;
		if (s.write_behind && !s.bitmap_file) {
			pr_err("write-behind mode requires a bitmap.\n");
			rv = 1;
			break;
		}
		if (s.raiddisks == 0) {
			pr_err("no raid-devices specified.\n");
			rv = 1;
			break;
		}

		if (s.bitmap_file) {
			if (strcmp(s.bitmap_file, "internal") == 0 ||
			    strcmp(s.bitmap_file, "clustered") == 0) {
				pr_err("'internal' and 'clustered' bitmaps not supported with --build\n");
				rv |= 1;
				break;
			}
		}
		rv = mdadm_build(devlist->devname, devlist->next, &s, &c);
		break;
	case CREATE:
		if (c.delay == 0)
			c.delay = DEFAULT_BITMAP_DELAY;

		if (c.nodes) {
			if (!s.bitmap_file ||
			    strcmp(s.bitmap_file, "clustered") != 0) {
				pr_err("--nodes argument only compatible with --bitmap=clustered\n");
				rv = 1;
				break;
			}

			if (s.level != 1 && s.level != 10) {
				pr_err("--bitmap=clustered is currently supported with raid1/10 only\n");
				rv = 1;
				break;
			}
		}

		if (s.write_behind && !s.bitmap_file) {
			pr_err("write-behind mode requires a bitmap.\n");
			rv = 1;
			break;
		}
		if (s.raiddisks == 0) {
			pr_err("no raid-devices specified.\n");
			rv = 1;
			break;
		}

		rv = mdadm_create(ss, devlist->devname,
			    ident.name, ident.uuid_set ? ident.uuid : NULL,
			    devs_found - 1, devlist->next, &s, &c);
		break;
	case MISC:
		if (devmode == 'E') {
			if (devlist == NULL && !c.scan) {
				pr_err("No devices to examine\n");
				exit(2);
			}
			rv = mdadm_examine(devlist, &c, ss);
		} else if (devmode == DetailPlatform) {
			rv = mdadm_detail_platform(ss ? ss->ss : NULL , ss ? c.scan : 1,
						   c.verbose, c.export,
						   devlist ? devlist->devname : NULL);
		} else if (devlist == NULL) {
			if (devmode == 'S' && c.scan)
				rv = mdadm_stop_scan(c.verbose);
			else if ((devmode == 'D' || devmode == Waitclean) &&
				 c.scan)
				rv = mdadm_misc_scan(devmode, &c);
			else if (devmode == UdevRules)
				rv = mdadm_write_rules(udev_filename);
			else {
				pr_err("No devices given.\n");
				exit(2);
			}
		} else
			rv = misc_list(devlist, &ident, dump_directory, ss, &c);
		break;
	case MONITOR:
		if (!devlist && !c.scan) {
			pr_err("Cannot monitor: need --scan or at least one device\n");
			rv = 1;
			break;
		}
		if (pidfile && !daemonise) {
			pr_err("Cannot write a pid file when not in daemon mode\n");
			rv = 1;
			break;
		}
		rv = mdadm_monitor(devlist, mailaddr, program,
			     &c, daemonise, oneshot,
			     dosyslog, pidfile, increments,
			     spare_sharing);
		break;

	case GROW:
		if (array_size > 0) {
			if (s.raiddisks || s.level != UnSet) {
				pr_err("cannot change array size in same operation as changing raiddisks or level.\n"
				       "    Change size first, then check that data is still intact.\n");
				rv = 1;
				break;
			}
			rv = mdadm_grow_set_size(mdfd, array_size);
			if (rv)
				break;
		}
		if (devs_found > 1 && s.raiddisks == 0 && s.level == UnSet) {
			/* must be '-a'. */
			if (s.size > 0 || s.chunk ||
			    s.layout_str || s.bitmap_file) {
				pr_err("--add cannot be used with other geometry changes in --grow mode\n");
				rv = 1;
				break;
			}
			for (dv = devlist->next; dv; dv = dv->next) {
				rv = mdadm_grow_add_device(devlist->devname, mdfd,
						     dv->devname);
				if (rv)
					break;
			}
		} else if (s.bitmap_file) {
			if (s.size > 0 || s.raiddisks || s.chunk ||
			    s.layout_str || devs_found > 1) {
				pr_err("--bitmap changes cannot be used with other geometry changes in --grow mode\n");
				rv = 1;
				break;
			}
			if (c.delay == 0)
				c.delay = DEFAULT_BITMAP_DELAY;
			rv = mdadm_grow_add_bitmap(devlist->devname, mdfd,
						   &c, &s);
		} else if (grow_continue)
			rv = mdadm_grow_continue_command(devlist->devname,
						   mdfd, c.backup_file,
						   c.verbose);
		else if (s.size > 0 || s.raiddisks || s.layout_str ||
			 s.chunk != 0 || s.level != UnSet ||
			 s.data_offset != INVALID_SECTORS) {
			rv = mdadm_grow_reshape(devlist->devname, mdfd,
						devlist->next, &c, &s);
		} else if (s.consistency_policy != CONSISTENCY_POLICY_UNKNOWN) {
			rv = mdadm_grow_consistency_policy(devlist->devname, mdfd, &c, &s);
		} else if (array_size == 0)
			pr_err("no changes to --grow\n");
		break;
	case INCREMENTAL:
		if (rebuild_map) {
			mdadm_rebuild_map();
		}
		if (c.scan) {
			rv = 1;
			if (devlist) {
				pr_err("In --incremental mode, a device cannot be given with --scan.\n");
				break;
			}
			if (c.runstop <= 0) {
				pr_err("--incremental --scan meaningless without --run.\n");
				break;
			}
			if (devmode == 'f') {
				pr_err("--incremental --scan --fail not supported.\n");
				break;
			}
			rv = mdadm_incremental_scan(&c, NULL);
		}
		if (!devlist) {
			if (!rebuild_map && !c.scan) {
				pr_err("--incremental requires a device.\n");
				rv = 1;
			}
			break;
		}
		if (devmode == 'f') {
			if (devlist->next) {
				pr_err("'--incremental --fail' can only handle one device.\n");
				rv = 1;
				break;
			}
			rv = mdadm_incremental_remove(devlist->devname,
						      remove_path,
						      c.verbose);
		} else
			rv = mdadm_incremental(devlist, &c, ss);
		break;
	case AUTODETECT:
		mdadm_autodetect();
		break;
	}
	if (locked)
		mdlib_cluster_release_dlmlock();
	if (mdfd > 0)
		close(mdfd);
	exit(rv);
}

static int misc_list(struct mddev_dev *devlist,
		     struct mddev_ident *ident,
		     char *dump_directory,
		     struct supertype *ss, struct context *c)
{
	struct mddev_dev *dv;
	int rv = 0;

	for (dv = devlist; dv; dv = (rv & 16) ? NULL : dv->next) {
		int mdfd = -1;

		switch(dv->disposition) {
		case 'D':
			rv |= mdadm_detail(dv->devname, c);
			continue;
		case KillOpt: /* Zero superblock */
			if (ss)
				rv |= mdadm_kill(dv->devname, ss, c->force, c->verbose,0);
			else {
				int v = c->verbose;
				do {
					rv |= mdadm_kill(dv->devname, NULL, c->force, v, 0);
					v = -1;
				} while (rv == 0);
				rv &= ~4;
			}
			continue;
		case 'Q':
			rv |= mdadm_query(dv->devname);
			continue;
		case 'X':
			rv |= mdadm_examine_bitmap(dv->devname, c->brief, ss);
			continue;
		case ExamineBB:
			rv |= mdadm_examine_badblocks(dv->devname, c->brief, ss);
			continue;
		case 'W':
		case WaitOpt:
			rv |= mdadm_wait(dv->devname);
			continue;
		case Waitclean:
			rv |= mdadm_wait_clean(dv->devname, c->verbose);
			continue;
		case KillSubarray:
			rv |= mdadm_kill_subarray(dv->devname, c->subarray, c->verbose);
			continue;
		case UpdateSubarray:
			if (c->update == NULL) {
				pr_err("-U/--update must be specified with --update-subarray\n");
				rv |= 1;
				continue;
			}
			rv |= mdadm_update_subarray(dv->devname, c->subarray,
					      c->update, ident, c->verbose);
			continue;
		case Dump:
			rv |= mdadm_dump_metadata(dv->devname, dump_directory, c, ss);
			continue;
		case Restore:
			rv |= mdadm_restore_metadata(dv->devname, dump_directory, c, ss,
					       (dv == devlist && dv->next == NULL));
			continue;
		case Action:
			rv |= mdadm_set_action(dv->devname, c->action);
			continue;
		}

		if (dv->devname[0] != '/')
			mdfd = mdadm_open_dev(dv->devname);
		if (dv->devname[0] == '/' || mdfd < 0)
			mdfd = mdadm_open_mddev(dv->devname, 1);

		if (mdfd >= 0) {
			switch(dv->disposition) {
			case 'R':
				c->runstop = 1;
				rv |= mdadm_manage_run(dv->devname, mdfd, c);
				break;
			case 'S':
				if (c->scan) {
					pr_err("--stop not meaningful with both a --scan assembly and a device name.\n");
					rv |= 1;
					break;
				}
				rv |= mdadm_manage_stop(dv->devname, mdfd, c->verbose, 0);
				break;
			case 'o':
				rv |= mdadm_manage_ro(dv->devname, mdfd, 1);
				break;
			case 'w':
				rv |= mdadm_manage_ro(dv->devname, mdfd, -1);
				break;
			}
			close(mdfd);
		} else
			rv |= 1;
	}
	return rv;
}
