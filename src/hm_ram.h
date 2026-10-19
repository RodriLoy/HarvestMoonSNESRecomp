/*
 * hm_ram.h — named WRAM locations for Harvest Moon (SNES, USA).
 *
 * This is the start of the long-term decompilation: every address the port
 * touches gets a name and a note here instead of a magic number in code.
 * Sources: ElisaS-Codes/HM-Decomp (src/constants/ram.asm, RamMap.txt) and
 * the routines cited next to each entry, cross-checked against live RAM
 * dumps taken from this port and from bsnes.
 *
 * Offsets index g_ram[0x20000]: $7E:xxxx -> 0x0xxxx, $7F:xxxx -> 0x1xxxx.
 */
#pragma once

#include <stdint.h>

/* ── Clock (WeatherTomorrow $82:8CF9 and the day-change code read these) ── */
#define HM_TIME_RUNNING     0x00973 /* 16b: 0 stopped, 1 running, 2 skip to next day */
#define HM_YEAR             0x11F18 /* 8b: 0 = first year */
#define HM_SEASON           0x11F19 /* 8b: 0 spring, 1 summer, 2 fall, 3 winter */
#define HM_WEEKDAY          0x11F1A /* 8b */
#define HM_DAY              0x11F1B /* 8b: 1..30; 0 before a diary is loaded */
#define HM_HOUR             0x11F1C /* 8b: 24h clock */
#define HM_MINUTES          0x11F1D /* 8b: 0..14; one game hour is 15 "minutes" */
#define HM_SECONDS          0x11F1E /* 8b: UpdateTime ($82:8000) adds 1 per frame
                                     * while HM_TIME_RUNNING bit 0 is set; at 60
                                     * it resets and HM_MINUTES advances. So one
                                     * game hour = 900 frames = 15 s, and the
                                     * 6:00-18:00 day lasts 3 real minutes. */

/* ── Text boxes (text printer at $83:9571) ── */
#define HM_TEXT_TICK        0x00189 /* 8b: +1 per frame; at 4 one glyph is printed
                                     * and it resets to 0. Holding A makes the
                                     * game itself store 4 here ($84:CF38), which
                                     * is its built-in fast text. Idle value: 4. */
#define HM_TEXT_X           0x00185 /* 16b: pen position of the next glyph */

/* ── Weather (WeatherTomorrow $82:8CF9) ── */
#define HM_WEATHER_TOMORROW 0x0098C /* 8b, codes below */
enum {
  HM_WEATHER_SUNNY      = 0x00,
  HM_WEATHER_RAIN       = 0x01,
  HM_WEATHER_SNOW       = 0x02,
  HM_WEATHER_HURRICANE  = 0x03,
  HM_WEATHER_FAIR_STORY = 0x04, /* year-1 scripted fair days: leave alone */
  HM_WEATHER_THUNDER    = 0x05,
  /* 0x06..0x0B: festival days (flower, harvest, thanksgiving, star night,
   * new year, egg). Never overridden. */
};

/* ── Player ── */
#define HM_MAX_STAMINA      0x00917 /* 8b */
#define HM_CURRENT_STAMINA  0x00918 /* 8b: 100 at the start of a diary */
#define HM_MONEY            0x11F04 /* 24b binary; AddMoney $83:B1C9 caps it */
#define HM_MONEY_MAX        999999u /* the cap AddMoney itself enforces */
#define HM_SHIPPING_MONEY   0x11F07 /* 24b, paid out overnight */

/* ── Farm stock (AddWood $83:B224 / AddGrass $83:B253 cap both at 999) ── */
#define HM_STORED_WOOD      0x11F0C /* 16b */
#define HM_STORED_GRASS     0x11F10 /* 16b: fodder in the silo */
#define HM_STOCK_MAX        999u

