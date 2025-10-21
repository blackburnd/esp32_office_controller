#include "weather.h"
#include "mqtt.h"  // For HA_BASE_URL and HA_ACCESS_TOKEN
#include <esp_log.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_crt_bundle.h"

extern lv_obj_t *weather_icon_label;
extern lv_obj_t *weather_temp_label;
extern lv_obj_t *weather_humidity_label;
extern lv_obj_t *weather_forecast_label; // New label for forecast

static const char *TAG = "weather";

void weather_fetch_and_display(void) {
    // Existing code...

    // Fetch daily and weekly forecast
    char url[256];
    snprintf(url, sizeof(url), "https://api.openweathermap.org/data/2.5/onecall?lat=%s&lon=%s&appid=%s&units=imperial", WEATHER_LAT, WEATHER_LON, OPENWEATHERKEY);
    
    // Existing HTTP client setup...

    if (status_code == 200) {
        // Parse JSON response
        cJSON *json = cJSON_Parse(buffer);
        if (json) {
            // Extract daily and weekly forecast data
            cJSON *daily = cJSON_GetObjectItem(json, "daily");
            if (daily) {
                // Loop through daily forecasts and build a string
                char forecast_text[512] = {0};
                for (int i = 0; i < cJSON_GetArraySize(daily); i++) {
                    cJSON *day = cJSON_GetArrayItem(daily, i);
                    // Extract relevant data (e.g., temperature, weather condition)
                    double temp_day = cJSON_GetObjectItem(day, "temp")->valuedouble;
                    const char *weather_desc = cJSON_GetObjectItem(day, "weather")->valuestring;
                    snprintf(forecast_text + strlen(forecast_text), sizeof(forecast_text) - strlen(forecast_text), "Day %d: %.1f°F, %s\n", i + 1, temp_day, weather_desc);
                }
                lv_label_set_text(weather_forecast_label, forecast_text); // Update the forecast label
            }
            cJSON_Delete(json);
        }
    }

    // Existing cleanup code...
}