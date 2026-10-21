/*
 * hm_pause.c — a Harvest Moon 64-style pause menu (quality-of-life feature
 * "pause_menu" of the harvestmoon.qol package).
 *
 * Opened with M on the keyboard or R3 (right stick click) on a controller —
 * neither exists on a SNES pad, so no game button is taken away. The host's
 * title-overlay hook freezes the guest at a frame boundary while the menu is
 * up, so reading and writing WRAM here is as safe as the game's own code
 * doing it between frames. The song keeps playing underneath
 * (SnesDesktopHostGame.title_overlay_keeps_music): the SPC runs on its own
 * while the CPU waits, the way a real pause screen sounds.
 *
 * Look. Everything is drawn at the SNES's own resolution, 256x224, and
 * doubled, so it is made of the same size of pixels as the game: a checkered
 * floor, wooden frames with parchment inside, pixel type (tools/gen_pixfont.py)
 * and 16x16 item icons (tools/gen_icons.py). The girls' faces are the game's
 * own sprites, read from your ROM when the menu opens (see kGirls).
 *
 * Pages (L / R):
 *   Tools   the hand and backpack slots at the top, the tool shed as a grid.
 *           Equip, swap and store follow the game's own shed routine
 *           CODE_8187C3 and the tool table at $81:BE0F (see hm_ram.h).
 *   Hearts  the five bachelorettes' affection ($7F1F1F.., 0..999).
 *   Farm    money, shipping, stock, cows, happiness, tomorrow's weather.
 *   Calendar  the season as a month: today, tomorrow's weather, the six
 *           festivals (the game's own "Events of the year" list) and where
 *           the girls go at weekends (the townsfolk's own hints). The d-pad
 *           picks a day; past the 1st or the 30th it turns to the next season.
 *   Map     the whole place you are in, with you and everyone around, and the
 *           switch for the small map at the bottom of the screen
 *           (hm_minimap.c).
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common_rtl.h"
#include "mod_runtime.h"
#include "snes_overlay_draw.h"   /* SNES_PAD_* */
#include "desktop/sdl_compat.h"
#include "desktop/config.h"      /* kGamepadBtn_R3 */
#include "hm_ram.h"
#include "hm_pixfont.h"
#include "hm_icons.h"
#include "hm_pause.h"
#include "hm_i18n.h"
#include "hm_widescreen.h"
#include "hm_minimap.h"

/* ── Canvas: 256x224 logical pixels, shown doubled (512x448) ─────────── */
enum { LW = 256, LH = 224, WMAX = 400, PH = LH * 2 };
static uint32_t s_canvas[LW * LH];
static uint32_t s_panel[WMAX * 2 * PH];
static int s_pw;                   /* panel width: the field's, doubled */

enum { GRID_COLS = 8, GRID_ROWS = 3 };

static struct {
  bool enabled;          /* feature on (activation plugin ran) */
  bool open;
  int tab;               /* 0 tools, 1 hearts, 2 farm, 3 calendar, 4 map */
  int cal_off, cal_day;  /* calendar: seasons from today's, picked day 1..30 */
  int cx, cy;            /* tools cursor: cy -1 = top slots (cx 0 hand, 1 bag) */
  char message[96];      /* feedback line, tools page */
  uint32_t prev_pad, held_since, last_repeat, held_dir;
  bool prev_m, prev_r3, nav_primed;
  /* headless self-test */
  long test_open_at, frames_polled;
  char test_keys[256];
  int test_pos, test_pumps;
} s = { .test_open_at = -2 };

/* ── Palette ─────────────────────────────────────────────────────────── */
#define RGB(c) (0xFF000000u | (uint32_t)(c))
#define CHECK_A     RGB(0xD9A867)
#define CHECK_B     RGB(0xC4904F)
#define WOOD_EDGE   RGB(0x3A1E0C)
#define WOOD_DARK   RGB(0x7A4320)
#define WOOD        RGB(0xA4622E)
#define WOOD_LIGHT  RGB(0xD08F4C)
#define WOOD_SHINE  RGB(0xEDB873)
#define PAPER       RGB(0xF7E7BC)
#define PAPER_DARK  RGB(0xE4CB8E)
#define PAPER_LINE  RGB(0xD2B272)
#define INK         RGB(0x4A2812)
#define INK_SOFT    RGB(0x8A6236)
#define CREAM       RGB(0xFFF4D6)
#define GREEN       RGB(0x3E9A3C)
#define GREEN_DARK  RGB(0x1F5E22)
#define BLUE        RGB(0x3A70D0)
#define BLUE_DARK   RGB(0x1C3C7A)
#define RED         RGB(0xD8382E)
#define GOLD        RGB(0xE8B030)
#define GOLD_DARK   RGB(0x9A6A10)
#define SELECT      RGB(0xFFF08A)

/* ── Drawing primitives (logical pixels) ─────────────────────────────── */
static inline void px(int x, int y, uint32_t c)
{
  if ((unsigned)x < LW && (unsigned)y < LH && (c >> 24)) s_canvas[y * LW + x] = c;
}
static void rect(int x, int y, int w, int h, uint32_t c)
{
  for (int j = y; j < y + h; j++)
    for (int i = x; i < x + w; i++) px(i, j, c);
}
static void hline(int x, int y, int w, uint32_t c) { rect(x, y, w, 1, c); }
static void vline(int x, int y, int h, uint32_t c) { rect(x, y, 1, h, c); }

/* A wooden frame, 4 px thick, with chamfered corners, filled with `fill`. */
static void frame(int x, int y, int w, int h, uint32_t fill)
{
  rect(x + 2, y, w - 4, h, WOOD_EDGE);
  rect(x, y + 2, w, h - 4, WOOD_EDGE);
  rect(x + 1, y + 1, w - 2, h - 2, WOOD_EDGE);
  rect(x + 2, y + 1, w - 4, h - 2, WOOD);
  rect(x + 1, y + 2, w - 2, h - 4, WOOD);
  hline(x + 2, y + 1, w - 4, WOOD_SHINE);            /* light from above */
  hline(x + 2, y + 2, w - 4, WOOD_LIGHT);
  vline(x + 1, y + 2, h - 4, WOOD_LIGHT);
  hline(x + 2, y + h - 2, w - 4, WOOD_DARK);
  vline(x + w - 2, y + 2, h - 4, WOOD_DARK);
  for (int i = x + 6; i < x + w - 6; i += 9)         /* grain */
    px(i, y + 2, WOOD), px(i + 3, y + h - 3, WOOD_DARK);
  rect(x + 3, y + 3, w - 6, h - 6, WOOD_EDGE);       /* inner edge */
  rect(x + 4, y + 4, w - 8, h - 8, fill);
  hline(x + 4, y + 4, w - 8, PAPER_DARK);            /* inset shadow */
  vline(x + 4, y + 5, h - 9, PAPER_DARK);
}

/* A slot for one item: coloured frame around a sunken square. */
static void slot(int x, int y, int sz, uint32_t c, uint32_t cdark, bool sel)
{
  rect(x + 1, y, sz - 2, sz, WOOD_EDGE);
  rect(x, y + 1, sz, sz - 2, WOOD_EDGE);
  rect(x + 1, y + 1, sz - 2, sz - 2, c);
  hline(x + 2, y + sz - 2, sz - 4, cdark);
  vline(x + sz - 2, y + 2, sz - 4, cdark);
  rect(x + 3, y + 3, sz - 6, sz - 6, WOOD_EDGE);
  rect(x + 4, y + 4, sz - 8, sz - 8, sel ? SELECT : PAPER);
  hline(x + 4, y + 4, sz - 8, sel ? GOLD : PAPER_DARK);
}

