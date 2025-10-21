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
    char *buffer = (char *)malloc(2048);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return;
    }

    // Show loading
    lvgl_port_lock(0);
    lv_label_set_text(weather_icon_label, LV_SYMBOL_REFRESH);
    lv_label_set_text(weather_temp_label, "Loading...");
    lv_label_set_text(weather_humidity_label, "Loading...");
    lv_label_set_text(weather_forecast_label, "Loading..."); // Update forecast label
    lvgl_port_unlock();

    char url[256];
    snprintf(url, sizeof(url), "https://api.openweathermap.org/data/2.5/onecall?lat=%s&lon=%s&appid=%s&units=imperial", WEATHER_LAT, WEATHER_LON, OPENWEATHERKEY);
    ESP_LOGI(TAG, "URL: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .use_global_ca_store = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .buffer_size = 2048,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        free(buffer);
        return;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP client: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(buffer);
        return;
    }

    esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    if (status_code == 200) {
        int total_len = esp_http_client_read(client, buffer, 2047);
        buffer[total_len] = '\0'; // Null-terminate the buffer

        // Parse JSON response
        cJSON *json = cJSON_Parse(buffer);
        if (json) {
            // Extract current weather
            cJSON *current = cJSON_GetObjectItem(json, "current");
            if (current) {
                float temp = cJSON_GetNumberValue(cJSON_GetObjectItem(current, "temp"));
                float humidity = cJSON_GetNumberValue(cJSON_GetObjectItem(current, "humidity"));
                lv_label_set_text(weather_temp_label, lv_snprintf("Temp: %.1f°F", temp));
                lv_label_set_text(weather_humidity_label, lv_snprintf("Humidity: %.1f%%", humidity));
            }

            // Extract daily forecast
            cJSON *daily = cJSON_GetObjectItem(json, "daily");
            if (daily) {
                char forecast_text[256] = "Forecast:\n";
                for (int i = 0; i < 7; i++) { // Get 7-day forecast
                    cJSON *day = cJSON_GetArrayItem(daily, i);
                    float day_temp = cJSON_GetNumberValue(cJSON_GetObjectItem(day, "temp"));
                    const char *weather_desc = cJSON_GetStringValue(cJSON_GetObjectItem(day, "weather"));
                    snprintf(forecast_text + strlen(forecast_text), sizeof(forecast_text) - strlen(forecast_text), "Day %d: %.1f°F, %s\n", i + 1, day_temp, weather_desc);
                }
                lv_label_set_text(weather_forecast_label, forecast_text); // Update forecast label
            }

            cJSON_Delete(json);
        }
    } else {
        ESP_LOGE(TAG, "Request failed: status %d", status_code);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
}