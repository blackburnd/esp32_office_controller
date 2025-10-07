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
#include "lcd.h"
#include "mqtt.h"
#include "wifi.h"
#include "camera_client.h" // Add this include
#include "src/extra/libs/png/lv_png.h"  // Corrected path for lv_png_init

#define TAG "CENTRALCONTROLLER"




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

    // Initialize PNG decoder
    lv_png_init();

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

static void relay_state_change_handler(int relay_index, bool state) {
    switch (relay_index) {
        case WATER_VALVE_INDEX:
            water_valve_state_cb(relay_index, state);
            break;
        case CENTRAL_VACUUM_INDEX:
            central_vacuum_state_cb(relay_index, state);
            break;
        case VACUUM_PUMP_INDEX:
            vacuum_pump_state_cb(relay_index, state);
            break;
    }
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
    // MQTT will be started from WiFi event handler after IP is acquired.
    esp_err_t touch_ret = app_touch_init();
    if (touch_ret != ESP_OK) {
        touch_handle = NULL;
    }

    ESP_ERROR_CHECK(app_lvgl_init());
    lvgl_port_lock(0);
    lcd_create_ui();
    lvgl_port_unlock();
    
    
    // Register relay callback after UI is created
    set_relay_state_change_callback(relay_state_change_handler);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    // Now that UI is created, update WiFi status
    lcd_update_wifi_status(NULL, NULL);

    camera_client_start();  // Still call to initialize, but no periodic fetch
}

// Example: Call this from MQTT event handler to update HA status
// update_ha_status_ui(true, "192.168.1.100");
// update_ha_status_ui(false, "192.168.1.100");
