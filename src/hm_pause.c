/*
 * hm_pause.c — a Harvest Moon 64-style pause menu (quality-of-life feature
 * "pause_menu" of the harvestmoon.qol package).
 *
 * Opened with M on the keyboard or R3 (right stick click) on a controller —
 * neither exists on a SNES pad, so no game button is taken away. The host's
 * title-overlay hook freezes the guest at a frame boundary while the menu is
 * up (no frames run, audio paused), so reading and writing WRAM here is as
 * safe as the game's own code doing it between frames.
 *
 * Tabs:
 *   Herramientas  hand / backpack / tool-shed contents; equip, swap, store,
 *                 following the game's own shed routine CODE_8187C3 and the
 *                 tool table at $81:BE0F (see hm_ram.h).
 *   Corazones     the five bachelorettes' affection ($7F1F1F.., 0..999) with
 *                 the thresholds the game's code actually tests.
 *   Granja        date, time, money, today's shipping, stock, cows, happiness,
 *                 tomorrow's weather.
 */

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
#include "hm_font.h"
#include "hm_pause.h"
#include "hm_i18n.h"

/* ── Panel ───────────────────────────────────────────────────────────── */
enum { PW = 512, PH = 448 };
static uint32_t s_panel[PW * PH];

static struct {
  bool enabled;          /* feature on (activation plugin ran) */
  bool open;
  int tab;               /* 0 tools, 1 hearts, 2 farm */
  int cursor;            /* tools tab: index into s_tool_rows */
  char message[96];      /* one-line feedback, tools tab */
  uint32_t prev_pad, held_since, last_repeat, held_dir;
  bool prev_m, prev_r3, nav_primed;
  /* headless self-test */
  long test_open_at, frames_polled;
  char test_keys[256];
  int test_pos, test_pumps;
} s = { .test_open_at = -2 };

/* ── Colours (ARGB) ──────────────────────────────────────────────────── */
#define COL_DIM        0xB0101008u
#define COL_WOOD       0xFF7A4A22u
#define COL_WOOD_DARK  0xFF4E2D12u
#define COL_WOOD_LIGHT 0xFFA8703Au
#define COL_PAPER      0xFFF4EBCFu
#define COL_PAPER_DARK 0xFFE2D3A8u
#define COL_INK        0xFF3A2614u
#define COL_INK_SOFT   0xFF7D6146u
#define COL_GREEN      0xFF4C8C3Au
#define COL_SELECT     0xFFFFE08Au
#define COL_RED        0xFFE0455Au
#define COL_PINK       0xFFF7A6B9u
#define COL_WHITE      0xFFFFFFFFu
#define COL_GRAY       0xFFB8AE98u

