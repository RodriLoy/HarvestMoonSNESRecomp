/*
 * hm_widescreen.c — 16:9 for Harvest Moon (feature harvestmoon.qol.widescreen).
 *
 * The PPU renders real extra columns either side of the authentic 256 (the
 * runner's native widescreen field, 342 px = 43 px a side). What is shown in
 * them is decided here, per frame, from the game's own state:
 *
 *   Backgrounds. BG1 (the map) lives in a 64x64-tile ring buffer that the game
 *   streams well ahead of the camera, so the columns just outside the native
 *   view hold real map data. The only place they do not is past the edge of
 *   the map, so each side's margin is exactly the distance from the camera to
 *   the map's own camera clamp ($ED left / $F1 right, loaded with each map):
 *   in the middle of the farm both sides open fully; walking up to the edge of
 *   the map, that side closes to the original black border; a one-screen room
 *   (clamps 0/0) stays 4:3, centred. The camera is read from the scroll the
 *   PPU actually rendered with as well as from the WRAM camera, and the
 *   smaller margin wins, so a frame where the two disagree never shows a
 *   column outside the map (WIDESCREEN_PATTERNS P1/P4).
 *
 *   BG3 is the text and clock layer. It stays in the native 256 columns.
 *
 *   Screens that are not a map (intro, title, menus, name entry) stay 4:3.
 *   The gate is the map loader's own record (P5): $22 < $57 is a map with a
 *   camera range; $57 and up are the single-picture screens (intro, title,
 *   name entry...). Measured across the intro, title, demo and a new game.
 *
 *   Sprites. PrepareOAMData ($85:83E0) drops an object component whose screen
 *   X is outside [-16, 256) — the OAM emitter gate (P8). The recompiler folds
 *   that compare's two immediates ($85:853B CMP #$0100, $85:8540 CMP #$FFF0)
 *   into the C; tools/apply_overrides.py (a generation postprocess hook,
 *   recomp/generate.toml) swaps them for hm_ws_oam_right_limit() /
 *   hm_ws_oam_left_limit() below, which return the ROM's own values unless
 *   a margin is open. The PPU's 9-bit X already places 256..298 in the right
 *   margin and 472..511 in the left one. Nothing else in the simulation
 *   changes (P16): camera, collision, AI and RNG never see the margins.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common_rtl.h"
#include "snes/ppu.h"
#include "desktop/display_aspect.h"
#include "hm_ram.h"
#include "hm_widescreen.h"

static struct {
  bool enabled;
  int oam_l, oam_r;                  /* margins the OAM clip admits */
  int last_l, last_r;
  int window_fw;                     /* field width the window was sized for */
} s = { .window_fw = 256 };

/* Host (desktop/host_main.c): resize the window to the current scale for the
 * current field width. */
void ChangeWindowScale(int scale_step);

static bool ws_debug(void)
{
  static int v = -1;
  if (v < 0) { const char *e = getenv("HM_WS_DEBUG"); v = e && *e && *e != '0'; }
  return v;
}

/* ── OAM emitter bounds (called from the generated PrepareOAMData) ───── */
uint16 hm_ws_oam_right_limit(void) { return (uint16)(0x0100 + s.oam_r); }
uint16 hm_ws_oam_left_limit(void)  { return (uint16)(0xFFF0 - s.oam_l); }

static void set_oam_bounds(int left, int right) { s.oam_l = left; s.oam_r = right; }

/* ── Per-frame policy ────────────────────────────────────────────────── */
static bool in_field_map(void)
{
  return g_ram[HM_MAP_ID] < HM_MAP_FIRST_PICTURE;
}

/* The three screens before the game starts, measured with the margins
 * forced open (HM-Decomp map ids):
 *   $5D  Natsume logo: a plain white picture, so it simply extends.
 *   $5C  title: the sky and clouds are a repeating layer and continue; the
 *        sign sits in the middle with empty space around it.
 *   $5E  diary selection ("New Diary" / "Choose Diary"): the green checker
 *        with the scrolling "Harvest Moon" pattern repeats and continues, but
 *        the wooden fence boxes (BG1) would repeat as well, so BG1 is kept to
 *        the native 256 columns and only the background fills the margins.
 *   $3C  intro road (the boy and his father walking): the road, fences and
 *        grass repeat every 16 pixels, but the tilemap past the 256 columns
 *        sits 8 pixels higher, so BG1/BG2 repeat the visible picture instead.
 *   $5B  the pan over the farm gate up to the sky: a real wide panorama, so it
 *        simply extends.
 * Every other single-picture screen (name entry...) stays 4:3. */
