#ifndef _RESHAPE_H
#define _RESHAPE_H

extern int child_monitor(int afd, struct mdinfo *sra, struct reshape *reshape,
			 struct supertype *st, unsigned long stripes,
			 int *fds, unsigned long long *offsets,
			 int dests, int *destfd, unsigned long long *destoffsets);
void abort_reshape(struct mdinfo *sra);

#endif /* _RESHAPE_H */
