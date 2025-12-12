#include "audio_player_ui.h"
#include "file_manager_ui.h"
#include "sunton_esp32_8048s050c.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <string.h>

#define TRANSITION_IGNORE_MS 300  // Ignore events for 300ms after screen transition

// Stats overlay
static lv_obj_t * cpu_label = NULL;
static uint32_t frame_count = 0;
static int64_t last_time = 0;
static int64_t last_transition_time = 0;

// Audio player UI elements
static lv_obj_t * title_label = NULL;
static lv_obj_t * progress_bar = NULL;
static lv_obj_t * time_label = NULL;
static lv_obj_t * time_total_label = NULL;
static lv_obj_t * autoplay_checkbox = NULL;
static lv_obj_t * continue_checkbox = NULL;
static lv_obj_t * audio_player_screen = NULL;

static void update_stats_timer_cb(lv_timer_t * timer)
{
    // Calculate FPS
    int64_t current_time = esp_timer_get_time();
    float fps = 0;
    
    if (last_time > 0) {
        int64_t elapsed_us = current_time - last_time;
        if (elapsed_us > 0) {
            fps = (float)frame_count * 1000000.0f / (float)elapsed_us;
        }
    }
    
    last_time = current_time;
    frame_count = 0;
    
    // Get CPU usage using heap free memory as a simple metric
    // (More accurate CPU stats would require configGENERATE_RUN_TIME_STATS)
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
    
    // Calculate a simple CPU activity metric based on frame rate
    // Higher FPS typically means higher CPU usage
    float cpu_usage = (fps / 60.0f) * 100.0f;
    if (cpu_usage > 100.0f) cpu_usage = 100.0f;
    
    // Update labels using snprintf for proper float formatting
    char cpu_text[32];
    snprintf(cpu_text, sizeof(cpu_text), "CPU: %.1f%%", cpu_usage);
    
    lv_label_set_text(cpu_label, cpu_text);
}

static void flush_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_FLUSH_FINISH) {
        frame_count++;
    }
}

static void progress_bar_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * bar = (lv_obj_t *)lv_event_get_target(e);
    
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSING) {
        lv_indev_t * indev = lv_indev_active();
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        
        // Get bar position and size
        int32_t bar_x = lv_obj_get_x(bar);
        int32_t bar_width = lv_obj_get_width(bar);
        
        // Calculate clicked position relative to bar
        int32_t click_pos = point.x - bar_x;
        if (click_pos < 0) click_pos = 0;
        if (click_pos > bar_width) click_pos = bar_width;
        
        // Calculate percentage
        int32_t new_value = (click_pos * 100) / bar_width;
        
        // Update progress bar
        lv_bar_set_value(bar, new_value, LV_ANIM_OFF);
        
        // TODO: Update time labels and trigger seek in audio playback
        // For now, just update the visual
    }
}

static void screen_gesture_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // Ignore all events for a short time after screen transition
    int64_t now = esp_timer_get_time() / 1000;  // Convert to ms
    if (now - last_transition_time < TRANSITION_IGNORE_MS) {
        ESP_LOGI("AudioPlayer", "Event ignored - too soon after transition (%lld ms)", now - last_transition_time);
        return;
    }
    
    // Log ALL events to see what's happening
    ESP_LOGI("AudioPlayer", "Event code: %d (PRESSED=%d, RELEASED=%d, GESTURE=%d)", 
             code, LV_EVENT_PRESSED, LV_EVENT_RELEASED, LV_EVENT_GESTURE);
    
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        
        ESP_LOGI("AudioPlayer", "Gesture detected, direction: %d (LEFT=%d, RIGHT=%d, TOP=%d, BOTTOM=%d)", 
                 dir, LV_DIR_LEFT, LV_DIR_RIGHT, LV_DIR_TOP, LV_DIR_BOTTOM);
        
        if (dir == LV_DIR_LEFT) {
            // Swipe left to show file manager
            ESP_LOGI("AudioPlayer", "Swipe LEFT detected, showing file manager");
            last_transition_time = esp_timer_get_time() / 1000;
            file_manager_show();
        }
    }
}

