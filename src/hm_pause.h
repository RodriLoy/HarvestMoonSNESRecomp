#pragma once
#include <stdint.h>

/* SnesDesktopHostGame.title_overlay_* implementation (hm_pause.c). */
int  hm_pause_poll_open(uint32_t inputs, uint32_t gamepad_raw);
int  hm_pause_is_open(void);
void hm_pause_handle_key(int key, int repeat);
void hm_pause_poll_nav(uint32_t inputs, uint32_t now_ms);
int  hm_pause_image(const uint32_t **panel, int *w, int *h);
void hm_pause_closed(void);
/* True while the pause_menu feature is on (the minimap lives in it). */
int  hm_pause_feature_on(void);
