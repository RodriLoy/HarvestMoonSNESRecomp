#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Endless game (src/hm_endless.c). */
void hm_endless_set_enabled(bool on);
void hm_endless_after_frame(void);        /* host after_run_frame */
uint8_t hm_endless_ending_day(void);      /* generated code: $83:DEEB */
uint16_t hm_year_suffix(uint16_t x, uint16_t rom_value); /* $82:F298 reads */
