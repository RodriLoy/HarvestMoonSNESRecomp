/*
 * hm_title.c — a fourth title-screen entry, OPTIONS, below HOW TO PLAY.
 *
 * How the stock menu works (measured, see hm_ram.h):
 *   - The words are pre-drawn images on BG1 (map $6000 rows 16-21, from
 *     column 8); the copyright block is BG1 rows 23-25. Row 22 is blank.
 *   - Highlighting never touches tiles: each word has its own fill colour
 *     (CGRAM $7D CONTINUE, $7F START, $29 HOW TO PLAY) which the game sets to
 *     pink ($59DF) for the cursor line and light pink ($7F3F) otherwise, in
 *     its CGRAM shadow at $7F:0900 that NMI uploads every frame.
 *   - Input: CODE_84C458 ($95 = 5, inputstate 4). Down: $098D += 1, 2 wraps
 *     to 0. Up: -1, 0 wraps to 2. A selects. Each move plays the cursor sound.
 *
 * What this adds, without changing the game's own menu logic:
 *   - The copyright rows move down one tile row (BG1 rows 23-25 -> 24-26),
 *     freeing rows 22-23 for a fourth line.
 *   - "OPTIONS" is composed from the letters of the three stock words, read
 *     out of VRAM at run time (no game graphics are stored in this source),
 *     and drawn as a PPU host object: native priority and brightness, fades
 *     included, and it is not guest state.
 *   - The cursor gains a fourth stop. Moves into and out of it are made by
 *     pre-positioning $098D so the game's OWN move lands where we want and
 *     plays its own sound. A on OPTIONS opens the options menu (the launcher).
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "common_rtl.h"
#include "snes/ppu.h"
#include "snes_overlay_draw.h"   /* SNES_PAD_* */
#include "desktop/host_main.h"
#include "hm_ram.h"
#include "hm_title.h"

#define BG1_MAP       0x6000u   /* word address, 32x32 screen 0 */
#define BG1_TILES     0x2000u   /* word address of BG1 character data */
#define MAP(r, c)     (BG1_MAP + (r) * 32u + (c))
#define CG_CONTINUE   0x7D
#define CG_START      0x7F
#define CG_HOWTO      0x29
#define CG_OUTLINE    0x79      /* palette 7 colour 9 */

enum { OW = 96, OH = 16 };      /* OPTIONS image */
enum { CLS_NONE, CLS_OUTLINE, CLS_FILL };

static struct {
  bool enabled;                 /* feature on */
  bool option_selected;         /* cursor is on OPTIONS */
  uint32_t prev_inputs;
  uint32_t swallow;             /* consumed buttons, hidden until released */
  bool glyphs_ok, glyphs_tried;
  uint8_t cls[OH][OW];          /* OPTIONS as outline/fill classes */
  int width;
  uint16_t pink;                /* the selected-line fill colour */
  uint16_t pixels[OW * OH];
  PpuExtraObject obj;
} s;

/* ── VRAM helpers ────────────────────────────────────────────────────── */
static uint16_t vram(uint32_t w) { return g_ppu->vram[w & 0x7fff]; }

/* Pixel colour index of BG1 tile `tile` (4bpp) at (x, y). */
static int tile_px(unsigned tile, int x, int y)
{
  uint32_t base = BG1_TILES + tile * 16u;
  uint16_t p01 = vram(base + y), p23 = vram(base + 8 + y);
  int b = 7 - x;
  return ((p01 >> b) & 1) | ((p01 >> (8 + b)) & 1) << 1 |
         ((p23 >> b) & 1) << 2 | ((p23 >> (8 + b)) & 1) << 3;
}

/* The stock title is on screen: the three word images sit where measured. */
static bool title_on_screen(void)
{
  return (vram(MAP(16, 8)) & 0x3ff) == 0x101 && (vram(MAP(18, 8)) & 0x3ff) == 0x2ec &&
         (vram(MAP(20, 8)) & 0x3ff) == 0x10e;
}
static bool title_menu_active(void)
{
  return g_ram[HM_INPUT_STATE] == 4 && g_ram[0x95] == 5 && title_on_screen();
}

