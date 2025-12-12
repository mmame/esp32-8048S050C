idf_component_register(SRCS "wifi_config_ui.cpp" "main.c" "sunton_esp32_8048s050c.c" "audio_player_ui.cpp" "file_manager_ui.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl esp_lcd esp_lcd_touch_gt911 esp_driver_ledc fatfs esp_wifi esp_netif esp_http_server esp_event vfs nvs_flash)