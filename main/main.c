#include <string.h>
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
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
#include "esp_system.h"
#include "lcd.h"
#include "mqtt.h"
#include "wifi.h"
#include "camera_client.h"
#include "esp_sntp.h"
#include "esp_crt_bundle.h"
#include "extra/libs/png/lv_png.h"
#include <time.h>

#define TAG "CENTRALCONTROLLER"

// Callback function for time synchronization
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized: %s", ctime(&tv->tv_sec));
    // Set timezone to Eastern Standard Time (EST) with daylight saving (EDT)
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1);
    tzset();
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
}

#define EXAMPLE_LCD_H_RES (800)
#define EXAMPLE_LCD_V_RES (480)

/* LCD settings */
#define EXAMPLE_LCD_LVGL_FULL_REFRESH (0)
#define EXAMPLE_LCD_LVGL_DIRECT_MODE (1)
#define EXAMPLE_LCD_LVGL_AVOID_TEAR (1)
#define EXAMPLE_LCD_RGB_BOUNCE_BUFFER_MODE (1)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (0)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (100)
#define EXAMPLE_LCD_RGB_BUFFER_NUMS (2)
#define EXAMPLE_LCD_RGB_BOUNCE_BUFFER_HEIGHT (10)

/* LCD pins - Waveshare ESP32-S3-Touch-LCD-7.0 */
#define EXAMPLE_LCD_GPIO_VSYNC (GPIO_NUM_3)
#define EXAMPLE_LCD_GPIO_HSYNC (GPIO_NUM_46)
#define EXAMPLE_LCD_GPIO_DE (GPIO_NUM_5)
#define EXAMPLE_LCD_GPIO_PCLK (GPIO_NUM_7)
#define EXAMPLE_LCD_GPIO_DISP (GPIO_NUM_NC)
#define EXAMPLE_LCD_GPIO_DATA0 (GPIO_NUM_14)
#define EXAMPLE_LCD_GPIO_DATA1 (GPIO_NUM_38)
#define EXAMPLE_LCD_GPIO_DATA2 (GPIO_NUM_18)
#define EXAMPLE_LCD_GPIO_DATA3 (GPIO_NUM_17)
#define EXAMPLE_LCD_GPIO_DATA4 (GPIO_NUM_10)
#define EXAMPLE_LCD_GPIO_DATA5 (GPIO_NUM_39)
#define EXAMPLE_LCD_GPIO_DATA6 (GPIO_NUM_0)
#define EXAMPLE_LCD_GPIO_DATA7 (GPIO_NUM_45)
#define EXAMPLE_LCD_GPIO_DATA8 (GPIO_NUM_48)
#define EXAMPLE_LCD_GPIO_DATA9 (GPIO_NUM_47)
#define EXAMPLE_LCD_GPIO_DATA10 (GPIO_NUM_21)
#define EXAMPLE_LCD_GPIO_DATA11 (GPIO_NUM_1)
#define EXAMPLE_LCD_GPIO_DATA12 (GPIO_NUM_2)
#define EXAMPLE_LCD_GPIO_DATA13 (GPIO_NUM_42)
#define EXAMPLE_LCD_GPIO_DATA14 (GPIO_NUM_41)
#define EXAMPLE_LCD_GPIO_DATA15 (GPIO_NUM_40)
#define COLOR_BLUE lv_color_hex(0x1976D2)
#define COLOR_GREY lv_color_hex(0x808080)
/* Touch settings */
#define EXAMPLE_TOUCH_I2C_NUM (0)
#define EXAMPLE_TOUCH_I2C_CLK_HZ (400000)

/* Touch pins - Waveshare ESP32-S3-Touch-LCD-7.0 */
#define EXAMPLE_TOUCH_I2C_SCL (GPIO_NUM_9)
#define EXAMPLE_TOUCH_I2C_SDA (GPIO_NUM_8)

