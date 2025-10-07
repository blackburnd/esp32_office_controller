#include "lcd.h"
#include <stdbool.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_rgb.h"
#include <string.h>
#include "mqtt.h"

static lv_obj_t *motion_event_textarea;
lv_obj_t *camera_img_widget; // New camera image widget
static lv_img_dsc_t camera_img_dsc; // New image descriptor

#define LCD_MAX_LINES 100  // Increased from 50 to 100 to keep more history in the scrolling text area
#define LCD_MAX_TEXTAREA_BUF 2048

// UI object pointers (static globals)
static lv_obj_t *title_bar = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *wifi_ssid_label = NULL;
static lv_obj_t *wifi_ip_label = NULL;
static lv_obj_t *ha_status_icon = NULL;
static lv_obj_t *ha_ip_label = NULL;
static lv_obj_t *water_switch = NULL;
static lv_obj_t *water_switch_label = NULL;
static lv_obj_t *central_vacuum_switch = NULL;
static lv_obj_t *central_vacuum_switch_label = NULL;
static lv_obj_t *vacuum_pump_switch = NULL;
static lv_obj_t *vacuum_pump_switch_label = NULL;
static lv_obj_t *fetch_image_button = NULL; // Button to fetch camera image
static lv_obj_t *stream_btn;  // Add this global or in a struct

// Forward declarations for callback handlers
void water_valve_state_cb(int relay_index, bool state);
void central_vacuum_state_cb(int relay_index, bool state);
void vacuum_pump_state_cb(int relay_index, bool state);


static void water_switch_event_cb(lv_event_t *e);
static void central_vacuum_switch_event_cb(lv_event_t *e);
static void vacuum_pump_switch_event_cb(lv_event_t *e);
static void fetch_image_button_event_cb(lv_event_t *e); // Event handler for fetch image button

// Callback for stream button
static void stream_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        extern void camera_client_start_stream(void);
        extern void camera_client_stop_stream(void);
        static bool streaming = false;
        if (!streaming) {
            camera_client_start_stream();
            lv_label_set_text(lv_obj_get_child(stream_btn, 0), "Stop Stream");
        } else {
            camera_client_stop_stream();
            lv_label_set_text(lv_obj_get_child(stream_btn, 0), "Start Stream");
        }
        streaming = !streaming;
    }
}


