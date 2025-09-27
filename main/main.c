
#include <string.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_private/wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/i2c.h"
#include "mqtt_relay_client.h"
#include "esp_system.h"
#define TAG "CENTRALCONTROLLER"

#define WIFI_SSID     "Sanctuary"
#define WIFI_PASSWORD "tikifire"

static void wifi_init_sta(void) {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI("wifi_init_sta", "wifi_init_sta finished. SSID:%s", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static lv_obj_t *title_bar = NULL;
static lv_obj_t *wifi_icon = NULL;
static lv_obj_t *wifi_ssid_label = NULL;
static lv_obj_t *wifi_ip_label = NULL;
static lv_obj_t *ha_status_icon = NULL;
static lv_obj_t *ha_ip_label = NULL;

// Forward declarations for UI update
static void update_wifi_status_ui(void);
static void update_ha_status_ui(bool connected, const char *ip);
// WiFi event handler to update UI on connect/disconnect/IP
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        update_wifi_status_ui();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        update_wifi_status_ui();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        update_wifi_status_ui();
    }
}

// Update WiFi status icon, SSID, and IP in the UI
static void update_wifi_status_ui(void) {
    if (!wifi_icon || !wifi_ssid_label || !wifi_ip_label) return;

    // Get SSID
    const char *ssid = "<unknown>";
    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
        ssid = (const char *)wifi_cfg.sta.ssid;
    }

    // Get IP
    char ip_str[16] = "0.0.0.0";
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", IP2STR(&ip_info.ip));
    }

    // Set SSID and IP labels
    lv_label_set_text_fmt(wifi_ssid_label, "%s", ssid);
    lv_label_set_text_fmt(wifi_ip_label, "%s", ip_str);

    // Optionally, update icon color or text for connection state
    if (strcmp(ip_str, "0.0.0.0") == 0) {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon, lv_palette_main(LV_PALETTE_GREY), 0);
    } else {
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_icon, lv_palette_main(LV_PALETTE_BLUE), 0);
        mqtt_init();

        update_ha_status_ui(true, "192.168.1.206");

    }
}

