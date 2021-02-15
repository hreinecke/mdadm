#ifndef _RESHAPE_H
#define _RESHAPE_H

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

extern int child_monitor(int afd, struct mdinfo *sra, struct reshape *reshape,
			 struct supertype *st, unsigned long stripes,
			 int *fds, unsigned long long *offsets,
			 int dests, int *destfd, unsigned long long *destoffsets);
void abort_reshape(struct mdinfo *sra);

#endif /* _RESHAPE_H */