/* ── Bags (counts; the corn-bag increment at $81:8503 saturates at $FF) ── */
#define HM_SEEDS_GRASS_N    0x00927
#define HM_SEEDS_CORN_N     0x00928
#define HM_SEEDS_TOMATO_N   0x00929
#define HM_SEEDS_POTATO_N   0x0092A
#define HM_SEEDS_TURNIP_N   0x0092B
#define HM_FEED_COW_N       0x0092C
#define HM_FEED_CHICKS_N    0x0092D

/* ── Player state ── */
#define HM_INPUT_STATE      0x0019A /* 8b: 1 = normal control, 2 no control,
                                     * 4 menu input, 5 name entry */
#define HM_PLAYER_ACTION    0x000D4 /* 16b: 0 idle; 1 walk, 2 run, 0A using a
                                     * tool, 0B tired, ... (ram.asm) */
#define HM_ITEM_ON_HAND     0x0091D /* 8b: object carried over the head */
#define HM_TOOL_SELECTED    0x00921 /* 8b: item id in hand */
#define HM_TOOL_BACKPACK    0x00923 /* 8b: item id in the backpack */
/* Item ids: 01 sickle 02 hoe 03 hammer 04 axe 05-08 corn/tomato/potato/
 * turnip seeds 09 cow medicine 0A miracle potion 0B bell 0C grass seeds
 * 0D paint 0E milker 0F brush 10 watering can 11-14 golden sickle/hoe/
 * hammer/axe 15 sprinkler 16 beanstalk 17 blue gem 18 blue feather
 * 19 chicken feed 1A cow feed.
 * Shed: bits in $7F1F00..03; the row/mask of item N is bytes 6/7 of entry N
 * of the 8-byte table at $81:BE0F (ROM file 0xBE0F). An item is in exactly
 * one place: shed bit set, hand, or backpack (shed routine $81:87C3). */
#define HM_SHED_ITEMS       0x11F00 /* 4 bytes */

/* ── People ── */
#define HM_HEARTS_BASE      0x11F1F /* 5 x 16b: Maria, Ann, Nina, Ellen, Eve.
                                     * Order proven by the names
                                     * FindMostLovedName ($82:8EC6) writes
                                     * (its branch labels in HM-Decomp are
                                     * swapped). Tested thresholds: 200
                                     * heart event, 450/650 children. */
#define HM_HAPPINESS        0x11F33 /* 16b, AddPlayerHappiness caps at 999 */
#define HM_DOG_HUGS         0x11F52 /* 16b */
#define HM_COW_N            0x11F0A /* 8b */

/* ── Misc ── */
#define HM_RNG_STATE        0x00100 /* 3 bytes, GetRNG $83:8138 (LFSR) */
#define HM_RNG_LAST         0x000DE /* last value handed to event scripts */

static inline uint16_t hm_rd16(const uint8_t *ram, uint32_t a) {
  return (uint16_t)(ram[a] | (ram[a + 1] << 8));
}
static inline void hm_wr16(uint8_t *ram, uint32_t a, uint16_t v) {
  ram[a] = (uint8_t)v; ram[a + 1] = (uint8_t)(v >> 8);
}
static inline uint32_t hm_rd24(const uint8_t *ram, uint32_t a) {
  return ram[a] | (ram[a + 1] << 8) | ((uint32_t)ram[a + 2] << 16);
}
static inline void hm_wr24(uint8_t *ram, uint32_t a, uint32_t v) {
  ram[a] = (uint8_t)v; ram[a + 1] = (uint8_t)(v >> 8); ram[a + 2] = (uint8_t)(v >> 16);
}

/* True once a diary (save) is loaded or a new one started: before that the
 * farm-data block is all zero (verified on the intro/title RAM dumps). */
static inline int hm_diary_loaded(const uint8_t *ram) {
  return ram[HM_DAY] >= 1 && ram[HM_DAY] <= 30 && ram[HM_SEASON] <= 3 &&
         ram[HM_MAX_STAMINA] != 0;
}
