#ifndef WEATHER_H
#define WEATHER_H

#include <stdbool.h>
#include "lvgl.h"  // Add this for lv_obj_t

// Add these defines for your location (replace with actual lat/lon)
#define WEATHER_LAT "26.1224"  // Example: Boynton Beach, FL latitude
#define WEATHER_LON "-80.1373" // Example: Boynton Beach, FL longitude

// Extern declarations for weather UI elements (add new ones)
extern lv_obj_t *weather_icon_label;
extern lv_obj_t *weather_temp_label;
extern lv_obj_t *weather_humidity_label;
extern lv_obj_t *weather_wind_label;  // New: Wind speed and direction
extern lv_obj_t *weather_conditions_label;  // New: Detailed conditions (e.g., "windy")
extern lv_obj_t *weather_forecast_list;

// Declare the task function for fetching weather
void weather_fetch_task_func(void *pvParameters);

// Declare the weather fetch function
void weather_fetch_and_display(void);

// Initialize weather auto-refresh timer (call once during startup)
void weather_init_timer(void);

// Make the helper visible to lcd.c (for setting JPG images)
bool set_jpg_or_error(lv_obj_t *img_obj, const uint8_t *start, const uint8_t *end, lv_obj_t *err_label);

#endif