void audio_player_ui_init(lv_display_t * disp)
{
    // Increment frame counter on each flush
    lv_display_add_event_cb(disp, flush_event_cb, LV_EVENT_FLUSH_FINISH, NULL);

    lv_lock();
    
    // Create main screen with black background
    lv_obj_t * screen = lv_screen_active();
    audio_player_screen = screen;  // Store reference
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);  // Disable scroll
    
    // Create song title label (large, scrolling text)
    title_label = lv_label_create(screen);
    lv_obj_set_width(title_label, SUNTON_ESP32_LCD_WIDTH - 40);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_48, 0);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(title_label, "This is a Very Very Very Very Long Title To Test Scrolling Feature");
    // Set constant scroll speed (pixels per second)
    const char * text = lv_label_get_text(title_label);
    lv_point_t text_size;
    lv_text_get_size(&text_size, text, lv_obj_get_style_text_font(title_label, 0), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    uint32_t anim_time = (text_size.x * 1000) / 90;  // pixels per second
    lv_obj_set_style_anim_time(title_label, anim_time, 0);
    
    // Create progress bar
    progress_bar = lv_bar_create(screen);
    lv_obj_set_size(progress_bar, SUNTON_ESP32_LCD_WIDTH - 80, 40);
    lv_obj_align(progress_bar, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(progress_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(progress_bar, lv_color_hex(0x888888), 0);
    lv_obj_set_style_border_width(progress_bar, 2, 0);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    lv_bar_set_range(progress_bar, 0, 100);
    lv_bar_set_value(progress_bar, 35, LV_ANIM_OFF);
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(progress_bar, progress_bar_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(progress_bar, progress_bar_event_cb, LV_EVENT_PRESSING, NULL);
    
    // Create time elapsed label (left side, below progress bar)
    time_label = lv_label_create(screen);
    lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 40, 180);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(time_label, "01:23");
    
    // Create time total label (right side, below progress bar)
    time_total_label = lv_label_create(screen);
    lv_obj_align(time_total_label, LV_ALIGN_TOP_RIGHT, -40, 180);
    lv_obj_set_style_text_color(time_total_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(time_total_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(time_total_label, "03:45");
    
    // Create control buttons (centered below time labels)
    int button_size = 100;
    int button_spacing = 25;
    int total_width = (button_size * 4) + (button_spacing * 3);
    int start_x = (SUNTON_ESP32_LCD_WIDTH - total_width) / 2;
    int button_y = 260;
    
    // Previous button
    lv_obj_t * btn_prev = lv_button_create(screen);
    lv_obj_set_size(btn_prev, button_size, button_size);
    lv_obj_set_pos(btn_prev, start_x, button_y);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(btn_prev, 40, 0);
    lv_obj_t * label_prev = lv_label_create(btn_prev);
    lv_label_set_text(label_prev, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(label_prev, &lv_font_montserrat_48, 0);
    lv_obj_center(label_prev);
    
    // Play button
    lv_obj_t * btn_play = lv_button_create(screen);
    lv_obj_set_size(btn_play, button_size, button_size);
    lv_obj_set_pos(btn_play, start_x + button_size + button_spacing, button_y);
    lv_obj_set_style_bg_color(btn_play, lv_color_hex(0x00AA00), 0);
    lv_obj_set_style_radius(btn_play, 40, 0);
    lv_obj_t * label_play = lv_label_create(btn_play);
    lv_label_set_text(label_play, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(label_play, &lv_font_montserrat_48, 0);
    lv_obj_center(label_play);
    
    // Pause button
    lv_obj_t * btn_pause = lv_button_create(screen);
    lv_obj_set_size(btn_pause, button_size, button_size);
    lv_obj_set_pos(btn_pause, start_x + (button_size + button_spacing) * 2, button_y);
    lv_obj_set_style_bg_color(btn_pause, lv_color_hex(0xAA6600), 0);
    lv_obj_set_style_radius(btn_pause, 40, 0);
    lv_obj_t * label_pause = lv_label_create(btn_pause);
    lv_label_set_text(label_pause, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_font(label_pause, &lv_font_montserrat_48, 0);
    lv_obj_center(label_pause);
    
    // Next button
    lv_obj_t * btn_next = lv_button_create(screen);
    lv_obj_set_size(btn_next, button_size, button_size);
    lv_obj_set_pos(btn_next, start_x + (button_size + button_spacing) * 3, button_y);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(btn_next, 40, 0);
    lv_obj_t * label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(label_next, &lv_font_montserrat_48, 0);
    lv_obj_center(label_next);
    
    // Create Auto-Play checkbox (bottom-left)
    autoplay_checkbox = lv_checkbox_create(screen);
    lv_checkbox_set_text(autoplay_checkbox, "Auto-Play");
    lv_obj_set_style_text_font(autoplay_checkbox, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(autoplay_checkbox, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(autoplay_checkbox, LV_ALIGN_BOTTOM_LEFT, 40, -20);
    lv_obj_set_style_bg_color(autoplay_checkbox, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    
    // Create Continue Playback checkbox (bottom-center)
    continue_checkbox = lv_checkbox_create(screen);
    lv_checkbox_set_text(continue_checkbox, "Continue Playback");
    lv_obj_set_style_text_font(continue_checkbox, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(continue_checkbox, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(continue_checkbox, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(continue_checkbox, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    
    // Create CPU label (bottom-right corner for debugging)
    cpu_label = lv_label_create(screen);
    lv_obj_set_style_text_color(cpu_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_color(cpu_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(cpu_label, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(cpu_label, 4, 0);
    lv_obj_align(cpu_label, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    lv_label_set_text(cpu_label, "CPU: --");
    
    // Add swipe gesture support to screen
    lv_obj_add_event_cb(screen, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(screen, screen_gesture_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen, screen_gesture_event_cb, LV_EVENT_RELEASED, NULL);
    
    // Create timer to update stats every second
    lv_timer_create(update_stats_timer_cb, 1000, NULL);
    
    lv_unlock();
}

// Getter functions
lv_obj_t * audio_player_get_screen(void)
{
    return audio_player_screen;
}

lv_obj_t * audio_player_get_title_label(void)
{
    return title_label;
}

lv_obj_t * audio_player_get_progress_bar(void)
{
    return progress_bar;
}

lv_obj_t * audio_player_get_time_label(void)
{
    return time_label;
}

lv_obj_t * audio_player_get_time_total_label(void)
{
    return time_total_label;
}

lv_obj_t * audio_player_get_autoplay_checkbox(void)
{
    return autoplay_checkbox;
}

lv_obj_t * audio_player_get_continue_checkbox(void)
{
    return continue_checkbox;
}
