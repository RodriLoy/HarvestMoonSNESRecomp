/*
 * hm_minimap.c — a map of the place you are in.
 *
 * Where the picture comes from. When the game loads a map ($80:B5xx,
 * Maps_Graphics_Table at $80:AA7C) it unpacks the whole BG1 tilemap of the
 * place into WRAM at $7E:2000, one row of tiles after another, $0181 screens
 * wide and $0182 screens high (32 tiles per screen). The PPU's own 64x64
 * tilemap is only a window that the game refills from there as the camera
 * moves. So the whole place — the farm with your crops, the town, a room — is
 * in memory at all times, and its tile graphics and colours are in VRAM and
 * CGRAM. This file averages each 8x8 tile into 2x2 "cells" (4x4 pixels each)
 * a few rows per frame, so the picture follows the time-of-day palettes and
 * the farm as it changes, and costs well under a millisecond per frame.
 *
 * People. The game's objects ($019C, 40 entries of 36 bytes, $7777 when in
 * use; world X/Y at +8/+10) are the townsfolk, the girls and the animals on
 * the current map. They are drawn as dots; you are the red one ($D6/$D8).
 *
 * On screen. While the option is on, a small map sits at the bottom right of
 * the picture, centred on you, whenever you can move freely (not during text,
 * fades or the pause menu). With the option off nothing here touches the
 * frame: the picture stays identical to the original.
 */
#include <stdio.h>
#include <string.h>

#include "common_rtl.h"
#include "snes/ppu.h"
#include "hm_ram.h"
#include "hm_minimap.h"
#include "hm_pause.h"

enum { MAP_BUF = 0x2000, MAP_BUF_SIZE = 0x8000 };
enum { CELLS_MAX = 256 };                 /* 4 screens x 32 tiles x 2 */
enum { GOBJ_BASE = 0x019C, GOBJ_SIZE = 0x24, GOBJ_N = 40, GOBJ_LIVE = 0x7777 };
enum { ROWS_PER_FRAME = 16 };

static struct {
  bool hud, hud_loaded;
  uint8_t map, sw, sh;          /* map id and size in screens the cells are of */
  int next_row;                 /* next tile row to refresh */
  bool ready;                   /* one full pass done for this map */
  uint32_t cell[CELLS_MAX * CELLS_MAX];
} s;

#define XRGB(r, g, b) (0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

/* ── Settings ────────────────────────────────────────────────────────── */
static const char *kOptionsFile = "hm_options.ini";   /* next to the game */

static void load_settings(void)
{
  if (s.hud_loaded) return;
  s.hud_loaded = true;
  FILE *f = fopen(kOptionsFile, "r");
  if (!f) return;
  char line[128];
  while (fgets(line, sizeof line, f)) {
    int v;
    if (sscanf(line, " minimap = %d", &v) == 1) s.hud = v != 0;
  }
  fclose(f);
}

bool hm_minimap_hud_on(void) { load_settings(); return s.hud; }

void hm_minimap_set_hud(bool on)
{
  load_settings();
  s.hud = on;
  FILE *f = fopen(kOptionsFile, "w");
  if (!f) return;
  fprintf(f, "# Harvest Moon options set from the pause menu\nminimap = %d\n", on ? 1 : 0);
  fclose(f);
}

/* ── The map buffer ──────────────────────────────────────────────────── */
static bool map_present(void)
{
  if (!g_ppu || !hm_diary_loaded(g_ram)) return false;
  if (g_ram[HM_MAP_ID] >= HM_MAP_FIRST_PICTURE) return false;
  int w = g_ram[HM_MAP_SCREENS_W], h = g_ram[HM_MAP_SCREENS_H];
  return w >= 1 && w <= 4 && h >= 1 && h <= 4 && w * h * 0x800 <= MAP_BUF_SIZE;
}

static uint32_t rgb555(uint16_t c)
{
  uint32_t r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
  return XRGB(r | r >> 5, g | g >> 5, b | b >> 5);
}