/* ── Drawing ─────────────────────────────────────────────────────────── */
static inline uint32_t blend(uint32_t dst, uint32_t src, unsigned a)
{
  if (a >= 255) return src | 0xFF000000u;
  unsigned ia = 255 - a;
  unsigned r = (((src >> 16) & 255) * a + ((dst >> 16) & 255) * ia) / 255;
  unsigned g = (((src >> 8) & 255) * a + ((dst >> 8) & 255) * ia) / 255;
  unsigned b = ((src & 255) * a + (dst & 255) * ia) / 255;
  unsigned da = dst >> 24; if (a > da) da = a;
  return (da << 24) | (r << 16) | (g << 8) | b;
}
static void px(int x, int y, uint32_t c, unsigned a)
{
  if (x < 0 || y < 0 || x >= PW || y >= PH) return;
  s_panel[y * PW + x] = blend(s_panel[y * PW + x], c, a);
}
static void rect(int x, int y, int w, int h, uint32_t c)
{
  for (int j = y; j < y + h; j++)
    for (int i = x; i < x + w; i++)
      if (i >= 0 && j >= 0 && i < PW && j < PH) s_panel[j * PW + i] = c;
}
static void rrect(int x, int y, int w, int h, int r, uint32_t c)
{
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      int dx = i < r ? r - i : (i >= w - r ? i - (w - r - 1) : 0);
      int dy = j < r ? r - j : (j >= h - r ? j - (h - r - 1) : 0);
      if (dx * dx + dy * dy <= r * r) px(x + i, y + j, c, 255);
    }
}
/* Wooden frame with a paper inside: the look of the game's signboards. */
static void board(int x, int y, int w, int h)
{
  rrect(x, y, w, h, 10, COL_WOOD_DARK);
  rrect(x + 3, y + 3, w - 6, h - 6, 8, COL_WOOD);
  for (int j = y + 6; j < y + h - 6; j += 7)      /* wood grain */
    for (int i = x + 6; i < x + w - 6; i++)
      if (((i * 7 + j * 3) % 23) < 2) px(i, j, COL_WOOD_LIGHT, 90);
  rrect(x + 9, y + 9, w - 18, h - 18, 6, COL_PAPER);
}

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
static const HmGlyph *glyph(const HmFont *f, uint32_t cp)
{
  for (int i = 0; i < f->n; i++) if (f->g[i].cp == cp) return &f->g[i];
  for (int i = 0; i < f->n; i++) if (f->g[i].cp == '?') return &f->g[i];
  return &f->g[0];
}
static int text_w(const HmFont *f, const char *t)
{
  int w = 0;
  while (*t) w += glyph(f, utf8_next(&t))->adv;
  return w;
}
static int text(const HmFont *f, int x, int y, const char *t, uint32_t col)
{
  while (*t) {
    const HmGlyph *g = glyph(f, utf8_next(&t));
    for (int j = 0; j < f->h; j++)
      for (int i = 0; i < g->w; i++) {
        unsigned a = f->a[g->off + j * g->w + i];
        if (a) px(x + i - 1, y + j, col, a);
      }
    x += g->adv;
  }
  return x;
}
static void text_c(const HmFont *f, int cx, int y, const char *t, uint32_t col)
{
  text(f, cx - text_w(f, t) / 2, y, t, col);
}
static void text_r(const HmFont *f, int rx, int y, const char *t, uint32_t col)
{
  text(f, rx - text_w(f, t), y, t, col);
}

/* A heart, filled from the bottom up to `fill` (0..1). The implicit curve
 * (x^2 + y^2 - 1)^3 - x^2 y^3 <= 0, sampled 4x per pixel for soft edges. */
static void heart(int cx, int cy, int size, double fill, uint32_t col)
{
  for (int j = -size; j <= size; j++)
    for (int i = -size; i <= size; i++) {
      int hits = 0;
      for (int sj = 0; sj < 2; sj++)
        for (int si = 0; si < 2; si++) {
          double X = (i + 0.25 + si * 0.5) / (size * 0.85);
          double Y = -(j + 0.25 + sj * 0.5) / (size * 0.85) + 0.25;
          double a = X * X + Y * Y - 1;
          if (a * a * a - X * X * Y * Y * Y <= 0) hits++;
        }
      if (!hits) continue;
      double level = 1.0 - (double)(j + size) / (2.0 * size);
      uint32_t c = level <= fill ? col : COL_PAPER_DARK;
      px(cx + i, cy + j, c, hits * 255 / 4);
    }
}

