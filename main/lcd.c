#include "lcd.h"
#include <stdbool.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_rgb.h"
#include <string.h>
#include "mqtt.h"
#include "weather.h" // Add this
#include "assets.h"  // << add this so embedded PNG symbols (clear_day_png_start...) are defined
#include <esp_log.h> // needed for ESP_LOGI/ESP_LOGE usage
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h> // added for versaries calculation

void water_valve_state_cb(int relay_index, bool state);
void central_vacuum_state_cb(int relay_index, bool state);
void vacuum_pump_state_cb(int relay_index, bool state);
// TODO: Implement versaries (anniversaries) functionality
// static void versaries_setup_timer_and_update(void);
// static void update_versaries_display(void);

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

// Versaries UI elements (TODO: implement versaries functionality)
// static lv_obj_t *versaries_date_label = NULL;
// static lv_obj_t *versaries_count_label = NULL;
// static lv_timer_t *versaries_update_timer = NULL;

// New birthday labels: Mia (5/8/1995) and Mine (8/22/1980)
static lv_obj_t *mia_date_label = NULL;
static lv_obj_t *mia_since_label = NULL;
static lv_obj_t *mia_until_label = NULL;
static lv_obj_t *my_date_label = NULL;
static lv_obj_t *my_since_label = NULL;
static lv_obj_t *my_until_label = NULL;

// Marriage date (Year, Month, Day)
#define MARRIAGE_DATE_YEAR  2023
#define MARRIAGE_DATE_MONTH 11
#define MARRIAGE_DATE_DAY   3

// Objects referenced from other compilation units: define non-static to match 'extern' in headers
lv_obj_t *camera_img_widget = NULL;            // declared extern in lcd.h
lv_obj_t *weather_forecast_list = NULL;        // declared extern in weather.h

// Weather UI elements (extern'd in weather.h) - define once here (non-static)
lv_obj_t *weather_icon_label = NULL;
lv_obj_t *weather_temp_label = NULL;
lv_obj_t *weather_humidity_label = NULL;
lv_obj_t *weather_wind_label = NULL;
lv_obj_t *weather_conditions_label = NULL;

// Controller / camera UI (private)
static lv_obj_t *water_switch = NULL;
static lv_obj_t *water_switch_label = NULL;
static lv_obj_t *central_vacuum_switch = NULL;
static lv_obj_t *central_vacuum_switch_label = NULL;
static lv_obj_t *vacuum_pump_switch = NULL;
static lv_obj_t *vacuum_pump_switch_label = NULL;
static lv_obj_t *fetch_image_button = NULL;

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