/* Average the four 4x4 quarters of one BG1 tile entry into cells. */
static void tile_cells(uint16_t e, uint32_t chr, uint32_t out[4])
{
  unsigned tile = e & 0x3ff, pal = (e >> 10) & 7;
  bool hf = e & 0x4000, vf = e & 0x8000;
  uint32_t sum[4][3] = {{0}};
  int n[4] = {0};
  const uint16_t *t = &g_ppu->vram[(chr + tile * 16) & 0x7fff];
  uint16_t back = g_ppu->cgram[0];
  for (int y = 0; y < 8; y++) {
    uint16_t p01 = t[y], p23 = t[8 + y];
    for (int x = 0; x < 8; x++) {
      int b = 7 - x;
      int c = ((p01 >> b) & 1) | (((p01 >> (8 + b)) & 1) << 1) |
              (((p23 >> b) & 1) << 2) | (((p23 >> (8 + b)) & 1) << 3);
      uint16_t col = c ? g_ppu->cgram[(pal * 16 + c) & 0xff] : back;
      int px = hf ? 7 - x : x, py = vf ? 7 - y : y;
      int q = (py >> 2) * 2 + (px >> 2);
      sum[q][0] += col & 31; sum[q][1] += (col >> 5) & 31; sum[q][2] += (col >> 10) & 31;
      n[q]++;
    }
  }
  for (int q = 0; q < 4; q++) {
    uint32_t r = sum[q][0] * 8 / n[q], g = sum[q][1] * 8 / n[q], b = sum[q][2] * 8 / n[q];
    out[q] = XRGB(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
  }
}

static void refresh_rows(int rows)
{
  int tw = s.sw * 32, th = s.sh * 32, stride = s.sw * 64;
  uint32_t chr = PPU_bgTileAdr(g_ppu, 0);
  for (int k = 0; k < rows; k++) {
    int ty = s.next_row;
    for (int tx = 0; tx < tw; tx++) {
      uint16_t e = hm_rd16(g_ram, MAP_BUF + ty * stride + tx * 2);
      uint32_t q[4];
      tile_cells(e, chr, q);
      uint32_t *c = &s.cell[(ty * 2) * CELLS_MAX + tx * 2];
      c[0] = q[0]; c[1] = q[1]; c[CELLS_MAX] = q[2]; c[CELLS_MAX + 1] = q[3];
    }
    if (++s.next_row >= th) { s.next_row = 0; s.ready = true; }
  }
}

/* Keep the cells for the current map; returns false when there is none. */
static bool track_map(int rows)
{
  if (!map_present()) return false;
  uint8_t map = g_ram[HM_MAP_ID], w = g_ram[HM_MAP_SCREENS_W], h = g_ram[HM_MAP_SCREENS_H];
  if (map != s.map || w != s.sw || h != s.sh) {
    s.map = map; s.sw = w; s.sh = h;
    s.next_row = 0; s.ready = false;
  }
  refresh_rows(rows);
  return true;
}

/* The part of the buffer the camera can show, in world pixels. */
static void extents(int *x0, int *y0, int *x1, int *y1)
{
  int wmax = s.sw * 256, hmax = s.sh * 256;
  int cl = hm_rd16(g_ram, HM_CAM_MIN_X), cr = hm_rd16(g_ram, HM_CAM_MAX_X);
  int cu = hm_rd16(g_ram, HM_CAM_MIN_Y), cd = hm_rd16(g_ram, HM_CAM_MAX_Y);
  *x0 = cl; *x1 = cr + 256; *y0 = cu; *y1 = cd + 224;
  if (*x0 < 0 || *x0 >= wmax || *x1 > wmax || *x1 <= *x0) { *x0 = 0; *x1 = wmax; }
  if (*y0 < 0 || *y0 >= hmax || *y1 > hmax || *y1 <= *y0) { *y0 = 0; *y1 = hmax; }
}

/* ── Who is who: faces from the game's own sprites ───────────────────── */
/* Each object's sprite id (+2) picks a frame description in bank $86 (ids
 * below $262, table $86:8080) or $87 ($87:8080); the object keeps a pointer
 * to it at +$10. A description is a count, then 5 bytes per 16x16 piece:
 * graphic G (16b), x, y (signed, from the feet) and flags (low nibble: the
 * palette in the OBJ palette table at $A9:DE00, $20: mirrored). G is turned
 * into a ROM address the way CODE_858CB2 does before its DMA: bank
 * $88 + G/256, $2000 * ((G/64) & 3), plus 32 * $86:8000[G & 63]; the lower
 * half of a 16x16 piece is $200 further on. So the picture of anyone on the
 * map — townsfolk, the girls, animals — can be drawn straight from the ROM,
 * and the face is the 16x16 window just under the top of the head. */
enum { SPRITE_IDS_86 = 0x262, FACE_CACHE = 48 };
static struct { uint16_t id; bool used; uint32_t px[16 * 16]; } s_faces[FACE_CACHE];
static int s_face_next;

static uint32_t rom_lo(uint8_t bank, uint16_t addr) { return (uint32_t)(bank & 0x7f) * 0x8000 + (addr - 0x8000); }
static uint16_t rom16(uint32_t o) { return (uint16_t)(g_rom[o] | g_rom[o + 1] << 8); }
static bool rom_ok(uint32_t o, uint32_t n) { return g_rom && o + n <= 0x200000; }

static bool piece_addr(uint16_t g, uint32_t *out)
{
  uint16_t blk = g >> 6, sub = g & 63;
  uint8_t bank = (uint8_t)(0x88 + (blk >> 2));
  uint32_t t = rom_lo(0x86, 0x8000) + sub * 2;
  if (!rom_ok(t, 2)) return false;
  uint32_t a = rom_lo(bank, (uint16_t)(0x8000 + (blk & 3) * 0x2000 + rom16(t) * 0x20));
  if (!rom_ok(a, 0x240)) return false;
  *out = a;
  return true;
}

static bool sprite_desc(uint16_t id, uint32_t *out)
{
  uint8_t bank = id < SPRITE_IDS_86 ? 0x86 : 0x87;
  uint32_t t = rom_lo(bank, 0x8080) + (uint32_t)(id < SPRITE_IDS_86 ? id : id - SPRITE_IDS_86) * 2;
  if (!rom_ok(t, 2)) return false;
  uint32_t p = rom_lo(bank, rom16(t));
  if (!rom_ok(p, 2)) return false;
  uint32_t d = rom_lo(bank, rom16(p));
  if (!rom_ok(d, 1 + 5 * 12) || g_rom[d] == 0 || g_rom[d] > 12) return false;
  *out = d;
  return true;
}

/* People and animals only: ids below are you and the items you carry,
 * above are crops, eggs and effects. */
static bool is_creature(uint16_t id) { return id >= 0x1A0 && id < 0x340; }

const uint32_t *hm_minimap_face(uint16_t id)
{
  for (int i = 0; i < FACE_CACHE; i++)
    if (s_faces[i].used && s_faces[i].id == id) return s_faces[i].px;
  uint32_t d;
  if (!sprite_desc(id, &d)) return NULL;
  enum { CW = 48, CH = 64, OX = 24, OY = 48 };
  static uint32_t canvas[CW * CH];
  memset(canvas, 0, sizeof canvas);
  int n = g_rom[d], top = 99, left = 0;
  for (int k = 0; k < n; k++) {
    const uint8_t *e = g_rom + d + 1 + k * 5;
    uint16_t g = (uint16_t)(e[0] | e[1] << 8);
    int x = (int8_t)e[2], y = (int8_t)e[3];
    uint8_t f = e[4];
    uint32_t a;
    if (!piece_addr(g, &a)) continue;
    uint32_t pal = 0x14DE00 + (f & 0x0F) * 0x20;
    if (y < top) { top = y; left = x; }
    for (int ty = 0; ty < 2; ty++)
      for (int tx = 0; tx < 2; tx++) {
        const uint8_t *t = g_rom + a + ty * 0x200 + tx * 0x20;
        for (int r = 0; r < 8; r++)
          for (int c = 0; c < 8; c++) {
            int b = 7 - c;
            int ci = ((t[r * 2] >> b) & 1) | (((t[r * 2 + 1] >> b) & 1) << 1) |
                     (((t[16 + r * 2] >> b) & 1) << 2) | (((t[16 + r * 2 + 1] >> b) & 1) << 3);
            if (!ci) continue;
            int px = tx * 8 + c;
            if (f & 0x20) px = 15 - px;
            int cx = OX + x + px, cy = OY + y + ty * 8 + r;
            if ((unsigned)cx < CW && (unsigned)cy < CH)
              canvas[cy * CW + cx] = rgb555(rom16(pal + ci * 2));
          }
      }
  }
  if (top == 99) return NULL;
  int slot = s_face_next++ % FACE_CACHE;
  s_faces[slot].used = true; s_faces[slot].id = id;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      int cx = OX + left + x, cy = OY + top + 6 + y;
      s_faces[slot].px[y * 16 + x] = ((unsigned)cx < CW && (unsigned)cy < CH) ? canvas[cy * CW + cx] : 0;
    }
  return s_faces[slot].px;
}

