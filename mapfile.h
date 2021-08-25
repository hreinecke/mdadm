#ifndef _MAPFILE_H
#define _MAPFILE_H

/* MAP_DIR should be somewhere that persists across the pivotroot
 * from early boot to late boot.
 * /run  seems to have emerged as the best standard.
 */
#ifndef MAP_DIR
#define MAP_DIR "/run/mdadm"
#endif /* MAP_DIR */
/* MAP_FILE is what we name the map file we put in MAP_DIR, in case you
 * want something other than the default of "map"
 */
#ifndef MAP_FILE
#define MAP_FILE "map"
#endif /* MAP_FILE */

struct map_ent {
	struct map_ent *next;
	char	devnm[32];
	char	metadata[20];
	int	uuid[4];
	int	bad;
	char	*path;
};
int map_update(struct map_ent **mpp, char *devnm, char *metadata,
	       int uuid[4], char *path);
void map_remove(struct map_ent **map, char *devnm);
struct map_ent *map_by_uuid(struct map_ent **map, int uuid[4]);
struct map_ent *map_by_devnm(struct map_ent **map, char *devnm);
void map_free(struct map_ent *map);
struct map_ent *map_by_name(struct map_ent **map, char *name);
void map_read(struct map_ent **melp);
int map_write(struct map_ent *mel);
void map_delete(struct map_ent **mapp, char *devnm);
void map_add(struct map_ent **melp,
	     char *devnm, char *metadata, int uuid[4], char *path);
int map_lock(struct map_ent **melp);
void map_unlock(struct map_ent **melp);
void map_fork(void);

#endif /* _MAPFILE_H */