#define EXAMPLE_LCD_PANEL_35HZ_RGB_TIMING() \
    {                                       \
        .pclk_hz = 16 * 1000 * 1000,        \
        .h_res = EXAMPLE_LCD_H_RES,         \
        .v_res = EXAMPLE_LCD_V_RES,         \
        .hsync_pulse_width = 4,             \
        .hsync_back_porch = 8,              \
        .hsync_front_porch = 8,             \
        .vsync_pulse_width = 4,             \
        .vsync_back_porch = 8,              \
        .vsync_front_porch = 8,             \
        .flags.pclk_active_neg = false,     \
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
    if (lcd_panel)
    {
        esp_lcd_panel_del(lcd_panel);
    }
    return ret;
}

static esp_err_t app_touch_init(void)
{
    esp_err_t ret;

    /* I2C initialization */
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_TOUCH_I2C_SDA,
        .scl_io_num = EXAMPLE_TOUCH_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = EXAMPLE_TOUCH_I2C_CLK_HZ,
    };
    ESP_GOTO_ON_ERROR(i2c_param_config(EXAMPLE_TOUCH_I2C_NUM, &i2c_conf), err, TAG, "I2C config failed");
    ESP_GOTO_ON_ERROR(i2c_driver_install(EXAMPLE_TOUCH_I2C_NUM, i2c_conf.mode, 0, 0, 0), err, TAG, "I2C driver install failed");

    /* Touch initialization */
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
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)EXAMPLE_TOUCH_I2C_NUM, &tp_io_config, &tp_io_handle), err, TAG, "I2C init failed");
    ESP_GOTO_ON_ERROR(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &touch_handle), err, TAG, "Touch init failed");

    return ESP_OK;

err:
    if (touch_handle)
    {
        esp_lcd_touch_del(touch_handle);
    }
    if (tp_io_handle)
    {
        esp_lcd_panel_io_del(tp_io_handle);
    }
    i2c_driver_delete(EXAMPLE_TOUCH_I2C_NUM);
    return ret;
}

static esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 8192,
        .task_affinity = -1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5
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
        }};
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
        }};
    lvgl_disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);

    /* Add touch input (for selected screen) - only if touch was initialized successfully */
    if (touch_handle != NULL)
    {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lvgl_disp,
            .handle = touch_handle,
        };
        lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    }
    else
    {
        lvgl_touch_indev = NULL;
    }

    // Set a custom theme with fixed colors to prevent palette changes and theme effects
    lv_theme_t *theme = lv_theme_default_init(lvgl_disp, COLOR_BLUE, COLOR_GREY, false, &lv_font_montserrat_14);
    lv_disp_set_theme(lvgl_disp, theme);

    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Initialize and connect WiFi
    wifi_init_sta();

    // Initialize SNTP for time synchronization (required for HTTPS certificate validation)
    // This registers the callback and initializes SNTP in polling mode
    initialize_sntp();

    // Initialize LCD/LVGL hardware
    ESP_ERROR_CHECK(app_lcd_init());

    /* Touch initialization (optional - continue if it fails) */
    // MQTT will be started from WiFi event handler after IP is acquired.
    esp_err_t touch_ret = app_touch_init();
    if (touch_ret != ESP_OK)
    {
        touch_handle = NULL;
    }

    ESP_ERROR_CHECK(app_lvgl_init());
    lvgl_port_lock(0);
    lcd_create_ui();
    lvgl_port_unlock();

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    // Now that UI is created, update WiFi status
    lcd_update_wifi_status(NULL, NULL);

    camera_client_start();

    // Wait for time to be set (up to 10 seconds)
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    while (timeinfo.tm_year < (2023 - 1900) && ++retry < 100) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, 100);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if (timeinfo.tm_year >= (2023 - 1900)) {
        ESP_LOGI(TAG, "System time set successfully");
    } else {
        ESP_LOGE(TAG, "Failed to set system time");
    }

    now = time(NULL);
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d", 
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}