/* The five girls, by the blocks of sprite ids that show their faces (read
 * off a sheet of every id's face: blue hair, orange ponytail, brown, blonde,
 * pink). */
const char *hm_minimap_name(uint16_t id)
{
  if ((id >= 0x21E && id <= 0x225) || (id >= 0x230 && id <= 0x232)) return "Maria";
  if ((id >= 0x226 && id <= 0x22D) || (id >= 0x235 && id <= 0x237)) return "Ann";
  if (id >= 0x23A && id <= 0x243) return "Ellen";
  if ((id >= 0x244 && id <= 0x24A) || (id >= 0x255 && id <= 0x257)) return "Eve";
  if (id >= 0x24B && id <= 0x252) return "Nina";
  return NULL;
}

int hm_minimap_people(int *xs, int *ys, uint16_t *ids, int max)
{
  int n = 0;
  int px = hm_rd16(g_ram, HM_PLAYER_X), py = hm_rd16(g_ram, HM_PLAYER_Y);
  for (int i = 0; i < GOBJ_N && n < max; i++) {
    uint32_t b = GOBJ_BASE + i * GOBJ_SIZE;
    if (hm_rd16(g_ram, b) != GOBJ_LIVE) continue;
    int x = hm_rd16(g_ram, b + 8), y = hm_rd16(g_ram, b + 10);
    uint16_t id = hm_rd16(g_ram, b + 2);
    if (x == px && y == py) continue;
    if (!is_creature(id)) continue;
    xs[n] = x; ys[n] = y; ids[n] = id; n++;
  }
  return n;
}