static void icon(int x, int y, uint8_t id)
{
  if (id == 0 || id >= HM_ICON_COUNT) return;
  const uint32_t *p = kHmIcons[id];
  for (int j = 0; j < 16; j++)
    for (int i = 0; i < 16; i++) px(x + i, y + j, p[j * 16 + i]);
}

/* ── Pixel text ──────────────────────────────────────────────────────── */
static uint32_t utf8_next(const char **p)
{
  const unsigned char *s8 = (const unsigned char *)*p;
  uint32_t c = s8[0];
  int n = 1;
  if (c >= 0xF0)      { c = ((c & 7) << 18) | ((s8[1] & 63) << 12) | ((s8[2] & 63) << 6) | (s8[3] & 63); n = 4; }
  else if (c >= 0xE0) { c = ((c & 15) << 12) | ((s8[1] & 63) << 6) | (s8[2] & 63); n = 3; }
  else if (c >= 0xC0) { c = ((c & 31) << 6) | (s8[1] & 63); n = 2; }
  *p += n;
  return c;
}
static const HmPixGlyph *glyph(const HmPixFont *f, uint32_t cp)
{
  for (int i = 0; i < f->n; i++) if (f->g[i].cp == cp) return &f->g[i];
  for (int i = 0; i < f->n; i++) if (f->g[i].cp == '?') return &f->g[i];
  return &f->g[0];
}
static int text_w(const HmPixFont *f, const char *t)
{
  int w = 0;
  while (*t) w += glyph(f, utf8_next(&t))->adv;
  return w;
}
/* `y` is the baseline. */
static int text(const HmPixFont *f, int x, int y, const char *t, uint32_t col)
{
  int top = y - f->ascent;
  while (*t) {
    const HmPixGlyph *g = glyph(f, utf8_next(&t));
    for (int j = 0; j < f->h; j++) {
      uint16_t bits = f->rows[g->off + j];
      for (int i = 0; bits; i++, bits >>= 1)
        if (bits & 1) px(x + i, top + j, col);
    }
    x += g->adv;
  }
  return x;
}
static int text_sh(const HmPixFont *f, int x, int y, const char *t, uint32_t col)
{
  text(f, x + 1, y + 1, t, WOOD_EDGE);
  return text(f, x, y, t, col);
}
static void text_r(const HmPixFont *f, int rx, int y, const char *t, uint32_t col)
{
  text(f, rx - text_w(f, t), y, t, col);
}
static void text_c(const HmPixFont *f, int cx, int y, const char *t, uint32_t col)
{
  text(f, cx - text_w(f, t) / 2, y, t, col);
}
#define BODY  (&kPix_body)
#define SMALL (&kPix_small)

/* A small heart, 7x6. */
static void mini_heart(int x, int y, uint32_t fill, bool full)
{
  static const char *art[6] = { ".##.##.", "#######", "#######", ".#####.", "..###..", "...#..." };
  for (int j = 0; j < 6; j++)
    for (int i = 0; i < 7; i++)
      if (art[j][i] == '#') {
        bool edge = (j == 0) || i == 0 || i == 6 || (j > 0 && (art[j - 1][i] != '#')) ||
                    (j < 5 && art[j + 1][i] != '#') || art[j][i - 1 < 0 ? 0 : i - 1] != '#' ||
                    (i < 6 && art[j][i + 1] != '#');
        px(x + i, y + j, edge ? WOOD_EDGE : (full ? fill : PAPER_DARK));
      }
  if (full) px(x + 2, y + 1, CREAM);
}

/* ── Game data ───────────────────────────────────────────────────────── */
typedef struct { uint8_t id; const char *name; } ToolName;
/* ids are the game's own item numbers (hm_ram.h, $0921); this order is the
 * grid's. */
static const ToolName kTools[] = {
  {0x01, "Sickle"}, {0x02, "Hoe"}, {0x03, "Hammer"}, {0x04, "Ax"},
  {0x10, "Watering Can"}, {0x0E, "Milker"}, {0x0F, "Brush"}, {0x0B, "Bell"},
  {0x05, "Corn Seeds"}, {0x06, "Tomato Seeds"}, {0x07, "Potato Seeds"}, {0x08, "Turnip Seeds"},
  {0x0C, "Grass Seeds"}, {0x09, "Cow Medicine"}, {0x0A, "Miracle Potion"}, {0x0D, "Paint"},
  {0x11, "Gold Sickle"}, {0x12, "Gold Hoe"}, {0x13, "Gold Hammer"}, {0x14, "Gold Ax"},
  {0x15, "Sprinkler"}, {0x16, "Magic Beans"}, {0x17, "Blue Diamond"}, {0x18, "Blue Feather"},
};
enum { NTOOLS = sizeof kTools / sizeof kTools[0] };

static const char *tool_name(uint8_t id)
{
  if (id == 0) return T("(nothing)");
  if (id == 0x19) return T("Chicken Feed");
  if (id == 0x1A) return T("Cow Feed");
  for (int i = 0; i < NTOOLS; i++) if (kTools[i].id == id) return T(kTools[i].name);
  return "?";
}
static unsigned item_count(uint8_t id)
{
  if (id >= 0x05 && id <= 0x08) return g_ram[HM_SEEDS_CORN_N + (id - 0x05)];
  if (id == 0x0C) return g_ram[HM_SEEDS_GRASS_N];
  return 0;
}
/* The shed slot of an item: DATA16_81BE0F, bytes 6 (row) and 7 (mask) of
 * each 8-byte entry. Read from the ROM so it can never disagree with it. */
static bool shed_slot(uint8_t id, int *row, uint8_t *mask)
{
  if (id == 0 || id > 0x1A || !g_rom) return false;
  const uint8_t *e = g_rom + 0xBE0F + id * 8;   /* $81:BE0F, LoROM file offset */
  *row = e[6]; *mask = e[7];
  return *mask != 0;
}
static bool in_shed(uint8_t id)
{
  int row; uint8_t mask;
  return shed_slot(id, &row, &mask) && (g_ram[0x11F00 + row] & mask);
}
static void set_in_shed(uint8_t id, bool on)
{
  int row; uint8_t mask;
  if (!shed_slot(id, &row, &mask)) return;
  if (on) g_ram[0x11F00 + row] |= mask; else g_ram[0x11F00 + row] &= (uint8_t)~mask;
}
static bool player_free(void)
{
  /* Normal control, nothing carried, and the player either idle (0), in the
   * standing-still idle animation (9), walking (1) or running (2): never in
   * the middle of using, showing or dropping something. */
  uint16_t act = hm_rd16(g_ram, HM_PLAYER_ACTION);
  return hm_diary_loaded(g_ram) && g_ram[HM_INPUT_STATE] == 1 &&
         (act == 0 || act == 1 || act == 2 || act == 9) && g_ram[HM_ITEM_ON_HAND] == 0;
}

static void say(const char *m) { snprintf(s.message, sizeof s.message, "%s", m); }

/* The item under the cursor (0 = none). */
static uint8_t cursor_item(void)
{
  if (s.cy < 0) return g_ram[s.cx == 0 ? HM_TOOL_SELECTED : HM_TOOL_BACKPACK];
  int i = s.cy * GRID_COLS + s.cx;
  return i < NTOOLS && in_shed(kTools[i].id) ? kTools[i].id : 0;
}