void lcd_create_ui(void)
{
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);

    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Title bar: dark blue background, white text (unchanged)
    title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, 800, 50);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1A237E), 0); // Indigo 900
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar icons and labels: white text (unchanged)
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
    lv_obj_set_style_bg_color(tabview, COLOR_BLUE, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(tabview, COLOR_LIGHT_GREY, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(tabview, COLOR_WHITE, LV_PART_ITEMS);
    lv_obj_set_style_text_color(tabview, COLOR_DARK_GREY, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(tabview, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(tabview, COLOR_BLUE, LV_PART_MAIN);
    lv_obj_set_style_border_width(tabview, 2, LV_PART_ITEMS);
    lv_obj_set_style_border_color(tabview, COLOR_GREY, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(tabview, 1, LV_PART_MAIN);  // Disable outlines
    lv_obj_set_style_shadow_width(tabview, 0, LV_PART_MAIN);
    // Add: Disable outlines on focus for tabs
    lv_obj_set_style_outline_width(tabview, 1, LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(tabview, 2, LV_PART_ITEMS | LV_STATE_FOCUSED);

    // Add tabs (styles applied above will override defaults)
    weather_tab = lv_tabview_add_tab(tabview, "Weather");
    cameras_tab = lv_tabview_add_tab(tabview, "Cameras");
    controller_tab = lv_tabview_add_tab(tabview, "Central Controller");
    // renamed tab label to "Versaries"
    versaries_tab = lv_tabview_add_tab(tabview, "Versaries");

    /* --- Weather Tab --- */
#define WEATHER_LEFT_WIDTH 200  // 25% of ~800px tab width
#define WEATHER_RIGHT_WIDTH 600  // 75% of ~800px tab width

    // Left side: Current forecast info (icon, temp, humidity, wind, conditions, button)
    weather_icon_label = lv_img_create(weather_tab);
    lv_obj_set_size(weather_icon_label, 64, 64);  // Set size to match your PNGs
    lv_obj_align(weather_icon_label, LV_ALIGN_TOP_LEFT, 10, 10);  // Top of left side

    weather_temp_label = lv_label_create(weather_tab);
    lv_label_set_text(weather_temp_label, "Temp: --°F");
    lv_obj_align(weather_temp_label, LV_ALIGN_TOP_LEFT, 10, 80);

    weather_humidity_label = lv_label_create(weather_tab);
    lv_label_set_text(weather_humidity_label, "Humidity: --%");
    lv_obj_align(weather_humidity_label, LV_ALIGN_TOP_LEFT, 10, 110);

    weather_wind_label = lv_label_create(weather_tab);
    lv_label_set_text(weather_wind_label, "Wind: -- mph --");
    lv_obj_align(weather_wind_label, LV_ALIGN_TOP_LEFT, 10, 140);

    weather_conditions_label = lv_label_create(weather_tab);
    lv_label_set_text(weather_conditions_label, "Conditions: --");
    lv_obj_align(weather_conditions_label, LV_ALIGN_TOP_LEFT, 10, 170);

    // Set initial weather icon (now that weather_conditions_label exists)
    set_png_or_error(weather_icon_label, clear_day_png_start, clear_day_png_end, weather_conditions_label);

    // Fetch Weather button: create on the left column, but place it at extreme bottom-left
    lv_obj_t *fetch_weather_btn = lv_btn_create(weather_tab);
    lv_obj_set_size(fetch_weather_btn, 180, 50);  // Fit left width
    lv_obj_add_event_cb(fetch_weather_btn, fetch_weather_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(fetch_weather_btn, lv_color_hex(0x1976D2), LV_PART_MAIN);
    lv_obj_t *weather_btn_label = lv_label_create(fetch_weather_btn);
    lv_label_set_text(weather_btn_label, "Refresh"); // Renamed from "Fetch Weather"
// place at extreme bottom-left of the weather tab with a small margin
    lv_obj_align(fetch_weather_btn, LV_ALIGN_BOTTOM_LEFT, 8, -8);
    lv_obj_align(weather_btn_label, LV_ALIGN_CENTER, 0, 0);

    // Right side: Forecast list (make 10% narrower and 15% shorter)
    weather_forecast_list = lv_list_create(weather_tab);
    lv_coord_t forecast_w = (WEATHER_RIGHT_WIDTH - 20) * 9 / 10;  // 10% narrower
    lv_coord_t forecast_h = (350 * 85) / 100;                    // 15% shorter than original 350
    lv_obj_set_size(weather_forecast_list, forecast_w, forecast_h);
    lv_obj_align(weather_forecast_list, LV_ALIGN_TOP_RIGHT, -10, 10);

    // Keep the PNG where it is, move the lower info labels down toward the bottom-left
    lv_obj_align(weather_icon_label, LV_ALIGN_TOP_LEFT, 10, 10); // unchanged
    lv_obj_align(weather_temp_label, LV_ALIGN_BOTTOM_LEFT, 10, -120);
    lv_obj_align(weather_humidity_label, LV_ALIGN_BOTTOM_LEFT, 10, -100);
    lv_obj_align(weather_wind_label, LV_ALIGN_BOTTOM_LEFT, 10, -80);
    lv_obj_align(weather_conditions_label, LV_ALIGN_BOTTOM_LEFT, 10, -60);

    // --- Cameras Tab ---
    camera_img_widget = lv_img_create(cameras_tab);
    lv_obj_remove_style_all(camera_img_widget);  // Remove all default/theme styles

    lv_obj_set_size(camera_img_widget, 405, 304);
    lv_obj_align(camera_img_widget, LV_ALIGN_TOP_LEFT, 0, 0);
    // Disable highlighting/selectability
    lv_obj_clear_flag(camera_img_widget, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(camera_img_widget, LV_OBJ_FLAG_CLICK_FOCUSABLE);  // Prevent focus highlighting
    lv_obj_clear_state(camera_img_widget, LV_STATE_PRESSED | LV_STATE_FOCUSED | LV_STATE_CHECKED);  // Force default state

    // Add a custom style to prevent borders/outlines and ensure consistency
    static lv_style_t camera_img_style;
    lv_style_init(&camera_img_style);
    lv_style_set_border_width(&camera_img_style, 0);  // No border
    lv_style_set_bg_opa(&camera_img_style, LV_OPA_TRANSP);  // Transparent background
    lv_style_set_outline_width(&camera_img_style, 0);  // No outline
    lv_obj_add_style(camera_img_widget, &camera_img_style, LV_STATE_DEFAULT);  // Apply to default state only


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
    lv_obj_set_style_bg_color(auto_refresh_switch, COLOR_BLUE, LV_PART_MAIN);      // Inactive color
    //lv_obj_set_style_bg_color(auto_refresh_switch, COLOR_BLUE, LV_PART_INDICATOR); // Active color
    //lv_obj_set_style_bg_color(auto_refresh_switch, COLOR_BLUE, LV_PART_KNOB);      // Knob color
    // Disable highlighting/selectability
    lv_obj_clear_flag(auto_refresh_switch, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    // Default to off (no LV_STATE_CHECKED)

    auto_refresh_switch_label = lv_label_create(cameras_tab);
    lv_label_set_text(auto_refresh_switch_label, "Auto Refresh");
    lv_obj_set_style_text_color(auto_refresh_switch_label, COLOR_DARK_GREY, 0);
    //lv_obj_set_style_bg_color(auto_refresh_switch_label, LV_OPA_TRANSP, LV_PART_MAIN);      // Inactive color

    lv_obj_align_to(auto_refresh_switch_label, auto_refresh_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // --- Camera button styles (shared for all camera buttons) ---
    static lv_style_t camera_btn_style;
lv_style_init(&camera_btn_style);

lv_style_set_radius(&camera_btn_style, 3);
lv_style_set_bg_opa(&camera_btn_style, LV_OPA_COVER);                      // Opaque background for gradient
lv_style_set_bg_color(&camera_btn_style, lv_color_lighten(COLOR_BLUE, LV_OPA_20)); // Lightened blue for top of gradient
lv_style_set_bg_grad_color(&camera_btn_style, lv_color_darken(COLOR_BLUE, LV_OPA_20)); // Darkened blue for bottom of gradient
lv_style_set_bg_grad_dir(&camera_btn_style, LV_GRAD_DIR_VER);             // Vertical gradient
lv_style_set_border_width(&camera_btn_style, 2);                          // Border width: 2 pixels
lv_style_set_border_color(&camera_btn_style, COLOR_GREY);                 // Fixed grey (no palette)
lv_style_set_shadow_width(&camera_btn_style, 5);                          // Shadow width: 5 pixels for 3D depth
lv_style_set_shadow_color(&camera_btn_style, lv_color_darken(COLOR_GREY, LV_OPA_50)); // Darker grey shadow
lv_style_set_shadow_ofs_x(&camera_btn_style, 2);                          // Shadow offset X
lv_style_set_shadow_ofs_y(&camera_btn_style, 2);                          // Shadow offset Y
lv_style_set_outline_color(&camera_btn_style, COLOR_BLUE);                // Fixed blue (no palette)
lv_style_set_text_color(&camera_btn_style, COLOR_WHITE);

static lv_style_t camera_btn_style_pressed;
lv_style_init(&camera_btn_style_pressed);

// Make pressed state look sunken (reverse gradient, reduced shadow)
lv_style_set_radius(&camera_btn_style_pressed, 3);
lv_style_set_bg_opa(&camera_btn_style_pressed, LV_OPA_COVER);                      // Opaque background for gradient
lv_style_set_bg_color(&camera_btn_style_pressed, lv_color_darken(COLOR_BLUE, LV_OPA_20)); // Darkened blue for top of gradient
lv_style_set_bg_grad_color(&camera_btn_style_pressed, lv_color_lighten(COLOR_BLUE, LV_OPA_20)); // Lightened blue for bottom of gradient
lv_style_set_bg_grad_dir(&camera_btn_style_pressed, LV_GRAD_DIR_VER);             // Vertical gradient
lv_style_set_border_width(&camera_btn_style_pressed, 2);                          // Border width: 2 pixels
lv_style_set_border_color(&camera_btn_style_pressed, COLOR_GREY);                 // Fixed grey (no palette)
lv_style_set_shadow_width(&camera_btn_style_pressed, 2);                          // Reduced shadow for pressed effect
lv_style_set_shadow_color(&camera_btn_style_pressed, lv_color_darken(COLOR_GREY, LV_OPA_50)); // Darker grey shadow
lv_style_set_shadow_ofs_x(&camera_btn_style_pressed, 1);                          // Reduced shadow offset X
lv_style_set_shadow_ofs_y(&camera_btn_style_pressed, 1);                          // Reduced shadow offset Y
lv_style_set_outline_width(&camera_btn_style_pressed, 1);                         // Outline width: 1 pixel
lv_style_set_outline_color(&camera_btn_style_pressed, COLOR_BLUE);                // Fixed blue (no palette)
lv_style_set_text_color(&camera_btn_style_pressed, COLOR_WHITE);

// Add a focused style identical to default (raised look)
static lv_style_t camera_btn_style_focused;
lv_style_init(&camera_btn_style_focused);
lv_style_set_radius(&camera_btn_style_focused, 3);
lv_style_set_bg_opa(&camera_btn_style_focused, LV_OPA_COVER);
lv_style_set_bg_color(&camera_btn_style_focused, lv_color_lighten(COLOR_BLUE, LV_OPA_20));
lv_style_set_bg_grad_color(&camera_btn_style_focused, lv_color_darken(COLOR_BLUE, LV_OPA_20));
lv_style_set_bg_grad_dir(&camera_btn_style_focused, LV_GRAD_DIR_VER);
lv_style_set_border_width(&camera_btn_style_focused, 2);                          // Border width: 2 pixels
lv_style_set_border_color(&camera_btn_style_focused, COLOR_GREY);
lv_style_set_shadow_width(&camera_btn_style_focused, 5);                          // Shadow width: 5 pixels for 3D depth
lv_style_set_shadow_color(&camera_btn_style_focused, lv_color_darken(COLOR_GREY, LV_OPA_50));
lv_style_set_shadow_ofs_x(&camera_btn_style_focused, 2);
lv_style_set_shadow_ofs_y(&camera_btn_style_focused, 2);
lv_style_set_outline_width(&camera_btn_style_focused, 1);                         // Outline width: 1 pixel
lv_style_set_outline_color(&camera_btn_style_focused, COLOR_BLUE);
lv_style_set_text_color(&camera_btn_style_focused, COLOR_WHITE);

    // North Driveway button (Row 0)
    fetch_image_button = lv_btn_create(cameras_tab);
    lv_obj_set_size(fetch_image_button, 225, 75);  // Updated size
    lv_obj_add_event_cb(fetch_image_button, fetch_north_driveway, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(fetch_image_button, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(fetch_image_button); // Remove theme styles (as in example)
    lv_obj_add_style(fetch_image_button, &camera_btn_style, LV_STATE_DEFAULT);  // Removed LV_PART_MAIN
    lv_obj_add_style(fetch_image_button, &camera_btn_style_pressed, LV_STATE_PRESSED);  // Removed LV_PART_MAIN
    lv_obj_add_style(fetch_image_button, &camera_btn_style_focused, LV_STATE_FOCUSED); // Added focused style
    lv_obj_t *image_btn_label = lv_label_create(fetch_image_button);
    lv_label_set_text(image_btn_label, "North Driveway");
    lv_obj_remove_style_all(image_btn_label);  // Remove all default styles
    lv_obj_align(image_btn_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(image_btn_label, 15, 0);  // Add 15px padding around the label
    lv_obj_set_style_bg_opa(image_btn_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(image_btn_label, 0, 0);
    lv_obj_clear_flag(image_btn_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(fetch_image_button, LV_ALIGN_TOP_LEFT, 435, 5);  // Shifted right by 10px

    // East Driveway button (Row 1)
    camera3_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera3_btn, 225, 75);  // Size unchanged
    lv_obj_add_event_cb(camera3_btn, east_driveway_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera3_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera3_btn); // Remove theme styles (as in example)
    lv_obj_add_style(camera3_btn, &camera_btn_style, LV_STATE_DEFAULT);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera3_btn, &camera_btn_style_pressed, LV_STATE_PRESSED);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera3_btn, &camera_btn_style_focused, LV_STATE_FOCUSED); // Added focused style
    lv_obj_t *cam3_label = lv_label_create(camera3_btn);
    lv_label_set_text(cam3_label, "East Driveway");
    lv_obj_remove_style_all(cam3_label);  // Remove all default styles
    lv_obj_align(cam3_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(cam3_label, 15, 0);  // Add 15px padding around the label
    lv_obj_set_style_bg_opa(cam3_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cam3_label, 0, 0);
    lv_obj_clear_flag(cam3_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera3_btn, LV_ALIGN_TOP_LEFT, 618, 5);  // Shifted right by 10px


    // South Driveway button (Row 2)
    camera_north_canal_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_north_canal_btn, 225, 75);  // Updated size
    lv_obj_add_event_cb(camera_north_canal_btn, camera_north_canal_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_north_canal_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_north_canal_btn); // Remove theme styles (as in example)
    lv_obj_add_style(camera_north_canal_btn, &camera_btn_style, LV_STATE_DEFAULT);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_north_canal_btn, &camera_btn_style_pressed, LV_STATE_PRESSED);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_north_canal_btn, &camera_btn_style_focused, LV_STATE_FOCUSED); // Added focused style
    lv_obj_t *north_canal_label = lv_label_create(camera_north_canal_btn);
    lv_label_set_text(north_canal_label, "South Driveway");
    lv_obj_remove_style_all(north_canal_label);  // Remove all default styles
    lv_obj_align(north_canal_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(north_canal_label, 15, 0);  // Add 15px padding around the label
    lv_obj_set_style_bg_opa(north_canal_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(north_canal_label, 0, 0);
    lv_obj_clear_flag(north_canal_label, LV_OBJ_FLAG_CLICKABLE);
    // Reduce vertical separation between camera button rows by half (205 -> 105)
    lv_obj_align(camera_north_canal_btn, LV_ALIGN_TOP_LEFT, 618, 85);  // Updated X to match East Driveway, Y halved


    // Front Door button (Row 3)
    camera_front_door_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_front_door_btn, 225, 75);  // Updated size
    lv_obj_add_event_cb(camera_front_door_btn, camera_front_door_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_front_door_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_front_door_btn); // Remove theme styles (as in example)
    lv_obj_add_style(camera_front_door_btn, &camera_btn_style, LV_STATE_DEFAULT);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_front_door_btn, &camera_btn_style_pressed, LV_STATE_PRESSED);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_front_door_btn, &camera_btn_style_focused, LV_STATE_FOCUSED); // Added focused style
    lv_obj_t *front_door_label = lv_label_create(camera_front_door_btn);
    lv_label_set_text(front_door_label, " Front Door ");
    lv_obj_remove_style_all(front_door_label);  // Remove all default styles
    lv_obj_align(front_door_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(front_door_label, 15, 0);  // Add 15px padding around the label
    lv_obj_set_style_bg_opa(front_door_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(front_door_label, 0, 0);
    lv_obj_clear_flag(front_door_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera_front_door_btn, LV_ALIGN_TOP_LEFT, 435, 85);  // Updated X and Y for even spacing


    // West Canal button (Row 4)
    camera_west_canal_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_west_canal_btn, 225, 75);  // Updated size
    lv_obj_add_event_cb(camera_west_canal_btn, camera_west_canal_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_west_canal_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_west_canal_btn); // Remove theme styles (as in example)
    lv_obj_add_style(camera_west_canal_btn, &camera_btn_style, LV_STATE_DEFAULT);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_west_canal_btn, &camera_btn_style_pressed, LV_STATE_PRESSED);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_west_canal_btn, &camera_btn_style_focused, LV_STATE_FOCUSED); // Added focused style
    lv_obj_t *west_canal_label = lv_label_create(camera_west_canal_btn);
    lv_label_set_text(west_canal_label, " West Canal ");
    lv_obj_remove_style_all(west_canal_label);  // Remove all default styles
    lv_obj_align(west_canal_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(west_canal_label, 15, 0);  // Add 15px padding around the label
    lv_obj_set_style_bg_opa(west_canal_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(west_canal_label, 0, 0);
    lv_obj_clear_flag(west_canal_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera_west_canal_btn, LV_ALIGN_TOP_LEFT, 618, 185);  // Updated X to match East Driveway, Y to match Front Door


    // Tiki button (Row 5)
    camera_tiki_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_tiki_btn, 225, 75);  // Updated size
    lv_obj_add_event_cb(camera_tiki_btn, camera_tiki_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_tiki_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_tiki_btn); // Remove theme styles (as in example)
    lv_obj_add_style(camera_tiki_btn, &camera_btn_style, LV_STATE_DEFAULT);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_tiki_btn, &camera_btn_style_pressed, LV_STATE_PRESSED);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_tiki_btn, &camera_btn_style_focused, LV_STATE_FOCUSED); // Added focused style
    lv_obj_t *tiki_label = lv_label_create(camera_tiki_btn);
    lv_label_set_text(tiki_label, "   Tiki    ");
    lv_obj_remove_style_all(tiki_label);  // Remove all default styles
    lv_obj_align(tiki_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(tiki_label, 15, 0);  // Add 15px padding around the label
    lv_obj_set_style_bg_opa(tiki_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tiki_label, 0, 0);
    lv_obj_clear_flag(tiki_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera_tiki_btn, LV_ALIGN_TOP_LEFT, 435, 185); // Adjusted X and Y for even spacing

    // North Canal button (Row 6)
    camera_south_yard_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera_south_yard_btn, 225, 75);  // Updated size
    lv_obj_add_event_cb(camera_south_yard_btn, camera_south_yard_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera_south_yard_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera_south_yard_btn); // Remove theme styles (as in example)
    lv_obj_add_style(camera_south_yard_btn, &camera_btn_style, LV_STATE_DEFAULT);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_south_yard_btn, &camera_btn_style_pressed, LV_STATE_PRESSED);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera_south_yard_btn, &camera_btn_style_focused, LV_STATE_FOCUSED); // Added focused style
    lv_obj_t *south_yard_label = lv_label_create(camera_south_yard_btn);
    lv_label_set_text(south_yard_label, " North Canal ");
    lv_obj_align(south_yard_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(south_yard_label, 15, 0);  // Add 15px padding around the label
    lv_obj_set_style_bg_opa(south_yard_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(south_yard_label, 0, 0);
    lv_obj_clear_flag(south_yard_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_invalidate(camera_south_yard_btn);

    lv_obj_align(camera_south_yard_btn, LV_ALIGN_TOP_LEFT, 425, 285); // Adjusted Y


    // South Yard button (Row 7)
    camera8_btn = lv_btn_create(cameras_tab);
    lv_obj_set_size(camera8_btn, 225, 75);  // Updated size
    lv_obj_add_event_cb(camera8_btn, camera8_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(camera8_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_remove_style_all(camera8_btn);
    lv_obj_add_style(camera8_btn, &camera_btn_style, LV_STATE_DEFAULT);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera8_btn, &camera_btn_style_pressed, LV_STATE_PRESSED);  // Removed LV_PART_MAIN
    lv_obj_add_style(camera8_btn, &camera_btn_style_focused, LV_STATE_FOCUSED); // Added focused style
    lv_obj_t *cam8_label = lv_label_create(camera8_btn);
    lv_label_set_text(cam8_label, " South Yard ");
    lv_obj_remove_style_all(cam8_label);  // Remove all default styles
    lv_obj_align(cam8_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(cam8_label, 15, 0);  // Add 15px padding around the label
    lv_obj_set_style_bg_opa(cam8_label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cam8_label, 0, 0);
    lv_obj_clear_flag(cam8_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(camera8_btn, LV_ALIGN_TOP_LEFT, 618, 285); // Adjusted Y


    // --- Central Controller Tab ---
    lv_color_t label_color = lv_color_hex(0x263238); // Blue Gray 900

    // Water Valve Switch and Label
    water_switch = lv_switch_create(controller_tab);
    lv_obj_set_size(water_switch, 80, 40);
    lv_obj_align(water_switch, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_event_cb(water_switch, water_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(water_switch, COLOR_BLUE, LV_PART_MAIN);      // Inactive color
    lv_obj_set_style_bg_color(water_switch, COLOR_BLUE, LV_PART_INDICATOR); // Active color
    lv_obj_set_style_bg_color(water_switch, COLOR_BLUE, LV_PART_KNOB);      // Knob color

    water_switch_label = lv_label_create(controller_tab);
    lv_label_set_text(water_switch_label, "Water Valve");
    lv_obj_set_style_text_color(water_switch_label, label_color, 0);
    lv_obj_align_to(water_switch_label, water_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Central Vacuum Switch and Label
    central_vacuum_switch = lv_switch_create(controller_tab);
    lv_obj_set_size(central_vacuum_switch, 80, 40);
    lv_obj_align(central_vacuum_switch, LV_ALIGN_TOP_LEFT, 10, 70);
    lv_obj_add_event_cb(central_vacuum_switch, central_vacuum_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(central_vacuum_switch, COLOR_BLUE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(central_vacuum_switch, COLOR_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(central_vacuum_switch, COLOR_BLUE, LV_PART_KNOB);

    central_vacuum_switch_label = lv_label_create(controller_tab);
    lv_label_set_text(central_vacuum_switch_label, "Central Vacuum");
    lv_obj_set_style_text_color(central_vacuum_switch_label, label_color, 0);
    lv_obj_align_to(central_vacuum_switch_label, central_vacuum_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Vacuum Pump Switch and Label
    vacuum_pump_switch = lv_switch_create(controller_tab);
    lv_obj_set_size(vacuum_pump_switch, 80, 40);
    lv_obj_align(vacuum_pump_switch, LV_ALIGN_TOP_LEFT, 10, 130);
    lv_obj_add_event_cb(vacuum_pump_switch, vacuum_pump_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(vacuum_pump_switch, COLOR_BLUE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(vacuum_pump_switch, COLOR_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(vacuum_pump_switch, COLOR_BLUE, LV_PART_KNOB);

    vacuum_pump_switch_label = lv_label_create(controller_tab);
    lv_label_set_text(vacuum_pump_switch_label, "Vacuum Pump");
    lv_obj_set_style_text_color(vacuum_pump_switch_label, label_color, 0);
    lv_obj_align_to(vacuum_pump_switch_label, vacuum_pump_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // --- Versaries Tab (replaces Events tab) ---
    // Show marriage date and elapsed time since marriage
    // TODO: Implement versaries display
    // versaries_date_label = lv_label_create(versaries_tab);
    // lv_label_set_text(versaries_date_label, "Married: --");
    // lv_obj_set_style_text_color(versaries_date_label, COLOR_DARK_GREY, 0);
    // lv_obj_align(versaries_date_label, LV_ALIGN_TOP_MID, 0, 40);

    // versaries_count_label = lv_label_create(versaries_tab);
    // lv_label_set_text(versaries_count_label, "-- years, -- months, -- days");
    // lv_obj_set_style_text_color(versaries_count_label, COLOR_DARK_GREY, 0);
    // lv_obj_align(versaries_count_label, LV_ALIGN_TOP_MID, 0, 80);

    // Mia's Birthday row (center date, left = days since, right = days until)
    mia_since_label = lv_label_create(versaries_tab);
    lv_label_set_text(mia_since_label, "0 days since");
    lv_obj_set_style_text_color(mia_since_label, COLOR_DARK_GREY, 0);
    lv_obj_align(mia_since_label, LV_ALIGN_TOP_MID, -160, 130);
    (void)mia_since_label;  // Suppress unused warning

    mia_date_label = lv_label_create(versaries_tab);
    lv_label_set_text(mia_date_label, "Mia: 05-258"); // remove year
    lv_obj_set_style_text_color(mia_date_label, COLOR_DARK_GREY, 0);
    lv_obj_align(mia_date_label, LV_ALIGN_TOP_MID, 0, 130);
    (void)mia_date_label;  // Suppress unused warning

    mia_until_label = lv_label_create(versaries_tab);
    lv_label_set_text(mia_until_label, "0 days until");
    lv_obj_set_style_text_color(mia_until_label, COLOR_DARK_GREY, 0);
    lv_obj_align(mia_until_label, LV_ALIGN_TOP_MID, 160, 130);
    (void)mia_until_label;  // Suppress unused warning

    // My Birthday row
    my_since_label = lv_label_create(versaries_tab);
    lv_label_set_text(my_since_label, "0 days since");
    lv_obj_set_style_text_color(my_since_label, COLOR_DARK_GREY, 0);
    lv_obj_align(my_since_label, LV_ALIGN_TOP_MID, -160, 170);
    (void)my_since_label;  // Suppress unused warning

    my_date_label = lv_label_create(versaries_tab);
    lv_label_set_text(my_date_label, "Daniel: 08-22"); // show name + month/day only
    lv_obj_set_style_text_color(my_date_label, COLOR_DARK_GREY, 0);
    lv_obj_align(my_date_label, LV_ALIGN_TOP_MID, 0, 170);
    (void)my_date_label;  // Suppress unused warning

    my_until_label = lv_label_create(versaries_tab);
    lv_label_set_text(my_until_label, "0 days until");
    lv_obj_set_style_text_color(my_until_label, COLOR_DARK_GREY, 0);
    lv_obj_align(my_until_label, LV_ALIGN_TOP_MID, 160, 170);
    (void)my_until_label;  // Suppress unused warning

    // TODO: Create a timer to refresh the versaries display every minute
    // versaries_setup_timer_and_update();
    // update_versaries_display();

    // call helper immediately to populate labels (helper defined below)
    // ...existing code...
 
    lv_obj_update_layout(cameras_tab);
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
void lcd_init(void)
{
    // LCD and LVGL init handled in main.c
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

// Task function wrapper for weather fetch
static void weather_fetch_task_func(void *pvParameters)
{
    weather_fetch_and_display();
    vTaskDelete(NULL);  // Delete the task when done
}

void lcd_start_weather_fetch(void)
{
    // Create a task to fetch weather data from HA
    xTaskCreate(weather_fetch_task_func, "weather_fetch", 8192, NULL, 5, NULL);
}

// --- Fetch Weather button event handler ---
static void fetch_weather_btn_event_cb(lv_event_t *e)
{
    ESP_LOGI("UI", "Manual weather refresh requested.");
    // Create a task to fetch weather data to avoid blocking the UI
    weather_fetch_and_display();
}

static void tab_change_event_cb(lv_event_t *e)
{
    lv_obj_t *tv = lv_event_get_target(e);
    uint32_t active_tab_idx = lv_tabview_get_tab_act(tv);

    // Tab indices: 0=Weather, 1=Cameras, 2=Controller, 3=Versaries
    if (active_tab_idx == 0) {
        ESP_LOGI("UI", "Weather tab selected.");
        // On weather tab selection, immediately fetch the weather.
        weather_fetch_and_display();
    }
}

// --- Versaries timer and display logic ---
// TODO: Implement versaries display update
// static void update_versaries_display(void)
// {
//     // Calculate and display anniversaries/important dates
// }