/* Your own sprite id: the object standing where you stand. */
uint16_t hm_minimap_player_sprite(void)
{
  int px = hm_rd16(g_ram, HM_PLAYER_X), py = hm_rd16(g_ram, HM_PLAYER_Y);
  for (int i = 0; i < GOBJ_N; i++) {
    uint32_t b = GOBJ_BASE + i * GOBJ_SIZE;
    if (hm_rd16(g_ram, b) == GOBJ_LIVE && hm_rd16(g_ram, b + 8) == px && hm_rd16(g_ram, b + 10) == py)
      return hm_rd16(g_ram, b + 2);
  }
  return 0;
}

/* ── Pause menu access ───────────────────────────────────────────────── */
bool hm_minimap_snapshot(int *w, int *h, int *x0, int *y0)
{
  if (!map_present()) return false;
  track_map(0);
  s.next_row = 0;
  refresh_rows(s.sh * 32);                  /* a full, current picture */
  int x1, y1;
  extents(x0, y0, &x1, &y1);
  *w = (x1 - *x0) / 4; *h = (y1 - *y0) / 4;
  return *w > 0 && *h > 0;
}

uint32_t hm_minimap_cell(int cx, int cy)
{
  if ((unsigned)cx >= CELLS_MAX || (unsigned)cy >= CELLS_MAX) return XRGB(0, 0, 0);
  return s.cell[cy * CELLS_MAX + cx];
}

