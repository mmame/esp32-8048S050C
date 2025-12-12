#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Initialize the audio player UI
void audio_player_ui_init(lv_display_t * disp);

// Get the audio player screen
lv_obj_t * audio_player_get_screen(void);

// Get UI element references
lv_obj_t * audio_player_get_title_label(void);
lv_obj_t * audio_player_get_progress_bar(void);
lv_obj_t * audio_player_get_time_label(void);
lv_obj_t * audio_player_get_time_total_label(void);
lv_obj_t * audio_player_get_autoplay_checkbox(void);
lv_obj_t * audio_player_get_continue_checkbox(void);

#ifdef __cplusplus
}
#endif