// Update Home Assistant status icon and IP
static void update_ha_status_ui(bool connected, const char *ip) {
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




#define EXAMPLE_LCD_H_RES   (800)
#define EXAMPLE_LCD_V_RES   (480)

/* LCD settings */
#define EXAMPLE_LCD_LVGL_FULL_REFRESH           (0)
#define EXAMPLE_LCD_LVGL_DIRECT_MODE            (1)
#define EXAMPLE_LCD_LVGL_AVOID_TEAR             (1)
#define EXAMPLE_LCD_RGB_BOUNCE_BUFFER_MODE      (1)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE            (0)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT            (100)
#define EXAMPLE_LCD_RGB_BUFFER_NUMS             (2)
#define EXAMPLE_LCD_RGB_BOUNCE_BUFFER_HEIGHT    (10)

/* LCD pins - Waveshare ESP32-S3-Touch-LCD-7.0 */
#define EXAMPLE_LCD_GPIO_VSYNC     (GPIO_NUM_3)
#define EXAMPLE_LCD_GPIO_HSYNC     (GPIO_NUM_46)
#define EXAMPLE_LCD_GPIO_DE        (GPIO_NUM_5)
#define EXAMPLE_LCD_GPIO_PCLK      (GPIO_NUM_7)
#define EXAMPLE_LCD_GPIO_DISP      (GPIO_NUM_NC)
#define EXAMPLE_LCD_GPIO_DATA0     (GPIO_NUM_14)
#define EXAMPLE_LCD_GPIO_DATA1     (GPIO_NUM_38)
#define EXAMPLE_LCD_GPIO_DATA2     (GPIO_NUM_18)
#define EXAMPLE_LCD_GPIO_DATA3     (GPIO_NUM_17)
#define EXAMPLE_LCD_GPIO_DATA4     (GPIO_NUM_10)
#define EXAMPLE_LCD_GPIO_DATA5     (GPIO_NUM_39)
#define EXAMPLE_LCD_GPIO_DATA6     (GPIO_NUM_0)
#define EXAMPLE_LCD_GPIO_DATA7     (GPIO_NUM_45)
#define EXAMPLE_LCD_GPIO_DATA8     (GPIO_NUM_48)
#define EXAMPLE_LCD_GPIO_DATA9     (GPIO_NUM_47)
#define EXAMPLE_LCD_GPIO_DATA10    (GPIO_NUM_21)
#define EXAMPLE_LCD_GPIO_DATA11    (GPIO_NUM_1)
#define EXAMPLE_LCD_GPIO_DATA12    (GPIO_NUM_2)
#define EXAMPLE_LCD_GPIO_DATA13    (GPIO_NUM_42)
#define EXAMPLE_LCD_GPIO_DATA14    (GPIO_NUM_41)
#define EXAMPLE_LCD_GPIO_DATA15    (GPIO_NUM_40)

/* Touch settings */
#define EXAMPLE_TOUCH_I2C_NUM       (0)
#define EXAMPLE_TOUCH_I2C_CLK_HZ    (400000)

/* Touch pins - Waveshare ESP32-S3-Touch-LCD-7.0 */
#define EXAMPLE_TOUCH_I2C_SCL       (GPIO_NUM_9)
#define EXAMPLE_TOUCH_I2C_SDA       (GPIO_NUM_8)

#define EXAMPLE_LCD_PANEL_35HZ_RGB_TIMING()  \
    {                                               \
        .pclk_hz = 16 * 1000 * 1000,                \
        .h_res = EXAMPLE_LCD_H_RES,                 \
        .v_res = EXAMPLE_LCD_V_RES,                 \
        .hsync_pulse_width = 4,                     \
        .hsync_back_porch = 8,                      \
        .hsync_front_porch = 8,                     \
        .vsync_pulse_width = 4,                     \
        .vsync_back_porch = 8,                      \
        .vsync_front_porch = 8,                     \
        .flags.pclk_active_neg = false,             \
    }


/* LCD IO and panel */
static esp_lcd_panel_handle_t lcd_panel = NULL;

// Forward declarations for minimal UI
static esp_lcd_touch_handle_t touch_handle = NULL;

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

static esp_err_t app_lcd_init(void)
{
    esp_err_t ret = ESP_OK;

    /* LCD initialization */
    esp_lcd_rgb_panel_config_t panel_conf = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .psram_trans_align = 64,
        .data_width = 16,
        .bits_per_pixel = 16,
        .de_gpio_num = EXAMPLE_LCD_GPIO_DE,
        .pclk_gpio_num = EXAMPLE_LCD_GPIO_PCLK,
        .vsync_gpio_num = EXAMPLE_LCD_GPIO_VSYNC,
        .hsync_gpio_num = EXAMPLE_LCD_GPIO_HSYNC,
        .disp_gpio_num = EXAMPLE_LCD_GPIO_DISP,
        .data_gpio_nums = {
            EXAMPLE_LCD_GPIO_DATA0,
            EXAMPLE_LCD_GPIO_DATA1,
            EXAMPLE_LCD_GPIO_DATA2,
            EXAMPLE_LCD_GPIO_DATA3,
            EXAMPLE_LCD_GPIO_DATA4,
            EXAMPLE_LCD_GPIO_DATA5,
            EXAMPLE_LCD_GPIO_DATA6,
            EXAMPLE_LCD_GPIO_DATA7,
            EXAMPLE_LCD_GPIO_DATA8,
            EXAMPLE_LCD_GPIO_DATA9,
            EXAMPLE_LCD_GPIO_DATA10,
            EXAMPLE_LCD_GPIO_DATA11,
            EXAMPLE_LCD_GPIO_DATA12,
            EXAMPLE_LCD_GPIO_DATA13,
            EXAMPLE_LCD_GPIO_DATA14,
            EXAMPLE_LCD_GPIO_DATA15,
        },
        .timings = EXAMPLE_LCD_PANEL_35HZ_RGB_TIMING(),
        .flags.fb_in_psram = 1,
        .num_fbs = EXAMPLE_LCD_RGB_BUFFER_NUMS,
#if EXAMPLE_LCD_RGB_BOUNCE_BUFFER_MODE
        .bounce_buffer_size_px = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_RGB_BOUNCE_BUFFER_HEIGHT,
#endif
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_rgb_panel(&panel_conf, &lcd_panel), err, TAG, "RGB init failed");
    ESP_GOTO_ON_ERROR(esp_lcd_panel_init(lcd_panel), err, TAG, "LCD init failed");

    return ret;

err:
    if (lcd_panel) {
        esp_lcd_panel_del(lcd_panel);
    }
    return ret;
}

