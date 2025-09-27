#include "lcd.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_rgb.h"
#include <string.h>

static lv_obj_t *title_bar = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *wifi_ssid_label = NULL;
static lv_obj_t *wifi_ip_label = NULL;
static lv_obj_t *ha_status_icon = NULL;
static lv_obj_t *ha_ip_label = NULL;
static lv_obj_t *toggle_btn_label = NULL;

void lcd_create_ui(void) {
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);
    title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, 800, 50);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_palette_lighten(LV_PALETTE_BLUE_GREY, 2), 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_border_width(title_bar, 3, 0);
    lv_obj_set_style_border_color(title_bar, lv_color_black(), 0);

    wifi_icon = lv_label_create(title_bar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(wifi_icon, LV_ALIGN_RIGHT_MID, -250, 0);

    wifi_ssid_label = lv_label_create(title_bar);
    lv_label_set_text(wifi_ssid_label, "Connecting...");
    lv_obj_align_to(wifi_ssid_label, wifi_icon, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    wifi_ip_label = lv_label_create(title_bar);
    lv_label_set_text(wifi_ip_label, "0.0.0.0");
    lv_obj_align_to(wifi_ip_label, wifi_ssid_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    ha_status_icon = lv_label_create(title_bar);
    lv_label_set_text(ha_status_icon, LV_SYMBOL_CLOSE " HA Connecting...");
    lv_obj_set_style_text_color(ha_status_icon, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(ha_status_icon, LV_ALIGN_LEFT_MID, 10, 0);

    ha_ip_label = lv_label_create(title_bar);
    lv_label_set_text(ha_ip_label, "-");
    lv_obj_align_to(ha_ip_label, ha_status_icon, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    if (title_bar) lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_CLICKABLE);
    if (wifi_icon) lv_obj_clear_flag(wifi_icon, LV_OBJ_FLAG_CLICKABLE);
    if (wifi_ssid_label) lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_CLICKABLE);
    if (wifi_ip_label) lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_CLICKABLE);
    if (ha_status_icon) lv_obj_clear_flag(ha_status_icon, LV_OBJ_FLAG_CLICKABLE);
    if (ha_ip_label) lv_obj_clear_flag(ha_ip_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *toggle_btn = lv_btn_create(scr);
    lv_obj_add_flag(toggle_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(toggle_btn, 160, 60);
    lv_obj_align(toggle_btn, LV_ALIGN_TOP_LEFT, 20, 120);
    lv_obj_set_style_border_width(toggle_btn, 0, 0);
    lv_obj_set_style_outline_width(toggle_btn, 0, 0);
    lv_obj_set_style_shadow_width(toggle_btn, 0, 0);
    lv_obj_set_style_radius(toggle_btn, 0, 0);
    lv_obj_set_style_bg_color(toggle_btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_bg_opa(toggle_btn, LV_OPA_COVER, 0);
    toggle_btn_label = lv_label_create(toggle_btn);
    lv_label_set_text(toggle_btn_label, "Turn Water On");
    lv_obj_center(toggle_btn_label);
    // TODO: Add event callback
}

void lcd_update_wifi_status(const char *ssid, const char *ip) {
    if (!wifi_icon || !wifi_ssid_label || !wifi_ip_label) return;
    lv_label_set_text_fmt(wifi_ssid_label, "%s", ssid);
    lv_label_set_text_fmt(wifi_ip_label, "%s", ip);
    if (strcmp(ip, "0.0.0.0") == 0) {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon, lv_palette_main(LV_PALETTE_GREY), 0);
    } else {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon, lv_palette_main(LV_PALETTE_BLUE), 0);
    }
}

void lcd_update_ha_status(bool connected, const char *ip) {
    if (!ha_status_icon || !ha_ip_label) return;
    if (connected) {
        lv_label_set_text(ha_status_icon, LV_SYMBOL_OK " HA Connected");
        lv_obj_set_style_text_color(ha_status_icon, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_label_set_text(ha_status_icon, LV_SYMBOL_CLOSE " HA Unavailable");
        lv_obj_set_style_text_color(ha_status_icon, lv_palette_main(LV_PALETTE_RED), 0);
    }
    lv_label_set_text_fmt(ha_ip_label, "%s", ip ? ip : "-");
}

void lcd_init(void) {
    // Only initialize once
    static bool initialized = false;
    if (initialized) return;
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
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags = { .buff_dma = true, .buff_spiram = true, .sw_rotate = false, .full_refresh = false, .direct_mode = true },
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = { .bb_mode = true, .avoid_tearing = true },
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
    if (tp_handle && disp) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = disp,
            .handle = tp_handle,
        };
        lvgl_port_add_touch(&touch_cfg);
    }
}
}
