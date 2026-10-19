#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Title-screen OPTIONS entry (hm_title.c). frame_begin/frame_end are the
 * RtlGameInfo hooks; set_enabled is driven by the pause/QoL activation. */
void hm_title_frame_begin(uint32_t *inputs);
void hm_title_frame_end(void);
void hm_title_set_enabled(bool on);
