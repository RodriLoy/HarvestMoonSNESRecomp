/*
 * hm_qol.c — quality-of-life plugins for Harvest Moon.
 *
 * Package "harvestmoon.qol" (mods/preloaded/packages/harvestmoon.qol).
 * Same rules as the cheats: act at frame boundaries, and only ever do what
 * the game itself could do.
 */

#include <stdbool.h>
#include <string.h>

#include "common_rtl.h"
#include "mod_runtime.h"
#include "hm_ram.h"

#define PKG "harvestmoon.qol"

static struct {
  bool fast_text;
  /* longer days: let `allow` clock ticks through out of every `period` */
  bool slow_clock;
  unsigned allow, period, phase;
  bool have_expect;
  uint8_t expect;
  bool frame_hooked;
} s;

static void qol_frame(void)
{
  uint8_t *ram = g_ram;

  /* Fast text: exactly what the game does while A is held ($84:CF38). The
   * printer prints a glyph on the frame it finds 4 here, so text runs at one
   * glyph per frame instead of one per four. */
  if (s.fast_text && ram[HM_TEXT_TICK] < 4)
    ram[HM_TEXT_TICK] = 4;

  /* Longer days. UpdateTime adds 1 to HM_SECONDS each frame the clock runs.
   * To cancel the NEXT tick we pre-decrement now, so the game's +1 brings it
   * back. Only after a frame on which the game actually ticked: if it stops
   * calling UpdateTime (menus, scenes) nothing accumulates. Pre-decrementing
   * 0 gives 255, which the game's 8-bit INC wraps back to 0 without a minute
   * change (it compares for == 60). */
  if (s.slow_clock) {
    if (!hm_diary_loaded(ram) || (hm_rd16(ram, HM_TIME_RUNNING) & 3) != 1) {
      s.have_expect = false;
    } else {
      uint8_t cur = ram[HM_SECONDS];
      bool ticked = s.have_expect && cur != s.expect;
      if (ticked) {
        s.phase = (s.phase + 1) % s.period;
        bool let_next_through = s.phase < s.allow;
        if (!let_next_through)
          ram[HM_SECONDS] = (uint8_t)(cur - 1);
      }
      s.expect = ram[HM_SECONDS];
      s.have_expect = true;
    }
  }
}

static void hook_frame(void)
{
  if (!s.frame_hooked) {
    snes_mod_register_frame_callback(qol_frame);
    s.frame_hooked = true;
  }
}

static void on_fast_text(void) { s.fast_text = true; hook_frame(); }

static void on_slow_clock(void)
{
  char v[16] = "2";
  snes_mod_runtime_feature_option_value_c(PKG, "day_length", "factor", v, sizeof v);
  /* factor F: one tick in F passes (1.5 = two in three) */
  if (!strcmp(v, "1.5"))    { s.allow = 2; s.period = 3; }
  else if (!strcmp(v, "3")) { s.allow = 1; s.period = 3; }
  else if (!strcmp(v, "4")) { s.allow = 1; s.period = 4; }
  else                      { s.allow = 1; s.period = 2; }
  s.slow_clock = true;
  hook_frame();
}

static void reset_qol(void)
{
  bool hooked = s.frame_hooked;
  memset(&s, 0, sizeof s);
  s.frame_hooked = hooked;
}

SNES_MOD_CONSTRUCTOR(register_hm_qol)
{
  snes_mod_register_reset_callback(reset_qol);
  snes_mod_register_activation_plugin("harvestmoon.qol.fast_text", on_fast_text);
  snes_mod_register_activation_plugin("harvestmoon.qol.day_length", on_slow_clock);
}
