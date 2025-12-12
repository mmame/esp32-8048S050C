#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// SD Card pin definitions
#define SD_PIN_CS       GPIO_NUM_10
#define SD_PIN_MOSI     GPIO_NUM_11
#define SD_PIN_SCK      GPIO_NUM_12
#define SD_PIN_MISO     GPIO_NUM_13

// Initialize the file manager UI
void file_manager_ui_init(lv_obj_t * parent);

// Show/hide the file manager screen
void file_manager_show(void);
void file_manager_hide(void);

// Initialize SD card
bool file_manager_sd_init(void);
void file_manager_sd_deinit(void);

// Refresh file list
void file_manager_refresh(void);

#ifdef __cplusplus
}
#endif
