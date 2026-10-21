#ifndef HM_MINIMAP_H
#define HM_MINIMAP_H

#include <stdbool.h>
#include <stdint.h>

/* The map of the place you are in, read from the game's own map buffer
 * (see hm_minimap.c): a small one at the bottom of the screen while you
 * play, switched on and off from the pause menu's Map page. */

/* end_sim_frame hook: keeps the picture fresh and draws the on-screen map. */
void hm_minimap_end_sim_frame(const uint8_t *field, unsigned number);

/* The on-screen minimap (remembered between runs in hm_options.ini). */
bool hm_minimap_hud_on(void);
void hm_minimap_set_hud(bool on);

/* For the pause menu. True when the current place has a map. The picture is
 * `w` x `h` cells, two cells per 8x8 tile in each direction; cell (0,0) is
 * the world pixel (x0, y0) and one cell covers 4x4 world pixels. */
bool hm_minimap_snapshot(int *w, int *h, int *x0, int *y0);
uint32_t hm_minimap_cell(int cx, int cy);
/* World positions (feet) and sprite ids of the people and animals on the
 * map, not you. */
int hm_minimap_people(int *xs, int *ys, uint16_t *ids, int max);
uint16_t hm_minimap_player_sprite(void);
/* 16x16 ARGB face of a sprite id, drawn from the ROM (0 = transparent), or
 * NULL. The girls' names (English, pass through T()), or NULL. */
const uint32_t *hm_minimap_face(uint16_t id);
const char *hm_minimap_name(uint16_t id);
/* The name of the place, in English (pass through T()). */
const char *hm_minimap_place(void);

#endif