void lcd_create_ui(void)
{
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);

    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Title bar: dark blue background, white text
    title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, 800, 50);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_color_hex(0x1A237E), 0); // Indigo 900
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Title bar icons and labels: white text
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

    // Switches: blue for active, gray for inactive, dark gray text
    lv_color_t accent = lv_color_hex(0x1976D2); // Blue 700
    lv_color_t inactive = lv_color_hex(0xB0BEC5); // Blue Gray 200
    lv_color_t label_color = lv_color_hex(0x263238); // Blue Gray 900

    // Motion event text area on the right half of the screen (shortened height to avoid overlap with image)
    motion_event_textarea = lv_textarea_create(scr);
    lv_obj_set_size(motion_event_textarea, 380, 200); // Reduced height from 400 to 200
    lv_obj_align(motion_event_textarea, LV_ALIGN_TOP_RIGHT, -10, 60);
    lv_textarea_set_text(motion_event_textarea, "Motion Events:\n");
    lv_obj_set_style_bg_color(motion_event_textarea, lv_color_hex(0xF5F5F5), 0); // Light gray background
    lv_obj_set_style_text_color(motion_event_textarea, lv_color_hex(0x263238), 0); // Dark text
    lv_obj_set_style_border_width(motion_event_textarea, 1, 0);

    // Create the camera image widget - always visible, positioned below the text area
    camera_img_widget = lv_img_create(scr);
    lv_obj_set_size(camera_img_widget, 320, 240); // Match camera resolution
    lv_obj_align_to(camera_img_widget, motion_event_textarea, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20); // Below text area, with 20px gap
    // Removed LV_OBJ_FLAG_HIDDEN to make it always visible

    // Create a container for the switches
    lv_obj_t *switch_container = lv_obj_create(scr);
    lv_obj_set_size(switch_container, 320, 200); // Increased width and adjusted height for horizontal layout
    lv_obj_align(switch_container, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_set_flex_flow(switch_container, LV_FLEX_FLOW_COLUMN); // Keep column for vertical stacking of switch rows

    // Water Valve Switch and Label
    water_switch = lv_switch_create(switch_container);
    lv_obj_set_size(water_switch, 80, 40);
    lv_obj_add_event_cb(water_switch, water_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(water_switch, inactive, LV_PART_MAIN);
    lv_obj_set_style_bg_color(water_switch, accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(water_switch, accent, LV_PART_KNOB);

    water_switch_label = lv_label_create(switch_container);
    lv_label_set_text(water_switch_label, "Water Valve");
    lv_obj_set_style_text_color(water_switch_label, label_color, 0);
    lv_obj_align_to(water_switch_label, water_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0); // Align label to the right of switch

    // Central Vacuum Switch and Label
    central_vacuum_switch = lv_switch_create(switch_container);
    lv_obj_set_size(central_vacuum_switch, 80, 40);
    lv_obj_add_event_cb(central_vacuum_switch, central_vacuum_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(central_vacuum_switch, inactive, LV_PART_MAIN);
    lv_obj_set_style_bg_color(central_vacuum_switch, accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(central_vacuum_switch, accent, LV_PART_KNOB);

    central_vacuum_switch_label = lv_label_create(switch_container);
    lv_label_set_text(central_vacuum_switch_label, "Central Vacuum");
    lv_obj_set_style_text_color(central_vacuum_switch_label, label_color, 0);
    lv_obj_align_to(central_vacuum_switch_label, central_vacuum_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Vacuum Pump Switch and Label
    vacuum_pump_switch = lv_switch_create(switch_container);
    lv_obj_set_size(vacuum_pump_switch, 80, 40);
    lv_obj_add_event_cb(vacuum_pump_switch, vacuum_pump_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(vacuum_pump_switch, inactive, LV_PART_MAIN);
    lv_obj_set_style_bg_color(vacuum_pump_switch, accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(vacuum_pump_switch, accent, LV_PART_KNOB);

    vacuum_pump_switch_label = lv_label_create(switch_container);
    lv_label_set_text(vacuum_pump_switch_label, "Vacuum Pump");
    lv_obj_set_style_text_color(vacuum_pump_switch_label, label_color, 0);
    lv_obj_align_to(vacuum_pump_switch_label, vacuum_pump_switch, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Add a button for manual image fetch (renamed and widened)
    fetch_image_button = lv_btn_create(scr);
    lv_obj_set_size(fetch_image_button, 200, 50); // Widened from 150 to 200
    lv_obj_align_to(fetch_image_button, switch_container, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);
    lv_obj_add_event_cb(fetch_image_button, fetch_image_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(fetch_image_button);
    lv_label_set_text(btn_label, "Front Door"); // Renamed from "Fetch Camera Image"

    // Stream button
    stream_btn = lv_btn_create(scr);
    lv_obj_set_size(stream_btn, 120, 40);
    lv_obj_align(stream_btn, LV_ALIGN_TOP_RIGHT, -10, 10);  // Adjust position
    lv_obj_add_event_cb(stream_btn, stream_btn_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *stream_label = lv_label_create(stream_btn);
    lv_label_set_text(stream_label, "Start Stream");
}

// --- Switch event handlers ---

static void water_switch_event_cb(lv_event_t *e) {
    bool new_state = lv_obj_has_state(water_switch, LV_STATE_CHECKED);
    mqtt_publish_water_valve_state(new_state);
}

static void central_vacuum_switch_event_cb(lv_event_t *e) {
    bool new_state = lv_obj_has_state(central_vacuum_switch, LV_STATE_CHECKED);
    mqtt_publish_central_vacuum_state(new_state);
}

static void vacuum_pump_switch_event_cb(lv_event_t *e) {
    bool new_state = lv_obj_has_state(vacuum_pump_switch, LV_STATE_CHECKED);
    mqtt_publish_vacuum_pump_state(new_state);
}

// New event handler for the button
static void fetch_image_button_event_cb(lv_event_t *e) {
    extern void camera_client_fetch_image(void);
    camera_client_fetch_image();
}

// --- Update relay state callbacks ---

void water_valve_state_cb(int relay_index, bool state)
{
    lvgl_port_lock(0);
    if (water_switch) {
        if (state) {
            lv_obj_add_state(water_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(water_switch, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();
}

void central_vacuum_state_cb(int relay_index, bool state)
{
    if (relay_index != CENTRAL_VACUUM_INDEX)
        return;
    lvgl_port_lock(0);
    if (central_vacuum_switch) {
        if (state) {
            lv_obj_add_state(central_vacuum_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(central_vacuum_switch, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();
}

void vacuum_pump_state_cb(int relay_index, bool state)
{
    if (relay_index != VACUUM_PUMP_INDEX)
        return;
    lvgl_port_lock(0);
    if (vacuum_pump_switch) {
        if (state) {
            lv_obj_add_state(vacuum_pump_switch, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(vacuum_pump_switch, LV_STATE_CHECKED);
        }
    }
    lvgl_port_unlock();
}

void lcd_update_wifi_status(const char *ssid, const char *ip)
{
    if (!wifi_icon || !wifi_ssid_label || !wifi_ip_label)
        return;
    if (wifi_ssid_label)
        lv_label_set_text_fmt(wifi_ssid_label, "SSID: %s", ssid ? ssid : "<unknown>");
    if (wifi_ip_label)
        lv_label_set_text_fmt(wifi_ip_label, "IP: %s", ip ? ip : "0.0.0.0");

    // Always white icon for consistency
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_white(), 0);
}

void lcd_update_ha_status(bool connected, const char *ip)
{
    if (!ha_status_icon || !ha_ip_label)
        return;
    if (connected)
    {
        lv_label_set_text(ha_status_icon, LV_SYMBOL_OK " HA Connected");
    }
    else
    {
        lv_label_set_text(ha_status_icon, LV_SYMBOL_CLOSE " HA Unavailable");
    }
    lv_obj_set_style_text_color(ha_status_icon, lv_color_white(), 0);
    lv_label_set_text_fmt(ha_ip_label, "%s", ip ? ip : "-");
}

void lcd_init(void)
{
    // Only initialize once
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    // --- LCD panel IO and panel handle setup (RGB panel) ---
    // These values should match your hardware. Adjust as needed.
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .timings = {
            .pclk_hz = 16000000,
            .h_res = 800,
            .v_res = 480,
            .hsync_pulse_width = 10,
            .hsync_back_porch = 20,
            .hsync_front_porch = 10,
            .vsync_pulse_width = 10,
            .vsync_back_porch = 10,
            .vsync_front_porch = 10,
            .flags.pclk_active_neg = false,
        },
        .data_width = 16,
        .psram_trans_align = 64,
        .num_fbs = 2,
        .bounce_buffer_size_px = 800 * 10,
        .flags.fb_in_psram = true,
        .flags.double_fb = true,
        // .flags.bounce_buffer = true, // Not present in this ESP-IDF version
        .flags.refresh_on_demand = false,
    };
    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // --- Register display with LVGL port ---
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL, // Not used for RGB
        .panel_handle = panel_handle,
        .buffer_size = 800 * 100, // Height of 100 lines per buffer
        .double_buffer = true,
        .hres = 800,
        .vres = 480,
        .monochrome = false,
        .rotation = {.swap_xy = false, .mirror_x = false, .mirror_y = false},
        .flags = {.buff_dma = true, .buff_spiram = true, .sw_rotate = false, .full_refresh = false, .direct_mode = true},
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {.bb_mode = true, .avoid_tearing = true},
    };
    lv_display_t *disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);

    // --- Touch panel (GT911) ---
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 800,
        .y_max = 480,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    // You must create the I2C panel IO for GT911 here (not shown for brevity)
    // ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(..., &io_handle));
    esp_lcd_touch_handle_t tp_handle = NULL;
    // ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io_handle, &tp_cfg, &tp_handle));
    // If you have a GT911, uncomment and configure the above two lines.
    // Register touch with LVGL if available
    if (tp_handle && disp)
    {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = disp,
            .handle = tp_handle,
        };
        lvgl_port_add_touch(&touch_cfg);
    }
}

static relay_state_change_callback_t relay_cb = NULL;

void set_relay_state_change_callback(relay_state_change_callback_t cb) {
    relay_cb = cb;
}

void lcd_append_motion_event(const char *event_text)
{
    lvgl_port_lock(0); // <-- Lock LVGL before UI update

    static char buffer[LCD_MAX_TEXTAREA_BUF];
    const char *current = lv_textarea_get_text(motion_event_textarea);

    // Ignore huge messages that can't fit in buffer
    if (strlen(event_text) > LCD_MAX_TEXTAREA_BUF / 2) {
        return;
    }

    // Copy current text to buffer
    strncpy(buffer, current, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // Append new event (truncate if needed)
    size_t buf_len = strlen(buffer);
    size_t event_len = strlen(event_text);
    size_t space_left = sizeof(buffer) - buf_len - 2; // for \n and \0
    if (event_len > space_left) event_len = space_left;
    strncat(buffer, event_text, event_len);
    buf_len = strlen(buffer);
    if (buf_len < sizeof(buffer) - 2) {
        buffer[buf_len] = '\n';
        buffer[buf_len + 1] = '\0';
    }

    // Count lines and keep only the last LCD_MAX_LINES
    int lines = 0;
    char *p = buffer;
    while (*p) {
        if (*p == '\n') lines++;
        p++;
    }
    if (lines > LCD_MAX_LINES) {
        // Find the Nth last newline
        int to_skip = lines - LCD_MAX_LINES;
        p = buffer;
        while (to_skip > 0 && *p) {
            if (*p == '\n') to_skip--;
            p++;
        }
        // p now points just after the Nth newline
        size_t len = strlen(p);
        memmove(buffer, p, len + 1);
    }

    // Set text area
    lv_textarea_set_text(motion_event_textarea, buffer);
    lv_textarea_set_cursor_pos(motion_event_textarea, LV_TEXTAREA_CURSOR_LAST);

    lvgl_port_unlock(); // <-- Unlock after UI update
}

void lcd_update_camera_snapshot(const uint8_t *jpeg_data, size_t jpeg_size) {
    if (!jpeg_data || jpeg_size == 0) {
        return;
    }

    lvgl_port_lock(0);

    // The image descriptor points directly to the data buffer
    camera_img_dsc.header.always_zero = 0;
    camera_img_dsc.header.w = 0; // Decoder will update this
    camera_img_dsc.header.h = 0; // Decoder will update this
    camera_img_dsc.data_size = jpeg_size;
    camera_img_dsc.data = jpeg_data;

    lv_img_set_src(camera_img_widget, &camera_img_dsc);

    lvgl_port_unlock();
}
