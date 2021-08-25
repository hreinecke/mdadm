#ifndef _SUPER_H
#define _SUPER_H

/* 'struct reshape' records the intermediate states of
 * a general reshape.
 * The starting geometry is converted to the 'before' geometry
 * by at most an atomic level change. They could be the same.
 * Similarly the 'after' geometry is converted to the final
 * geometry by at most a level change.
 * Note that 'before' and 'after' must have the same level.
 * 'blocks' is the minimum number of sectors for a reshape unit.
 * This will be a multiple of the stripe size in each of the
 * 'before' and 'after' geometries.
 * If 'blocks' is 0, no restriping is necessary.
 * 'min_offset_change' is the minimum change to data_offset to
 * allow the reshape to happen.  It is at least the larger of
 * the old  and new chunk sizes, and typically the same as 'blocks'
 * divided by number of data disks.
 */
struct reshape {
	int level;
	int parity; /* number of parity blocks/devices */
	struct {
		int layout;
		int data_disks;
	} before, after;
	unsigned long long backup_blocks;
	unsigned long long min_offset_change;
	unsigned long long stripes; /* number of old stripes that comprise 'blocks'*/
	unsigned long long new_size; /* New size of array in sectors */
};

/* A superswitch provides entry point to a metadata handler.
 *
 * The superswitch primarily operates on some "metadata" that
 * is accessed via the 'supertype'.
 * This metadata has one of three possible sources.
 * 1/ It is read from a single device.  In this case it may not completely
 *    describe the array or arrays as some information might be on other
 *    devices.
 * 2/ It is read from all devices in a container.  In this case all
 *    information is present.
 * 3/ It is created by ->init_super / ->add_to_super.  In this case it will
 *    be complete once enough ->add_to_super calls have completed.
 *
 * When creating an array inside a container, the metadata will be
 * formed by a combination of 2 and 3.  The metadata or the array is read,
 * then new information is added.
 *
 * The metadata must sometimes have a concept of a 'current' array
 * and a 'current' device.
 * The 'current' array is set by init_super to be the newly created array,
 * or is set by super_by_fd when it finds it is looking at an array inside
 * a container.
 *
 * The 'current' device is either the device that the metadata was read from
 * in case 1, or the last device added by add_to_super in case 3.
 * Case 2 does not identify a 'current' device.
 */