/* Equip a shed item -- CODE_8187C3: the hand tool drops into an empty
 * backpack; otherwise it goes back to the shed (the game never lets an item
 * be in two places). Then the new item leaves the shed. */
static void equip_from_shed(uint8_t id)
{
  uint8_t hand = g_ram[HM_TOOL_SELECTED];
  if (hand) {
    if (g_ram[HM_TOOL_BACKPACK] == 0) g_ram[HM_TOOL_BACKPACK] = hand;
    else set_in_shed(hand, true);
  }
  g_ram[HM_TOOL_SELECTED] = id;
  set_in_shed(id, false);
  char m[96]; snprintf(m, sizeof m, T("Now holding: %s"), tool_name(id)); say(m);
}
static void swap_hand_bag(void)
{
  uint8_t t = g_ram[HM_TOOL_SELECTED];       /* the game's own X swap */
  g_ram[HM_TOOL_SELECTED] = g_ram[HM_TOOL_BACKPACK];
  g_ram[HM_TOOL_BACKPACK] = t;
  say(T("Swapped hand and backpack."));
}

static void action_a(void)
{
  if (s.tab != 0) return;
  if (!player_free()) { say(T("You can't change tools right now.")); return; }
  if (s.cy < 0) { if (g_ram[HM_TOOL_BACKPACK] || g_ram[HM_TOOL_SELECTED]) swap_hand_bag(); return; }
  uint8_t id = cursor_item();
  if (id) equip_from_shed(id);
}
static void action_x(void)
{
  if (s.tab != 0) return;
  if (!player_free()) { say(T("You can't change tools right now.")); return; }
  swap_hand_bag();
}
static void action_y(void)   /* put the hand (or the selected bag) item away */
{
  if (s.tab != 0) return;
  if (!player_free()) { say(T("You can't change tools right now.")); return; }
  uint32_t where = (s.cy < 0 && s.cx == 1) ? HM_TOOL_BACKPACK : HM_TOOL_SELECTED;
  uint8_t id = g_ram[where];
  int row; uint8_t mask;
  if (!id) { say(T("Nothing to put away.")); return; }
  if (!shed_slot(id, &row, &mask) || row > 2) { say(T("That doesn't go in the tool shed.")); return; }
  set_in_shed(id, true);
  g_ram[where] = 0;
  char m[96]; snprintf(m, sizeof m, T("Put away: %s"), tool_name(id)); say(m);
}

/* ── The girls' faces: the game's own sprites, read from your ROM ────── */
/* Front-facing frame of each girl in the NPC sprite sheet ($8F:8000, file
 * 0x78000: 4bpp tiles, 16 per 0x200-byte row; a frame is 2 tiles wide, its
 * top two rows are the head) and her sprite palette ($A9:DE20.., the game's
 * OBJ palette table). Found by watching the flower festival: talking to each
 * girl raised exactly one heart counter, and the tiles she was drawn with
 * matched these ROM bytes. Hearts order: Maria, Ann, Nina, Ellen, Eve. */
static const struct { uint32_t frame, palette; } kGirls[5] = {
  { 0x078200, 0x14DE20 },   /* Maria: blue hair */
  { 0x078B00, 0x14DE20 },   /* Ann: orange ponytail */
  { 0x07B2C0, 0x14DE40 },   /* Nina: pink hair */
  { 0x07A280, 0x14DE80 },   /* Ellen: brown hair */
  { 0x07A3C0, 0x14DE40 },   /* Eve: blonde */
};
static uint32_t s_face[5][16 * 16];
static bool s_faces_ok;

static void load_faces(void)
{
  if (s_faces_ok || !g_rom) return;
  for (int g = 0; g < 5; g++) {
    uint32_t pal[16];
    for (int i = 0; i < 16; i++) {
      uint16_t v = g_rom[kGirls[g].palette + i * 2] | g_rom[kGirls[g].palette + i * 2 + 1] << 8;
      uint32_t r = (v & 31) << 3, gg = ((v >> 5) & 31) << 3, b = ((v >> 10) & 31) << 3;
      pal[i] = i ? (0xFF000000u | r << 16 | gg << 8 | b | (r >> 5) << 16 | (gg >> 5) << 8 | (b >> 5)) : 0;
    }
    for (int ty = 0; ty < 2; ty++)
      for (int tx = 0; tx < 2; tx++) {
        const uint8_t *t = g_rom + kGirls[g].frame + ty * 0x200 + tx * 0x20;
        for (int y = 0; y < 8; y++)
          for (int x = 0; x < 8; x++) {
            int b = 7 - x;
            int c = ((t[y * 2] >> b) & 1) | (((t[y * 2 + 1] >> b) & 1) << 1) |
                    (((t[16 + y * 2] >> b) & 1) << 2) | (((t[16 + y * 2 + 1] >> b) & 1) << 3);
            s_face[g][(ty * 8 + y) * 16 + tx * 8 + x] = pal[c];
          }
      }
  }
  s_faces_ok = true;
}