static esp_err_t app_touch_init(void)
{
    /* Initialize I2C */
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_TOUCH_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = EXAMPLE_TOUCH_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = EXAMPLE_TOUCH_I2C_CLK_HZ
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(EXAMPLE_TOUCH_I2C_NUM, &i2c_conf), TAG, "I2C configuration failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(EXAMPLE_TOUCH_I2C_NUM, i2c_conf.mode, 0, 0, 0), TAG, "I2C initialization failed");

    /* Initialize touch HW */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)EXAMPLE_TOUCH_I2C_NUM, &tp_io_config, &tp_io_handle), TAG, "Touch panel IO creation failed");
    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &touch_handle);
}

static esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,         /* LVGL task priority */
        .task_stack = 6144,         /* LVGL task stack size */
        .task_affinity = -1,        /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500,   /* Maximum sleep in LVGL task */
        .timer_period_ms = 5        /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    uint32_t buff_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_DRAW_BUFF_HEIGHT;
#if EXAMPLE_LCD_LVGL_FULL_REFRESH || EXAMPLE_LCD_LVGL_DIRECT_MODE
    buff_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES;
#endif

    /* Add LCD screen */
    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = lcd_panel,
        .buffer_size = buff_size,
        .double_buffer = EXAMPLE_LCD_DRAW_BUFF_DOUBLE,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = false,
            .buff_spiram = false,
#if EXAMPLE_LCD_LVGL_FULL_REFRESH
            .full_refresh = true,
#elif EXAMPLE_LCD_LVGL_DIRECT_MODE
            .direct_mode = true,
#endif
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = false,
#endif
        }
    };
    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
#if EXAMPLE_LCD_RGB_BOUNCE_BUFFER_MODE
            .bb_mode = true,
#else
            .bb_mode = false,
#endif
#if EXAMPLE_LCD_LVGL_AVOID_TEAR
            .avoid_tearing = true,
#else
            .avoid_tearing = false,
#endif
        }
    };
    lvgl_disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);

    /* Add touch input (for selected screen) - only if touch was initialized successfully */
    if (touch_handle != NULL) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lvgl_disp,
            .handle = touch_handle,
        };
        lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    } else {
        lvgl_touch_indev = NULL;
    }

    return ESP_OK;
}


// Forward declaration for toggle button and label
static lv_obj_t *toggle_btn = NULL;
static lv_obj_t *toggle_btn_label = NULL;

#define WATER_VALVE_INDEX 2 // relay_ids[2] is "water_valve"

// Called when MQTT state for water_valve changes
static void water_valve_state_cb(int relay_index, bool state) {
    if (relay_index != WATER_VALVE_INDEX) return;
    if (!toggle_btn || !toggle_btn_label) return;
    if (state) {
        lv_obj_add_state(toggle_btn, LV_STATE_CHECKED);
        lv_label_set_text(toggle_btn_label, "Turn Water Off");
    } else {
        lv_obj_clear_state(toggle_btn, LV_STATE_CHECKED);
        lv_label_set_text(toggle_btn_label, "Turn Water On");
    }
}

