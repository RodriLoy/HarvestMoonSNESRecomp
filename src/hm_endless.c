/*
 * hm_endless.c — endless game (feature harvestmoon.qol.endless).
 *
 * The game ends on the morning of Summer 30, Year 3: when the house map
 * loads, its map handler ($83:DED3) compares the date with Y3/Summer/30 and
 * jumps to the ending ($83:F4D8: parents' visit, evaluation, credits). The
 * ending finishes on a night landscape that never leaves.
 *
 * With the feature on, the ending still plays in full. Then the game goes
 * back to that same morning and carries on (Fall Year 3, Year 4, 5...):
 *
 *   1. While it is Summer 30 of Year 3 and the ending has not begun, the
 *      whole machine is kept in memory every frame. It has to be the
 *      rollback form of a save state (the one netplay uses): that morning is
 *      mid-transition, the CPU is inside a routine at the frame boundary, and
 *      only the rollback form carries where the interpreter must resume. This covers both ways of reaching that morning:
 *      sleeping on the 29th, or loading a diary saved that night.
 *   2. The ending begins: the game sets bit 1 of $7F1F5E. The kept state is
 *      the frame just before, and it is frozen.
 *   3. The final landscape is reached (map $36, clock flag on, the "ending
 *      shown" bit $8000 of $7F1F5E) and stays for a few seconds: the kept
 *      state is loaded back, and the date check is told to fail once
 *      (hm_endless_ending_day(), patched into the generated code by
 *      tools/apply_overrides.py), so the house loads like any other day.
 *
 * Feature off: hm_endless_ending_day() returns the ROM's own 30 and nothing
 * else here runs, so the game is untouched.
 *
 * Years after the third. The year is one byte and only Year 3 is special.
 * The one thing that breaks is the diary screen's ordinal ("1st", "2nd",
 * "3rd"), read from a three-entry table ($82:F298): hm_year_suffix() supplies
 * "th" (and "st"/"nd"/"rd" for 21st, 22nd...) past the end of it.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common_rtl.h"
#include "hm_ram.h"
#include "hm_endless.h"

#define FLAGS_5E      0x11F5E   /* 16b; bit 1 ending started, bit 15 shown */
#define ENDING_MAP    0x36      /* the closing landscape */
#define SETTLE_FRAMES 900       /* final landscape this long (15 s), then back;
                                 * the love texts before it keep the clock
                                 * flag on for at most ~300 frames each */

static bool g_enabled;
static void  *g_state;          /* the kept frame */
static size_t g_state_cap, g_state_len;
static enum { WATCH, IN_ENDING } g_phase;
static bool g_skip_check;       /* the reloaded morning: no ending this time */
static int  g_settle;

void hm_endless_set_enabled(bool on) {
  g_enabled = on;
  if (!on) { g_phase = WATCH; g_skip_check = false; g_state_len = 0; }
}

static bool is_last_day(const uint8_t *r) {
  return r[HM_YEAR] == 2 && r[HM_SEASON] == 1 && r[HM_DAY] == 30;
}
static uint16_t flags5e(const uint8_t *r) { return r[FLAGS_5E] | r[FLAGS_5E + 1] << 8; }

uint8_t hm_endless_ending_day(void) {
  if (g_enabled && g_skip_check) return 0xFF;   /* never equals the day */
  return 0x1E;
}

static bool keep_state(void) {
  if (!g_state) {
    g_state_cap = RtlRollbackSnapshotBound();
    if (!g_state_cap) return false;
    g_state = malloc(g_state_cap);
    if (!g_state) return false;
  }
  g_state_len = RtlRollbackSaveToMemory(g_state, g_state_cap);
  return g_state_len != 0;
}

void hm_endless_after_frame(void) {
  if (!g_enabled) return;
  const uint8_t *r = g_ram;
  static int dbg = -1, fr;
  if (dbg < 0) dbg = getenv("HM_ENDLESS_DEBUG") != NULL;
  fr++;
  if (dbg && is_last_day(r))
    fprintf(stderr, "endless f=%d phase=%d map=%02x tr=%02x f5e=%04x len=%zu skip=%d gs=%04x\n", fr, g_phase,
            r[HM_MAP_ID], r[HM_TIME_RUNNING], flags5e(r), g_state_len, g_skip_check, r[0xD2] | r[0xD3] << 8);
  bool last_day = is_last_day(r);
  uint16_t f = flags5e(r);

  if (!last_day) {                  /* a different day: back to normal */
    g_skip_check = false;
    g_phase = WATCH;
    g_state_len = 0;
    return;
  }
  switch (g_phase) {
  case WATCH:
    if (f & 0x0002) {               /* the ending just began */
      if (g_state_len) { g_phase = IN_ENDING; g_settle = 0; }
      else fprintf(stderr, "hm_endless: ending began with no resumable frame kept; "
                           "it will play as in the original\n");
      return;
    }
    /* Only frames the machine can resume from: not in the middle of a
     * screen transition ($D2 bit 14), where the CPU sits inside a loader
     * that a restored state cannot re-enter (measured: restoring the frame
     * just before the house loads hangs on the blank screen). */
    if (!g_skip_check && !((r[0xD2] | r[0xD3] << 8) & 0x4000)) keep_state();
    return;
  case IN_ENDING: {
    bool final_scene = r[HM_MAP_ID] == ENDING_MAP && (f & 0x8000) &&
                       (r[HM_TIME_RUNNING] & 1);
    g_settle = final_scene ? g_settle + 1 : 0;
    if (g_settle < SETTLE_FRAMES) return;
    if (RtlRollbackLoadFromMemory(g_state, g_state_len)) {
      g_skip_check = true;          /* the morning resumes as a normal day */
      fprintf(stderr, "hm_endless: ending shown, back to Summer 30 Year 3\n");
    }
    g_phase = WATCH;
    return;
  }
  }
}

uint16_t hm_year_suffix(uint16_t x, uint16_t rom_value) {
  if (x < 12) return rom_value;     /* 1st, 2nd, 3rd: the game's own table */
  unsigned year = x / 4u + 1u;
  unsigned n100 = year % 100u, n10 = year % 10u;
  const char *s = (n100 >= 11 && n100 <= 13) ? "th"
                : n10 == 1 ? "st" : n10 == 2 ? "nd" : n10 == 3 ? "rd" : "th";
  return (uint16_t)(s[(x & 2) ? 1 : 0] - 'a');   /* font: 'a' is tile 0 */
}