const char *hm_minimap_place(void)
{
  uint8_t m = g_ram[HM_MAP_ID];
  if (m <= 0x03) return "Your farm";
  if (m <= 0x07) return "Town";
  switch (m) {
    case 0x08: return "Flower Festival";
    case 0x09: return "Harvest Festival";
    case 0x0A: case 0x14: case 0x39: return "Star Night Festival";
    case 0x0B: return "Egg Festival";
    case 0x15: case 0x16: case 0x17: return "Your house";
    case 0x18: case 0x19: return "Mayor's house";
    case 0x1A: return "Maria's room";
    case 0x1B: return "Church";
    case 0x1C: case 0x1D: return "Flower shop";
    case 0x1E: case 0x1F: return "Bar";
    case 0x20: case 0x21: return "Restaurant";
    case 0x22: case 0x23: return "General store";
    case 0x24: return "Animal shop";
    case 0x25: return "Witch's house";
    case 0x26: return "Tool shed";
    case 0x27: return "Barn";
    case 0x28: return "Chicken coop";
    case 0x29: return "Cave";
    case 0x3A: return "New Year";
    default: break;
  }
  if (m >= 0x0C && m <= 0x0F) return "Crossroads";
  if (m >= 0x10 && m <= 0x13) return "Mountain";
  if (m >= 0x31 && m <= 0x34) return "Mountain top";
  return "";
}

/* ── On screen ───────────────────────────────────────────────────────── */
enum { HUD_W = 76, HUD_H = 52, HUD_B = 3, HUD_GAP = 5 };

static inline void put(uint32_t *row, int x, int width, uint32_t c)
{
  if ((unsigned)x < (unsigned)width) row[x] = c;
}

