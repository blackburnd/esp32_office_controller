#include "lcd.h"
#include <stdbool.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_rgb.h"
#include <string.h>
#include "mqtt.h"
#include "weather.h"
#include "assets.h"
#include <esp_log.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include <time.h>

// Watchdog and health monitoring
static esp_timer_handle_t health_monitor_timer = NULL;
#define HEALTH_CHECK_INTERVAL_MS 10000  // Check every 10 seconds

// Global base style to override theme defaults
static lv_style_t style_base_no_visuals;

void water_valve_state_cb(int relay_index, bool state);
void central_vacuum_state_cb(int relay_index, bool state);
void vacuum_pump_state_cb(int relay_index, bool state);
static void versaries_setup_timer_and_update(void);
static void update_versaries_display(void);

static void water_switch_event_cb(lv_event_t *e);
static void central_vacuum_switch_event_cb(lv_event_t *e);
static void vacuum_pump_switch_event_cb(lv_event_t *e);
static void fetch_north_driveway(lv_event_t *e); // Event handler for fetch image button

// Add this forward declaration so it's visible at the call site in lcd_create_ui
static void auto_refresh_switch_event_cb(lv_event_t *e);

// --- Event handlers for new camera buttons ---
static void camera8_btn_event_cb(lv_event_t *e);
static void fetch_north_driveway(lv_event_t *e);
static void east_driveway_btn_event_cb(lv_event_t *e);
static void camera_north_canal_btn_event_cb(lv_event_t *e);
static void camera_front_door_btn_event_cb(lv_event_t *e);
static void camera_west_canal_btn_event_cb(lv_event_t *e);
static void camera_tiki_btn_event_cb(lv_event_t *e);
static void camera_south_yard_btn_event_cb(lv_event_t *e);

// Add this forward declaration
static void fetch_weather_btn_event_cb(lv_event_t *e);
static void tab_change_event_cb(lv_event_t *e);

// Static globals (moved to top) - cleaned duplicates
// Cleaned globals — single definitions only
// Private UI state
static lv_obj_t *title_bar = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *wifi_ssid_label = NULL;
static lv_obj_t *wifi_ip_label = NULL;
static lv_obj_t *ha_status_icon = NULL;
static lv_obj_t *ha_ip_label = NULL;
static lv_obj_t *tabview = NULL;
static lv_obj_t *weather_tab = NULL;
static lv_obj_t *cameras_tab = NULL;
static lv_obj_t *controller_tab = NULL;
// renamed events_tab -> versaries_tab
static lv_obj_t *versaries_tab = NULL;

// Versaries UI elements
static lv_obj_t *versaries_date_label = NULL;
static lv_obj_t *versaries_count_label = NULL;
static lv_timer_t *versaries_update_timer = NULL;

// New birthday labels: Mia (5/8/1995) and Mine (8/22/1980)
static lv_obj_t *mia_date_label = NULL;
static lv_obj_t *mia_since_label = NULL;
static lv_obj_t *mia_until_label = NULL;
static lv_obj_t *my_date_label = NULL;
static lv_obj_t *my_since_label = NULL;
static lv_obj_t *my_until_label = NULL;

// Marriage date (Year, Month, Day)
#define MARRIAGE_DATE_YEAR 2023
#define MARRIAGE_DATE_MONTH 11
#define MARRIAGE_DATE_DAY 3

// Mia's birthday (May 8, 1995)
#define MIA_BIRTH_YEAR 1995
#define MIA_BIRTH_MONTH 5
#define MIA_BIRTH_DAY 8

// Daniel's birthday (August 22, 1980)
#define MY_BIRTH_YEAR 1980
#define MY_BIRTH_MONTH 8
#define MY_BIRTH_DAY 22

// Objects referenced from other compilation units: define non-static to match 'extern' in headers
lv_obj_t *camera_img_widget = NULL;     // declared extern in lcd.h
lv_obj_t *weather_forecast_list = NULL; // declared extern in weather.h

// Weather UI elements (extern'd in weather.h) - define once here (non-static)
lv_obj_t *weather_icon_label = NULL;
lv_obj_t *weather_temp_label = NULL;
lv_obj_t *weather_humidity_label = NULL;
lv_obj_t *weather_wind_label = NULL;
lv_obj_t *weather_conditions_label = NULL;
static lv_obj_t *weather_timestamp_label = NULL;

// Track previous tab to detect actual tab changes
static uint32_t previous_tab_idx = 0;  // Start on weather tab

// Controller / camera UI (private)
static lv_obj_t *water_switch = NULL;
static lv_obj_t *water_switch_label = NULL;
static lv_obj_t *central_vacuum_switch = NULL;
static lv_obj_t *central_vacuum_switch_label = NULL;
static lv_obj_t *vacuum_pump_switch = NULL;
static lv_obj_t *vacuum_pump_switch_label = NULL;
static lv_obj_t *fetch_image_button = NULL;

// Thermostat (Nest) UI forward declarations/state (must be before lcd_create_ui)
static lv_obj_t *thermo_arc = NULL;
static lv_obj_t *thermo_label_ambient = NULL;
static lv_obj_t *thermo_label_setpoint = NULL;
static lv_obj_t *thermo_label_humidity = NULL;
static lv_obj_t *thermo_btn_plus = NULL;   // Increment button
static lv_obj_t *thermo_btn_minus = NULL;  // Decrement button
static lv_timer_t *thermo_request_timer = NULL;  // Timer to request updates from HA
static lv_timer_t *thermo_publish_debounce_timer = NULL;  // Debounce timer to prevent spam
static float thermostat_ambient_c = -1000.0f;  // Actually stores °F (variable name kept for compatibility)
static float thermostat_setpoint_c = 72.0f;     // Actually stores °F (variable name kept for compatibility)
static float thermostat_humidity_pct = -1.0f;
static bool thermostat_dragging = false;
static int64_t last_publish_time_us = 0;  // Track last publish time for rate limiting
static void thermostat_arc_event_cb(lv_event_t *e);
static void thermostat_update_labels_locked(void);
static void thermo_btn_plus_event_cb(lv_event_t *e);
static void thermo_btn_minus_event_cb(lv_event_t *e);
static void thermo_request_timer_cb(lv_timer_t *timer);
static void thermo_publish_debounce_cb(lv_timer_t *timer);
static void schedule_debounced_publish(void);

// Camera button globals (private)
static lv_obj_t *camera3_btn = NULL;
static lv_obj_t *camera8_btn = NULL;
static lv_obj_t *camera_north_canal_btn = NULL;
static lv_obj_t *camera_front_door_btn = NULL;
static lv_obj_t *camera_west_canal_btn = NULL;
static lv_obj_t *camera_south_yard_btn = NULL;
static lv_obj_t *camera_tiki_btn = NULL;

// Auto-refresh
static lv_obj_t *auto_refresh_switch = NULL;
static lv_obj_t *auto_refresh_switch_label = NULL;
static lv_timer_t *auto_refresh_timer = NULL;
static int current_camera_index = 0; // Track currently displayed camera

// Limits
#define LCD_MAX_LINES 100
#define LCD_MAX_TEXTAREA_BUF 2048

// Color definitions
#define COLOR_BLUE lv_color_hex(0x1976D2)
#define COLOR_GREY lv_color_hex(0x808080)
#define COLOR_LIGHT_GREY lv_color_hex(0xE0E0E0)
#define COLOR_DARK_GREY lv_color_hex(0x263238)
#define COLOR_WHITE lv_color_white()
#define COLOR_BLACK lv_color_black()
#define COLOR_INDIGO lv_color_hex(0x1A237E)
#define COLOR_OFF_WHITE lv_color_hex(0xF5F5F5)

// --- Event handlers for new camera buttons ---
static void camera8_btn_event_cb(lv_event_t *e)
{
    current_camera_index = 7;
    extern void camera_client_fetch_image(int camera_index);
    camera_client_fetch_image(7);
}
static void fetch_north_driveway(lv_event_t *e)
{
    current_camera_index = 1;
    extern void camera_client_fetch_image(int camera_index);
    camera_client_fetch_image(1);
}
static void east_driveway_btn_event_cb(lv_event_t *e)
{
    current_camera_index = 3;
    extern void camera_client_fetch_image(int camera_index);
    camera_client_fetch_image(3);
}
static void camera_north_canal_btn_event_cb(lv_event_t *e)
{
    current_camera_index = 4;
    extern void camera_client_fetch_image(int camera_index);
    camera_client_fetch_image(4);
}
static void camera_front_door_btn_event_cb(lv_event_t *e)
{
    current_camera_index = 2;
    extern void camera_client_fetch_image(int camera_index);
    camera_client_fetch_image(2);
}
static void camera_west_canal_btn_event_cb(lv_event_t *e)
{
    current_camera_index = 6;
    extern void camera_client_fetch_image(int camera_index);
    camera_client_fetch_image(6);
}
static void camera_tiki_btn_event_cb(lv_event_t *e)
{
    current_camera_index = 8;
    extern void camera_client_fetch_image(int camera_index);
    camera_client_fetch_image(8);
}
static void camera_south_yard_btn_event_cb(lv_event_t *e)
{
    current_camera_index = 5;
    extern void camera_client_fetch_image(int camera_index);
    camera_client_fetch_image(5);
}

// Color definitions for motion detection
#define COLOR_MOTION_DETECTED lv_color_hex(0xFF4444)  // Bright red
#define COLOR_MOTION_CLEARED COLOR_BLUE  // Return to normal blue
#define MOTION_AUTO_CLEAR_MS 3000  // Auto-clear motion after 3 seconds

// Motion timer handles for each camera (8 cameras)
static esp_timer_handle_t motion_timers[8] = {NULL};

// Helper function to get camera button by channel index
static lv_obj_t* get_camera_button_by_channel(int channel) {
    switch(channel) {
        case 0: return fetch_image_button;      // North Driveway
        case 1: return camera_front_door_btn;   // Front Door
        case 2: return camera_north_canal_btn;  // South Driveway (labeled as North Canal in UI)
        case 3: return camera3_btn;             // East Driveway (camera3_btn)
        case 4: return camera_south_yard_btn;   // North Canal (labeled as South Yard in UI)
        case 5: return camera_west_canal_btn;   // West Canal
        case 6: return camera_tiki_btn;         // Tiki
        case 7: return camera8_btn;             // South Yard (camera8_btn)
        default: return NULL;
    }
}