extern struct superswitch {

	/* Used to report details of metadata read from a component
	 * device. ->load_super has been called.
	 */
	void (*examine_super)(struct supertype *st, char *homehost);
	void (*brief_examine_super)(struct supertype *st, int verbose);
	void (*brief_examine_subarrays)(struct supertype *st, int verbose);
	void (*export_examine_super)(struct supertype *st);
	int (*examine_badblocks)(struct supertype *st, int fd, char *devname);
	int (*copy_metadata)(struct supertype *st, int from, int to);

	/* Used to report details of an active array.
	 * ->load_super was possibly given a 'component' string.
	 */
	void (*detail_super)(struct supertype *st, char *homehost,
			     char *subarray);
	void (*brief_detail_super)(struct supertype *st, char *subarray);
	void (*export_detail_super)(struct supertype *st);

	/* Optional: platform hardware / firmware details */
	int (*detail_platform)(int verbose, int enumerate_only, char *controller_path);
	int (*export_detail_platform)(int verbose, char *controller_path);

	/* Used:
	 *   to get uuid to storing in bitmap metadata
	 *   and 'reshape' backup-data metadata
	 *   To see if a device is being re-added to an array it was part of.
	 */
	void (*uuid_from_super)(struct supertype *st, int uuid[4]);

	/* Extract generic details from metadata.  This could be details about
	 * the container, or about an individual array within the container.
	 * The determination is made either by:
	 *   load_super being given a 'component' string.
	 *   validate_geometry determining what to create.
	 * The info includes both array information and device information.
	 * The particular device should be:
	 *   The last device added by add_to_super
	 *   The device the metadata was loaded from by load_super
	 * If 'map' is present, then it is an array raid_disks long
	 * (raid_disk must already be set and correct) and it is filled
	 * with 1 for slots that are thought to be active and 0 for slots which
	 * appear to be failed/missing.
	 * *info is zeroed out before data is added.
	 */
	void (*getinfo_super)(struct supertype *st, struct mdinfo *info, char *map);
	struct mdinfo *(*getinfo_super_disks)(struct supertype *st);
	/* Check if the given metadata is flagged as belonging to "this"
	 * host.  0 for 'no', 1 for 'yes', -1 for "Don't record homehost"
	 */
	int (*match_home)(struct supertype *st, char *homehost);

	/* Make one of several generic modifications to metadata
	 * prior to assembly (or other times).
	 *   sparc2.2  - first bug in early 0.90 metadata
	 *   super-minor - change name of 0.90 metadata
	 *   summaries - 'correct' any redundant data
	 *   resync - mark array as dirty to trigger a resync.
	 *   uuid - set new uuid - only 0.90 or 1.x
	 *   name - change the name of the array (where supported)
	 *   homehost - change which host this array is tied to.
	 *   devicesize - If metadata is at start of device, change recorded
	 *               device size to match actual device size
	 *   byteorder - swap bytes for 0.90 metadata
	 *
	 *   force-one  - mark that device as uptodate, not old or failed.
	 *   force-array - mark array as clean if it would not otherwise
	 *               assemble
	 *   assemble   - not sure how this is different from force-one...
	 *   linear-grow-new - add a new device to a linear array, but don't
	 *                   change the size: so superblock still matches
	 *   linear-grow-update - now change the size of the array.
	 *   writemostly - set the WriteMostly1 bit in the superblock devflags
	 *   readwrite - clear the WriteMostly1 bit in the superblock devflags
	 *   failfast - set the FailFast1 bit in the superblock
	 *   nofailfast - clear the FailFast1 bit
	 *   no-bitmap - clear any record that a bitmap is present.
	 *   bbl       - add a bad-block-log if possible
	 *   no-bbl    - remove any bad-block-log is it is empty.
	 *   force-no-bbl - remove any bad-block-log even if empty.
	 *   revert-reshape - If a reshape is in progress, modify metadata so
	 *                    it will resume going in the opposite direction.
	 */
	int (*update_super)(struct supertype *st, struct mdinfo *info,
			    char *update,
			    char *devname, int verbose,
			    int uuid_set, char *homehost);

	/* Create new metadata for new array as described.  This could
	 * be a new container, or an array in a pre-existing container.
	 * Also used to zero metadata prior to writing it to invalidate old
	 * metadata.
	 */
	int (*init_super)(struct supertype *st, mdu_array_info_t *info,
			  struct shape *s, char *name,
			  char *homehost, int *uuid,
			  unsigned long long data_offset);

	/* update the metadata to include new device, either at create or
	 * when hot-adding a spare.
	 */
	int (*add_to_super)(struct supertype *st, mdu_disk_info_t *dinfo,
			    int fd, char *devname,
			    unsigned long long data_offset);
	/* update the metadata to delete a device,
	 * when hot-removing.
	 */
	int (*remove_from_super)(struct supertype *st, mdu_disk_info_t *dinfo);

	/* Write metadata to one device when fixing problems or adding
	 * a new device.
	 */
	int (*store_super)(struct supertype *st, int fd);

	/*  Write all metadata for this array.
	 */
	int (*write_init_super)(struct supertype *st);
	/* Check if metadata read from one device is compatible with an array,
	 * used when assembling an array, or pseudo-assembling was with
	 * "--examine --brief"
	 * If "st" has not yet been loaded the superblock from, "tst" is
	 * moved in, otherwise the superblock in 'st' is compared with
	 * 'tst'.
	 */
	int (*compare_super)(struct supertype *st, struct supertype *tst,
			     int verbose);
	/* Load metadata from a single device.  If 'devname' is not NULL
	 * print error messages as appropriate */
	int (*load_super)(struct supertype *st, int fd, char *devname);
	/* 'fd' is a 'container' md array - load array metadata from the
	 * whole container.
	 */
	int (*load_container)(struct supertype *st, int fd, char *devname);
	/* If 'arg' is a valid name of this metadata type, allocate and
	 * return a 'supertype' for the particular minor version */
	struct supertype * (*match_metadata_desc)(char *arg);
	/* If a device has the given size, and the data_offset has been
	 * requested - work out how much space is available for data.
	 * This involves adjusting for reserved space (e.g. bitmaps)
	 * and for any rounding.
	 * 'mdadm' only calls this for existing arrays where a possible
	 * spare is being added.  However some super-handlers call it
	 * internally from validate_geometry when creating an array.
	 */
	__u64 (*avail_size)(struct supertype *st, __u64 size,
			    unsigned long long data_offset);
	/*
	 * Return spare criteria for array:
	 * - minimum disk size can be used in array;
	 * - sector size can be used in array.
	 * Return values: 0 - for success and -EINVAL on error.
	 */
	int (*get_spare_criteria)(struct supertype *st,
				  struct spare_criteria *sc);
	/* Find somewhere to put a bitmap - possibly auto-size it - and
	 * update the metadata to record this.  The array may be newly
	 * created, in which case data_size may be updated, or it might
	 * already exist.  Metadata handler can know if init_super
	 * has been called, but not write_init_super.
	 *  0:     Success
	 * -Exxxx: On error
	 */
	int (*add_internal_bitmap)(struct supertype *st, int *chunkp,
				   int delay, int write_behind,
				   unsigned long long size, int may_change, int major);
	/* Perform additional setup required to activate a bitmap.
	 */
	int (*set_bitmap)(struct supertype *st, struct mdinfo *info);
	/* Seek 'fd' to start of write-intent-bitmap.  Must be an
	 * md-native format bitmap
	 */
	int (*locate_bitmap)(struct supertype *st, int fd, int node_num);
	/* if add_internal_bitmap succeeded for existing array, this
	 * writes it out.
	 */
	int (*write_bitmap)(struct supertype *st, int fd, enum bitmap_update update);
	/* Free the superblock and any other allocated data */
	void (*free_super)(struct supertype *st);

	/* validate_geometry is called with an st returned by
	 * match_metadata_desc.
	 * It should check that the geometry described is compatible with
	 * the metadata type.  It will be called repeatedly as devices
	 * added to validate changing size and new devices.  If there are
	 * inter-device dependencies, it should record sufficient details
	 * so these can be validated.
	 * Both 'size' and '*freesize' are in sectors.  chunk is KiB.
	 * Return value is:
	 *  1: everything is OK
	 *  0: not OK for some reason - if 'verbose', then error was reported.
	 * -1: st->sb was NULL, 'subdev' is a member of a container of this
	 *     type, but array is not acceptable for some reason
	 *     message was reported even if verbose is 0.
	 */
	int (*validate_geometry)(struct supertype *st, int level, int layout,
				 int raiddisks,
				 int *chunk, unsigned long long size,
				 unsigned long long data_offset,
				 char *subdev, unsigned long long *freesize,
				 int consistency_policy, int verbose);

	/* Return a linked list of 'mdinfo' structures for all arrays
	 * in the container.  For non-containers, it is like
	 * getinfo_super with an allocated mdinfo.*/
	struct mdinfo *(*container_content)(struct supertype *st, char *subarray);
	/* query the supertype for default geometry */
	void (*default_geometry)(struct supertype *st, int *level, int *layout, int *chunk); /* optional */
	/* Permit subarray's to be deleted from inactive containers */
	int (*kill_subarray)(struct supertype *st,
			     char *subarray_id); /* optional */
	/* Permit subarray's to be modified */
	int (*update_subarray)(struct supertype *st, char *subarray,
			       char *update, struct mddev_ident *ident); /* optional */
	/* Check if reshape is supported for this external format.
	 * st is obtained from super_by_fd() where st->subarray[0] is
	 * initialized to indicate if reshape is being performed at the
	 * container or subarray level
	 */
#define APPLY_METADATA_CHANGES		1
#define ROLLBACK_METADATA_CHANGES	0

	int (*reshape_super)(struct supertype *st,
			     unsigned long long size, int level,
			     int layout, int chunksize, int raid_disks,
			     int delta_disks, char *backup, char *dev,
			     int direction,
			     int verbose); /* optional */
	int (*manage_reshape)( /* optional */
		int afd, struct mdinfo *sra, struct reshape *reshape,
		struct supertype *st, unsigned long blocks,
		int *fds, unsigned long long *offsets,
		int dests, int *destfd, unsigned long long *destoffsets);

/* for mdmon */
	int (*open_new)(struct supertype *c, struct active_array *a,
			int inst);

	/* Tell the metadata handler the current state of the array.
	 * This covers whether it is known to be consistent (no pending writes)
	 * and how far along a resync is known to have progressed
	 * (in a->resync_start).
	 * resync status is really irrelevant if the array is not consistent,
	 * but some metadata (DDF!) have a place to record the distinction.
	 * If 'consistent' is '2', then the array can mark it dirty if a
	 * resync/recovery/whatever is required, or leave it clean if not.
	 * Return value is 0 dirty (not consistent) and 1 if clean.
	 * it is only really important if consistent is passed in as '2'.
	 */
	int (*set_array_state)(struct active_array *a, int consistent);

	/* When the state of a device might have changed, we call set_disk to
	 * tell the metadata what the current state is.
	 * Typically this happens on spare->in_sync and (spare|in_sync)->faulty
	 * transitions.
	 * set_disk might be called when the state of the particular disk has
	 * not in fact changed.
	 */
	void (*set_disk)(struct active_array *a, int n, int state);
	void (*sync_metadata)(struct supertype *st);
	void (*process_update)(struct supertype *st,
			       struct metadata_update *update);
	/* Prepare updates allocates extra memory that might be
	 * needed.  If the update cannot be understood,  return 0.
	 */
	int (*prepare_update)(struct supertype *st,
			       struct metadata_update *update);

	/* activate_spare will check if the array is degraded and, if it
	 * is, try to find some spare space in the container.
	 * On success, it add appropriate updates (For process_update) to
	 * to the 'updates' list and returns a list of 'mdinfo' identifying
	 * the device, or devices as there might be multiple missing
	 * devices and multiple spares available.
	 */
	struct mdinfo *(*activate_spare)(struct active_array *a,
					 struct metadata_update **updates);
	/*
	 * Return statically allocated string that represents metadata specific
	 * controller domain of the disk. The domain is used in disk domain
	 * matching functions. Disks belong to the same domain if the they have
	 * the same domain from mdadm.conf and belong the same metadata domain.
	 * Returning NULL or not providing this handler means that metadata
	 * does not distinguish the differences between disks that belong to
	 * different controllers. They are in the domain specified by
	 * configuration file (mdadm.conf).
	 * In case when the metadata has the notion of domains based on disk
	 * it shall return NULL for disks that do not belong to the controller
	 * the supported domains. Such disks will form another domain and won't
	 * be mixed with supported ones.
	 */
	const char *(*get_disk_controller_domain)(const char *path);

	/* for external backup area */
	int (*recover_backup)(struct supertype *st, struct mdinfo *info);

	/* validate container after assemble */
	int (*validate_container)(struct mdinfo *info);

	/* write initial empty PPL on device */
	int (*write_init_ppl)(struct supertype *st, struct mdinfo *info, int fd);

	/* validate ppl before assemble */
	int (*validate_ppl)(struct supertype *st, struct mdinfo *info,
			    struct mdinfo *disk);

	/* records new bad block in metadata */
	int (*record_bad_block)(struct active_array *a, int n,
					unsigned long long sector, int length);

	/* clears bad block from metadata */
	int (*clear_bad_block)(struct active_array *a, int n,
					unsigned long long sector, int length);

	/* get list of bad blocks from metadata */
	struct md_bb *(*get_bad_blocks)(struct active_array *a, int n);

	int swapuuid; /* true if uuid is bigending rather than hostendian */
	int external;
	const char *name; /* canonical metadata name */
} *superlist[];

extern struct superswitch super0, super1;
extern struct superswitch super_imsm, super_ddf;
extern struct superswitch mbr, gpt;

struct supertype *super_by_fd(int fd, char **subarray);
enum guess_types { guess_any, guess_array, guess_partitions };
struct supertype *guess_super_type(int fd, enum guess_types guess_type);

static inline struct supertype *guess_super(int fd) {
	return guess_super_type(fd, guess_any);
}
struct supertype *dup_super(struct supertype *st);
int get_dev_size(int fd, char *dname, unsigned long long *sizep);
int get_dev_sector_size(int fd, char *dname, unsigned int *sectsizep);
int must_be_container(int fd);
int dev_size_from_id(dev_t id, unsigned long long *size);
int dev_sector_size_from_id(dev_t id, unsigned int *size);
void wait_for(char *dev, int fd);

char *fname_from_uuid(struct supertype *st,
		      struct mdinfo *info, char *buf, char sep);

#endif /* _SUPER_H */