/* ── Pieces of the screen ────────────────────────────────────────────── */
static const char *kDays[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *kSeasons[4] = { "Spring", "Summer", "Fall", "Winter" };
static const uint32_t kSeasonCol[4] = { RGB(0xE8789A), RGB(0x3E9A3C), RGB(0xD8762A), RGB(0x4A84D8) };

static void checker(void)
{
  for (int y = 0; y < LH; y++)
    for (int x = 0; x < LW; x++)
      s_canvas[y * LW + x] = (((x >> 4) ^ (y >> 4)) & 1) ? CHECK_B : CHECK_A;
}

static void clock_face(int cx, int cy, int r)
{
  for (int y = -r - 1; y <= r + 1; y++)
    for (int x = -r - 1; x <= r + 1; x++) {
      int d2 = x * x + y * y;
      if (d2 <= r * r) px(cx + x, cy + y, CREAM);
      else if (d2 <= (r + 1) * (r + 1) + 1) px(cx + x, cy + y, WOOD_EDGE);
    }
  for (int k = 0; k < 12; k++) {
    double a = k * M_PI / 6;
    px(cx + (int)lround(sin(a) * (r - 1)), cy - (int)lround(cos(a) * (r - 1)), INK_SOFT);
  }
  unsigned hour = g_ram[HM_HOUR] % 12, minute = (g_ram[HM_MINUTES] % 15) * 4;
  double am = minute * M_PI / 30, ah = (hour + minute / 60.0) * M_PI / 6;
  for (int i = 1; i <= r - 2; i++)
    px(cx + (int)lround(sin(am) * i), cy - (int)lround(cos(am) * i), INK_SOFT);
  for (int i = 1; i <= r - 5; i++)
    px(cx + (int)lround(sin(ah) * i), cy - (int)lround(cos(ah) * i), RED);
  px(cx, cy, WOOD_EDGE);
}

static void draw_top(void)
{
  uint8_t *r = g_ram;
  char buf[64];
  /* date and clock */
  frame(4, 4, 96, 46, PAPER);
  int season = r[HM_SEASON] & 3;
  const char *sname = T(kSeasons[season]);
  int sw = text_w(SMALL, sname) + 8;
  rect(9, 9, sw, 10, kSeasonCol[season]);
  hline(9, 18, sw, WOOD_EDGE);
  text_sh(SMALL, 13, 16, sname, CREAM);
  snprintf(buf, sizeof buf, "%u", r[HM_DAY]);
  int nx = text(BODY, 10, 36, buf, INK);
  text(SMALL, nx + 3, 36, T(kDays[r[HM_WEEKDAY] % 7]), INK_SOFT);
  snprintf(buf, sizeof buf, T("Year %u"), r[HM_YEAR] + 1u);
  text(SMALL, 10, 45, buf, INK_SOFT);
  clock_face(84, 21, 10);                        /* hands... */
  snprintf(buf, sizeof buf, "%u:%02u", r[HM_HOUR], (r[HM_MINUTES] % 15) * 4);
  text_c(SMALL, 84, 45, buf, INK);               /* ...and the time in figures */

  /* hand and backpack */
  bool on_top = s.tab == 0 && s.cy < 0;
  slot(104, 8, 26, GREEN, GREEN_DARK, on_top && s.cx == 0);
  slot(134, 8, 26, BLUE, BLUE_DARK, on_top && s.cx == 1);
  icon(109, 13, r[HM_TOOL_SELECTED]);
  icon(139, 13, r[HM_TOOL_BACKPACK]);
  text_c(SMALL, 117, 45, T("Hand"), CREAM);
  text_c(SMALL, 147, 45, T("Bag"), CREAM);
  text_c(SMALL, 117, 44, T("Hand"), WOOD_EDGE);
  text_c(SMALL, 147, 44, T("Bag"), WOOD_EDGE);

  /* money and stamina */
  frame(164, 4, 88, 46, PAPER);
  for (int y = -4; y <= 4; y++)                   /* coin */
    for (int x = -4; x <= 4; x++) {
      int d2 = x * x + y * y;
      if (d2 <= 16) px(175 + x, 21 + y, d2 >= 10 ? GOLD_DARK : GOLD);
    }
  px(174, 19, CREAM);
  snprintf(buf, sizeof buf, "%u G", (unsigned)hm_rd24(r, HM_MONEY) * 10u);
  text_r(BODY, 245, 26, buf, INK);
  text(SMALL, 170, 38, T("Stamina"), INK_SOFT);
  unsigned st = r[HM_CURRENT_STAMINA], mx = r[HM_MAX_STAMINA] ? r[HM_MAX_STAMINA] : 1;
  if (st > mx) st = mx;
  rect(170, 41, 76, 5, WOOD_EDGE);
  rect(171, 42, 74, 3, PAPER_DARK);
  rect(171, 42, (int)(74 * st / mx), 3, st * 4 < mx ? RED : GREEN);
}

enum { NTABS = 5, TAB_CAL = 3, TAB_MAP = 4 };
static const char *kTabs[NTABS] = { "Tools", "Hearts", "Farm", "Calendar", "Map" };

static void draw_tabs(void)
{
  /* as much padding as the five names leave room for */
  int pad = 12, total;
  do {
    total = 8;
    for (int t = 0; t < NTABS; t++) total += text_w(SMALL, T(kTabs[t])) + pad + 2;
  } while (total > 250 && --pad > 6);
  int x = 8;
  for (int t = 0; t < NTABS; t++) {
    const char *name = T(kTabs[t]);
    int w = text_w(SMALL, name) + pad;
    bool on = t == s.tab;
    int y = on ? 53 : 55;
    rect(x + 1, y, w - 2, 70 - y, WOOD_EDGE);
    rect(x, y + 1, w, 69 - y, WOOD_EDGE);
    rect(x + 1, y + 1, w - 2, 69 - y, on ? PAPER : WOOD);
    hline(x + 2, y + 1, w - 4, on ? CREAM : WOOD_LIGHT);
    if (on) text(SMALL, x + pad / 2, y + 9, name, INK);
    else text_sh(SMALL, x + pad / 2, y + 9, name, CREAM);
    x += w + 2;
  }
  int hw = text_w(SMALL, T("L / R  tabs"));
  if (x + 4 <= 250 - hw) text_sh(SMALL, 250 - hw, 63, T("L / R  tabs"), CREAM);
}

static void draw_tools(int x, int y)
{
  /* the shed, every shed item in a fixed place; missing ones are empty */
  text(SMALL, x, y + 7, T("IN THE TOOL SHED"), INK_SOFT);
  for (int i = 0; i < NTOOLS; i++) {
    int gx = i % GRID_COLS, gy = i / GRID_COLS;
    int sx = x + gx * 21, sy = y + 11 + gy * 21;
    bool sel = s.cy == gy && s.cx == gx;
    bool have = in_shed(kTools[i].id);
    rect(sx, sy, 20, 20, sel ? GOLD_DARK : PAPER_LINE);
    rect(sx + 1, sy + 1, 18, 18, sel ? SELECT : (have ? PAPER : PAPER_DARK));
    if (have) {
      icon(sx + 2, sy + 2, kTools[i].id);
      unsigned n = item_count(kTools[i].id);
      if (n) { char c[8]; snprintf(c, sizeof c, "%u", n); text_r(SMALL, sx + 20, sy + 19, c, INK); }
    }
  }
  /* the cursor: a little hand pointing at the slot */
  int px0, py0;
  if (s.cy < 0) { px0 = 104 + s.cx * 30 - 9; py0 = 18; }
  else { px0 = x + s.cx * 21 - 7; py0 = y + 11 + s.cy * 21 + 6; }
  static const char *hand[7] = { "..##....", ".#cc####", "#cccccc#", "#ccc####", "#cccc#..", ".####...", "........" };
  for (int j = 0; j < 7; j++)
    for (int i = 0; i < 8; i++)
      if (hand[j][i] == '#') px(px0 + i, py0 + j, WOOD_EDGE);
      else if (hand[j][i] == 'c') px(px0 + i, py0 + j, CREAM);

  /* right column: what is under the cursor */
  int rx = x + GRID_COLS * 21 + 6;
  uint8_t id = cursor_item();
  rect(rx - 4, y + 2, 1, 76, PAPER_LINE);
  if (s.cy < 0) text(SMALL, rx, y + 9, s.cx == 0 ? T("In hand:") : T("In backpack:"), INK_SOFT);
  if (id) {
    icon(rx, y + 13, id);
    const char *name = tool_name(id);
    /* wrap a two-word name over two lines when it is too wide */
    char a[48]; snprintf(a, sizeof a, "%s", name);
    char *sp = strchr(a, ' ');
    if (sp && text_w(SMALL, a) > 58) { *sp = 0; text(SMALL, rx, y + 38, a, INK); text(SMALL, rx, y + 46, sp + 1, INK); }
    else text(SMALL, rx, y + 38, a, INK);
    unsigned n = item_count(id);
    if (n) { char c[16]; snprintf(c, sizeof c, "×%u", n); text(SMALL, rx + 20, y + 25, c, INK_SOFT); }
  } else {
    text(SMALL, rx, y + 38, T("(nothing)"), INK_SOFT);
  }
}

static uint32_t heart_colour(unsigned v)
{
  static const uint32_t tier[8] = { RGB(0x5A4A5A), RGB(0x8A5AC8), RGB(0x4A78E0), RGB(0x46A846),
                                    RGB(0xE0C028), RGB(0xEC8A2A), RGB(0xF27AA8), RGB(0xE0303A) };
  static const unsigned lim[8] = { 100, 200, 350, 450, 550, 650, 800, 1000 };
  for (int i = 0; i < 8; i++) if (v < lim[i]) return tier[i];
  return tier[7];
}

static void draw_hearts(int x, int y)
{
  static const char *names[5] = { "Maria", "Ann", "Nina", "Ellen", "Eve" };
  load_faces();
  for (int i = 0; i < 5; i++) {
    int ry = y + i * 19;
    unsigned v = hm_rd16(g_ram, HM_HEARTS_BASE + i * 2);
    if (v > 999) v = 999;
    /* portrait frame */
    rect(x, ry, 18, 18, WOOD_EDGE);
    rect(x + 1, ry + 1, 16, 16, PAPER_DARK);
    if (s_faces_ok)
      for (int j = 0; j < 16; j++)
        for (int k = 0; k < 16; k++) px(x + 1 + k, ry + 1 + j, s_face[i][j * 16 + k]);
    text(BODY, x + 24, ry + 13, T(names[i]), INK);
    unsigned full = (v + 50) / 100;
    uint32_t col = heart_colour(v);
    for (int h = 0; h < 10; h++) mini_heart(x + 76 + h * 9, ry + 6, col, (unsigned)h < full);
    char num[8]; snprintf(num, sizeof num, "%u", v);
    text_r(SMALL, x + 236, ry + 12, num, INK_SOFT);
    if (i < 4) hline(x + 22, ry + 18, 214, PAPER_LINE);
  }
}

static const char *kWeatherNames[12] = { "Sunny", "Rain", "Snow", "Hurricane", "Fair", "Storm",
  "Flower Festival", "Harvest Festival", "Thanksgiving",
  "Star Night Festival", "New Year", "Egg Festival" };

static void draw_farm(int x, int y)
{
  uint8_t *r = g_ram;
  char v[64];
  int row = 0, col = 0;
#define ITEM(label, ...) do { snprintf(v, sizeof v, __VA_ARGS__); \
    int ix = x + col * 120, iy = y + row * 19; \
    text(SMALL, ix, iy + 8, T(label), INK_SOFT); text_r(BODY, ix + 114, iy + 16, v, INK); \
    hline(ix, iy + 18, 114, PAPER_LINE); \
    if (++row == 5) { row = 0; col++; } } while (0)
  ITEM("Time", "%u:%02u", r[HM_HOUR], (r[HM_MINUTES] % 15) * 4);
  ITEM("Shipped today", "%u G", (unsigned)hm_rd24(r, HM_SHIPPING_MONEY) * 10u);
  ITEM("Lumber", "%u", hm_rd16(r, HM_STORED_WOOD));
  ITEM("Fodder", "%u", hm_rd16(r, HM_STORED_GRASS));
  ITEM("Cows", "%u", r[HM_COW_N]);
  ITEM("Happiness", "%u / 999", hm_rd16(r, HM_HAPPINESS));
  ITEM("Dog hugs", "%u", hm_rd16(r, HM_DOG_HUGS));
  ITEM("Tomorrow's weather", "%s", r[HM_WEATHER_TOMORROW] < 12 ? T(kWeatherNames[r[HM_WEATHER_TOMORROW]]) : "?");
  ITEM("Stamina", "%u / %u", r[HM_CURRENT_STAMINA], r[HM_MAX_STAMINA]);
#undef ITEM
}