static void toggle_event_cb(lv_event_t *e) {
    bool is_checked = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    if (toggle_btn_label) {
        lv_label_set_text(toggle_btn_label, is_checked ? "Turn Water Off" : "Turn Water On");
    }
    // Publish to MQTT
    mqtt_publish_relay_state(WATER_VALVE_INDEX + 1, is_checked); // relay_index is 1-based
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());


    // Initialize and connect WiFi
    wifi_init_sta();

    // Initialize MQTT client before any publish

    ESP_ERROR_CHECK(app_lcd_init());

    /* Touch initialization (optional - continue if it fails) */
    esp_err_t touch_ret = app_touch_init();
    if (touch_ret != ESP_OK) {
        touch_handle = NULL;
    }

    ESP_ERROR_CHECK(app_lvgl_init());
    lvgl_port_lock(0);
    // Create title bar
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);
    title_bar = lv_obj_create(scr);
    lv_obj_set_size(title_bar, EXAMPLE_LCD_H_RES, 50);
    lv_obj_align(title_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(title_bar, lv_palette_lighten(LV_PALETTE_BLUE_GREY, 2), 0);
    lv_obj_set_style_radius(title_bar, 0, 0);
    lv_obj_set_style_border_width(title_bar, 3, 0);
    lv_obj_set_style_border_color(title_bar, lv_color_black(), 0);

    // WiFi status in title bar (right side, horizontal alignment)
    wifi_icon = lv_label_create(title_bar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(wifi_icon, LV_ALIGN_RIGHT_MID, -250, 0);

    wifi_ssid_label = lv_label_create(title_bar);
    lv_label_set_text(wifi_ssid_label, "Connecting...");
    lv_obj_align_to(wifi_ssid_label, wifi_icon, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    wifi_ip_label = lv_label_create(title_bar);
    lv_label_set_text(wifi_ip_label, "0.0.0.0");
    lv_obj_align_to(wifi_ip_label, wifi_ssid_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Home Assistant status indicator (in title bar)
    ha_status_icon = lv_label_create(title_bar);
    lv_label_set_text(ha_status_icon, LV_SYMBOL_CLOSE " HA Connecting...");
    lv_obj_set_style_text_color(ha_status_icon, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(ha_status_icon, LV_ALIGN_LEFT_MID, 10, 0);

    ha_ip_label = lv_label_create(title_bar);
    lv_label_set_text(ha_ip_label, "-");
    lv_obj_align_to(ha_ip_label, ha_status_icon, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    // Remove highlighting/clickable state from title bar and all labels (must be after creation)
    if (title_bar) lv_obj_clear_flag(title_bar, LV_OBJ_FLAG_CLICKABLE);
    if (wifi_icon) lv_obj_clear_flag(wifi_icon, LV_OBJ_FLAG_CLICKABLE);
    if (wifi_ssid_label) lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_CLICKABLE);
    if (wifi_ip_label) lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_CLICKABLE);
    if (ha_status_icon) lv_obj_clear_flag(ha_status_icon, LV_OBJ_FLAG_CLICKABLE);
    if (ha_ip_label) lv_obj_clear_flag(ha_ip_label, LV_OBJ_FLAG_CLICKABLE);

    // Toggle button for water pump (move down)
    toggle_btn = lv_btn_create(scr);
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
    lv_obj_add_event_cb(toggle_btn, toggle_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Register callback for water_valve state
    set_relay_state_change_callback(water_valve_state_cb);

    // Initial Home Assistant status (disconnected, no IP)
    update_ha_status_ui(false, "-");

    // Initial update (after all objects are created)
    update_wifi_status_ui();
    lvgl_port_unlock();
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
}

// Example: Call this from MQTT event handler to update HA status
// update_ha_status_ui(true, "192.168.1.100");
// update_ha_status_ui(false, "192.168.1.100");