static bool open_boot_screen(uint8_t map, uint8_t *clamp_extra, uint8_t *repeat)
{
  *clamp_extra = 0;
  *repeat = 0;
  switch (map) {
  case 0x5D: case 0x5C: case 0x5B: return true;
  case 0x5E: *clamp_extra = 1u << 0; return true;
  case 0x3C: *repeat = (1u << 0) | (1u << 1); return true;
  default: return false;
  }
}

/* The world camera the PPU rendered BG1 with: its 10-bit scroll, unwrapped
 * next to the WRAM camera. */
static int ppu_camera(int wram_cam)
{
  int hs = g_ppu->hScroll[0] & 0x3ff;
  int c = (wram_cam & ~0x3ff) | hs;
  if (c - wram_cam > 512) c -= 1024;
  else if (wram_cam - c > 512) c += 1024;
  return c;
}

static int imin(int a, int b) { return a < b ? a : b; }
static int iclamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* HM_WIDESCREEN=1/0 overrides the feature (headless tests). */
static void env_override(void)
{
  static int v = -2;
  if (v == -2) { const char *e = getenv("HM_WIDESCREEN"); v = e && *e ? (*e != '0') : -1; }
  if (v >= 0) s.enabled = v;
}

void hm_ws_begin_sim_frame(unsigned number)
{
  (void)number;
  if (!g_ppu) return;
  env_override();
  int budget = g_ppu->extraLeftRight;
  /* The window is created for a 256-wide field before any feature is
   * active. When the field width changes (feature turned on or off), give
   * the window the matching shape once; fullscreen and maximized windows are
   * left alone by the host. */
  int fw = 256 + 2 * budget;
  if (fw != s.window_fw) { s.window_fw = fw; ChangeWindowScale(0); }
  int l = 0, r = 0;
  /* Boot screens that are one picture but whose background really goes on
   * past 256 columns (see open_boot_screen): open fully. */
  uint8_t clamp_extra = 0, repeat = 0;
  if (s.enabled && budget > 0 && open_boot_screen(g_ram[HM_MAP_ID], &clamp_extra, &repeat)) { l = r = budget; }
  else if (s.enabled && budget > 0 && in_field_map()) {
    int cl = hm_rd16(g_ram, HM_CAM_MIN_X), cr = hm_rd16(g_ram, HM_CAM_MAX_X);
    int cam = hm_rd16(g_ram, HM_CAM_X);
    if (cr >= cl) {
      int pc = ppu_camera(cam);
      l = imin(cam - cl, pc - cl);
      r = imin(cr - cam, cr - pc);
      l = iclamp(l, 0, budget);
      r = iclamp(r, 0, budget);
    }
  }
  PpuSetExtraSideSpace(g_ppu, l, r, 0);
  /* The line renderer only writes the columns it is given; whatever the
   * closed part of a margin held last frame would otherwise stay on screen.
   * Black it now, before this frame's lines are drawn. */
  if (budget > 0 && g_ppu->renderBuffer) {
    int width = 256 + 2 * budget;
    for (int y = 0; y < 240; y++) {
      uint32_t *row = (uint32_t *)(g_ppu->renderBuffer + (size_t)y * g_ppu->renderPitch);
      if ((int)(g_ppu->renderPitch / 4) < width) break;
      for (int x = 0; x < budget - l; x++) row[x] = 0;
      for (int x = budget + 256 + r; x < width; x++) row[x] = 0;
    }
  }
  PpuSetWidescreenLayerRepeat(g_ppu, (l || r) ? repeat : 0);
  if (l || r) PpuSetWidescreenLayerClamp(g_ppu, (uint8_t)((1u << 2) | clamp_extra));   /* BG3: text, clock */
  set_oam_bounds(s.enabled ? l : 0, s.enabled ? r : 0);
  if (ws_debug() && (l != s.last_l || r != s.last_r))
    fprintf(stderr, "[hm-ws] margins L=%d R=%d (map %02x cam %d clamp %d..%d)\n", l, r,
            g_ram[HM_MAP_ID], hm_rd16(g_ram, HM_CAM_X), hm_rd16(g_ram, HM_CAM_MIN_X),
            hm_rd16(g_ram, HM_CAM_MAX_X));
  s.last_l = l; s.last_r = r;
}

void hm_ws_prepare_frame(int drawable_w, int drawable_h, int *frame_w, int *frame_h)
{
  (void)drawable_w; (void)drawable_h; (void)frame_h;
  env_override();
  /* One fixed field while the feature is on, so the window never changes
   * shape between screens: 4:3 screens are pillarboxed inside it. */
  *frame_w = s.enabled ? SnesDisplayAspect_ComputeWideFrameWidth(256) : 256;
}

int hm_ws_field_width(void) { return s.window_fw; }

void hm_ws_set_enabled(bool on)
{
  s.enabled = on;
  if (!on) set_oam_bounds(0, 0);
}

bool hm_ws_enabled(void) { return s.enabled; }
