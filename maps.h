#ifndef _MAPS_H
#define _MAPS_H

/* maps.c */
extern char *map_num(mapping_t *map, int num);
extern char *map_num_s(mapping_t *map, int num);
extern struct map_ent *map_by_devnm(struct map_ent **map, char *devnm);
extern void map_free(struct map_ent *map);
extern void map_free(struct map_ent *map);
extern int map_lock(struct map_ent **melp);
extern void map_unlock(struct map_ent **melp);

#endif /* _MAPS_H */
