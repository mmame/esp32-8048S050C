#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "sunton_esp32_8048s050c.h"
#include "audio_player_ui.h"
#include "file_manager_ui.h"
#include "wifi_config_ui.h"

void app_main(void)
{
    // Initialize NVS first (required by WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize networking infrastructure BEFORE LCD to lock clocks
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Now initialize LCD with stable clocks
    sunton_esp32s3_backlight_init();

    lv_display_t * disp = sunton_esp32s3_lcd_init();

    i2c_master_bus_handle_t i2c_master = sunton_esp32s3_i2c_master();
    sunton_esp32s3_touch_init(i2c_master);

    // Initialize audio player UI
    audio_player_ui_init(disp);
    
    // Initialize file manager UI (hidden by default)
    lv_lock();
    file_manager_ui_init(lv_screen_active());
    
    // Initialize WiFi config UI (hidden by default)
    wifi_config_ui_init(lv_screen_active());
    lv_unlock();
}