/* Decode a 16x(12*8) word image from BG1 rows r, r+1, columns 8..19. */
static void word_image(int r, uint8_t out[16][96])
{
  for (int c = 0; c < 12; c++)
    for (int h = 0; h < 2; h++) {
      uint16_t e = vram(MAP(r + h, 8 + c));
      unsigned t = e & 0x3ff;
      bool hf = e & 0x4000, vf = e & 0x8000;
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
          out[h * 8 + y][c * 8 + x] =
              (uint8_t)tile_px(t, hf ? 7 - x : x, vf ? 7 - y : y);
    }
}
/* Columns [x0, x1) of the non-empty runs of a word image. */
static int runs(uint8_t img[16][96], int out[][2], int max)
{
  int n = 0;
  for (int x = 0; x < 96 && n < max;) {
    bool any = false;
    for (int y = 0; y < 16; y++) if (img[y][x]) any = true;
    if (!any) { x++; continue; }
    int x0 = x;
    for (;;) {
      bool col = false;
      if (x < 96) for (int y = 0; y < 16; y++) if (img[y][x]) col = true;
      if (!col) break;
      x++;
    }
    out[n][0] = x0; out[n][1] = x; n++;
  }
  return n;
}

/* Build OPTIONS from O,N,T,I (CONTINUE), S (START) and P (HOW TO PLAY).
 * Every word's outline and fill use different colour indices, so each source
 * is normalised to two classes first. Fails closed: if the runs do not have
 * the measured shape (another ROM revision, a hack) nothing is drawn. */
static void build_glyphs(void)
{
  static uint8_t cont[16][96], start[16][96], howto[16][96];
  int rc[12][2], rs[12][2], rh[12][2];
  s.glyphs_tried = true;
  word_image(16, cont); word_image(18, start); word_image(20, howto);
  int nc = runs(cont, rc, 12), ns = runs(start, rs, 12), nh = runs(howto, rh, 12);
  if (nc < 5 || ns < 1 || nh < 5) return;
  /* CONTINUE: C O N T I ... ; START: S ...; HOW TO PLAY: H OW T O P ... */
  struct { uint8_t (*img)[96]; int x0, x1; int outline; } L[7] = {
    { cont,  rc[1][0], rc[1][1], 9 },   /* O */
    { howto, rh[4][0], rh[4][1], 7 },   /* P */
    { cont,  rc[3][0], rc[3][1], 9 },   /* T */
    { cont,  rc[4][0], rc[4][1], 9 },   /* I */
    { cont,  rc[1][0], rc[1][1], 9 },   /* O */
    { cont,  rc[2][0], rc[2][1], 9 },   /* N */
    { start, rs[0][0], rs[0][1], 9 },   /* S */
  };
  int widths[7] = {12, 12, 12, 6, 12, 14, 12};
  memset(s.cls, 0, sizeof s.cls);
  int x = 1;
  for (int i = 0; i < 7; i++) {
    int w = L[i].x1 - L[i].x0;
    if (w < widths[i] - 1 || w > widths[i] + 1) return;   /* not the expected letter */
    for (int y = 0; y < 16; y++)
      for (int j = 0; j < w && x + j < OW; j++) {
        int ci = L[i].img[y][L[i].x0 + j];
        if (ci) s.cls[y][x + j] = ci == L[i].outline ? CLS_OUTLINE : CLS_FILL;
      }
    x += w + 1;
  }
  s.width = x;
  s.glyphs_ok = true;
}

/* ── Copyright block: BG1 rows 23-25 -> 24-26 (idempotent) ───────────── */
static void move_copyright(void)
{
  uint16_t blank = vram(MAP(22, 0));
  bool row23_blank = true;
  for (int c = 0; c < 32; c++) if (vram(MAP(23, c)) != blank) row23_blank = false;
  if (row23_blank) return;   /* already moved */
  for (int r = 25; r >= 23; r--)
    for (int c = 0; c < 32; c++) g_ppu->vram[MAP(r + 1, c)] = vram(MAP(r, c));
  for (int c = 0; c < 32; c++) g_ppu->vram[MAP(23, c)] = blank;
}

