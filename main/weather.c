#include "weather.h"
#include "mqtt.h"  // For HA_BASE_URL and HA_ACCESS_TOKEN
#include <esp_log.h>
#include <esp_http_client.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "assets.h"
#include "esp_lvgl_port.h"   // declares lvgl_port_lock / lvgl_port_unlock
#include "esp_crt_bundle.h"  // declares esp_crt_bundle_attach
#include <string.h>          // memcmp, strlen, etc.
#include <time.h>
#include "extra/libs/png/lv_png.h"
static const char *TAG = "weather";


// Increase buffer size for full JSON response (32 KB should suffice for forecast API)
#define WEATHER_BUFFER_SIZE 32768
static char buffer[WEATHER_BUFFER_SIZE];

// Function to map weather condition to LVGL symbol
static const char* get_weather_icon(const char *condition) {
    if (!condition) return LV_SYMBOL_CLOSE;  // Default (question not available)
    if (strstr(condition, "sunny") || strstr(condition, "clear")) return LV_SYMBOL_OK;  // Use OK for sun
    if (strstr(condition, "cloudy") || strstr(condition, "partly")) return LV_SYMBOL_SETTINGS;  // Use settings for cloud
    if (strstr(condition, "rain") || strstr(condition, "drizzle")) return LV_SYMBOL_DOWN;
    if (strstr(condition, "snow")) return LV_SYMBOL_CLOSE;  // Approx for snow
    if (strstr(condition, "storm") || strstr(condition, "thunder")) return LV_SYMBOL_WARNING;
    if (strstr(condition, "fog") || strstr(condition, "mist")) return LV_SYMBOL_EYE_CLOSE;
    return LV_SYMBOL_CLOSE;  // Unknown
}

// Function to map condition to PNG
static const uint8_t* get_weather_icon_png(const char *condition) {
    if (!condition) return clear_day_png_start;  // Default
    if (strstr(condition, "clear") || strstr(condition, "sunny")) return clear_day_png_start;
    if (strstr(condition, "partly cloudy") || strstr(condition, "few clouds")) return cloudy_1_day_png_start;
    if (strstr(condition, "cloudy") || strstr(condition, "overcast")) return cloudy_png_start;
    if (strstr(condition, "rain") || strstr(condition, "drizzle")) return rainy_1_png_start;
    if (strstr(condition, "heavy rain")) return rainy_3_png_start;
    if (strstr(condition, "snow")) return snowy_1_png_start;
    if (strstr(condition, "heavy snow")) return snowy_3_png_start;
    if (strstr(condition, "thunder") || strstr(condition, "storm")) return thunderstorms_png_start;
    if (strstr(condition, "fog") || strstr(condition, "mist")) return fog_png_start;
    if (strstr(condition, "haze")) return haze_png_start;
    if (strstr(condition, "wind")) return wind_png_start;
    // Add more mappings based on your PNGs (e.g., hail, tornado)
    return clear_day_png_start;  // Fallback
}

// Add parallel mapping for the "end" symbols
static const uint8_t* get_weather_icon_png_end(const char *condition) {
    if (!condition) return clear_day_png_end;
    if (strstr(condition, "clear") || strstr(condition, "sunny")) return clear_day_png_end;
    if (strstr(condition, "partly cloudy") || strstr(condition, "few clouds")) return cloudy_1_day_png_end;
    if (strstr(condition, "cloudy") || strstr(condition, "overcast")) return cloudy_png_end;
    if (strstr(condition, "rain") || strstr(condition, "drizzle")) return rainy_1_png_end;
    if (strstr(condition, "heavy rain")) return rainy_3_png_end;
    if (strstr(condition, "snow")) return snowy_1_png_end;
    if (strstr(condition, "heavy snow")) return snowy_3_png_end;
    if (strstr(condition, "thunder") || strstr(condition, "storm")) return thunderstorms_png_end;
    if (strstr(condition, "fog") || strstr(condition, "mist")) return fog_png_end;
    if (strstr(condition, "haze")) return haze_png_end;
    if (strstr(condition, "wind")) return wind_png_end;
    return clear_day_png_end;
}

// Function to get wind direction from degrees
static const char* get_wind_direction(int degrees) {
    if (degrees >= 337.5 || degrees < 22.5) return "N";
    if (degrees >= 22.5 && degrees < 67.5) return "NE";
    if (degrees >= 67.5 && degrees < 112.5) return "E";
    if (degrees >= 112.5 && degrees < 157.5) return "SE";
    if (degrees >= 157.5 && degrees < 202.5) return "S";
    if (degrees >= 202.5 && degrees < 247.5) return "SW";
    if (degrees >= 247.5 && degrees < 292.5) return "W";
    if (degrees >= 292.5 && degrees < 337.5) return "NW";
    return "Unknown";
}