/* ── Game data ───────────────────────────────────────────────────────── */
typedef struct { uint8_t id; const char *name; } ToolName;
/* ids are the game's own item numbers (hm_ram.h, $0921) */
static const ToolName kTools[] = {
  {0x01, "Sickle"}, {0x02, "Hoe"}, {0x03, "Hammer"}, {0x04, "Ax"},
  {0x10, "Watering Can"}, {0x05, "Corn Seeds"}, {0x06, "Tomato Seeds"},
  {0x07, "Potato Seeds"}, {0x08, "Turnip Seeds"}, {0x0C, "Grass Seeds"},
  {0x0E, "Milker"}, {0x0F, "Brush"}, {0x0B, "Bell"}, {0x09, "Cow Medicine"},
  {0x0A, "Miracle Potion"}, {0x0D, "Paint"}, {0x11, "Gold Sickle"}, {0x12, "Gold Hoe"},
  {0x13, "Gold Hammer"}, {0x14, "Gold Ax"}, {0x15, "Sprinkler"},
  {0x16, "Magic Beans"}, {0x17, "Blue Diamond"}, {0x18, "Blue Feather"},
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

/* Rows of the tools tab: 0 = hand, 1 = backpack, then shed items. */
static int s_tool_rows[2 + NTOOLS];
static int s_ntool_rows;
static void build_tool_rows(void)
{
  s_ntool_rows = 0;
  s_tool_rows[s_ntool_rows++] = -1;   /* hand */
  s_tool_rows[s_ntool_rows++] = -2;   /* backpack */
  for (int i = 0; i < NTOOLS; i++)
    if (in_shed(kTools[i].id)) s_tool_rows[s_ntool_rows++] = i;
  if (s.cursor >= s_ntool_rows) s.cursor = s_ntool_rows - 1;
  if (s.cursor < 0) s.cursor = 0;
}

static void say(const char *m) { snprintf(s.message, sizeof s.message, "%s", m); }

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

static void action_a(void)
{
  if (s.tab != 0) return;
  if (!player_free()) { say(T("You can't change tools right now.")); return; }
  int r = s_tool_rows[s.cursor];
  if (r >= 0) equip_from_shed(kTools[r].id);
  else if (r == -2 && g_ram[HM_TOOL_BACKPACK]) {
    uint8_t t = g_ram[HM_TOOL_SELECTED];       /* the game's own X swap */
    g_ram[HM_TOOL_SELECTED] = g_ram[HM_TOOL_BACKPACK];
    g_ram[HM_TOOL_BACKPACK] = t;
    say(T("Swapped hand and backpack."));
  }
  build_tool_rows();
}
static void action_x(void)   /* swap hand and backpack */
{
  if (s.tab != 0) return;
  if (!player_free()) { say(T("You can't change tools right now.")); return; }
  uint8_t t = g_ram[HM_TOOL_SELECTED];
  g_ram[HM_TOOL_SELECTED] = g_ram[HM_TOOL_BACKPACK];
  g_ram[HM_TOOL_BACKPACK] = t;
  say(T("Swapped hand and backpack."));
}
static void action_y(void)   /* put the selected hand/backpack item away */
{
  if (s.tab != 0) return;
  if (!player_free()) { say(T("You can't change tools right now.")); return; }
  int r = s_tool_rows[s.cursor];
  uint32_t slot = r == -2 ? HM_TOOL_BACKPACK : HM_TOOL_SELECTED;
  uint8_t id = g_ram[slot];
  int row; uint8_t mask;
  if (!id) { say(T("Nothing to put away.")); return; }
  if (!shed_slot(id, &row, &mask) || row > 2) { say(T("That doesn't go in the tool shed.")); return; }
  set_in_shed(id, true);
  g_ram[slot] = 0;
  char m[96]; snprintf(m, sizeof m, T("Put away: %s"), tool_name(id)); say(m);
  build_tool_rows();
}

/* ── Tabs ────────────────────────────────────────────────────────────── */
static const char *kTabs[3] = { "Tools", "Hearts", "Farm" };

static void draw_tools(int x, int y)
{
  const HmFont *b = &kHmFont_body;
  for (int i = 0; i < s_ntool_rows; i++) {
    int r = s_tool_rows[i];
    int ry = y + (i < 2 ? i * 22 : 64 + (i - 2) * 19);
    if (i == 2) text(&kHmFont_small, x, y + 46, T("IN THE TOOL SHED"), COL_INK_SOFT);
    if (ry > 372) break;
    if (i == s.cursor) rrect(x - 6, ry - 2, 300, b->h + 4, 4, COL_SELECT);
    if (r < 0) {
      const char *label = r == -1 ? T("In hand:") : T("In backpack:");
      uint8_t id = g_ram[r == -1 ? HM_TOOL_SELECTED : HM_TOOL_BACKPACK];
      int nx = text(b, x, ry, label, COL_INK_SOFT);
      text(b, nx + 6, ry, tool_name(id), id ? COL_INK : COL_GRAY);
    } else {
      int nx = text(b, x + 10, ry, T(kTools[r].name), COL_INK);
      uint8_t id = kTools[r].id;
      uint32_t cnt = 0;
      if (id >= 0x05 && id <= 0x08) cnt = g_ram[HM_SEEDS_CORN_N + (id - 0x05)];
      if (id == 0x0C) cnt = g_ram[HM_SEEDS_GRASS_N];
      if (cnt) { char c[16]; snprintf(c, sizeof c, "×%u", (unsigned)cnt); text(b, nx + 6, ry, c, COL_INK_SOFT); }
    }
  }
  /* help column */
  /* Small type: the longest translation ("X  hand ↔ backpack") must fit
   * between the list and the board's edge. */
  const HmFont *h = &kHmFont_small;
  int hx = x + 308;
  text(h, hx, y + 2, T("A  equip"), COL_INK);
  text(h, hx, y + 20, T("X  hand ↔ backpack"), COL_INK);
  text(h, hx, y + 38, T("Y  put away in"), COL_INK);
  text(h, hx + 15, y + 52, T("the tool shed"), COL_INK);
  if (!player_free())
    text(&kHmFont_small, hx, y + 90, T("(only while you can move)"), COL_RED);
  if (s.message[0]) text(&kHmFont_small, x, 368, s.message, COL_GREEN);
}

static void draw_hearts(int x, int y)
{
  static const char *names[5] = { "Maria", "Ann", "Nina", "Ellen", "Eve" };
  const HmFont *b = &kHmFont_body;
  for (int i = 0; i < 5; i++) {
    int ry = y + i * 58;
    unsigned v = hm_rd16(g_ram, HM_HEARTS_BASE + i * 2);
    if (v > 999) v = 999;
    heart(x + 16, ry + 14, 14, v / 999.0, COL_RED);
    text(&kHmFont_title, x + 40, ry + 2, T(names[i]), COL_INK);
    /* bar with the thresholds the game's code tests */
    int bx = x + 140, bw = 250, by = ry + 8;
    rrect(bx, by, bw, 12, 5, COL_PAPER_DARK);
    int fw = (int)(bw * v / 999);
    if (fw > 0) rrect(bx, by, fw < 10 ? 10 : fw, 12, 5, COL_PINK);
    static const unsigned marks[3] = { 200, 450, 650 };
    for (int k = 0; k < 3; k++) {
      int mx = bx + (int)(bw * marks[k] / 999);
      rect(mx, by - 3, 2, 18, v >= marks[k] ? COL_RED : COL_INK_SOFT);
    }
    char num[16]; snprintf(num, sizeof num, "%u", v);
    text_r(b, bx + bw + 46, by - 2, num, COL_INK);
  }
  text(&kHmFont_small, x, y + 284, T("Marks: 200 heart event · 450 first child ·"), COL_INK_SOFT);
  text(&kHmFont_small, x, y + 297, T("650 second child · maximum 999"), COL_INK_SOFT);
}

static void draw_farm(int x, int y)
{
  static const char *days[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
  static const char *seasons[4] = { "Spring", "Summer", "Fall", "Winter" };
  static const char *weather[12] = { "Sunny", "Rain", "Snow", "Hurricane", "Fair", "Storm",
    "Flower Festival", "Harvest Festival", "Thanksgiving",
    "Star Night Festival", "New Year", "Egg Festival" };
  const HmFont *b = &kHmFont_body;
  uint8_t *r = g_ram;
  char v[96];
  int row = 0, lx = x, vx = x + 190;
#define LINE(label, ...) do { snprintf(v, sizeof v, __VA_ARGS__); \
    text(b, lx, y + row * 26, T(label), COL_INK_SOFT); text(b, vx, y + row * 26, v, COL_INK); row++; } while (0)
  /* Argument order is fixed (weekday, season, day, year); each language's
   * format string arranges the words around it. */
  LINE("Date", T("%s, %s %u, Year %u"), T(days[r[HM_WEEKDAY] % 7]),
       T(seasons[r[HM_SEASON] & 3]), r[HM_DAY], r[HM_YEAR] + 1u);
  LINE("Time", "%u:%02u", r[HM_HOUR], (r[HM_MINUTES] % 15) * 4);
  LINE("Money", "%u G", (unsigned)hm_rd24(r, HM_MONEY) * 10u);
  LINE("Shipped today", "%u G", (unsigned)hm_rd24(r, HM_SHIPPING_MONEY) * 10u);
  LINE("Lumber", "%u", hm_rd16(r, HM_STORED_WOOD));
  LINE("Fodder", "%u", hm_rd16(r, HM_STORED_GRASS));
  LINE("Cows", "%u", r[HM_COW_N]);
  LINE("Happiness", "%u / 999", hm_rd16(r, HM_HAPPINESS));
  LINE("Dog hugs", "%u", hm_rd16(r, HM_DOG_HUGS));
  LINE("Tomorrow's weather", "%s", r[HM_WEATHER_TOMORROW] < 12 ? T(weather[r[HM_WEATHER_TOMORROW]]) : "?");
  LINE("Stamina", "%u / %u", r[HM_CURRENT_STAMINA], r[HM_MAX_STAMINA]);
#undef LINE
}

static void render(void)
{
  for (int i = 0; i < PW * PH; i++) s_panel[i] = COL_DIM;
  board(16, 16, PW - 32, PH - 32);
  /* tabs */
  int tx = 34;
  for (int t = 0; t < 3; t++) {
    int w = text_w(&kHmFont_body, T(kTabs[t])) + 24;
    rrect(tx, 30, w, 26, 6, t == s.tab ? COL_GREEN : COL_PAPER_DARK);
    text(&kHmFont_body, tx + 12, 35, T(kTabs[t]), t == s.tab ? COL_WHITE : COL_INK_SOFT);
    tx += w + 8;
  }
  text_r(&kHmFont_small, PW - 36, 38, T("L / R  tabs"), COL_INK_SOFT);
  rect(30, 62, PW - 60, 2, COL_PAPER_DARK);
  if (s.tab == 0) draw_tools(42, 78);
  else if (s.tab == 1) draw_hearts(42, 78);
  else draw_farm(42, 80);
  rect(30, 392, PW - 60, 2, COL_PAPER_DARK);
  text_c(&kHmFont_small, PW / 2, 400, T("B / M / Esc  close     ·     the game is paused"), COL_INK_SOFT);
}

/* ── Input ───────────────────────────────────────────────────────────── */
static void nav(int dir)   /* -1 up, +1 down */
{
  if (s.tab != 0) return;
  s.cursor += dir;
  if (s.cursor < 0) s.cursor = s_ntool_rows - 1;
  if (s.cursor >= s_ntool_rows) s.cursor = 0;
  s.message[0] = 0;
}
static void tab(int dir) { s.tab = (s.tab + 3 + dir) % 3; s.message[0] = 0; }
static void close_menu(void) { s.open = false; }

static void do_action(char a)
{
  switch (a) {
    case 'u': nav(-1); break;
    case 'd': nav(+1); break;
    case 'l': case 'L': tab(-1); break;
    case 'r': case 'R': tab(+1); break;
    case 'A': action_a(); break;
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
    case SDLK_LEFT:  case SDLK_q: case SDLK_c: do_action('L'); break;
    case SDLK_RIGHT: case SDLK_e: case SDLK_v: case SDLK_TAB: do_action('R'); break;
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
  if (pressed & (SNES_PAD_L | SNES_PAD_LEFT)) do_action('L');
  if (pressed & (SNES_PAD_R | SNES_PAD_RIGHT)) do_action('R');
  uint32_t dir = in & (SNES_PAD_UP | SNES_PAD_DOWN);
  if (dir && (pressed & dir)) {
    do_action(dir & SNES_PAD_UP ? 'u' : 'd');
    s.held_dir = dir; s.held_since = now; s.last_repeat = now;
  } else if (dir && dir == s.held_dir && now - s.held_since > SNES_OVL_REPEAT_DELAY &&
             now - s.last_repeat > SNES_OVL_REPEAT_RATE) {
    do_action(dir & SNES_PAD_UP ? 'u' : 'd');
    s.last_repeat = now;
  } else if (!dir) s.held_dir = 0;

  /* headless self-test: one scripted action every 6 pumps, then close */
  if (s.test_keys[0] && s.test_pumps++ % 6 == 5) {
    char c = s.test_keys[s.test_pos];
    if (c) { s.test_pos++; do_action(c); }
    else close_menu();
  }
}

/* ── Host hooks ──────────────────────────────────────────────────────── */
static void open_menu(void)
{
  s.open = true;
  s.nav_primed = false;
  s.message[0] = 0;
  build_tool_rows();
  render();
}

int hm_pause_poll_open(uint32_t inputs, uint32_t gamepad_raw)
{
  (void)inputs;
  s.frames_polled++;
  if (s.test_open_at == -2) {
    /* HM_PAUSE_TEST=<frame>:<actions>  u d l r A B X Y, e.g. 6100:RRRLLAd */
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
void hm_pause_handle_key(int k, int repeat) { key(k, repeat); }
void hm_pause_poll_nav(uint32_t inputs, uint32_t now_ms) { pad_nav(inputs, now_ms); }
int hm_pause_image(const uint32_t **panel, int *w, int *h)
{
  if (!s.open) return 0;
  *panel = s_panel; *w = PW; *h = PH;
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
