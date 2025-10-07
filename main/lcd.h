#ifndef LCD_H
#define LCD_H
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
#include <stddef.h> // For size_t

#define WATER_VALVE_INDEX 1
#define CENTRAL_VACUUM_INDEX 2
#define VACUUM_PUMP_INDEX 3

typedef void (*relay_state_change_callback_t)(int relay_index, bool state);
void set_relay_state_change_callback(relay_state_change_callback_t cb);
void water_valve_state_cb(int relay_index, bool state);
void central_vacuum_state_cb(int relay_index, bool state);
void vacuum_pump_state_cb(int relay_index, bool state);
void lcd_init(void);
void lcd_update_wifi_status(const char *ssid, const char *ip);
void lcd_update_ha_status(bool connected, const char *ip);
void lcd_create_ui(void);
void update_wifi_status_ui(void);
void update_ha_status_ui(bool connected, const char *ip);
void update_wifi_status_ui(void);
void update_ha_status_ui(bool connected, const char *ip);
void lcd_append_motion_event(const char *event_text);
void lcd_update_camera_snapshot(const uint8_t *jpeg_data, size_t jpeg_size);
extern lv_obj_t *camera_img_widget;

#endif // LCD_H