/* ── Calendar ────────────────────────────────────────────────────────── */
/* The festivals, from the game's own "Events of the year" message. */
enum { FEST_NEWYEAR, FEST_FLOWER, FEST_HARVEST, FEST_EGG, FEST_THANKS, FEST_STARS, NFEST };
static const struct { uint8_t season, day; const char *name, *note; } kFests[NFEST] = {
  { 0,  1, "New Year",            "See the first sunrise." },
  { 0, 23, "Flower Festival",     "At the town square." },
  { 2, 12, "Harvest Festival",    "At the town square." },
  { 2, 20, "Egg Festival",        "At the town square." },
  { 3, 10, "Thanksgiving",        "The girls bake cakes." },
  { 3, 24, "Star Night Festival", "Ann goes to the spa, Ellen to the town square." },
};
static int festival_on(int season, int day)
{
  for (int i = 0; i < NFEST; i++)
    if (kFests[i].season == season && kFests[i].day == day) return i;
  return -1;
}

/* 7x7 pictures. Letters pick the colour. */
static const char *kFestArt[NFEST][7] = {
  { ".......", "..RRR..", ".RRRRR.", "RRRRRRR", "KKKKKKK", ".......", "......." },   /* sunrise */
  { "..P.P..", ".PPPPP.", "PPPYPPP", ".PPPPP.", "..PGP..", "...G...", "..GGG.." },   /* flower */
  { "...G...", "..GO...", ".OOOOO.", "OOoOoOO", "OOoOoOO", "OOoOoOO", ".OOOOO." },   /* pumpkin */
  { "..KKK..", ".KCCCK.", "KCCCCCK", "KCCCCCK", "KCCCCCK", ".KCCCK.", "..KKK.." },   /* egg */
  { "...R...", "...C...", ".CCCCC.", "PPPPPPP", "CCCCCCC", "WWWWWWW", "WWWWWWW" },   /* cake */
  { "...Y...", "..YYY..", "YYYYYYY", ".YYYYY.", "..YYY..", ".YY.YY.", "YY...YY" },   /* star */
};
static const char *kWeatherArt[4][7] = {
  { "...Y...", ".Y...Y.", "..YYY..", "Y.YYY.Y", "..YYY..", ".Y...Y.", "...Y..." },   /* sun */
  { "..SSS..", ".SSSSS.", "SSSSSSS", ".......", ".B.B.B.", "B.B.B..", "......." },   /* rain */
  { "...b...", ".b.b.b.", "..bbb..", "bbbbbbb", "..bbb..", ".b.b.b.", "...b..." },   /* snow */
  { ".DDDDD.", "DDDDDDD", ".DDDDD.", "...YY..", "..YY...", ".YYYY..", "...Y..." },   /* storm */
};
static void art7(int x, int y, const char *const art[7])
{
  for (int j = 0; j < 7; j++)
    for (int i = 0; i < 7; i++) {
      uint32_t c = 0;
      switch (art[j][i]) {
        case 'R': c = RED; break;            case 'K': c = INK_SOFT; break;
        case 'P': c = RGB(0xF27AA8); break;  case 'Y': c = GOLD; break;
        case 'G': c = GREEN; break;          case 'O': c = RGB(0xEC8A2A); break;
        case 'o': c = RGB(0xB85A16); break;  case 'C': c = CREAM; break;
        case 'W': c = WOOD; break;           case 'S': c = RGB(0x9AA4B0); break;
        case 'B': c = BLUE; break;           case 'b': c = RGB(0x6AA8E8); break;
        case 'D': c = RGB(0x5A6270); break;
      }
      if (c) px(x + i, y + j, c);
    }
}
/* Tomorrow's weather as a picture; -1 for festival codes (the festival's own
 * picture is shown on that day instead). */
static int weather_art(uint8_t code)
{
  switch (code) {
    case HM_WEATHER_SUNNY: case HM_WEATHER_FAIR_STORY: return 0;
    case HM_WEATHER_RAIN: return 1;
    case HM_WEATHER_SNOW: return 2;
    case HM_WEATHER_HURRICANE: case HM_WEATHER_THUNDER: return 3;
    default: return -1;
  }
}

/* Seasons are 30 days and the week never skips, so any day's weekday
 * follows from today's. Seasons are counted from the first spring. */
static int season_index(void) { return g_ram[HM_YEAR] * 4 + (g_ram[HM_SEASON] & 3); }
static int weekday_of(int sidx, int day)
{
  long delta = (long)(sidx - season_index()) * 30 + (day - g_ram[HM_DAY]);
  return (int)(((g_ram[HM_WEEKDAY] % 7 + delta) % 7 + 7) % 7);
}

