#ifndef LCD_H
#define LCD_H
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
void lcd_init(void);
void lcd_update_wifi_status(const char *ssid, const char *ip);
void lcd_update_ha_status(bool connected, const char *ip);
void lcd_create_ui(void);
#endif // LCD_H
