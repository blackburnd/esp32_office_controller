// In lcd.c

#include "weather.h" // Include the weather header for fetching data

static lv_obj_t *weather_screen; // New screen for weather display
static lv_obj_t *current_temp_label;
static lv_obj_t *current_humidity_label;
static lv_obj_t *current_condition_label;
static lv_obj_t *weekly_forecast_label;

void lcd_create_weather_screen(void) {
    weather_screen = lv_obj_create(lv_scr_act()); // Create a new screen
    lv_obj_set_size(weather_screen, LV_HOR_RES, LV_VER_RES);
    
    // Current weather labels
    current_temp_label = lv_label_create(weather_screen);
    lv_label_set_text(current_temp_label, "Temperature: -- °F");
    lv_obj_align(current_temp_label, LV_ALIGN_TOP_MID, 0, 10);

    current_humidity_label = lv_label_create(weather_screen);
    lv_label_set_text(current_humidity_label, "Humidity: -- %");
    lv_obj_align(current_humidity_label, LV_ALIGN_TOP_MID, 0, 40);

    current_condition_label = lv_label_create(weather_screen);
    lv_label_set_text(current_condition_label, "Condition: --");
    lv_obj_align(current_condition_label, LV_ALIGN_TOP_MID, 0, 70);

    // Weekly forecast label
    weekly_forecast_label = lv_label_create(weather_screen);
    lv_label_set_text(weekly_forecast_label, "Weekly Forecast:\n");
    lv_obj_align(weekly_forecast_label, LV_ALIGN_TOP_MID, 0, 100);
}