/* Write `t` over lines no wider than `w`, breaking at spaces. */
static int wrap(const HmPixFont *f, int x, int y, int w, int lh, const char *t, uint32_t col, int ymax)
{
  char line[160] = "", word[64];
  while (*t) {
    int n = 0;
    while (*t == ' ') t++;
    while (*t && *t != ' ' && n < (int)sizeof word - 1) word[n++] = *t++;
    word[n] = 0;
    if (!n) break;
    char trial[160];
    snprintf(trial, sizeof trial, line[0] ? "%s %s" : "%s%s", line, word);
    if (line[0] && text_w(f, trial) > w) {
      if (y <= ymax) text(f, x, y, line, col);
      y += lh;
      snprintf(line, sizeof line, "%s", word);
    } else {
      snprintf(line, sizeof line, "%s", trial);
    }
  }
  if (line[0]) { if (y <= ymax) text(f, x, y, line, col); y += lh; }
  return y;
}

static void cal_reset(void) { s.cal_off = 0; s.cal_day = g_ram[HM_DAY] ? g_ram[HM_DAY] : 1; }

static void cal_move(int d)
{
  int day = s.cal_day + d, off = s.cal_off;
  while (day < 1)  { day += 30; off--; }
  while (day > 30) { day -= 30; off++; }
  if (season_index() + off < 0) { off = -season_index(); day = 1; }
  if (off > 8) { off = 8; day = 30; }
  s.cal_off = off; s.cal_day = day;
}

static void draw_calendar(int x, int y)
{
  static const char *ini[7] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
  enum { CW = 18, CH = 14 };
  int sidx = season_index() + s.cal_off;
  int season = sidx & 3, year = sidx / 4;
  int first = weekday_of(sidx, 1);
  int today_off = 0, today = g_ram[HM_DAY];
  /* tomorrow, possibly in the next season */
  int tom_sidx = season_index() + (today >= 30), tom_day = today >= 30 ? 1 : today + 1;
  int wart = weather_art(g_ram[HM_WEATHER_TOMORROW]);

  for (int c = 0; c < 7; c++)
    text_c(SMALL, x + c * CW + CW / 2, y + 7, T(ini[c]),
           c == 0 ? RED : c == 6 ? BLUE : INK_SOFT);
  int gy = y + 10;
  for (int d = 1; d <= 30; d++) {
    int cell = first + d - 1, cx = x + (cell % 7) * CW, cy = gy + (cell / 7) * CH;
    bool is_today = s.cal_off == today_off && d == today;
    bool past = s.cal_off < 0 || (s.cal_off == 0 && d < today);
    int f = festival_on(season, d);
    uint32_t fill = is_today ? SELECT : past ? PAPER_DARK : f >= 0 ? CREAM : PAPER;
    rect(cx, cy, CW - 1, CH - 1, is_today ? GOLD_DARK : PAPER_LINE);
    rect(cx + 1, cy + 1, CW - 3, CH - 3, fill);
    if (f >= 0) hline(cx + 1, cy + 1, CW - 3, kSeasonCol[season]);
    char n[4]; snprintf(n, sizeof n, "%d", d);
    int wd = cell % 7;
    text(SMALL, cx + 2, cy + 7, n, past ? INK_SOFT : wd == 0 ? RED : wd == 6 ? BLUE : INK);
    if (f >= 0) art7(cx + 9, cy + 5, kFestArt[f]);
    else if (wart >= 0 && sidx == tom_sidx && d == tom_day) art7(cx + 9, cy + 5, kWeatherArt[wart]);
    if (d == s.cal_day) {                       /* the cursor: a red frame */
      rect(cx - 1, cy - 1, CW + 1, 1, RED); rect(cx - 1, cy + CH - 1, CW + 1, 1, RED);
      rect(cx - 1, cy - 1, 1, CH + 1, RED); rect(cx + CW - 1, cy - 1, 1, CH + 1, RED);
    }
  }

  /* right column: the picked day */
  int rx = x + 7 * CW + 6, rw = 236 - rx + 8;
  rect(rx - 4, y + 1, 1, 96, PAPER_LINE);
  char buf[96];
  snprintf(buf, sizeof buf, T("Year %u"), (unsigned)year + 1u);
  int sw = text_w(SMALL, T(kSeasons[season])) + 8;
  rect(rx, y + 1, sw, 10, kSeasonCol[season]);
  hline(rx, y + 10, sw, WOOD_EDGE);
  text_sh(SMALL, rx + 4, y + 8, T(kSeasons[season]), CREAM);
  text(SMALL, rx + sw + 4, y + 8, buf, INK_SOFT);

  int wd = weekday_of(sidx, s.cal_day);
  snprintf(buf, sizeof buf, "%d", s.cal_day);
  int nx = text(BODY, rx, y + 25, buf, INK);
  text(SMALL, nx + 3, y + 25, T(kDays[wd]), wd == 0 ? RED : wd == 6 ? BLUE : INK_SOFT);

  int ly = y + 36, ymax = y + 96;
  bool is_today = s.cal_off == 0 && s.cal_day == today;
  bool is_tom = sidx == tom_sidx && s.cal_day == tom_day;
  if (is_today) { text(SMALL, rx, ly, T("Today"), GOLD_DARK); ly += 9; }
  if (is_tom) {
    snprintf(buf, sizeof buf, "%s: %s", T("Tomorrow"),
             wart >= 0 ? T(kWeatherNames[g_ram[HM_WEATHER_TOMORROW]]) : T("festival"));
    if (wart >= 0) art7(rx, ly - 6, kWeatherArt[wart]);
    text(SMALL, rx + (wart >= 0 ? 10 : 0), ly, buf, INK);
    ly += 9;
  }
  int f = festival_on(season, s.cal_day);
  if (f >= 0) {
    art7(rx, ly - 6, kFestArt[f]);
    ly = wrap(SMALL, rx + 10, ly, rw - 10, 9, T(kFests[f].name), INK, ymax);
    ly = wrap(SMALL, rx, ly, rw, 9, T(kFests[f].note), INK_SOFT, ymax);
  }
  if (wd == 6 || wd == 0) {
    static const char *sat[] = { "Maria: at the church", "Nina: picking herbs", "Ellen: behind the pen",
                                 "Eve: on the mountain", "Ann: her lucky day" };
    static const char *sun[] = { "Maria: at the church", "Nina: on the mountain", "Eve: on the mountain",
                                 "The bar is closed" };
    const char **l = wd == 6 ? sat : sun;
    int n = wd == 6 ? 5 : 4;
    if (ly + 9 <= ymax) { ly += 2; }
    for (int i = 0; i < n && ly <= ymax; i++) { text(SMALL, rx, ly, T(l[i]), INK_SOFT); ly += 9; }
  } else if (f < 0 && !is_today && !is_tom) {
    text(SMALL, rx, ly, T("A normal day."), INK_SOFT);
  }
}

/* ── Map ─────────────────────────────────────────────────────────────── */
/* Someone's face from their own sprite: an 18x18 frame for the list on the
 * right, a 12x12 one (the middle of the face) on the map itself. Red is you. */
