#ifndef _MAPFILE_H
#define _MAPFILE_H

extern int map_update(struct map_ent **mpp, char *devnm, char *metadata,
		      int uuid[4], char *path);
extern void map_remove(struct map_ent **map, char *devnm);
extern struct map_ent *map_by_uuid(struct map_ent **map, int uuid[4]);
extern void map_read(struct map_ent **melp);
extern int map_write(struct map_ent *mel);
extern void map_delete(struct map_ent **mapp, char *devnm);
extern void map_add(struct map_ent **melp,
		    char *devnm, char *metadata, int uuid[4], char *path);
extern void map_fork(void);
extern char *map_num(mapping_t *map, int num);
extern int map_name(mapping_t *map, char *name);
extern int map_lock(struct map_ent **melp);
extern void map_unlock(struct map_ent **melp);
extern void map_free(struct map_ent *map);
extern struct map_ent *map_by_devnm(struct map_ent **map, char *devnm);
extern struct map_ent *map_by_name(struct map_ent **map, char *name);

#endif /* _MAPFILE_H */
