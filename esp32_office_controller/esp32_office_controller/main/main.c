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

static const char *TAG = "weather";

void weather_fetch_and_display(void) {
    // Allocate buffer for the response
    char *buffer = (char *)malloc(4096);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return;
    }

    // Show loading
    lvgl_port_lock(0);
    lv_label_set_text(weather_icon_label, LV_SYMBOL_REFRESH);
    lv_label_set_text(weather_temp_label, "Loading...");
    lv_label_set_text(weather_humidity_label, "Loading...");
    lvgl_port_unlock();

    // Fetch daily and weekly weather data
    char url[256];
    snprintf(url, sizeof(url), "https://api.openweathermap.org/data/2.5/onecall?lat=%s&lon=%s&exclude=minutely,hourly&appid=%s&units=imperial", WEATHER_LAT, WEATHER_LON, OPENWEATHERKEY);
    ESP_LOGI(TAG, "URL: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .use_global_ca_store = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .buffer_size = 4096,
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

    int total_len = esp_http_client_read(client, buffer, 4095);
    buffer[total_len] = '\0';  // Null-terminate the response

    // Parse JSON response
    cJSON *json = cJSON_Parse(buffer);
    if (json) {
        // Extract daily and weekly forecast data
        cJSON *daily = cJSON_GetObjectItem(json, "daily");
        if (daily) {
            // Process daily forecast data
            for (int i = 0; i < cJSON_GetArraySize(daily); i++) {
                cJSON *day = cJSON_GetArrayItem(daily, i);
                // Extract relevant data (e.g., temperature, weather condition)
                // Update your UI elements accordingly
            }
        }
        cJSON_Delete(json);
    } else {
        ESP_LOGE(TAG, "Failed to parse JSON");
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
}