static void badge_sized(int x, int y, uint16_t id, bool me, int sz)
{
  int in = sz - 2, off = (16 - in) / 2;
  rect(x + 1, y, sz - 2, sz, me ? RED : WOOD_EDGE);
  rect(x, y + 1, sz, sz - 2, me ? RED : WOOD_EDGE);
  rect(x + 1, y + 1, in, in, CREAM);
  const uint32_t *f = hm_minimap_face(id);
  if (f)
    for (int j = 0; j < in; j++)
      for (int i = 0; i < in; i++) {
        int fy = j + (sz < 18 ? 1 : 0), fx = i + off;
        uint32_t c = f[fy * 16 + fx];
        if (c) px(x + 1 + i, y + 1 + j, c);
      }
}
static void badge(int x, int y, uint16_t id, bool me) { badge_sized(x, y, id, me, 18); }

static void draw_map(int x, int y)
{
  enum { AW = 150, AH = 128 };
  int w, h, x0, y0;
  int xs[40], ys[40], n_people = 0;
  uint16_t ids[40];
  /* the picture: as big as fits, whole cells or averages of k x k */
  if (hm_minimap_snapshot(&w, &h, &x0, &y0)) {
    int k = 1;
    while (w / k > AW || h / k > AH) k++;
    int u = 1;
    if (k == 1) while (w * (u + 1) <= AW && h * (u + 1) <= AH && u < 3) u++;
    int pw = w / k * u, ph = h / k * u;
    int mx = x + (AW - pw) / 2, my = y + (AH - ph) / 2;
    rect(mx - 2, my - 2, pw + 4, ph + 4, WOOD_EDGE);
    rect(mx - 1, my - 1, pw + 2, ph + 2, WOOD_DARK);
    int cx0 = x0 / 4, cy0 = y0 / 4;
    for (int j = 0; j < h / k; j++)
      for (int i = 0; i < w / k; i++) {
        uint32_t r = 0, g = 0, b = 0;
        for (int dy = 0; dy < k; dy++)
          for (int dx = 0; dx < k; dx++) {
            uint32_t c = hm_minimap_cell(cx0 + i * k + dx, cy0 + j * k + dy);
            r += (c >> 16) & 255; g += (c >> 8) & 255; b += c & 255;
          }
        uint32_t c = RGB(((r / (k * k)) << 16) | ((g / (k * k)) << 8) | (b / (k * k)));
        rect(mx + i * u, my + j * u, u, u, c);
      }
    /* where world (wx, wy) lands on the picture */
#define MAPX(wx) (mx + ((wx) - x0) / 4 / k * u + u / 2)
#define MAPY(wy) (my + ((wy) - y0) / 4 / k * u + u / 2)
    n_people = hm_minimap_people(xs, ys, ids, 40);
    for (int i = 0; i < n_people; i++)
      badge_sized(MAPX(xs[i]) - 6, MAPY(ys[i] - 8) - 6, ids[i], false, 12);
    badge_sized(MAPX(hm_rd16(g_ram, HM_PLAYER_X)) - 6, MAPY(hm_rd16(g_ram, HM_PLAYER_Y) - 8) - 6,
                hm_minimap_player_sprite(), true, 12);
#undef MAPX
#undef MAPY
  } else {
    text_c(SMALL, x + AW / 2, y + AH / 2, T("No map here."), INK_SOFT);
  }

  /* right column: the place, and the girls who are here */
  int rx = x + AW + 10, rw = 244 - rx;
  rect(rx - 5, y, 1, AH + 4, PAPER_LINE);
  int ly = wrap(BODY, rx, y + 12, rw, 13, T(hm_minimap_place()), INK, y + 60);
  ly += 2;
  int shown = 0;
  for (int i = 0; i < n_people && shown < 3; i++) {
    const char *name = hm_minimap_name(ids[i]);
    if (!name) continue;
    badge(rx, ly - 2, ids[i], false);
    text(SMALL, rx + 22, ly + 10, T(name), INK);
    ly += 20; shown++;
  }
  if (!shown) {
    ly += 2;
    badge(rx, ly - 2, hm_minimap_player_sprite(), true);
    text(SMALL, rx + 22, ly + 10, T("You"), INK_SOFT);
    ly += 20;
  }
  ly += 4;
  ly = wrap(SMALL, rx, ly, rw, 9, T("Minimap on screen:"), INK, y + AH);
  bool on = hm_minimap_hud_on();
  rect(rx, ly - 6, 30, 12, WOOD_EDGE);
  rect(rx + 1, ly - 5, 28, 10, on ? GREEN : PAPER_DARK);
  text_c(SMALL, rx + 15, ly + 3, on ? T("ON") : T("OFF"), on ? CREAM : INK_SOFT);
  text(SMALL, rx + 34, ly + 3, T("A  switch"), INK_SOFT);
}

static void draw_textbox(void)
{
  frame(4, 176, 248, 44, PAPER);
  int x = 12;
  if (s.tab == 0) {
    if (s.message[0]) text(BODY, x, 193, s.message, GREEN_DARK);
    else if (!player_free()) text(BODY, x, 193, T("(only while you can move)"), RED);
    else text(BODY, x, 193, T("A  equip"), INK);
    char help[128];
    snprintf(help, sizeof help, "%s    %s %s", T("X  hand ↔ backpack"), T("Y  put away in"), T("the tool shed"));
    text(SMALL, x, 205, help, INK_SOFT);
  } else if (s.tab == 1) {
    text(SMALL, x, 192, T("Marks: 200 heart event · 450 first child ·"), INK);
    text(SMALL, x, 201, T("650 second child · maximum 999"), INK);
  } else if (s.tab == 2) {
    char d[96];
    snprintf(d, sizeof d, T("%s, %s %u, Year %u"), T(kDays[g_ram[HM_WEEKDAY] % 7]),
             T(kSeasons[g_ram[HM_SEASON] & 3]), g_ram[HM_DAY], g_ram[HM_YEAR] + 1u);
    text(BODY, x, 196, d, INK);
  } else {
    int sidx = season_index() + s.cal_off;
    char d[96];
    snprintf(d, sizeof d, T("%s, %s %u, Year %u"), T(kDays[weekday_of(sidx, s.cal_day)]),
             T(kSeasons[sidx & 3]), (unsigned)s.cal_day, (unsigned)(sidx / 4) + 1u);
    text(BODY, x, 193, d, INK);
    text(SMALL, x, 205, T("D-pad  pick a day (and other seasons)     A  today"), INK_SOFT);
  }
  text(SMALL, x, 214, T("B / M / Esc  close     ·     the game is paused"), INK_SOFT);
  /* the blinking-arrow corner of a game text box, standing still */
  for (int j = 0; j < 3; j++) hline(241 + j, 210 + j, 5 - 2 * j, WOOD_DARK);
}

static void render(void)
{
  checker();
  draw_top();
  draw_tabs();
  if (s.tab == TAB_MAP) {
    /* the map takes the text box's room too */
    frame(4, 68, 248, 152, PAPER);
    draw_map(10, 74);
    text(SMALL, 12, 214, T("B / M / Esc  close     ·     the game is paused"), INK_SOFT);
  } else {
    frame(4, 68, 248, 106, PAPER);
    if (s.tab == 0) draw_tools(12, 72);
    else if (s.tab == 1) draw_hearts(10, 73);
    else if (s.tab == 2) draw_farm(12, 73);
    else draw_calendar(12, 72);
    draw_textbox();
  }
  /* Double every pixel. With widescreen on, the field is wider than 256:
   * the floor carries on into the margins (same squares, same phase), so
   * the menu covers the whole picture. */
  int fw = hm_ws_field_width();
  if (fw < LW) fw = LW;
  if (fw > WMAX) fw = WMAX;
  int ox = (fw - LW) / 2;
  s_pw = fw * 2;
  for (int y = 0; y < PH; y++) {
    int ly = y >> 1;
    uint32_t *dst = s_panel + y * s_pw;
    for (int x = 0; x < s_pw; x++) {
      int lx = (x >> 1) - ox;
      dst[x] = (lx >= 0 && lx < LW) ? s_canvas[ly * LW + lx]
             : ((((lx >> 4) ^ (ly >> 4)) & 1) ? CHECK_B : CHECK_A);
    }
  }
}

