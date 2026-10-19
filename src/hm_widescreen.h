#ifndef HM_WIDESCREEN_H
#define HM_WIDESCREEN_H

#include <stdbool.h>

/* 16:9 policy for Harvest Moon (see hm_widescreen.c). */
void hm_ws_prepare_frame(int drawable_w, int drawable_h, int *frame_w, int *frame_h);
void hm_ws_begin_sim_frame(unsigned number);
void hm_ws_set_enabled(bool on);
bool hm_ws_enabled(void);

#endif