// Timer callback to auto-clear motion state
static void motion_timer_callback(void *arg) {
    int camera_channel = (int)(intptr_t)arg;
    static const char *TAG = "LCD";
    
    lv_obj_t *btn = get_camera_button_by_channel(camera_channel);
    if (!btn) {
        ESP_LOGW(TAG, "Camera %d: button not found in timer callback", camera_channel);
        return;
    }

    // Lock LVGL and clear motion state with extended timeout and retry
    int retry_count = 0;
    while (retry_count < 3) {
        if (lvgl_port_lock(200)) {  // Increased to 200ms timeout
            lv_obj_set_style_bg_color(btn, lv_color_lighten(COLOR_BLUE, LV_OPA_20), LV_STATE_DEFAULT);
            lv_obj_set_style_bg_grad_color(btn, lv_color_darken(COLOR_BLUE, LV_OPA_20), LV_STATE_DEFAULT);
            lv_obj_invalidate(btn);
            lvgl_port_unlock();
            ESP_LOGI(TAG, "Camera %d motion AUTO-CLEARED after 3s - button BLUE", camera_channel);
            return;
        }
        retry_count++;
        ESP_LOGW(TAG, "Camera %d motion auto-clear: LVGL lock attempt %d/3 failed", camera_channel, retry_count);
        vTaskDelay(pdMS_TO_TICKS(50));  // Small delay before retry
    }
    ESP_LOGE(TAG, "Camera %d motion auto-clear: FAILED after 3 retries - UI may be frozen", camera_channel);
}

// Public function to set camera button motion state (called from MQTT handler)
void lcd_set_camera_button_motion(int camera_channel, bool motion_detected) {
    static const char *TAG = "LCD";
    
    if (camera_channel < 0 || camera_channel >= 8) {
        ESP_LOGW(TAG, "Invalid camera channel: %d", camera_channel);
        return;
    }
    
    lv_obj_t *btn = get_camera_button_by_channel(camera_channel);
    if (!btn) {
        ESP_LOGW(TAG, "Camera %d: button object is NULL", camera_channel);
        return;
    }

    // Lock LVGL before modifying UI with retry logic
    int retry_count = 0;
    bool lock_acquired = false;
    while (retry_count < 2 && !lock_acquired) {
        if (lvgl_port_lock(200)) {  // 200ms timeout
            lock_acquired = true;
            if (motion_detected) {
                // Highlight button with motion color
                lv_obj_set_style_bg_color(btn, COLOR_MOTION_DETECTED, LV_STATE_DEFAULT);
                lv_obj_set_style_bg_grad_color(btn, lv_color_darken(COLOR_MOTION_DETECTED, LV_OPA_30), LV_STATE_DEFAULT);
                ESP_LOGI(TAG, "Camera %d motion DETECTED - button highlighted RED", camera_channel);
                
                // Stop existing timer if running to prevent accumulation
                if (motion_timers[camera_channel] != NULL) {
                    esp_err_t err = esp_timer_stop(motion_timers[camera_channel]);
                    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                        ESP_LOGW(TAG, "Camera %d: Failed to stop timer: %s", camera_channel, esp_err_to_name(err));
                    }
                } else {
                    // Create timer on first use
                    esp_timer_create_args_t timer_args = {
                        .callback = motion_timer_callback,
                        .arg = (void *)(intptr_t)camera_channel,
                        .name = "motion_timer",
                        .skip_unhandled_events = true  // Prevent timer queue accumulation
                    };
                    esp_err_t err = esp_timer_create(&timer_args, &motion_timers[camera_channel]);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Camera %d: Failed to create timer: %s", camera_channel, esp_err_to_name(err));
                        lvgl_port_unlock();
                        return;
                    }
                }
                
                // Start 3-second auto-clear timer
                esp_err_t err = esp_timer_start_once(motion_timers[camera_channel], MOTION_AUTO_CLEAR_MS * 1000);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Camera %d: Failed to start timer: %s", camera_channel, esp_err_to_name(err));
                }
            } else {
                // Manual clear from MQTT "OFF" - stop timer and reset color
                if (motion_timers[camera_channel] != NULL) {
                    esp_timer_stop(motion_timers[camera_channel]);
                }
                lv_obj_set_style_bg_color(btn, lv_color_lighten(COLOR_BLUE, LV_OPA_20), LV_STATE_DEFAULT);
                lv_obj_set_style_bg_grad_color(btn, lv_color_darken(COLOR_BLUE, LV_OPA_20), LV_STATE_DEFAULT);
                ESP_LOGI(TAG, "Camera %d motion CLEARED - button normal", camera_channel);
            }
            lv_obj_invalidate(btn);
            lvgl_port_unlock();
        } else {
            retry_count++;
            ESP_LOGW(TAG, "Camera %d motion: LVGL lock attempt %d/2 failed", camera_channel, retry_count);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
    
    if (!lock_acquired) {
        ESP_LOGE(TAG, "Camera %d motion: FAILED to lock LVGL after retries (heap: %" PRIu32 ")", 
                 camera_channel, esp_get_free_heap_size());
    }
}

