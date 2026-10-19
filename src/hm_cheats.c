/*
 * hm_cheats.c — trusted cheat plugins for Harvest Moon.
 *
 * Each cheat is a feature of the built-in "harvestmoon.cheats" package
 * (mods/preloaded/packages/harvestmoon.cheats). The launcher's Mods page turns
 * them on and off; on Play the runtime calls the activation plugin of every
 * enabled feature, and one frame callback then applies the active cheats at
 * each frame boundary.
 *
 * Rules every cheat follows:
 *   - Do nothing until a diary is loaded (hm_diary_loaded): the farm-data
 *     block is zero during the intro/title and must not be written there.
 *   - Only ever write values the game itself can produce (the caps come from
 *     the game's own Add* routines, see hm_ram.h), so a save made with a cheat
 *     on stays a valid save with it off.
 */

#include <stdbool.h>
#include <string.h>

#include "common_rtl.h"   /* g_ram */
#include "mod_runtime.h"
#include "hm_ram.h"

#define PKG "harvestmoon.cheats"

enum WeatherMode { WEATHER_STOCK, WEATHER_SUNNY, WEATHER_RAIN, WEATHER_NO_STORMS };

static struct {
  bool money, stamina, freeze_clock, stock, bags;
  enum WeatherMode weather;
  bool frame_hooked;
  /* freeze_clock: the moment being held, and the day it belongs to */
  bool clock_held;
  uint8_t held_hour, held_min, held_day, held_season, held_year;
} s;

static void cheats_frame(void)
{
  uint8_t *ram = g_ram;
  if (!hm_diary_loaded(ram)) { s.clock_held = false; return; }

  if (s.money && hm_rd24(ram, HM_MONEY) != HM_MONEY_MAX)
    hm_wr24(ram, HM_MONEY, HM_MONEY_MAX);

  if (s.stamina && ram[HM_CURRENT_STAMINA] < ram[HM_MAX_STAMINA])
    ram[HM_CURRENT_STAMINA] = ram[HM_MAX_STAMINA];

  if (s.stock) {
    if (hm_rd16(ram, HM_STORED_WOOD) < HM_STOCK_MAX)  hm_wr16(ram, HM_STORED_WOOD, HM_STOCK_MAX);
    if (hm_rd16(ram, HM_STORED_GRASS) < HM_STOCK_MAX) hm_wr16(ram, HM_STORED_GRASS, HM_STOCK_MAX);
  }

  if (s.bags) {
    /* Refill only bags the player already owns (count > 0): a zero count
     * means the bag is not in the inventory at all. */
    static const uint32_t bags[] = {
      HM_SEEDS_GRASS_N, HM_SEEDS_CORN_N, HM_SEEDS_TOMATO_N, HM_SEEDS_POTATO_N,
      HM_SEEDS_TURNIP_N, HM_FEED_COW_N, HM_FEED_CHICKS_N,
    };
    for (size_t i = 0; i < sizeof bags / sizeof bags[0]; i++)
      if (ram[bags[i]] != 0 && ram[bags[i]] < 99) ram[bags[i]] = 99;
  }

  if (s.freeze_clock) {
    /* Only hold the clock while the game itself is running it. Whenever the
     * game stops time (new diary setup, cutscenes, menus, sleeping) it may
     * SET the clock -- 6:00 on a new day, 7:00 after the intro -- and those
     * writes must pass through; the held moment is re-taken afterwards.
     * (First version held from diary load and pinned a new game at 0:00.) */
    bool running = hm_rd16(ram, HM_TIME_RUNNING) == 1;
    bool same_day = ram[HM_DAY] == s.held_day && ram[HM_SEASON] == s.held_season &&
                    ram[HM_YEAR] == s.held_year;
    if (!running || !s.clock_held || !same_day) {
      s.clock_held = running;
      s.held_hour = ram[HM_HOUR]; s.held_min = ram[HM_MINUTES];
      s.held_day = ram[HM_DAY]; s.held_season = ram[HM_SEASON]; s.held_year = ram[HM_YEAR];
    } else {
      ram[HM_HOUR] = s.held_hour;
      ram[HM_MINUTES] = s.held_min;
    }
  }

  if (s.weather != WEATHER_STOCK) {
    uint8_t w = ram[HM_WEATHER_TOMORROW];
    bool ordinary = w == HM_WEATHER_SUNNY || w == HM_WEATHER_RAIN ||
                    w == HM_WEATHER_SNOW || w == HM_WEATHER_HURRICANE ||
                    w == HM_WEATHER_THUNDER;
    if (ordinary) {
      uint8_t want = w;
      switch (s.weather) {
        case WEATHER_SUNNY: want = HM_WEATHER_SUNNY; break;
        case WEATHER_RAIN:
          want = ram[HM_SEASON] == 3 ? HM_WEATHER_SNOW : HM_WEATHER_RAIN; break;
        case WEATHER_NO_STORMS:
          if (w == HM_WEATHER_HURRICANE || w == HM_WEATHER_THUNDER)
            want = HM_WEATHER_SUNNY;
          break;
        default: break;
      }
      ram[HM_WEATHER_TOMORROW] = want;
    }
  }
}

static void hook_frame(void)
{
  if (!s.frame_hooked) {
    snes_mod_register_frame_callback(cheats_frame);
    s.frame_hooked = true;
  }
}

static void on_money(void)   { s.money = true;   hook_frame(); }
static void on_stamina(void) { s.stamina = true; hook_frame(); }
static void on_clock(void)   { s.freeze_clock = true; hook_frame(); }
static void on_stock(void)   { s.stock = true;   hook_frame(); }
static void on_bags(void)    { s.bags = true;    hook_frame(); }
static void on_weather(void)
{
  char v[32] = "sunny";
  snes_mod_runtime_feature_option_value_c(PKG, "weather", "mode", v, sizeof v);
  s.weather = !strcmp(v, "rain") ? WEATHER_RAIN
            : !strcmp(v, "no_storms") ? WEATHER_NO_STORMS : WEATHER_SUNNY;
  hook_frame();
}

static void reset_cheats(void)
{
  bool hooked = s.frame_hooked;
  memset(&s, 0, sizeof s);
  s.frame_hooked = hooked;
}

SNES_MOD_CONSTRUCTOR(register_hm_cheats)
{
  snes_mod_register_reset_callback(reset_cheats);
  snes_mod_register_activation_plugin("harvestmoon.cheat.money", on_money);
  snes_mod_register_activation_plugin("harvestmoon.cheat.stamina", on_stamina);
  snes_mod_register_activation_plugin("harvestmoon.cheat.freeze_clock", on_clock);
  snes_mod_register_activation_plugin("harvestmoon.cheat.stock", on_stock);
  snes_mod_register_activation_plugin("harvestmoon.cheat.bags", on_bags);
  snes_mod_register_activation_plugin("harvestmoon.cheat.weather", on_weather);
}