/* ── Input ───────────────────────────────────────────────────────────── */
static void tab(int dir);
static void move(int dx, int dy)
{
  if (s.tab == TAB_CAL) { cal_move(dx + dy * 7); return; }
  if (s.tab != 0) { if (dx) tab(dx); return; }
  s.message[0] = 0;
  if (dy) {
    s.cy += dy;
    if (s.cy < -1) s.cy = GRID_ROWS - 1;
    if (s.cy >= GRID_ROWS) s.cy = -1;
    if (s.cy < 0) s.cx = s.cx >= GRID_COLS / 2 ? 1 : 0;
    else if (dy > 0 && s.cy == 0) s.cx = s.cx ? 5 : 1;
  }
  if (dx) {
    int cols = s.cy < 0 ? 2 : GRID_COLS;
    s.cx = (s.cx + cols + dx) % cols;
  }
}
static void tab(int dir)
{
  s.tab = (s.tab + NTABS + dir) % NTABS;
  s.message[0] = 0;
  if (s.tab == TAB_CAL) cal_reset();
}
static void close_menu(void) { s.open = false; }

static void do_action(char a)
{
  switch (a) {
    case 'u': move(0, -1); break;
    case 'd': move(0, +1); break;
    case 'l': move(-1, 0); break;
    case 'r': move(+1, 0); break;
    case 'L': tab(-1); break;
    case 'R': tab(+1); break;
    case 'A':
      if (s.tab == TAB_CAL) cal_reset();
      else if (s.tab == TAB_MAP) hm_minimap_set_hud(!hm_minimap_hud_on());
      else action_a();
      break;
    case 'X': action_x(); break;
    case 'Y': action_y(); break;
    case 'B': close_menu(); break;
  }
  render();
}

static void key(int k, int repeat)
{
  (void)repeat;
  switch (k) {
    case SDLK_UP:    do_action('u'); break;
    case SDLK_DOWN:  do_action('d'); break;
    case SDLK_LEFT:  do_action('l'); break;
    case SDLK_RIGHT: do_action('r'); break;
    case SDLK_q: case SDLK_c: do_action('L'); break;
    case SDLK_e: case SDLK_v: case SDLK_TAB: do_action('R'); break;
    case SDLK_RETURN: case SDLK_SPACE: case SDLK_x: do_action('A'); break;
    case SDLK_s: do_action('X'); break;
    case SDLK_a: do_action('Y'); break;
    case SDLK_ESCAPE: case SDLK_BACKSPACE: case SDLK_z: case SDLK_m: do_action('B'); break;
    default: break;
  }
}

static void pad_nav(uint32_t in, uint32_t now)
{
  if (!s.nav_primed) { s.prev_pad = in; s.nav_primed = true; return; }
  uint32_t pressed = in & ~s.prev_pad;
  s.prev_pad = in;
  if (pressed & SNES_PAD_A) do_action('A');
  if (pressed & SNES_PAD_X) do_action('X');
  if (pressed & SNES_PAD_Y) do_action('Y');
  if (pressed & (SNES_PAD_B | SNES_PAD_START)) do_action('B');
  if (pressed & SNES_PAD_L) do_action('L');
  if (pressed & SNES_PAD_R) do_action('R');
  uint32_t dir = in & (SNES_PAD_UP | SNES_PAD_DOWN | SNES_PAD_LEFT | SNES_PAD_RIGHT);
  char c = dir & SNES_PAD_UP ? 'u' : dir & SNES_PAD_DOWN ? 'd' : dir & SNES_PAD_LEFT ? 'l' : 'r';
  if (dir && (pressed & dir)) {
    do_action(c);
    s.held_dir = dir; s.held_since = now; s.last_repeat = now;
  } else if (dir && dir == s.held_dir && now - s.held_since > SNES_OVL_REPEAT_DELAY &&
             now - s.last_repeat > SNES_OVL_REPEAT_RATE) {
    do_action(c);
    s.last_repeat = now;
  } else if (!dir) s.held_dir = 0;

  /* headless self-test: one scripted action every 6 pumps, then close */
  if (s.test_keys[0] && s.test_pumps++ % 6 == 5) {
    char k = s.test_keys[s.test_pos];
    if (k) { s.test_pos++; do_action(k); }
    else close_menu();
  }
}

/* ── Host hooks ──────────────────────────────────────────────────────── */
static void open_menu(void)
{
  s.open = true;
  s.nav_primed = false;
  s.message[0] = 0;
  if (s.cy >= GRID_ROWS || s.cy < -1) s.cy = 0;
  cal_reset();
  render();
}

int hm_pause_poll_open(uint32_t inputs, uint32_t gamepad_raw)
{
  (void)inputs;
  s.frames_polled++;
  if (s.test_open_at == -2) {
    /* HM_PAUSE_TEST=<frame>:<actions>  u d l r L R A B X Y, e.g. 6100:RRLLAd */
    const char *t = getenv("HM_PAUSE_TEST");
    s.test_open_at = -1;
    if (t) { s.test_open_at = strtol(t, NULL, 10); const char *c = strchr(t, ':');
             if (c) snprintf(s.test_keys, sizeof s.test_keys, "%s", c + 1); }
  }
  if (!s.enabled) return 0;
  const bool *keys = (const bool *)SDL_GetKeyboardState(NULL);
  bool m = keys && keys[SDL_SCANCODE_M];
  bool r3 = (gamepad_raw >> kGamepadBtn_R3) & 1u;
  bool want = (m && !s.prev_m) || (r3 && !s.prev_r3) ||
              (s.test_open_at >= 0 && s.frames_polled == s.test_open_at);
  s.prev_m = m; s.prev_r3 = r3;
  if (!want || !hm_diary_loaded(g_ram)) return 0;
  open_menu();
  return 1;
}
int hm_pause_is_open(void) { return s.open; }
int hm_pause_feature_on(void) { return s.enabled; }
void hm_pause_handle_key(int k, int repeat) { key(k, repeat); }
void hm_pause_poll_nav(uint32_t inputs, uint32_t now_ms) { pad_nav(inputs, now_ms); }
int hm_pause_image(const uint32_t **panel, int *w, int *h)
{
  if (!s.open) return 0;
  *panel = s_panel; *w = s_pw; *h = PH;
  return 1;
}
void hm_pause_closed(void)
{
  /* The key that closed the menu may still be held; do not reopen on it. */
  const bool *keys = (const bool *)SDL_GetKeyboardState(NULL);
  s.prev_m = keys && keys[SDL_SCANCODE_M];
}

static void on_pause_menu(void) { s.enabled = true; }
static void reset_pause(void) { s.enabled = false; s.open = false; }

SNES_MOD_CONSTRUCTOR(register_hm_pause)
{
  snes_mod_register_reset_callback(reset_pause);
  snes_mod_register_activation_plugin("harvestmoon.qol.pause_menu", on_pause_menu);
}
