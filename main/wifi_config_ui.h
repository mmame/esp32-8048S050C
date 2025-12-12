#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Initialize the WiFi config UI
void wifi_config_ui_init(lv_obj_t *parent);

// Show/hide the WiFi config screen
void wifi_config_show(void);
void wifi_config_hide(void);

// Get the WiFi config screen
lv_obj_t * wifi_config_get_screen(void);

#ifdef __cplusplus
}
#endif