/* ── Frame hooks ─────────────────────────────────────────────────────── */
void hm_title_frame_begin(uint32_t *inputs)
{
  uint32_t in = *inputs & 0xfff;
  uint32_t pressed = in & ~s.prev_inputs;
  s.prev_inputs = in;
  /* A button we consumed stays hidden from the game until it is released:
   * otherwise its second held frame reaches the game as a fresh press. */
  s.swallow &= in;
  *inputs &= ~s.swallow;
  if (!s.enabled || !s.glyphs_ok || !title_menu_active()) {
    if (!title_on_screen()) s.option_selected = false;
    return;
  }
  uint8_t *cursor = &g_ram[HM_TITLE_CURSOR];
  if (s.option_selected) {
    if (pressed & (SNES_PAD_A | SNES_PAD_START)) {
      s.swallow |= in & (SNES_PAD_A | SNES_PAD_START);
      *inputs &= ~s.swallow;
      snesrecomp_desktop_request_launcher();
    } else if (pressed & SNES_PAD_UP) {
      *cursor = 0;                      /* game's Up: 0 -> 2 (HOW TO PLAY) */
      s.option_selected = false;
    } else if (pressed & SNES_PAD_DOWN) {
      *cursor = 2;                      /* game's Down: 2 -> 0 (CONTINUE) */
      s.option_selected = false;
    }
  } else {
    if ((pressed & SNES_PAD_DOWN) && *cursor == 2) {
      *cursor = 1;                      /* game's Down: 1 -> 2, then OPTIONS */
      s.option_selected = true;
    } else if ((pressed & SNES_PAD_UP) && *cursor == 0) {
      s.option_selected = true;         /* game's Up: 0 -> 2, then OPTIONS */
    }
  }
}

static uint16_t cg(int i) { return g_ppu->cgram[i & 0xff]; }

void hm_title_frame_end(void)
{
  if (!s.enabled || !title_on_screen()) {
    PpuSetExtraObjects(g_ppu, NULL, 0);
    s.glyphs_tried = s.glyphs_ok = false;
    s.option_selected = false;
    return;
  }
  if (!s.glyphs_tried) build_glyphs();
  if (!s.glyphs_ok) { PpuSetExtraObjects(g_ppu, NULL, 0); return; }
  move_copyright();

  uint8_t cursor = g_ram[HM_TITLE_CURSOR];
  /* The fill of the line the game has highlighted is the pink; any other
   * line's fill is the plain colour. */
  static const int fills[3] = { CG_CONTINUE, CG_START, CG_HOWTO };
  uint16_t sel_fill = cg(fills[cursor % 3]);
  uint16_t plain = cg(fills[(cursor + 1) % 3]);
  if (sel_fill != plain) s.pink = sel_fill;
  uint16_t fill = plain;
  if (s.option_selected && title_menu_active()) {
    fill = s.pink ? s.pink : sel_fill;
    /* un-highlight HOW TO PLAY in the shadow the game uploads, and now */
    hm_wr16(g_ram, HM_CGRAM_SHADOW + CG_HOWTO * 2, plain);
    g_ppu->cgram[CG_HOWTO] = plain;
  }
  uint16_t outline = cg(CG_OUTLINE);
  for (int y = 0; y < OH; y++)
    for (int x = 0; x < OW; x++) {
      uint8_t k = s.cls[y][x];
      s.pixels[y * OW + x] = k == CLS_OUTLINE ? (uint16_t)(0x8000 | outline)
                           : k == CLS_FILL    ? (uint16_t)(0x8000 | fill) : 0;
    }
  s.obj = (PpuExtraObject){ .x = 64, .y = 22 * 8, .width = OW, .height = OH,
                            .stride = OW, .priority = 3, .math = 0, .oamSlot = 0,
                            .order = 1, .pixels = s.pixels };
  PpuSetExtraObjects(g_ppu, &s.obj, 1);
}

void hm_title_set_enabled(bool on)
{
  s.enabled = on;
  if (!on && g_ppu) PpuSetExtraObjects(g_ppu, NULL, 0);
}