static void draw_hud(void)
{
  uint8_t *buf = g_ppu->renderBuffer;
  if (!buf) return;
  int width = 256 + 2 * g_ppu->extraLeftRight, height = 224;
  if ((int)(g_ppu->renderPitch / 4) < width) return;
  int bx = width - HUD_W - 2 * HUD_B - HUD_GAP, by = height - HUD_H - 2 * HUD_B - HUD_GAP;

  int x0, y0, x1, y1;
  extents(&x0, &y0, &x1, &y1);
  /* One pixel is a whole 8x8 tile outdoors; a room (no wider than the
   * window at 4 world pixels per pixel) is shown twice as big. */
  int u = (x1 - x0) / 4 <= HUD_W ? 4 : 8;
  int tx0 = x0 / u, ty0 = y0 / u, tx1 = x1 / u, ty1 = y1 / u;
  int ptx = hm_rd16(g_ram, HM_PLAYER_X) / u, pty = hm_rd16(g_ram, HM_PLAYER_Y) / u;
  /* the window shown, centred on you and kept inside the map */
  int vx = ptx - HUD_W / 2, vy = pty - HUD_H / 2;
  if (tx1 - tx0 <= HUD_W) vx = tx0 - (HUD_W - (tx1 - tx0)) / 2;
  else { if (vx < tx0) vx = tx0; if (vx + HUD_W > tx1) vx = tx1 - HUD_W; }
  if (ty1 - ty0 <= HUD_H) vy = ty0 - (HUD_H - (ty1 - ty0)) / 2;
  else { if (vy < ty0) vy = ty0; if (vy + HUD_H > ty1) vy = ty1 - HUD_H; }

  static const uint32_t edge = XRGB(0x3A, 0x1E, 0x0C), wood = XRGB(0xA4, 0x62, 0x2E),
                        light = XRGB(0xD0, 0x8F, 0x4C), empty = XRGB(0x26, 0x1A, 0x10);
  for (int y = 0; y < HUD_H + 2 * HUD_B; y++) {
    uint32_t *row = (uint32_t *)(buf + (size_t)(by + y) * g_ppu->renderPitch);
    for (int x = 0; x < HUD_W + 2 * HUD_B; x++) {
      bool corner = (x == 0 || x == HUD_W + 2 * HUD_B - 1) && (y == 0 || y == HUD_H + 2 * HUD_B - 1);
      if (corner) continue;
      uint32_t c;
      if (x == 0 || y == 0 || x == HUD_W + 2 * HUD_B - 1 || y == HUD_H + 2 * HUD_B - 1) c = edge;
      else if (x < HUD_B || y < HUD_B || x >= HUD_W + HUD_B || y >= HUD_H + HUD_B)
        c = (y == 1 || x == 1) ? light : (x == HUD_B - 1 || y == HUD_B - 1 ||
             x == HUD_W + HUD_B || y == HUD_H + HUD_B) ? edge : wood;
      else {
        int tx = vx + (x - HUD_B), ty = vy + (y - HUD_B);
        if (tx < tx0 || tx >= tx1 || ty < ty0 || ty >= ty1) c = empty;
        else if (u == 4) {
          c = s.cell[ty * CELLS_MAX + tx];       /* one pixel per cell */
        } else {
          /* one pixel per tile: the average of its four cells */
          const uint32_t *q = &s.cell[(ty * 2) * CELLS_MAX + tx * 2];
          uint32_t a = q[0], b = q[1], d = q[CELLS_MAX], e = q[CELLS_MAX + 1];
          uint32_t r = (((a >> 16) & 255) + ((b >> 16) & 255) + ((d >> 16) & 255) + ((e >> 16) & 255)) / 4;
          uint32_t g = (((a >> 8) & 255) + ((b >> 8) & 255) + ((d >> 8) & 255) + ((e >> 8) & 255)) / 4;
          uint32_t bl = ((a & 255) + (b & 255) + (d & 255) + (e & 255)) / 4;
          c = XRGB(r, g, bl);
        }
      }
      put(row, bx + x, width, c);
    }
  }
  /* everyone around as a little face (their own sprite), you on top in a
   * red frame */
  int xs[GOBJ_N], ys[GOBJ_N];
  uint16_t ids[GOBJ_N];
  int n = hm_minimap_people(xs, ys, ids, GOBJ_N);
  static const uint32_t cream = XRGB(0xFF, 0xF4, 0xD6), red = XRGB(0xE8, 0x30, 0x2A);
  for (int i = 0; i <= n; i++) {
    bool me = i == n;
    uint16_t id = me ? hm_minimap_player_sprite() : ids[i];
    int cx = (me ? hm_rd16(g_ram, HM_PLAYER_X) : xs[i]) / u - vx;
    int cy = ((me ? hm_rd16(g_ram, HM_PLAYER_Y) : ys[i]) - 8) / u - vy;   /* feet -> body */
    if (cx < 0 || cy < 0 || cx >= HUD_W || cy >= HUD_H) continue;
    /* a 9x9 badge (the face's 7x7 middle), kept inside the map window */
    enum { BS = 9 };
    int ix = cx - BS / 2, iy = cy - BS / 2;
    if (ix < 0) ix = 0;
    if (iy < 0) iy = 0;
    if (ix > HUD_W - BS) ix = HUD_W - BS;
    if (iy > HUD_H - BS) iy = HUD_H - BS;
    const uint32_t *face = hm_minimap_face(id);
    for (int y = 0; y < BS; y++) {
      uint32_t *row = (uint32_t *)(buf + (size_t)(by + HUD_B + iy + y) * g_ppu->renderPitch);
      for (int x = 0; x < BS; x++) {
        bool border = x == 0 || y == 0 || x == BS - 1 || y == BS - 1;
        bool corner = (x == 0 || x == BS - 1) && (y == 0 || y == BS - 1);
        if (corner) continue;
        uint32_t c = border ? (me ? red : edge) : cream;
        if (!border && face) {
          uint32_t f = face[(y + 2) * 16 + (x + 4)];
          if (f) c = f;
        }
        put(row, bx + HUD_B + ix + x, width, c);
      }
    }
  }
}

void hm_minimap_end_sim_frame(const uint8_t *field, unsigned number)
{
  (void)field; (void)number;
  if (!hm_minimap_hud_on() || !hm_pause_feature_on()) return;
  if (!track_map(ROWS_PER_FRAME) || !s.ready) return;
  if (g_ram[HM_INPUT_STATE] != 1) return;                 /* text, cut-scenes */
  if (PPU_forcedBlank(g_ppu) || PPU_brightness(g_ppu) < 15) return;   /* fades */
  draw_hud();
}