// Fetch and display weather forecast (enhanced)
void weather_fetch_and_display(void) {
    // Add checks to prevent crashes if UI is not ready
    if (!weather_icon_label || !weather_temp_label || !weather_humidity_label || !weather_wind_label || !weather_conditions_label || !weather_forecast_list) {
        ESP_LOGE(TAG, "Weather UI not initialized yet");
        return;
    }

    // Show loading
    lvgl_port_lock(0);
    lv_label_set_text(weather_temp_label, "Loading...");
    lv_label_set_text(weather_humidity_label, "Loading...");
    lv_label_set_text(weather_wind_label, "Loading...");
    lv_label_set_text(weather_conditions_label, "Loading...");
    lv_obj_clean(weather_forecast_list);
    lvgl_port_unlock();

    char url[256];
    snprintf(url, sizeof(url), "https://api.openweathermap.org/data/2.5/forecast?lat=%s&lon=%s&appid=%s&units=imperial", WEATHER_LAT, WEATHER_LON, OPENWEATHERKEY);
    ESP_LOGI(TAG, "URL: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .use_global_ca_store = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP client: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return;
    }

    esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    ESP_LOGI(TAG, "HTTP status: %d, err: %s, content_length: %d", status_code, esp_err_to_name(err), content_length);

    if (status_code == 200) {
        int total_read = 0;
        while (1) {
            int len = esp_http_client_read(client, buffer + total_read, sizeof(buffer) - total_read - 1);  // Leave space for null
            if (len <= 0) break;
            total_read += len;
            if (total_read >= sizeof(buffer) - 1) break;  // Prevent overflow
        }
        buffer[total_read] = '\0';  // Null-terminate

        ESP_LOGI(TAG, "Total read len: %d", total_read);
        if (total_read > 0) {
            cJSON *root = cJSON_Parse(buffer);
            if (root) {
                cJSON *list = cJSON_GetObjectItem(root, "list");
                if (list && cJSON_IsArray(list)) {
                    // Group by day and find daily highs/lows
                    struct {
                        time_t date;
                        double min_temp, max_temp;
                        const char *condition;
                    } daily[7] = {0};  // Up to 7 days
                    int day_count = 0;

                    int array_size = cJSON_GetArraySize(list);
                    for (int i = 0; i < array_size; i++) {
                        cJSON *item = cJSON_GetArrayItem(list, i);
                        cJSON *dt = cJSON_GetObjectItem(item, "dt");
                        cJSON *main = cJSON_GetObjectItem(item, "main");
                        cJSON *weather = cJSON_GetObjectItem(item, "weather");
                        if (dt && main && weather && cJSON_IsArray(weather) && cJSON_GetArraySize(weather) > 0) {
                            time_t timestamp = dt->valueint;
                            struct tm *timeinfo = localtime(&timestamp);
                            int day_of_year = timeinfo->tm_yday;

                            // Find or create daily entry
                            int day_idx = -1;
                            for (int j = 0; j < day_count; j++) {
                                struct tm *d = localtime(&daily[j].date);
                                if (d->tm_yday == day_of_year) {
                                    day_idx = j;
                                    break;
                                }
                            }
                            if (day_idx == -1 && day_count < 7) {
                                daily[day_count].date = timestamp;
                                daily[day_count].min_temp = 999;
                                daily[day_count].max_temp = -999;
                                day_idx = day_count++;
                            }

                            if (day_idx != -1) {
                                cJSON *temp = cJSON_GetObjectItem(main, "temp");
                                cJSON *current_weather = cJSON_GetArrayItem(weather, 0);
                                cJSON *desc = cJSON_GetObjectItem(current_weather, "description");
                                if (temp) {
                                    if (temp->valuedouble < daily[day_idx].min_temp) daily[day_idx].min_temp = temp->valuedouble;
                                    if (temp->valuedouble > daily[day_idx].max_temp) daily[day_idx].max_temp = temp->valuedouble;
                                }
                                if (desc && !daily[day_idx].condition) daily[day_idx].condition = desc->valuestring;
                            }
                        }
                    }

                    // Display today's summary (enhanced)
                    if (day_count > 0) {
                        // Existing: icon, temp
                        const uint8_t *icon_png = get_weather_icon_png(daily[0].condition);
                        const uint8_t *icon_png_end = get_weather_icon_png_end(daily[0].condition);
                        char temp_str[32];
                        char hum_str[32] = "N/A";  // Still placeholder
                        char wind_str[64];
                        char conditions_str[128];

                        // Parse wind and conditions from the first item (today's forecast)
                        cJSON *first_item = cJSON_GetArrayItem(list, 0);
                        cJSON *wind = cJSON_GetObjectItem(first_item, "wind");
                        cJSON *weather_array = cJSON_GetObjectItem(first_item, "weather");
                        if (wind) {
                            cJSON *speed = cJSON_GetObjectItem(wind, "speed");
                            cJSON *deg = cJSON_GetObjectItem(wind, "deg");
                            if (speed && deg) {
                                snprintf(wind_str, sizeof(wind_str), "%.1f mph %s", speed->valuedouble * 2.237, get_wind_direction((int)deg->valuedouble));  // Convert m/s to mph
                            } else {
                                strcpy(wind_str, "N/A");
                            }
                        } else {
                            strcpy(wind_str, "N/A");
                        }

                        if (weather_array && cJSON_IsArray(weather_array) && cJSON_GetArraySize(weather_array) > 0) {
                            cJSON *weather_item = cJSON_GetArrayItem(weather_array, 0);
                            cJSON *desc = cJSON_GetObjectItem(weather_item, "description");
                            if (desc) {
                                snprintf(conditions_str, sizeof(conditions_str), "%s", desc->valuestring);
                            } else {
                                strcpy(conditions_str, "N/A");
                            }
                        } else {
                            strcpy(conditions_str, "N/A");
                        }

                        snprintf(temp_str, sizeof(temp_str), "%.1f°F / %.1f°F", daily[0].max_temp, daily[0].min_temp);

                        lvgl_port_lock(0);
                        set_png_or_error(weather_icon_label, icon_png, icon_png_end, weather_conditions_label);
                        lv_label_set_text(weather_temp_label, temp_str);
                        lv_label_set_text(weather_humidity_label, hum_str);
                        lv_label_set_text(weather_wind_label, wind_str);
                        lv_label_set_text(weather_conditions_label, conditions_str);
                        lvgl_port_unlock();

                        ESP_LOGI(TAG, "Successfully updated weather.");
                    }

                    // Display weekly forecast (enhanced with conditions)
                    lvgl_port_lock(0);
                    for (int i = 1; i < day_count && i < 6; i++) {
                        struct tm *timeinfo = localtime(&daily[i].date);
                        char day_name[4];
                        strftime(day_name, sizeof(day_name), "%a", timeinfo);  // e.g., "Mon"
                        // Parse conditions for each day
                        char forecast_text[128];
                        const char *day_condition = daily[i].condition ? daily[i].condition : "N/A";
                        snprintf(forecast_text, sizeof(forecast_text), "%s: %.1f°F / %.1f°F, %s", day_name, daily[i].max_temp, daily[i].min_temp, day_condition);
                        lv_obj_t *list_item = lv_list_add_btn(weather_forecast_list, get_weather_icon(daily[i].condition), forecast_text);
                        lv_obj_set_style_text_font(list_item, lv_font_default(), 0);
                    }
                    lvgl_port_unlock();
                } else {
                    ESP_LOGE(TAG, "Invalid forecast JSON structure");
                    lvgl_port_lock(0);
                    // lv_label_set_text(weather_icon_label, LV_SYMBOL_CLOSE);  // Comment out or remove
                    lv_label_set_text(weather_temp_label, "No data");
                    lv_label_set_text(weather_humidity_label, "No data");
                    lvgl_port_unlock();
                }
                cJSON_Delete(root);
            } else {
                ESP_LOGE(TAG, "JSON parse failed");
                lvgl_port_lock(0);
                // lv_label_set_text(weather_icon_label, LV_SYMBOL_CLOSE);  // Comment out or remove
                lv_label_set_text(weather_temp_label, "Parse error");
                lv_label_set_text(weather_humidity_label, "Parse error");
                lvgl_port_unlock();
            }
        } else {
            ESP_LOGE(TAG, "No data read from response");
            lvgl_port_lock(0);
            // lv_label_set_text(weather_icon_label, LV_SYMBOL_CLOSE);  // Comment out or remove
            lv_label_set_text(weather_temp_label, "No data");
            lv_label_set_text(weather_humidity_label, "No data");
            lvgl_port_unlock();
        }
    } else {
        ESP_LOGE(TAG, "Request failed: status %d, err %s", status_code, esp_err_to_name(err));
        lvgl_port_lock(0);
        // lv_label_set_text(weather_icon_label, LV_SYMBOL_CLOSE);  // Comment out or remove
        lv_label_set_text(weather_temp_label, "Request failed");
        lv_label_set_text(weather_humidity_label, "Request failed");
        lvgl_port_unlock();
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
}

bool set_png_or_error(lv_obj_t *img_obj, const uint8_t *start, const uint8_t *end, lv_obj_t *err_label)
{
    if (!img_obj) {
        ESP_LOGE(TAG, "img_obj is NULL");
        return false;
    }

    if (!start || !end || end <= start) {
        ESP_LOGE(TAG, "Asset missing or zero-length (%p .. %p)", (void*)start, (void*)end);
        if (err_label) lv_label_set_text(err_label, "PNG: missing");
        return false;
    }

    size_t size = (size_t)(end - start);
    ESP_LOGI(TAG, "Asset pointers: start=%p end=%p size=%zu", (void*)start, (void*)end, size);

    const unsigned char png_sig[8] = {0x89, 'P','N','G',0x0D,0x0A,0x1A,0x0A};
    if (size < sizeof(png_sig) || memcmp(start, png_sig, sizeof(png_sig)) != 0) {
        ESP_LOGE(TAG, "Asset not a valid PNG (bad signature / too small) size=%zu", size);
        if (err_label) lv_label_set_text(err_label, "PNG: invalid");
        return false;
    }

    // Ensure LVGL PNG decoder initialised (no-op if already done)
    //lv_png_init();

    // Set image source to embedded pointer (LVGL will decode)
    lv_img_set_src(img_obj, start);
    lv_obj_invalidate(img_obj);

    ESP_LOGI(TAG, "PNG set OK, size=%zu", size);
    return true;
}