void lcd_create_ui(void)
{
    // Initialize base style to override all theme visuals
    lv_style_init(&style_base_no_visuals);
    lv_style_set_bg_grad_dir(&style_base_no_visuals, LV_GRAD_DIR_NONE);
    lv_style_set_border_width(&style_base_no_visuals, 0);
    lv_style_set_border_opa(&style_base_no_visuals, LV_OPA_TRANSP);
    lv_style_set_outline_width(&style_base_no_visuals, 0);
    lv_style_set_outline_opa(&style_base_no_visuals, LV_OPA_TRANSP);
    lv_style_set_shadow_width(&style_base_no_visuals, 0);
    lv_style_set_shadow_opa(&style_base_no_visuals, LV_OPA_TRANSP);

    lv_obj_t *scr = lv_disp_get_scr_act(NULL);

    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, COLOR_WHITE, 0);
    lv_obj_add_style(scr, &style_base_no_visuals, 0);

    title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, 800, 50);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1A237E), 0); // Indigo 900
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    wifi_icon = lv_label_create(title_bar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_white(), 0);
    lv_obj_align(wifi_icon, LV_ALIGN_LEFT_MID, 10, 0);

    wifi_ssid_label = lv_label_create(title_bar);
    lv_label_set_text(wifi_ssid_label, "Connecting...");
    lv_obj_set_style_text_color(wifi_ssid_label, lv_color_white(), 0);
    lv_obj_align_to(wifi_ssid_label, wifi_icon, LV_ALIGN_OUT_RIGHT_MID, 12, 0);

    wifi_ip_label = lv_label_create(title_bar);
    lv_label_set_text(wifi_ip_label, "0.0.0.0");
    lv_obj_set_style_text_color(wifi_ip_label, lv_color_white(), 0);
    lv_obj_align_to(wifi_ip_label, wifi_ssid_label, LV_ALIGN_OUT_RIGHT_MID, 48, 0);

    ha_status_icon = lv_label_create(title_bar);
    lv_label_set_text(ha_status_icon, LV_SYMBOL_CLOSE " HA Connecting...");
    lv_obj_set_style_text_color(ha_status_icon, lv_color_white(), 0);
    lv_obj_align(ha_status_icon, LV_ALIGN_RIGHT_MID, -180, 0);

    ha_ip_label = lv_label_create(title_bar);
    lv_label_set_text(ha_ip_label, "-");
    lv_obj_set_style_text_color(ha_ip_label, lv_color_white(), 0);
    lv_obj_align_to(ha_ip_label, ha_status_icon, LV_ALIGN_OUT_RIGHT_MID, 16, 0);

    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(wifi_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ha_status_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ha_ip_label, LV_OBJ_FLAG_CLICKABLE);

    // Create tabview below title bar
    tabview = lv_tabview_create(scr, LV_DIR_TOP, 50); // Tab bar height 50px
    lv_obj_set_size(tabview, 800, 430);               // Full width, height from y=50 to bottom
    lv_obj_align(tabview, LV_ALIGN_TOP_LEFT, 0, 50);
    lv_obj_add_event_cb(tabview, tab_change_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Override tabview styles to use fixed colors (no palette, no borders/outlines)
   lv_obj_set_style_bg_color(tabview, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tabview, COLOR_WHITE, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(tabview, COLOR_WHITE, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(tabview, LV_OPA_TRANSP, 0);
    
    // Prevent tab buttons from changing background when other widgets are pressed
    lv_obj_set_style_bg_opa(tabview, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(tabview, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(tabview, LV_OPA_TRANSP, LV_PART_ITEMS | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_bg_opa(tabview, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(tabview, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_FOCUSED);

    //lv_obj_set_style_text_color(tabview, COLOR_WHITE, LV_PART_ITEMS);
    //lv_obj_set_style_text_color(tabview, COLOR_DARK_GREY, LV_PART_ITEMS | LV_STATE_DEFAULT);
    //lv_obj_set_style_border_width(tabview, 2, LV_PART_MAIN);
    //lv_obj_set_style_border_color(tabview, COLOR_BLUE, LV_PART_MAIN);
   // lv_obj_set_style_border_width(tabview, 2, LV_PART_ITEMS);
   // lv_obj_set_style_border_color(tabview, COLOR_GREY, LV_PART_ITEMS);
   // lv_obj_set_style_outline_width(tabview, 1, LV_PART_MAIN); // Disable outlines
   // lv_obj_set_style_shadow_width(tabview, 0, LV_PART_MAIN);
    // Add: Disable outlines on focus for tabs
   // lv_obj_set_style_outline_width(tabview, 1, LV_PART_ITEMS | LV_STATE_FOCUSED);
   //// lv_obj_set_style_border_width(tabview, 2, LV_PART_ITEMS | LV_STATE_FOCUSED);

    // Disable gradients on tabview
    lv_obj_set_style_bg_grad_dir(tabview, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_bg_grad_dir(tabview, LV_GRAD_DIR_NONE, LV_PART_ITEMS);

    // Add tabs (styles applied above will override defaults)
    weather_tab = lv_tabview_add_tab(tabview, "Weather");
    cameras_tab = lv_tabview_add_tab(tabview, "Cameras");
    controller_tab = lv_tabview_add_tab(tabview, "Central Controller");
    versaries_tab = lv_tabview_add_tab(tabview, "Versaries");

    // Apply base style to all tabs to disable gradients, borders, shadows
    lv_obj_add_style(weather_tab, &style_base_no_visuals, 0);
    // Remove any default page padding so content aligns exactly with tab area
    lv_obj_set_style_pad_all(weather_tab, 0, 0);
    lv_obj_add_style(cameras_tab, &style_base_no_visuals, 0);
    lv_obj_add_style(controller_tab, &style_base_no_visuals, 0);
    lv_obj_add_style(versaries_tab, &style_base_no_visuals, 0);
    
    // Set all tabs to solid white background (opaque) to block content underneath
    lv_obj_clear_flag(weather_tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(weather_tab, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(weather_tab, LV_OPA_COVER, 0);

    lv_obj_set_style_bg_color(cameras_tab, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(cameras_tab, LV_OPA_COVER, 0);

    lv_obj_set_style_bg_color(controller_tab, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(controller_tab, LV_OPA_COVER, 0);
    // Lock controller tab background on all states to prevent color changes
    lv_obj_set_style_bg_opa(controller_tab, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(controller_tab, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(controller_tab, LV_OPA_COVER, LV_STATE_EDITED);
    lv_obj_set_style_bg_opa(controller_tab, LV_OPA_COVER, LV_STATE_FOCUS_KEY);

    lv_obj_set_style_bg_color(versaries_tab, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(versaries_tab, LV_OPA_COVER, 0);

    /* --- Weather Tab --- */
#define WEATHER_LEFT_WIDTH 200  // 25% of ~800px tab width
#define WEATHER_RIGHT_WIDTH 600 // 75% of ~800px tab width

   
    weather_icon_label = lv_img_create(weather_tab);
    lv_obj_set_size(weather_icon_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT); 

    weather_temp_label = lv_label_create(weather_tab);
    lv_obj_remove_style_all(weather_temp_label);

    lv_label_set_text(weather_temp_label, "Loading...");
    lv_obj_set_style_text_font(weather_temp_label, &lv_font_montserrat_28, 0);
    lv_obj_clear_flag(weather_temp_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(weather_temp_label, LV_OBJ_FLAG_CLICK_FOCUSABLE);     
    lv_obj_set_style_bg_opa(weather_temp_label, LV_OPA_TRANSP, 0);


    weather_humidity_label = lv_label_create(weather_tab);
    lv_label_set_text(weather_humidity_label, "");
    lv_obj_set_style_text_font(weather_humidity_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_bg_opa(weather_humidity_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(weather_humidity_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(weather_humidity_label, LV_OPA_TRANSP, 0);

    weather_wind_label = lv_label_create(weather_tab);
    lv_label_set_text(weather_wind_label, "");
    lv_obj_set_style_text_font(weather_wind_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_bg_opa(weather_wind_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(weather_wind_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(weather_wind_label, LV_OPA_TRANSP, 0);

    weather_conditions_label = lv_label_create(weather_tab);
    lv_label_set_text(weather_conditions_label, "");
    lv_obj_set_style_text_font(weather_conditions_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_bg_opa(weather_conditions_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(weather_conditions_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(weather_conditions_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(weather_conditions_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(weather_conditions_label, LV_OPA_TRANSP, 0);

    // Set initial weather icon (now that weather_conditions_label exists)
    set_jpg_or_error(weather_icon_label, clear_day_jpg_start, clear_day_jpg_end, weather_conditions_label);

    // Create timestamp label at top right of weather tab
    weather_timestamp_label = lv_label_create(weather_tab);
    lv_obj_remove_style_all(weather_timestamp_label);
    lv_label_set_text(weather_timestamp_label, "Last updated: --:--");
    lv_obj_set_style_text_font(weather_timestamp_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(weather_timestamp_label, lv_color_hex(0x666666), 0); // Gray text
    lv_obj_set_style_bg_opa(weather_timestamp_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(weather_timestamp_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(weather_timestamp_label, LV_OPA_TRANSP, 0);
    lv_obj_align(weather_timestamp_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_clear_flag(weather_timestamp_label, LV_OBJ_FLAG_CLICKABLE);

    // Fetch Weather button: create on the left column, but place it at extreme bottom-left
    lv_obj_t *fetch_weather_btn = lv_btn_create(weather_tab);
    lv_obj_remove_style_all(fetch_weather_btn); // Remove all default/theme styles
    lv_obj_set_size(fetch_weather_btn, 180, 50); // Fit left width
    lv_obj_add_event_cb(fetch_weather_btn, fetch_weather_btn_event_cb, LV_EVENT_CLICKED, NULL);
    // Re-add clickable flag after removing all styles
    lv_obj_add_flag(fetch_weather_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(fetch_weather_btn, lv_color_hex(0x1976D2), 0);
    lv_obj_set_style_bg_opa(fetch_weather_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(fetch_weather_btn, 5, 0); // Optional: rounded corners
    lv_obj_set_style_pad_all(fetch_weather_btn, 10, 0); // Padding for text
    // Remove shadow on pressed state
    //lv_obj_set_style_shadow_width(fetch_weather_btn, 0, LV_STATE_PRESSED);

    lv_obj_t *weather_btn_label = lv_label_create(fetch_weather_btn);
    lv_obj_remove_style_all(weather_btn_label); // Remove all default styles from label
    lv_label_set_text(weather_btn_label, "Refresh");
    lv_obj_set_style_text_color(weather_btn_label, lv_color_white(), 0); // White text
    // Make sure label doesn't intercept clicks
    //lv_obj_clear_flag(weather_btn_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_align(fetch_weather_btn, LV_ALIGN_BOTTOM_RIGHT, 2, 2);
    lv_obj_align(weather_btn_label, LV_ALIGN_CENTER, 0, 0);

    // Right side: Forecast list (make 10% narrower and 15% shorter)
    weather_forecast_list = lv_list_create(weather_tab);
    lv_coord_t forecast_w = (WEATHER_RIGHT_WIDTH - 20) * 9 / 9.5;
    lv_coord_t forecast_h = (350 * 95) / 100;
    lv_obj_set_size(weather_forecast_list, forecast_w, forecast_h);
    // Tuck list slightly inward and closer to the top; avoid shifting past right/top edges
    lv_obj_align(weather_forecast_list, LV_ALIGN_TOP_RIGHT, -10, 10);

    lv_obj_set_style_bg_opa(weather_tab, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(weather_tab, 0, 0);

    lv_obj_set_style_bg_opa(weather_forecast_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(weather_forecast_list, 0, 0); // No border
    lv_obj_set_style_border_width(weather_icon_label, 0, 0);
    lv_obj_set_style_bg_opa(weather_icon_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(weather_temp_label, 0, 0);
    lv_obj_set_style_border_width(weather_humidity_label, 0, 0);
    lv_obj_set_style_border_width(weather_wind_label, 0, 0);
    lv_obj_set_style_border_width(weather_conditions_label, 0, 0);

    lv_obj_align(weather_icon_label, LV_ALIGN_TOP_LEFT, 22, 1);
    lv_obj_align(weather_temp_label, LV_ALIGN_BOTTOM_LEFT, 22, -150);
    lv_obj_align(weather_humidity_label, LV_ALIGN_BOTTOM_LEFT, 22, -100);
    lv_obj_align(weather_wind_label, LV_ALIGN_BOTTOM_LEFT, 22, -50);
    lv_obj_align(weather_conditions_label, LV_ALIGN_BOTTOM_LEFT, 22, 1);

    // --- Cameras Tab ---
    camera_img_widget = lv_img_create(cameras_tab);
    lv_obj_remove_style_all(camera_img_widget); // Remove all default/theme styles

    lv_obj_set_size(camera_img_widget, 405, 304);
    lv_obj_align(camera_img_widget, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(camera_img_widget, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(camera_img_widget, LV_OBJ_FLAG_CLICK_FOCUSABLE);                             // Prevent focus highlighting
    lv_obj_clear_state(camera_img_widget, LV_STATE_PRESSED | LV_STATE_FOCUSED | LV_STATE_CHECKED); // Force default state

    // Add a custom style to prevent borders/outlines and ensure consistency
    static lv_style_t camera_img_style;
    lv_style_init(&camera_img_style);
    lv_style_set_border_width(&camera_img_style, 0);                          // No border
    lv_style_set_bg_opa(&camera_img_style, LV_OPA_COVER);                    // Transparent background
    lv_style_set_outline_width(&camera_img_style, 0);                         // No outline
    lv_obj_add_style(camera_img_widget, &camera_img_style, LV_STATE_DEFAULT); // Apply to default state only

    lv_obj_set_style_bg_color(camera_img_widget, lv_color_hex(0x1976D2), LV_PART_MAIN);      // Inactive color
    lv_obj_set_style_bg_color(camera_img_widget, lv_color_hex(0x1976D2), LV_PART_INDICATOR); // Active color
    lv_obj_set_style_bg_color(camera_img_widget, lv_color_hex(0x1976D2), LV_PART_KNOB);      // Knob color
    // No style changes for pressed/focused states to prevent flashing
    lv_obj_invalidate(camera_img_widget);

    // Auto Refresh Switch and Label (underneath video frame)
    auto_refresh_switch = lv_switch_create(cameras_tab);
    lv_obj_set_size(auto_refresh_switch, 80, 40);
    lv_obj_align(auto_refresh_switch, LV_ALIGN_TOP_LEFT, 10, 314); // Adjusted for new video height: 304 + 10
    lv_obj_add_event_cb(auto_refresh_switch, auto_refresh_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(auto_refresh_switch, COLOR_BLUE, LV_PART_MAIN); // Inactive color
    // lv_obj_set_style_bg_color(auto_refresh_switch, COLOR_BLUE, LV_PART_INDICATOR); // Active color
    // lv_obj_set_style_bg_color(auto_refresh_switch, COLOR_BLUE, LV_PART_KNOB);      // Knob color
    //  Disable highlighting/selectability
    lv_obj_clear_flag(auto_refresh_switch, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    // Default to off (no LV_STATE_CHECKED)

    auto_refresh_switch_label = lv_label_create(cameras_tab);
    lv_label_set_text(auto_refresh_switch_label, "Auto Refresh");
    lv_obj_set_style_text_color(auto_refresh_switch_label, COLOR_DARK_GREY, 0);
    // lv_obj_set_style_bg_color(auto_refresh_switch_label, LV_OPA_TRANSP, LV_PART_MAIN);      // Inactive color

    lv_obj_align_to(auto_refresh_switch_label, auto_refresh_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // --- Camera button styles (shared for all camera buttons) ---
    static lv_style_t camera_btn_style;
    lv_style_init(&camera_btn_style);

    lv_style_set_radius(&camera_btn_style, 3);
    lv_style_set_bg_opa(&camera_btn_style, LV_OPA_COVER);                                  // Opaque background for gradient
    lv_style_set_bg_color(&camera_btn_style, lv_color_lighten(COLOR_BLUE, LV_OPA_20));     // Lightened blue for top of gradient
    lv_style_set_bg_grad_color(&camera_btn_style, lv_color_darken(COLOR_BLUE, LV_OPA_20)); // Darkened blue for bottom of gradient
    lv_style_set_bg_grad_dir(&camera_btn_style, LV_GRAD_DIR_VER);                          // Vertical gradient
    lv_style_set_border_width(&camera_btn_style, 2);                                       // Border width: 2 pixels
    lv_style_set_border_color(&camera_btn_style, COLOR_GREY);                              // Fixed grey (no palette)
    lv_style_set_shadow_width(&camera_btn_style, 5);                                       // Shadow width: 5 pixels for 3D depth
    lv_style_set_shadow_color(&camera_btn_style, lv_color_darken(COLOR_GREY, LV_OPA_50));  // Darker grey shadow
    lv_style_set_shadow_ofs_x(&camera_btn_style, 2);                                       // Shadow offset X
    lv_style_set_shadow_ofs_y(&camera_btn_style, 2);                                       // Shadow offset Y
    lv_style_set_outline_color(&camera_btn_style, COLOR_BLUE);                             // Fixed blue (no palette)
    lv_style_set_text_color(&camera_btn_style, COLOR_WHITE);

    static lv_style_t camera_btn_style_pressed;
    lv_style_init(&camera_btn_style_pressed);

    // Make pressed state look sunken (reverse gradient, reduced shadow)
    lv_style_set_radius(&camera_btn_style_pressed, 3);
    lv_style_set_bg_opa(&camera_btn_style_pressed, LV_OPA_COVER);                                   // Opaque background for gradient
    lv_style_set_bg_color(&camera_btn_style_pressed, lv_color_darken(COLOR_BLUE, LV_OPA_20));       // Darkened blue for top of gradient
    lv_style_set_bg_grad_color(&camera_btn_style_pressed, lv_color_lighten(COLOR_BLUE, LV_OPA_20)); // Lightened blue for bottom of gradient
    lv_style_set_bg_grad_dir(&camera_btn_style_pressed, LV_GRAD_DIR_VER);                           // Vertical gradient
    lv_style_set_border_width(&camera_btn_style_pressed, 2);                                        // Border width: 2 pixels
    lv_style_set_border_color(&camera_btn_style_pressed, COLOR_GREY);                               // Fixed grey (no palette)
    lv_style_set_shadow_width(&camera_btn_style_pressed, 2);                                        // Reduced shadow for pressed effect
    lv_style_set_shadow_color(&camera_btn_style_pressed, lv_color_darken(COLOR_GREY, LV_OPA_50));   // Darker grey shadow
    lv_style_set_shadow_ofs_x(&camera_btn_style_pressed, 1);                                        // Reduced shadow offset X
    lv_style_set_shadow_ofs_y(&camera_btn_style_pressed, 1);                                        // Reduced shadow offset Y
    lv_style_set_outline_width(&camera_btn_style_pressed, 1);                                       // Outline width: 1 pixel
    lv_style_set_outline_color(&camera_btn_style_pressed, COLOR_BLUE);                              // Fixed blue (no palette)
    lv_style_set_text_color(&camera_btn_style_pressed, COLOR_WHITE);

    // North Driveway button (Row 0)
    fetch_image_button = lv_btn_create(cameras_tab);
    lv_obj_set_size(fetch_image_button, 225, 75); // Updated size
    lv_obj_add_event_cb(fetch_image_button, fetch_north_driveway, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(fetch_image_button, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(fetch_image_button);                                       // Remove theme styles
    lv_obj_add_style(fetch_image_button, &camera_btn_style, LV_STATE_DEFAULT);
    lv_obj_add_style(fetch_image_button, &camera_btn_style_pressed, LV_STATE_PRESSED);
    lv_obj_t *image_btn_label = lv_label_create(fetch_image_button);
    lv_label_set_text(image_btn_label, "North Driveway");
    lv_obj_remove_style_all(image_btn_label); // Remove all default styles
    lv_obj_align(image_btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(image_btn_label, 15, 0); // Add 15px padding around the label
    lv_obj_set_style_bg_opa(image_btn_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(image_btn_label, 0, 0);
    lv_obj_clear_flag(image_btn_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(fetch_image_button, LV_ALIGN_TOP_LEFT, 435, 5); // Shifted right by 10px

    // East Driveway button (Row 1)
    camera3_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera3_btn, 225, 75); // Size unchanged
    lv_obj_add_event_cb(camera3_btn, east_driveway_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera3_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera3_btn);                                       // Remove theme styles (as in example)
    lv_obj_add_style(camera3_btn, &camera_btn_style, LV_STATE_DEFAULT);         // Removed LV_PART_MAIN
    lv_obj_add_style(camera3_btn, &camera_btn_style_pressed, LV_STATE_PRESSED); // Removed LV_PART_MAIN
    lv_obj_t *cam3_label = lv_label_create(camera3_btn);
    lv_label_set_text(cam3_label, "East Driveway");
    lv_obj_remove_style_all(cam3_label); // Remove all default styles
    lv_obj_align(cam3_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(cam3_label, 15, 0); // Add 15px padding around the label
    lv_obj_set_style_bg_opa(cam3_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cam3_label, 0, 0);
    lv_obj_clear_flag(cam3_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera3_btn, LV_ALIGN_TOP_LEFT, 618, 5); // Shifted right by 10px

    // South Driveway button (Row 2)
    camera_north_canal_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_north_canal_btn, 225, 75); // Updated size
    lv_obj_add_event_cb(camera_north_canal_btn, camera_north_canal_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_north_canal_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_north_canal_btn);                                       // Remove theme styles (as in example)
    lv_obj_add_style(camera_north_canal_btn, &camera_btn_style, LV_STATE_DEFAULT);         // Removed LV_PART_MAIN
    lv_obj_add_style(camera_north_canal_btn, &camera_btn_style_pressed, LV_STATE_PRESSED); // Removed LV_PART_MAIN
    lv_obj_t *north_canal_label = lv_label_create(camera_north_canal_btn);
    lv_label_set_text(north_canal_label, "South Driveway");
    lv_obj_remove_style_all(north_canal_label); // Remove all default styles
    lv_obj_align(north_canal_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(north_canal_label, 15, 0); // Add 15px padding around the label
    lv_obj_set_style_bg_opa(north_canal_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(north_canal_label, 0, 0);
    lv_obj_clear_flag(north_canal_label, LV_OBJ_FLAG_CLICKABLE);
    // Reduce vertical separation between camera button rows by half (205 -> 105)
    lv_obj_align(camera_north_canal_btn, LV_ALIGN_TOP_LEFT, 618, 85); // Updated X to match East Driveway, Y halved

    // Front Door button (Row 3)
    camera_front_door_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_front_door_btn, 225, 75); // Updated size
    lv_obj_add_event_cb(camera_front_door_btn, camera_front_door_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_front_door_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_front_door_btn);                                       // Remove theme styles (as in example)
    lv_obj_add_style(camera_front_door_btn, &camera_btn_style, LV_STATE_DEFAULT);         // Removed LV_PART_MAIN
    lv_obj_add_style(camera_front_door_btn, &camera_btn_style_pressed, LV_STATE_PRESSED); // Removed LV_PART_MAIN
    lv_obj_t *front_door_label = lv_label_create(camera_front_door_btn);
    lv_label_set_text(front_door_label, " Front Door ");
    lv_obj_remove_style_all(front_door_label); // Remove all default styles
    lv_obj_align(front_door_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(front_door_label, 15, 0); // Add 15px padding around the label
    lv_obj_set_style_bg_opa(front_door_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(front_door_label, 0, 0);
    lv_obj_clear_flag(front_door_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera_front_door_btn, LV_ALIGN_TOP_LEFT, 435, 85); // Updated X and Y for even spacing

    // West Canal button (Row 4)
    camera_west_canal_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_west_canal_btn, 225, 75); // Updated size
    lv_obj_add_event_cb(camera_west_canal_btn, camera_west_canal_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_west_canal_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_west_canal_btn);                                       // Remove theme styles (as in example)
    lv_obj_add_style(camera_west_canal_btn, &camera_btn_style, LV_STATE_DEFAULT);         // Removed LV_PART_MAIN
    lv_obj_add_style(camera_west_canal_btn, &camera_btn_style_pressed, LV_STATE_PRESSED); // Removed LV_PART_MAIN
    lv_obj_t *west_canal_label = lv_label_create(camera_west_canal_btn);
    lv_label_set_text(west_canal_label, " West Canal ");
    lv_obj_remove_style_all(west_canal_label); // Remove all default styles
    lv_obj_align(west_canal_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(west_canal_label, 15, 0); // Add 15px padding around the label
    lv_obj_set_style_bg_opa(west_canal_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(west_canal_label, 0, 0);
    lv_obj_clear_flag(west_canal_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera_west_canal_btn, LV_ALIGN_TOP_LEFT, 618, 185); // Updated X to match East Driveway, Y to match Front Door

    // Tiki button (Row 5)
    camera_tiki_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_tiki_btn, 225, 75); // Updated size
    lv_obj_add_event_cb(camera_tiki_btn, camera_tiki_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_tiki_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_tiki_btn);                                       // Remove theme styles (as in example)
    lv_obj_add_style(camera_tiki_btn, &camera_btn_style, LV_STATE_DEFAULT);         // Removed LV_PART_MAIN
    lv_obj_add_style(camera_tiki_btn, &camera_btn_style_pressed, LV_STATE_PRESSED); // Removed LV_PART_MAIN
    lv_obj_t *tiki_label = lv_label_create(camera_tiki_btn);
    lv_label_set_text(tiki_label, "   Tiki    ");
    lv_obj_remove_style_all(tiki_label); // Remove all default styles
    lv_obj_align(tiki_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(tiki_label, 15, 0); // Add 15px padding around the label
    lv_obj_set_style_bg_opa(tiki_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tiki_label, 0, 0);
    lv_obj_clear_flag(tiki_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera_tiki_btn, LV_ALIGN_TOP_LEFT, 435, 185); // Adjusted X and Y for even spacing

    // North Canal button (Row 6)
    camera_south_yard_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_south_yard_btn, 225, 75); // Updated size
    lv_obj_add_event_cb(camera_south_yard_btn, camera_south_yard_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_south_yard_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_south_yard_btn);                                       // Remove theme styles (as in example)
    lv_obj_add_style(camera_south_yard_btn, &camera_btn_style, LV_STATE_DEFAULT);         // Removed LV_PART_MAIN
    lv_obj_add_style(camera_south_yard_btn, &camera_btn_style_pressed, LV_STATE_PRESSED); // Removed LV_PART_MAIN
    lv_obj_t *south_yard_label = lv_label_create(camera_south_yard_btn);
    lv_label_set_text(south_yard_label, " North Canal ");
    lv_obj_align(south_yard_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(south_yard_label, 15, 0); // Add 15px padding around the label
    lv_obj_set_style_bg_opa(south_yard_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(south_yard_label, 0, 0);
    lv_obj_clear_flag(south_yard_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_invalidate(camera_south_yard_btn);

    lv_obj_align(camera_south_yard_btn, LV_ALIGN_TOP_LEFT, 425, 285); // Adjusted Y

    // South Yard button (Row 7)
    camera8_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera8_btn, 225, 75); // Updated size
    lv_obj_add_event_cb(camera8_btn, camera8_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera8_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera8_btn);
    lv_obj_add_style(camera8_btn, &camera_btn_style, LV_STATE_DEFAULT);         // Removed LV_PART_MAIN
    lv_obj_add_style(camera8_btn, &camera_btn_style_pressed, LV_STATE_PRESSED); // Removed LV_PART_MAIN
    lv_obj_t *cam8_label = lv_label_create(camera8_btn);
    lv_label_set_text(cam8_label, " South Yard ");
    lv_obj_remove_style_all(cam8_label); // Remove all default styles
    lv_obj_align(cam8_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(cam8_label, 15, 0); // Add 15px padding around the label
    lv_obj_set_style_bg_opa(cam8_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cam8_label, 0, 0);
    lv_obj_clear_flag(cam8_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera8_btn, LV_ALIGN_TOP_LEFT, 618, 285); // Adjusted Y

    // --- Central Controller Tab ---
    lv_color_t label_color = lv_color_hex(0x263238); // Blue Gray 900

    // Water Valve Switch and Label
    water_switch = lv_switch_create(controller_tab);
    lv_obj_set_size(water_switch, 80, 40);
    lv_obj_align(water_switch, LV_ALIGN_TOP_LEFT, 10, 110);
    lv_obj_add_event_cb(water_switch, water_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // Unchecked (OFF) state - gray background
    lv_obj_set_style_bg_color(water_switch, lv_color_hex(0x9E9E9E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(water_switch, lv_color_hex(0x9E9E9E), LV_PART_INDICATOR);
    // Checked (ON) state - green indicator and knob
    lv_obj_set_style_bg_color(water_switch, lv_color_hex(0x4CAF50), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(water_switch, lv_color_white(), LV_PART_KNOB);

    water_switch_label = lv_label_create(controller_tab);
    lv_label_set_text(water_switch_label, "Water Valve");
    lv_obj_set_style_text_color(water_switch_label, label_color, 0);
    lv_obj_set_style_bg_opa(water_switch_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(water_switch_label, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(water_switch_label, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(water_switch_label, LV_OPA_TRANSP, LV_STATE_EDITED);
    lv_obj_align_to(water_switch_label, water_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Central Vacuum Switch and Label
    central_vacuum_switch = lv_switch_create(controller_tab);
    lv_obj_set_size(central_vacuum_switch, 80, 40);
    lv_obj_align(central_vacuum_switch, LV_ALIGN_TOP_LEFT, 10, 170);
    lv_obj_add_event_cb(central_vacuum_switch, central_vacuum_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // Unchecked (OFF) state - gray background
    lv_obj_set_style_bg_color(central_vacuum_switch, lv_color_hex(0x9E9E9E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(central_vacuum_switch, lv_color_hex(0x9E9E9E), LV_PART_INDICATOR);
    // Checked (ON) state - green indicator and knob
    lv_obj_set_style_bg_color(central_vacuum_switch, lv_color_hex(0x4CAF50), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(central_vacuum_switch, lv_color_white(), LV_PART_KNOB);

    central_vacuum_switch_label = lv_label_create(controller_tab);
    lv_label_set_text(central_vacuum_switch_label, "Central Vacuum");
    lv_obj_set_style_text_color(central_vacuum_switch_label, label_color, 0);
    lv_obj_set_style_bg_opa(central_vacuum_switch_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(central_vacuum_switch_label, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(central_vacuum_switch_label, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(central_vacuum_switch_label, LV_OPA_TRANSP, LV_STATE_EDITED);
    lv_obj_align_to(central_vacuum_switch_label, central_vacuum_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Vacuum Pump Switch and Label
    vacuum_pump_switch = lv_switch_create(controller_tab);
    lv_obj_set_size(vacuum_pump_switch, 80, 40);
    lv_obj_align(vacuum_pump_switch, LV_ALIGN_TOP_LEFT, 10, 230);
    lv_obj_add_event_cb(vacuum_pump_switch, vacuum_pump_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // Unchecked (OFF) state - gray background
    lv_obj_set_style_bg_color(vacuum_pump_switch, lv_color_hex(0x9E9E9E), LV_PART_MAIN);
    lv_obj_set_style_bg_color(vacuum_pump_switch, lv_color_hex(0x9E9E9E), LV_PART_INDICATOR);
    // Checked (ON) state - green indicator and knob
    lv_obj_set_style_bg_color(vacuum_pump_switch, lv_color_hex(0x4CAF50), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(vacuum_pump_switch, lv_color_white(), LV_PART_KNOB);

    vacuum_pump_switch_label = lv_label_create(controller_tab);
    lv_label_set_text(vacuum_pump_switch_label, "Vacuum Pump");
    lv_obj_set_style_text_color(vacuum_pump_switch_label, label_color, 0);
    lv_obj_set_style_bg_opa(vacuum_pump_switch_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(vacuum_pump_switch_label, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(vacuum_pump_switch_label, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(vacuum_pump_switch_label, LV_OPA_TRANSP, LV_STATE_EDITED);
    lv_obj_align_to(vacuum_pump_switch_label, vacuum_pump_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Thermostat dial (Nest) - circular arc with ambient + setpoint labels (display in °F)
    thermo_arc = lv_arc_create(controller_tab);
    
    // CRITICAL: Disable LVGL's automatic state management for the arc to prevent theme-based background changes
    // This prevents LVGL from automatically adding/removing PRESSED, FOCUSED, etc. styles
    lv_obj_remove_style_all(thermo_arc);
    
    // IMPORTANT: Restore size and position after removing all styles
    lv_obj_set_size(thermo_arc, 260, 260);
    lv_obj_align(thermo_arc, LV_ALIGN_RIGHT_MID, -100, -40); // right side of tab
    
    // Set arc configuration
    lv_arc_set_bg_angles(thermo_arc, 135, 45);              // 270-degree dial
    lv_arc_set_rotation(thermo_arc, 0);
    // Display range: 60.0°F .. 86.0°F
    lv_arc_set_range(thermo_arc, 6000, 8600);
    int initial_f = (int)(thermostat_setpoint_c * 100.0f);
    lv_arc_set_value(thermo_arc, initial_f);
    
    // Get the theme's primary color for the indicator
    lv_color_t indicator_color = lv_palette_main(LV_PALETTE_BLUE);  // Use palette instead of theme function
    // Use a consistent grey for the arc track
    lv_color_t arc_bg_color = lv_color_hex(0xE0E0E0);
    lv_color_t knob_color = lv_color_white();
    
    // Manually set all styles after removing defaults
    // Main part (background track)
    lv_obj_set_style_bg_opa(thermo_arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(thermo_arc, arc_bg_color, LV_PART_MAIN);
    lv_obj_set_style_arc_width(thermo_arc, 15, LV_PART_MAIN);
    lv_obj_set_style_arc_color(thermo_arc, arc_bg_color, LV_PART_MAIN);
    
    // Indicator part (filled arc)
    lv_obj_set_style_bg_opa(thermo_arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(thermo_arc, indicator_color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(thermo_arc, 15, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(thermo_arc, indicator_color, LV_PART_INDICATOR);
    
    // Knob part (draggable handle)
    lv_obj_set_style_bg_opa(thermo_arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(thermo_arc, knob_color, LV_PART_KNOB);
    lv_obj_set_style_pad_all(thermo_arc, 5, LV_PART_KNOB);
    
    lv_obj_add_event_cb(thermo_arc, thermostat_arc_event_cb, LV_EVENT_ALL, NULL);

    // Labels inside the dial
    // Setpoint label at top - large and prominent
    thermo_label_setpoint = lv_label_create(thermo_arc);
    lv_obj_align(thermo_label_setpoint, LV_ALIGN_CENTER, 0, -30);  // Moved higher up
    // Use theme's primary color (same as arc indicator) and make font 3x larger
    lv_obj_set_style_text_color(thermo_label_setpoint, lv_theme_get_color_primary(thermo_arc), 0);
    lv_obj_set_style_text_font(thermo_label_setpoint, &lv_font_montserrat_48, 0);  // Large font (3x the default ~16px = 48px)
    lv_obj_set_style_bg_opa(thermo_label_setpoint, LV_OPA_TRANSP, 0);  // Transparent background
    // Prevent label background from changing on any state
    lv_obj_set_style_bg_opa(thermo_label_setpoint, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(thermo_label_setpoint, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(thermo_label_setpoint, LV_OPA_TRANSP, LV_STATE_EDITED);
    lv_obj_set_style_bg_opa(thermo_label_setpoint, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
    lv_label_set_text(thermo_label_setpoint, "--.-°F");

    // Ambient temperature and humidity on same line, positioned near bottom of arc
    thermo_label_ambient = lv_label_create(thermo_arc);
    lv_obj_align(thermo_label_ambient, LV_ALIGN_CENTER, 0, 50);  // Move down (positive Y value)
    lv_obj_set_style_text_color(thermo_label_ambient, COLOR_DARK_GREY, 0);
    lv_obj_set_style_bg_opa(thermo_label_ambient, LV_OPA_TRANSP, 0);  // Transparent background
    // Prevent label background from changing on any state
    lv_obj_set_style_bg_opa(thermo_label_ambient, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(thermo_label_ambient, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(thermo_label_ambient, LV_OPA_TRANSP, LV_STATE_EDITED);
    lv_obj_set_style_bg_opa(thermo_label_ambient, LV_OPA_TRANSP, LV_STATE_FOCUS_KEY);
    lv_label_set_text(thermo_label_ambient, "--.-°F, --% RH");

    // Keep humidity label pointer for compatibility but make it hidden
    thermo_label_humidity = lv_label_create(thermo_arc);
    lv_obj_add_flag(thermo_label_humidity, LV_OBJ_FLAG_HIDDEN);  // Hide it since we combined with ambient
    lv_obj_set_style_bg_opa(thermo_label_humidity, LV_OPA_TRANSP, 0);  // Transparent background
    
    // Add +/- buttons below the arc for manual temperature adjustment
    thermo_btn_minus = lv_btn_create(controller_tab);
    lv_obj_set_size(thermo_btn_minus, 120, 60);
    lv_obj_align_to(thermo_btn_minus, thermo_arc, LV_ALIGN_OUT_BOTTOM_MID, -80, 10);
    // Prevent background color changes when pressing buttons
    lv_obj_clear_flag(thermo_btn_minus, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(thermo_btn_minus, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    // Get the default button background color and apply it to all states
    lv_color_t btn_bg_color = lv_obj_get_style_bg_color(thermo_btn_minus, LV_PART_MAIN);
    lv_obj_set_style_bg_color(thermo_btn_minus, btn_bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(thermo_btn_minus, btn_bg_color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(thermo_btn_minus, btn_bg_color, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(thermo_btn_minus, btn_bg_color, LV_PART_MAIN | LV_STATE_EDITED);
    lv_obj_set_style_bg_color(thermo_btn_minus, btn_bg_color, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_event_cb(thermo_btn_minus, thermo_btn_minus_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *minus_label = lv_label_create(thermo_btn_minus);
    lv_label_set_text(minus_label, "-");
    lv_obj_set_style_text_font(minus_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_bg_opa(minus_label, LV_OPA_TRANSP, 0);  // Transparent label background
    lv_obj_set_style_bg_opa(minus_label, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(minus_label, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(minus_label, LV_OPA_TRANSP, LV_STATE_EDITED);
    lv_obj_center(minus_label);
    
    thermo_btn_plus = lv_btn_create(controller_tab);
    lv_obj_set_size(thermo_btn_plus, 120, 60);
    lv_obj_align_to(thermo_btn_plus, thermo_arc, LV_ALIGN_OUT_BOTTOM_MID, 80, 10);
    // Prevent background color changes when pressing buttons
    lv_obj_clear_flag(thermo_btn_plus, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(thermo_btn_plus, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    // Get the default button background color and apply it to all states
    btn_bg_color = lv_obj_get_style_bg_color(thermo_btn_plus, LV_PART_MAIN);
    lv_obj_set_style_bg_color(thermo_btn_plus, btn_bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(thermo_btn_plus, btn_bg_color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(thermo_btn_plus, btn_bg_color, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_bg_color(thermo_btn_plus, btn_bg_color, LV_PART_MAIN | LV_STATE_EDITED);
    lv_obj_set_style_bg_color(thermo_btn_plus, btn_bg_color, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_add_event_cb(thermo_btn_plus, thermo_btn_plus_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *plus_label = lv_label_create(thermo_btn_plus);
    lv_label_set_text(plus_label, "+");
    lv_obj_set_style_text_font(plus_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_bg_opa(plus_label, LV_OPA_TRANSP, 0);  // Transparent label background
    lv_obj_set_style_bg_opa(plus_label, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(plus_label, LV_OPA_TRANSP, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(plus_label, LV_OPA_TRANSP, LV_STATE_EDITED);
    lv_obj_center(plus_label);
    
    // Initialize labels with current internal values
    lvgl_port_lock(0);
    thermostat_update_labels_locked();
    lvgl_port_unlock();
    
    // Create timer to request thermostat state from HA every 30 seconds
    // Request immediately, then every 30 seconds
    thermo_request_timer = lv_timer_create(thermo_request_timer_cb, 30000, NULL);
    lv_timer_ready(thermo_request_timer);  // Fire immediately

    // --- Versaries Tab (replaces Events tab) ---
    // Show marriage date and elapsed time since marriage
    versaries_date_label = lv_label_create(versaries_tab);
    lv_label_set_text(versaries_date_label, "Married: Nov 3, 2023");
    lv_obj_set_style_text_color(versaries_date_label, COLOR_DARK_GREY, 0);
    lv_obj_align(versaries_date_label, LV_ALIGN_TOP_MID, 0, 40);

    versaries_count_label = lv_label_create(versaries_tab);
    lv_label_set_text(versaries_count_label, "-- years, -- months, -- days");
    lv_obj_set_style_text_color(versaries_count_label, COLOR_DARK_GREY, 0);
    lv_obj_align(versaries_count_label, LV_ALIGN_TOP_MID, 0, 80);

    // Mia's Birthday row (center date, left = days since, right = days until)
    mia_since_label = lv_label_create(versaries_tab);
    lv_label_set_text(mia_since_label, "0 days since");
    lv_obj_set_style_text_color(mia_since_label, COLOR_DARK_GREY, 0);
    lv_obj_align(mia_since_label, LV_ALIGN_TOP_MID, -160, 130);

    mia_date_label = lv_label_create(versaries_tab);
    lv_label_set_text(mia_date_label, "Mia: 05-08");
    lv_obj_set_style_text_color(mia_date_label, COLOR_DARK_GREY, 0);
    lv_obj_align(mia_date_label, LV_ALIGN_TOP_MID, 0, 130);

    mia_until_label = lv_label_create(versaries_tab);
    lv_label_set_text(mia_until_label, "0 days until");
    lv_obj_set_style_text_color(mia_until_label, COLOR_DARK_GREY, 0);
    lv_obj_align(mia_until_label, LV_ALIGN_TOP_MID, 160, 130);

    // My Birthday row
    my_since_label = lv_label_create(versaries_tab);
    lv_label_set_text(my_since_label, "0 days since");
    lv_obj_set_style_text_color(my_since_label, COLOR_DARK_GREY, 0);
    lv_obj_align(my_since_label, LV_ALIGN_TOP_MID, -160, 170);

    my_date_label = lv_label_create(versaries_tab);
    lv_label_set_text(my_date_label, "Daniel: 08-22");
    lv_obj_set_style_text_color(my_date_label, COLOR_DARK_GREY, 0);
    lv_obj_align(my_date_label, LV_ALIGN_TOP_MID, 0, 170);

    my_until_label = lv_label_create(versaries_tab);
    lv_label_set_text(my_until_label, "0 days until");
    lv_obj_set_style_text_color(my_until_label, COLOR_DARK_GREY, 0);
    lv_obj_align(my_until_label, LV_ALIGN_TOP_MID, 160, 170);

    // Setup timer to refresh the versaries display every minute
    versaries_setup_timer_and_update();

    // call helper immediately to populate labels (helper defined below)
    // ...existing code...

    lv_obj_update_layout(cameras_tab);

    // Start weather auto-refresh timer (10s retry until success, then 30min)
    weather_init_timer();
}

// --- Switch event handlers ---

static void water_switch_event_cb(lv_event_t *e)
{
    bool new_state = lv_obj_has_state(water_switch, LV_STATE_CHECKED);
    mqtt_publish_water_valve_state(new_state);
}

static void central_vacuum_switch_event_cb(lv_event_t *e)
{
    bool new_state = lv_obj_has_state(central_vacuum_switch, LV_STATE_CHECKED);
    mqtt_publish_central_vacuum_state(new_state);
}

static void vacuum_pump_switch_event_cb(lv_event_t *e)
{
    bool new_state = lv_obj_has_state(vacuum_pump_switch, LV_STATE_CHECKED);
    mqtt_publish_vacuum_pump_state(new_state);
}

// --- Update relay state callbacks ---

void water_valve_state_cb(int relay_index, bool state)
{
    lvgl_port_lock(0);
    if (water_switch)
    {
        if (state)
        {
            lv_obj_add_state(water_switch, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(water_switch, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();
}

void central_vacuum_state_cb(int relay_index, bool state)
{
    lvgl_port_lock(0);
    if (central_vacuum_switch)
    {
        if (state)
        {
            lv_obj_add_state(central_vacuum_switch, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(central_vacuum_switch, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();
}

void vacuum_pump_state_cb(int relay_index, bool state)
{
    lvgl_port_lock(0);
    if (vacuum_pump_switch)
    {
        if (state)
        {
            lv_obj_add_state(vacuum_pump_switch, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(vacuum_pump_switch, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();
}

void lcd_update_wifi_status(const char *ssid, const char *ip)
{
    lvgl_port_lock(0);
    if (wifi_ssid_label && ssid)
    {
        lv_label_set_text(wifi_ssid_label, ssid);
    }
    if (wifi_ip_label && ip)
    {
        lv_label_set_text(wifi_ip_label, ip);
    }
    lvgl_port_unlock();
}

void lcd_update_ha_status(bool connected, const char *ip)
{
    lvgl_port_lock(0);
    if (ha_status_icon)
    {
        if (connected)
        {
            lv_label_set_text(ha_status_icon, LV_SYMBOL_OK " HA Connected");
        }
        else
        {
            lv_label_set_text(ha_status_icon, LV_SYMBOL_CLOSE " HA Disconnected");
        }
    }
    if (ha_ip_label && ip)
    {
        lv_label_set_text(ha_ip_label, ip);
    }
    lvgl_port_unlock();
}

// Timer callback for auto refresh
static void auto_refresh_timer_cb(lv_timer_t *timer)
{
    if (current_camera_index > 0)
    {
        extern void camera_client_fetch_image(int camera_index);
        camera_client_fetch_image(current_camera_index);
    }
}

// Health monitoring callback to detect and log system issues
static void health_monitor_callback(void *arg) {
    static const char *TAG = "HEALTH";
    static uint32_t last_min_heap = 0;
    
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_heap = esp_get_minimum_free_heap_size();
    
    // Log heap status every check
    ESP_LOGI(TAG, "Heap: free=%" PRIu32 ", min=%" PRIu32 ", LVGL_running=%d, MQTT=%d", 
             free_heap, min_heap, 1, mqtt_is_connected());
    
    // Warn if heap is getting low
    if (free_heap < 100000) {
        ESP_LOGW(TAG, "Low heap warning: %" PRIu32 " bytes remaining", free_heap);
    }
    
    // Detect if minimum heap is decreasing (potential leak)
    if (last_min_heap > 0 && min_heap < last_min_heap - 10000) {
        ESP_LOGW(TAG, "Heap leak detected: min heap dropped from %" PRIu32 " to %" PRIu32 "", last_min_heap, min_heap);
    }
    last_min_heap = min_heap;
}

void lcd_init(void)
{
    // LCD and LVGL init handled in main.c
    // Initialize PNG decoder once and keep LVGL image cache small to save RAM
    // Limit image cache entries (decoded bitmaps) to reduce memory usage
    static bool s_img_cache_set = false;
    if (!s_img_cache_set)
    {
        lv_img_cache_set_size(1);
        s_img_cache_set = true;
    }
    
    // Start health monitoring timer
    esp_timer_create_args_t health_timer_args = {
        .callback = health_monitor_callback,
        .arg = NULL,
        .name = "health_monitor",
        .skip_unhandled_events = true
    };
    
    if (esp_timer_create(&health_timer_args, &health_monitor_timer) == ESP_OK) {
        esp_timer_start_periodic(health_monitor_timer, HEALTH_CHECK_INTERVAL_MS * 1000);
        ESP_LOGI("LCD", "Health monitor started (10s interval)");
    } else {
        ESP_LOGE("LCD", "Failed to create health monitor timer");
    }
}

static relay_state_change_callback_t relay_cb = NULL;

void set_relay_state_change_callback(relay_state_change_callback_t cb)
{
    relay_cb = cb;
}

void lcd_append_motion_event(const char *event_text)
{
    (void)event_text;
    // Motion / scrolling events removed per request — no-op now.
}

void lcd_update_camera_snapshot(const uint8_t *jpeg_data, size_t jpeg_size)
{
    // Placeholder - handled in camera_client.c
}

// Add a new function to update weather forecast (placeholder)
void lcd_update_weather_forecast(const char *forecast_text)
{
    // Placeholder
}

// Add event handler for auto refresh switch
static void auto_refresh_switch_event_cb(lv_event_t *e)
{
    bool is_on = lv_obj_has_state(auto_refresh_switch, LV_STATE_CHECKED);
    if (is_on)
    {
        if (auto_refresh_timer == NULL)
        {
            auto_refresh_timer = lv_timer_create(auto_refresh_timer_cb, 5000, NULL); // 5 seconds
        }
    }
    else
    {
        if (auto_refresh_timer)
        {
            lv_timer_del(auto_refresh_timer);
            auto_refresh_timer = NULL;
        }
    }
}

void lcd_start_weather_fetch(void)
{
    // Create a task to fetch weather data from HA
    // Need large stack due to 32KB weather buffer + HTTP client overhead
    xTaskCreate(weather_fetch_task_func, "weather_fetch", 12288, NULL, 5, NULL);
}

// Update weather timestamp label
void lcd_update_weather_timestamp(void)
{
    if (weather_timestamp_label == NULL) {
        return;
    }

    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Format timestamp
    char timestamp_buf[64];
    strftime(timestamp_buf, sizeof(timestamp_buf), "Updated: %I:%M %p", &timeinfo);

    // Update label (with LVGL lock)
    lvgl_port_lock(0);
    lv_label_set_text(weather_timestamp_label, timestamp_buf);
    lvgl_port_unlock();
}

// --- Fetch Weather button event handler ---
static void fetch_weather_btn_event_cb(lv_event_t *e)
{
    ESP_LOGI("UI", "Manual weather refresh requested.");
    // Create a task to fetch weather data to avoid blocking the UI
    weather_fetch_and_display();
}

// Helper function to check if weather has been fetched (checks timestamp label)
static bool weather_has_been_fetched(void) {
    if (weather_timestamp_label == NULL) {
        return false;
    }
    const char *text = lv_label_get_text(weather_timestamp_label);
    // Check if timestamp shows actual time (not the default "--:--")
    return (text != NULL && strstr(text, "--:--") == NULL);
}

static void tab_change_event_cb(lv_event_t *e)
{
    lv_obj_t *tv = lv_event_get_target(e);
    uint32_t active_tab_idx = lv_tabview_get_tab_act(tv);

    // Tab indices: 0=Weather, 1=Cameras, 2=Controller, 3=Versaries
    if (active_tab_idx == 0 && previous_tab_idx != 99)
    {
        ESP_LOGI("UI", "Weather tab selected (from tab %u).", (unsigned int)previous_tab_idx);

        // Only fetch if weather has already been fetched before
        // This prevents initial fetch but allows refresh when revisiting tab
        if (weather_has_been_fetched()) {
            ESP_LOGI("UI", "Weather already fetched, refreshing...");
            weather_fetch_and_display();
        } else {
            ESP_LOGI("UI", "Weather not yet fetched, skipping auto-fetch (use refresh button)");
        }
    }

    previous_tab_idx = active_tab_idx;
}

// --- Versaries timer and display logic ---

// Helper function to calculate days between two dates
static int days_between_dates(struct tm *date1, struct tm *date2)
{
    time_t time1 = mktime(date1);
    time_t time2 = mktime(date2);
    return (int)difftime(time2, time1) / (60 * 60 * 24);
}

// Calculate next occurrence of a birthday
static void get_next_birthday(int birth_month, int birth_day, struct tm *now, struct tm *next_bday)
{
    *next_bday = *now;
    next_bday->tm_mon = birth_month - 1; // tm_mon is 0-11
    next_bday->tm_mday = birth_day;
    next_bday->tm_hour = 0;
    next_bday->tm_min = 0;
    next_bday->tm_sec = 0;

    mktime(next_bday); // Normalize

    // If birthday already passed this year, move to next year
    if (mktime(next_bday) < mktime(now))
    {
        next_bday->tm_year++;
        mktime(next_bday); // Normalize again
    }
}

// Calculate last occurrence of a birthday
static void get_last_birthday(int birth_month, int birth_day, struct tm *now, struct tm *last_bday)
{
    *last_bday = *now;
    last_bday->tm_mon = birth_month - 1;
    last_bday->tm_mday = birth_day;
    last_bday->tm_hour = 0;
    last_bday->tm_min = 0;
    last_bday->tm_sec = 0;

    mktime(last_bday);

    // If birthday hasn't occurred yet this year, use last year
    if (mktime(last_bday) > mktime(now))
    {
        last_bday->tm_year--;
        mktime(last_bday);
    }
}

static void update_versaries_display(void)
{
    time_t now_time;
    struct tm now_tm;
    time(&now_time);
    localtime_r(&now_time, &now_tm);

    char buf[128];

    // --- Marriage Anniversary ---
    struct tm marriage_date = {0};
    marriage_date.tm_year = MARRIAGE_DATE_YEAR - 1900;
    marriage_date.tm_mon = MARRIAGE_DATE_MONTH - 1;
    marriage_date.tm_mday = MARRIAGE_DATE_DAY;
    mktime(&marriage_date);

    int days_married = days_between_dates(&marriage_date, &now_tm);
    int years = days_married / 365;
    int months = (days_married % 365) / 30;
    int days = (days_married % 365) % 30;

    if (versaries_count_label)
    {
        lvgl_port_lock(0);
        snprintf(buf, sizeof(buf), "%d years, %d months, %d days", years, months, days);
        lv_label_set_text(versaries_count_label, buf);
        lvgl_port_unlock();
    }

    // --- Mia's Birthday ---
    struct tm mia_next, mia_last;
    get_next_birthday(MIA_BIRTH_MONTH, MIA_BIRTH_DAY, &now_tm, &mia_next);
    get_last_birthday(MIA_BIRTH_MONTH, MIA_BIRTH_DAY, &now_tm, &mia_last);

    int days_until_mia = days_between_dates(&now_tm, &mia_next);
    int days_since_mia = days_between_dates(&mia_last, &now_tm);

    lvgl_port_lock(0);
    if (mia_since_label)
    {
        snprintf(buf, sizeof(buf), "%d days since", days_since_mia);
        lv_label_set_text(mia_since_label, buf);
    }
    if (mia_until_label)
    {
        snprintf(buf, sizeof(buf), "%d days until", days_until_mia);
        lv_label_set_text(mia_until_label, buf);
    }
    lvgl_port_unlock();

    // --- Daniel's Birthday ---
    struct tm my_next, my_last;
    get_next_birthday(MY_BIRTH_MONTH, MY_BIRTH_DAY, &now_tm, &my_next);
    get_last_birthday(MY_BIRTH_MONTH, MY_BIRTH_DAY, &now_tm, &my_last);

    int days_until_mine = days_between_dates(&now_tm, &my_next);
    int days_since_mine = days_between_dates(&my_last, &now_tm);

    lvgl_port_lock(0);
    if (my_since_label)
    {
        snprintf(buf, sizeof(buf), "%d days since", days_since_mine);
        lv_label_set_text(my_since_label, buf);
    }
    if (my_until_label)
    {
        snprintf(buf, sizeof(buf), "%d days until", days_until_mine);
        lv_label_set_text(my_until_label, buf);
    }
    lvgl_port_unlock();
}

// Timer callback for periodic versaries updates
static void versaries_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    update_versaries_display();
}

// Setup timer and do initial update
static void versaries_setup_timer_and_update(void)
{
    // Create timer to update every minute (60000 ms)
    // First update will happen after 60 seconds, giving time for WiFi and SNTP to sync
    versaries_update_timer = lv_timer_create(versaries_timer_cb, 60000, NULL);
}

// =====================
// Thermostat (Nest) UI
// =====================

// Timer callback to request thermostat state from HA via MQTT
static void thermo_request_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    extern esp_err_t mqtt_request_nest_state(void);
    mqtt_request_nest_state();
}

// Event handler for temperature decrement button (-)
static void thermo_btn_minus_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // Rate limiting: prevent button spam (minimum 200ms between clicks)
        int64_t now_us = esp_timer_get_time();
        int64_t elapsed_ms = (now_us - last_publish_time_us) / 1000;
        if (elapsed_ms < 200 && last_publish_time_us > 0) {
            ESP_LOGW("lcd", "Thermostat button clicked too quickly, ignoring (elapsed: %lld ms)", elapsed_ms);
            return;
        }
        
        // Decrease by 1°F, respecting min limit
        float new_temp = thermostat_setpoint_c - 1.0f;
        if (new_temp < 60.0f) new_temp = 60.0f;  // Min 60°F
        
        thermostat_setpoint_c = new_temp;
        
        // Update UI
        lvgl_port_lock(0);
        thermostat_update_labels_locked();
        if (thermo_arc)
        {
            int v_f = (int)(thermostat_setpoint_c * 100.0f);
            lv_arc_set_value(thermo_arc, v_f);
        }
        lvgl_port_unlock();
        
        // Schedule debounced publish (allows rapid clicks to settle)
        schedule_debounced_publish();
    }
}

// Event handler for temperature increment button (+)
static void thermo_btn_plus_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // Rate limiting: prevent button spam (minimum 200ms between clicks)
        int64_t now_us = esp_timer_get_time();
        int64_t elapsed_ms = (now_us - last_publish_time_us) / 1000;
        if (elapsed_ms < 200 && last_publish_time_us > 0) {
            ESP_LOGW("lcd", "Thermostat button clicked too quickly, ignoring (elapsed: %lld ms)", elapsed_ms);
            return;
        }
        
        // Increase by 1°F, respecting max limit
        float new_temp = thermostat_setpoint_c + 1.0f;
        if (new_temp > 86.0f) new_temp = 86.0f;  // Max 86°F
        
        thermostat_setpoint_c = new_temp;
        
        // Update UI
        lvgl_port_lock(0);
        thermostat_update_labels_locked();
        if (thermo_arc)
        {
            int v_f = (int)(thermostat_setpoint_c * 100.0f);
            lv_arc_set_value(thermo_arc, v_f);
        }
        lvgl_port_unlock();
        
        // Schedule debounced publish (allows rapid clicks to settle)
        schedule_debounced_publish();
    }
}

static void thermostat_update_labels_locked(void)
{
    char buf_ambient_combined[48];
    char buf_setpoint[32];
    // Values already in Fahrenheit, no conversion needed
    float amb_f = (thermostat_ambient_c > -999) ? thermostat_ambient_c : 0.0f;
    float sp_f = thermostat_setpoint_c;
    
    // Setpoint label: "72°F"
    snprintf(buf_setpoint, sizeof(buf_setpoint), "%.0f°F", sp_f);
    
    // Ambient label: combine temperature and humidity on one line
    if (thermostat_ambient_c > -999 && thermostat_humidity_pct >= 0.0f) {
        // Both values available: "72°F, 45% RH"
        snprintf(buf_ambient_combined, sizeof(buf_ambient_combined), "%.0f°F, %.0f%% RH", amb_f, thermostat_humidity_pct);
    } else if (thermostat_ambient_c > -999) {
        // Only temperature available: "72°F, --% RH"
        snprintf(buf_ambient_combined, sizeof(buf_ambient_combined), "%.0f°F, --%% RH", amb_f);
    } else if (thermostat_humidity_pct >= 0.0f) {
        // Only humidity available: "--°F, 45% RH"
        snprintf(buf_ambient_combined, sizeof(buf_ambient_combined), "--°F, %.0f%% RH", thermostat_humidity_pct);
    } else {
        // Neither available: keep placeholder
        snprintf(buf_ambient_combined, sizeof(buf_ambient_combined), "--.-°F, --%% RH");
    }
    
    if (thermo_label_ambient) lv_label_set_text(thermo_label_ambient, buf_ambient_combined);
    if (thermo_label_setpoint) lv_label_set_text(thermo_label_setpoint, buf_setpoint);
    // thermo_label_humidity is now hidden and unused
}

// Debounce timer callback - publishes setpoint after delay
static void thermo_publish_debounce_cb(lv_timer_t *timer)
{
    (void)timer;
    
    // Delete the one-shot timer
    if (thermo_publish_debounce_timer) {
        lv_timer_del(thermo_publish_debounce_timer);
        thermo_publish_debounce_timer = NULL;
    }
    
    // Publish to MQTT - convert Fahrenheit to Celsius
    // ESP32 stores in F, but MQTT expects C
    extern esp_err_t mqtt_publish_nest_setpoint_cool(float setpoint_c);
    float setpoint_c = (thermostat_setpoint_c - 32.0f) * 5.0f / 9.0f;
    mqtt_publish_nest_setpoint_cool(setpoint_c);
    last_publish_time_us = esp_timer_get_time();
    ESP_LOGI("lcd", "Thermostat debounce complete: publishing setpoint %.0f°F (%.1f°C)", thermostat_setpoint_c, setpoint_c);
}

// Helper to schedule a debounced publish
static void schedule_debounced_publish(void)
{
    // Cancel any existing debounce timer
    if (thermo_publish_debounce_timer) {
        lv_timer_del(thermo_publish_debounce_timer);
        thermo_publish_debounce_timer = NULL;
    }
    
    // Create new one-shot timer with 500ms delay
    // This gives user time to settle on final value without spamming Nest
    thermo_publish_debounce_timer = lv_timer_create(thermo_publish_debounce_cb, 500, NULL);
    lv_timer_set_repeat_count(thermo_publish_debounce_timer, 1);
}

static void thermostat_arc_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    // Only respond to user touch events, not programmatic value changes
    if (code == LV_EVENT_PRESSING)
    {
        // User is actively dragging - update UI only, don't publish yet
        thermostat_dragging = true;
        
        // Cancel any pending publish while user is still adjusting
        if (thermo_publish_debounce_timer) {
            lv_timer_del(thermo_publish_debounce_timer);
            thermo_publish_debounce_timer = NULL;
        }
        
        if (thermo_arc)
        {
            int v = lv_arc_get_value(thermo_arc);
            // v is in Fahrenheit*100; store directly as Fahrenheit
            float f = v / 100.0f;
            thermostat_setpoint_c = f;
            lvgl_port_lock(0);
            thermostat_update_labels_locked();
            lvgl_port_unlock();
        }
    }
    else if (code == LV_EVENT_RELEASED)
    {
        thermostat_dragging = false;
        
        // Schedule debounced publish after user releases control
        // This prevents spamming Nest with requests during drag
        schedule_debounced_publish();
        ESP_LOGI("lcd", "Thermostat arc released: scheduling debounced publish for %.0f°F", thermostat_setpoint_c);
    }
}

void lcd_publish_thermostat_setpoint(float setpoint_f)
{
    extern esp_err_t mqtt_publish_nest_setpoint_cool(float setpoint_f);
    mqtt_publish_nest_setpoint_cool(setpoint_f);
}

void lcd_update_thermostat_readings(float ambient_c, float setpoint_c, const char *mode)
{
    (void)mode; // reserved for future use (color/style by mode)
    
    // Don't update anything from MQTT while user is actively dragging the control
    if (thermostat_dragging) {
        ESP_LOGI("lcd", "Ignoring MQTT update while user is dragging thermostat control");
        return;
    }
    
    // Convert Celsius to Fahrenheit for display
    // MQTT now sends Celsius values, but ESP32 stores/displays in Fahrenheit
    if (ambient_c > -999.0f) thermostat_ambient_c = (ambient_c * 9.0f / 5.0f) + 32.0f;
    if (setpoint_c > -999.0f) thermostat_setpoint_c = (setpoint_c * 9.0f / 5.0f) + 32.0f;

    lvgl_port_lock(0);
    thermostat_update_labels_locked();
    if (thermo_arc)
    {
        int v_f = (int)(thermostat_setpoint_c * 100.0f);
        lv_arc_set_value(thermo_arc, v_f);
    }
    lvgl_port_unlock();
}

void lcd_update_thermostat_humidity(float humidity_percent)
{
    if (humidity_percent >= 0.0f) {
        thermostat_humidity_pct = humidity_percent;
        lvgl_port_lock(0);
        thermostat_update_labels_locked();
        lvgl_port_unlock();
    }
}