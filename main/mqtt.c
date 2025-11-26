#include "mqtt.h"
#include "lcd.h"
#include "esp_timer.h"  // Add this for timer
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi.h"
#include <math.h>
#include <strings.h>

// Forward declaration for the text area MQTT handler
static void mqtt_textarea_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// External functions from lcd.c
extern void lcd_update_weather_forecast(const char *forecast_text);
extern void water_valve_state_cb(int relay_index, bool state);
extern void central_vacuum_state_cb(int relay_index, bool state);
extern void vacuum_pump_state_cb(int relay_index, bool state);
extern void lcd_update_thermostat_readings(float ambient_c, float setpoint_c, const char *mode);

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

// Availability/diagnostics topics for easier debugging and HA device status
static const char *AVAIL_TOPIC = "esp32_office_controller/availability"; // retained: "online"/"offline"
static const char *DIAG_PREFIX = "esp32_office_controller/diag";         // retained last values

// Callback for relay state changes
static relay_state_change_callback_t relay_callback = NULL;

void mqtt_set_relay_callback(relay_state_change_callback_t cb)
{
    relay_callback = cb;
}

bool mqtt_is_connected(void)
{
    return mqtt_connected;
}

esp_err_t mqtt_publish_water_valve_state(bool state)
{
    if (!mqtt_connected)
        return ESP_FAIL;
    const char *topic = "water_valve/cmd";
    const char *payload = state ? "ON" : "OFF";
    ESP_LOGI(TAG, "Publishing Water Valve state: %s to topic: %s", payload, topic); // <-- LOG
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_central_vacuum_state(bool state)
{
    if (!mqtt_connected)
        return ESP_FAIL;
    const char *topic = "central_vac/cmd";
    const char *payload = state ? "ON" : "OFF";
    ESP_LOGI(TAG, "Publishing Central Vacuum state: %s to topic: %s", payload, topic); // <-- LOG
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_vacuum_pump_state(bool state)
{
    if (!mqtt_connected)
        return ESP_FAIL;
    const char *topic = "vacuum_pump/cmd";
    const char *payload = state ? "ON" : "OFF";
    ESP_LOGI(TAG, "Publishing Vacuum Pump state: %s to topic: %s", payload, topic); // <-- LOG
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

static void handle_command_message(const char *topic, const char *data, int data_len)
{
    char payload[16] = {0};
    memcpy(payload, data, data_len < 15 ? data_len : 15);
    payload[data_len < 15 ? data_len : 15] = '\0';
    ESP_LOGI(TAG, "Received MQTT command: topic='%s', payload='%s'", topic, payload);

    if (strstr(topic, "water_valve/cmd") && relay_callback)
    {
        bool new_state = (strncmp(data, "ON", data_len) == 0);
        relay_callback(WATER_VALVE_INDEX, new_state);
    }
    else if (strstr(topic, "central_vac/cmd") && relay_callback)
    {
        bool new_state = (strncmp(data, "ON", data_len) == 0);
        relay_callback(CENTRAL_VACUUM_INDEX, new_state);
    }
    else if (strstr(topic, "vacuum_pump/cmd") && relay_callback)
    {
        bool new_state = (strncmp(data, "ON", data_len) == 0);
        relay_callback(VACUUM_PUMP_INDEX, new_state);
    }
}

// --- Thermostat helpers ---
static void handle_thermostat_state(const char *topic, const char *data, int data_len)
{
    // Topics:
    //   nest/ambient/state            -> float Fahrenheit
    //   nest/humidity/state           -> float percent (0-100)
    //   nest/setpoint_cool/state      -> float Fahrenheit
    //   nest/mode/state               -> string ("HEAT"|"COOL"|"OFF"|"HEATCOOL"|"ECO")
    char payload[32] = {0};
    int n = data_len < (int)sizeof(payload) - 1 ? data_len : (int)sizeof(payload) - 1;
    memcpy(payload, data, n);
    payload[n] = '\0';

    ESP_LOGI(TAG, "Received thermostat state: topic='%s', payload='%s'", topic, payload);

    static float last_ambient = -1000.0f;
    static float last_setpoint = -1000.0f;
    static char last_mode[16] = "";
    static float last_humidity = -1.0f;

    bool changed = false;
    if (strstr(topic, "nest/ambient/state") != NULL)
    {
        float amb = strtof(payload, NULL);
        ESP_LOGI(TAG, "Ambient: parsed=%.1f°C, last=%.1f°C", amb, last_ambient);
        last_ambient = amb;
        changed = true;  // Always update LCD when ambient arrives from HA
    }
    else if (strstr(topic, "nest/humidity/state") != NULL)
    {
        // Strip quotes if present (HA sometimes publishes "58.0" instead of 58.0)
        char *start = payload;
        if (payload[0] == '"') start++;  // Skip leading quote
        float rh = strtof(start, NULL);
        ESP_LOGI(TAG, "Humidity: parsed=%.1f%%, last=%.1f%%", rh, last_humidity);
        if (rh != last_humidity)
        {
            last_humidity = rh;
            // Update humidity label immediately
            lcd_update_thermostat_humidity(last_humidity);
        }
    }
    else if (strstr(topic, "nest/setpoint_cool/state") != NULL)
    {
        float sp = strtof(payload, NULL);
        ESP_LOGI(TAG, "Setpoint: parsed=%.1f°C, last=%.1f°C", sp, last_setpoint);
        // Only update if setpoint actually changed (prevent feedback loop)
        // Use 0.3°C threshold (~0.5°F equivalent)
        if (fabs(sp - last_setpoint) > 0.3f)
        {
            last_setpoint = sp;
            changed = true;
        }
    }
    else if (strstr(topic, "nest/mode/state") != NULL)
    {
        ESP_LOGI(TAG, "Mode: parsed='%s', last='%s'", payload, last_mode);
        strncpy(last_mode, payload, sizeof(last_mode) - 1);
        last_mode[sizeof(last_mode) - 1] = '\0';
        changed = true;  // Always update LCD when mode arrives from HA
    }

    if (changed)
    {
        ESP_LOGI(TAG, "Updating LCD: ambient=%.1f°C, setpoint=%.1f°C, mode='%s'", 
                 last_ambient, last_setpoint, last_mode[0] ? last_mode : "NULL");
        lcd_update_thermostat_readings(last_ambient, last_setpoint, last_mode[0] ? last_mode : NULL);
    }
}

// --- HA REST seed (fallback) ---
// If MQTT state topics aren't (yet) published by HA automations, fetch once from HA REST
// and seed the TFT with ambient temperature, humidity, setpoint, and mode.
static char *http_get_simple(const char *url, const char *token)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 8000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return NULL;
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    char auth_hdr[256];
    snprintf(auth_hdr, sizeof(auth_hdr), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth_hdr);
    esp_http_client_set_header(client, "Accept", "application/json");

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HA seed HTTP GET failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }
    int status = esp_http_client_get_status_code(client);
    int len = esp_http_client_get_content_length(client);
    if (status != 200 || len <= 0) {
        ESP_LOGW(TAG, "HA seed HTTP GET unexpected status=%d len=%d", status, len);
        // Continue reading whatever data is available
    }
    // Read body
    char *buf = NULL;
    int total = 0;
    int chunk;
    const int CHUNK = 1024;
    do {
        if (!buf) {
            buf = (char *)malloc(CHUNK);
            if (!buf) break;
            buf[0] = '\0';
        }
        if (total + CHUNK + 1 > 128 * 1024) break; // hard cap
        char tmp[CHUNK];
        chunk = esp_http_client_read(client, tmp, CHUNK);
        if (chunk > 0) {
            char *nbuf = (char *)realloc(buf, total + chunk + 1);
            if (!nbuf) { free(buf); buf = NULL; break; }
            buf = nbuf;
            memcpy(buf + total, tmp, chunk);
            total += chunk;
            buf[total] = '\0';
        }
    } while (chunk > 0);
    esp_http_client_cleanup(client);
    return buf;
}

static void ha_seed_task(void *param)
{
    (void)param;
    // Give WiFi/MQTT a moment to fully settle
    vTaskDelay(pdMS_TO_TICKS(1500));

    // Ensure WiFi has an IP before attempting REST
    bool wifi_ok = false;
    for (int i = 0; i < 10; ++i) {
        wifi_is_connected(&wifi_ok);
        if (wifi_ok) break;
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    if (!HA_BASE_URL || strlen(HA_BASE_URL) == 0 || !HA_ACCESS_TOKEN || strlen(HA_ACCESS_TOKEN) == 0) {
        ESP_LOGW(TAG, "HA seed skipped: HA_BASE_URL or HA_ACCESS_TOKEN not configured");
        vTaskDelete(NULL);
        return;
    }

    // Build URL - only need climate entity (temp/humidity are attributes)
    char url_climate[256];
    snprintf(url_climate, sizeof(url_climate), "%s/api/states/climate.living_room", HA_BASE_URL);

    // Try a few times in case HA is slow to come up
    for (int attempt = 0; attempt < 3; ++attempt) {
        // Fetch climate entity for all data (ambient, humidity, setpoint, mode)
        float ambient_c = NAN;
        float humid = NAN;
        float setpoint_c = NAN;
        const char *mode = NULL;
        char *body_c = http_get_simple(url_climate, HA_ACCESS_TOKEN);
        if (body_c) {
            cJSON *root = cJSON_Parse(body_c);
            if (root) {
                cJSON *attrs = cJSON_GetObjectItem(root, "attributes");
                if (attrs) {
                    // Get current temperature from climate entity attributes
                    cJSON *cur = cJSON_GetObjectItem(attrs, "current_temperature");
                    if (cJSON_IsNumber(cur)) ambient_c = (float)cur->valuedouble;
                    
                    // Get current humidity from climate entity attributes
                    cJSON *hum = cJSON_GetObjectItem(attrs, "current_humidity");
                    if (cJSON_IsNumber(hum)) humid = (float)hum->valuedouble;
                    
                    // Get setpoint temperature
                    cJSON *temp = cJSON_GetObjectItem(attrs, "temperature");
                    if (cJSON_IsNumber(temp)) setpoint_c = (float)temp->valuedouble;
                    cJSON *t_high = cJSON_GetObjectItem(attrs, "target_temp_high");
                    if (cJSON_IsNumber(t_high)) setpoint_c = (float)t_high->valuedouble;
                    
                    // Get HVAC mode
                    cJSON *hvac = cJSON_GetObjectItem(attrs, "hvac_mode");
                    if (cJSON_IsString(hvac)) mode = hvac->valuestring;
                    
                    // System is now pure Fahrenheit - no conversion needed
                    // Values from HA are in °F and we store/display in °F
                }
                cJSON_Delete(root);
            }
            free(body_c);
        }

        // Apply updates if we learned anything this attempt
        if (!isnan(ambient_c) || !isnan(setpoint_c) || !isnan(humid) || mode) {
            if (!isnan(ambient_c) || !isnan(setpoint_c) || mode)
                lcd_update_thermostat_readings(isnan(ambient_c) ? -1000.0f : ambient_c,
                                               isnan(setpoint_c) ? -1000.0f : setpoint_c,
                                               mode);
            if (!isnan(humid))
                lcd_update_thermostat_humidity(humid);
            break; // done
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

esp_err_t mqtt_publish_nest_setpoint_cool(float setpoint_f)
{
    if (!mqtt_connected) return ESP_FAIL;
    char buf[16];
    // Publish as whole degrees Fahrenheit to match Nest/HA UI
    snprintf(buf, sizeof(buf), "%.0f", setpoint_f);
    const char *topic = "nest/setpoint_cool/cmd";
    ESP_LOGI(TAG, "Publishing Nest cool setpoint: %.0f°F to topic: %s", setpoint_f, topic);
    // Do not retain commands to avoid reapplying old setpoints on reconnect
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, buf, 0, 1, 0);

    // Also publish diagnostics to help verify broker reception and for HA troubleshooting
    esp_mqtt_client_publish(mqtt_client, "esp32_office_controller/diag/last_setpoint_cmd_f", buf, 0, 1, 1);
    // Non-retained event stream (optional listener in HA MQTT dev tools)
    esp_mqtt_client_publish(mqtt_client, "esp32_office_controller/events/setpoint_cmd_f", buf, 0, 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

// Request thermostat state update from Home Assistant via MQTT
// HA automation will respond by publishing to nest/* state topics
esp_err_t mqtt_request_nest_state(void)
{
    if (!mqtt_connected) return ESP_FAIL;
    const char *topic = "nest/request_state";
    const char *payload = "refresh";
    ESP_LOGI(TAG, "Requesting Nest state from HA via MQTT: %s", topic);
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, strlen(payload), 1, 0);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

static void mqtt_publish_discovery_config(void)
{
    // Water Valve
    const char *water_valve_config_topic = "homeassistant/switch/esp32_office_controller_water_valve/config";
    const char *water_valve_config_payload =
        "{"
        "\"name\":\"Office Water Valve\","
        "\"unique_id\":\"esp32_office_controller_water_valve\","
        "\"command_topic\":\"water_valve/cmd\","
        "\"state_topic\":\"water_valve/state\","
        "\"payload_on\":\"ON\","
        "\"payload_off\":\"OFF\","
        "\"state_on\":\"ON\","
        "\"state_off\":\"OFF\","
        "\"icon\":\"mdi:valve\","
            "\"availability\":{"
            "\"topic\":\"esp32_office_controller/availability\","
            "\"payload_available\":\"online\","
            "\"payload_not_available\":\"offline\""
            "},"
        "\"device\":{"
        "\"identifiers\":[\"esp32_office_controller\"],"
        "\"name\":\"ESP32 Office Controller\","
        "\"manufacturer\":\"YourNameOrBrand\","
        "\"model\":\"ESP32-S3 + ST7701\""
        "}"
        "}";
    esp_mqtt_client_publish(mqtt_client, water_valve_config_topic, water_valve_config_payload, 0, 1, 1);

    // Central Vacuum
    const char *central_vacuum_config_topic = "homeassistant/switch/esp32_office_controller_central_vacuum/config";
    const char *central_vacuum_config_payload =
        "{"
        "\"name\":\"Office Central Vacuum\","
        "\"unique_id\":\"esp32_office_controller_central_vacuum\","
        "\"command_topic\":\"central_vac/cmd\","
        "\"state_topic\":\"central_vac/state\","
        "\"payload_on\":\"ON\","
        "\"payload_off\":\"OFF\","
        "\"state_on\":\"ON\","
        "\"state_off\":\"OFF\","
        "\"icon\":\"mdi:vacuum\","
            "\"availability\":{"
            "\"topic\":\"esp32_office_controller/availability\","
            "\"payload_available\":\"online\","
            "\"payload_not_available\":\"offline\""
            "},"
        "\"device\":{"
        "\"identifiers\":[\"esp32_office_controller\"],"
        "\"name\":\"ESP32 Office Controller\","
        "\"manufacturer\":\"YourNameOrBrand\","
        "\"model\":\"ESP32-S3 + ST7701\""
        "}"
        "}";
    esp_mqtt_client_publish(mqtt_client, central_vacuum_config_topic, central_vacuum_config_payload, 0, 1, 1);

    // Vacuum Pump
    const char *vacuum_pump_config_topic = "homeassistant/switch/esp32_office_controller_vacuum_pump/config";
    const char *vacuum_pump_config_payload =
        "{"
        "\"name\":\"Office Vacuum Pump\","
        "\"unique_id\":\"esp32_office_controller_vacuum_pump\","
        "\"command_topic\":\"vacuum_pump/cmd\","
        "\"state_topic\":\"vacuum_pump/state\","
        "\"payload_on\":\"ON\","
        "\"payload_off\":\"OFF\","
        "\"state_on\":\"ON\","
        "\"state_off\":\"OFF\","
        "\"icon\":\"mdi:pump\","
            "\"availability\":{"
            "\"topic\":\"esp32_office_controller/availability\","
            "\"payload_available\":\"online\","
            "\"payload_not_available\":\"offline\""
            "},"
        "\"device\":{"
        "\"identifiers\":[\"esp32_office_controller\"],"
        "\"name\":\"ESP32 Office Controller\","
        "\"manufacturer\":\"YourNameOrBrand\","
        "\"model\":\"ESP32-S3 + ST7701\""
        "}"
        "}";
    esp_mqtt_client_publish(mqtt_client, vacuum_pump_config_topic, vacuum_pump_config_payload, 0, 1, 1);

    // --- Thermostat-related sensors (read-only) ---
    // Ambient Temperature (°C)
    const char *ambient_temp_cfg_topic = "homeassistant/sensor/esp32_office_controller_ambient_temp/config";
    const char *ambient_temp_cfg_payload =
        "{"
        "\"name\":\"Ambient Temperature\"," 
        "\"unique_id\":\"esp32_office_controller_ambient_temp\"," 
        "\"state_topic\":\"nest/ambient/state\"," 
        "\"unit_of_measurement\":\"°C\"," 
        "\"device_class\":\"temperature\"," 
        "\"state_class\":\"measurement\"," 
        "\"suggested_display_precision\":1," 
        "\"icon\":\"mdi:thermometer\"," 
        "\"device\":{"
            "\"identifiers\":[\"esp32_office_controller\"],"
            "\"name\":\"ESP32 Office Controller\","
            "\"manufacturer\":\"YourNameOrBrand\","
            "\"model\":\"ESP32-S3 + ST7701\""
        "}"
        "}";
    esp_mqtt_client_publish(mqtt_client, ambient_temp_cfg_topic, ambient_temp_cfg_payload, 0, 1, 1);

    // Cool Setpoint (°C)
    const char *setpoint_cool_cfg_topic = "homeassistant/sensor/esp32_office_controller_setpoint_cool/config";
    const char *setpoint_cool_cfg_payload =
        "{"
        "\"name\":\"Cool Setpoint\"," 
        "\"unique_id\":\"esp32_office_controller_setpoint_cool\"," 
        "\"state_topic\":\"nest/setpoint_cool/state\"," 
        "\"unit_of_measurement\":\"°C\","
        "\"device_class\":\"temperature\"," 
        "\"state_class\":\"measurement\","
        "\"suggested_display_precision\":1,"
        "\"icon\":\"mdi:thermostat\","
        "\"device\":{"
            "\"identifiers\":[\"esp32_office_controller\"],"
            "\"name\":\"ESP32 Office Controller\","
            "\"manufacturer\":\"YourNameOrBrand\","
            "\"model\":\"ESP32-S3 + ST7701\""
        "}"
        "}";
    esp_mqtt_client_publish(mqtt_client, setpoint_cool_cfg_topic, setpoint_cool_cfg_payload, 0, 1, 1);

    // Humidity (%)
    const char *humidity_cfg_topic = "homeassistant/sensor/esp32_office_controller_humidity/config";
    const char *humidity_cfg_payload =
        "{"
        "\"name\":\"Ambient Humidity\"," 
        "\"unique_id\":\"esp32_office_controller_humidity\"," 
        "\"state_topic\":\"nest/humidity/state\","
        "\"unit_of_measurement\":\"%\","
        "\"device_class\":\"humidity\","
        "\"state_class\":\"measurement\","
        "\"icon\":\"mdi:water-percent\","
        "\"device\":{"
            "\"identifiers\":[\"esp32_office_controller\"],"
            "\"name\":\"ESP32 Office Controller\","
            "\"manufacturer\":\"YourNameOrBrand\","
            "\"model\":\"ESP32-S3 + ST7701\""
        "}"
        "}";
    esp_mqtt_client_publish(mqtt_client, humidity_cfg_topic, humidity_cfg_payload, 0, 1, 1);

    // HVAC Mode (string)
    const char *mode_cfg_topic = "homeassistant/sensor/esp32_office_controller_hvac_mode/config";
    const char *mode_cfg_payload =
        "{"
        "\"name\":\"HVAC Mode\"," 
        "\"unique_id\":\"esp32_office_controller_hvac_mode\","
        "\"state_topic\":\"nest/mode/state\","
        "\"icon\":\"mdi:hvac\","
            "\"availability\":{"
            "\"topic\":\"esp32_office_controller/availability\","
            "\"payload_available\":\"online\","
            "\"payload_not_available\":\"offline\""
            "},"
        "\"device\":{"
            "\"identifiers\":[\"esp32_office_controller\"],"
            "\"name\":\"ESP32 Office Controller\","
            "\"manufacturer\":\"YourNameOrBrand\","
            "\"model\":\"ESP32-S3 + ST7701\""
        "}"
        "}";
    esp_mqtt_client_publish(mqtt_client, mode_cfg_topic, mode_cfg_payload, 0, 1, 1);
}

// Call this after MQTT connects, e.g. in MQTT_EVENT_CONNECTED:
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT connected");
        
        // Subscribe to relay command topics
        esp_mqtt_client_subscribe(mqtt_client, "water_valve/cmd", 1);
        esp_mqtt_client_subscribe(mqtt_client, "central_vac/cmd", 1);
        esp_mqtt_client_subscribe(mqtt_client, "vacuum_pump/cmd", 1);
        
        // Thermostat state subscriptions (published by HA automation/templates)
        ESP_LOGI(TAG, "Subscribing to Nest thermostat topics...");
        esp_mqtt_client_subscribe(mqtt_client, "nest/ambient/state", 1);
        esp_mqtt_client_subscribe(mqtt_client, "nest/humidity/state", 1);
        esp_mqtt_client_subscribe(mqtt_client, "nest/setpoint_cool/state", 1);
        esp_mqtt_client_subscribe(mqtt_client, "nest/mode/state", 1);
        ESP_LOGI(TAG, "Subscribed to nest/ambient/state, nest/humidity/state, nest/setpoint_cool/state, nest/mode/state");
        
        mqtt_publish_discovery_config();

        // Weather fetch is handled by weather_init_timer() in lcd.c
        // Don't trigger weather fetch on MQTT connect to avoid memory pressure causing disconnects

        // Mark this device as online (retained) for HA and visibility in broker
        esp_mqtt_client_publish(mqtt_client, AVAIL_TOPIC, "online", 0, 1, 1);
        
        // If HA automations aren't loaded to republish retained states,
        // seed the TFT once from HA REST after a short delay in a task.
        xTaskCreate(ha_seed_task, "ha_seed_task", 12288, NULL, 4, NULL);
        lcd_update_ha_status(true, "192.168.1.206");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGI(TAG, "MQTT disconnected");
        lcd_update_ha_status(false, NULL);
        break;
    case MQTT_EVENT_DATA:
        // Handle command topics and thermostat state topics
        // CRITICAL: event->topic and event->data are NOT null-terminated!
        // We must create null-terminated copies before using string functions
        if (event->topic && event->topic_len > 0)
        {
            // Create null-terminated topic string
            char topic[128] = {0};
            int topic_len = event->topic_len < 127 ? event->topic_len : 127;
            memcpy(topic, event->topic, topic_len);
            topic[topic_len] = '\0';
            
            // Check if this is a nest/ topic
            if (strstr(topic, "nest/") != NULL)
            {
                handle_thermostat_state(topic, event->data, event->data_len);
            }
            else
            {
                handle_command_message(topic, event->data, event->data_len);
            }
        }
        // Remove the weather parsing from MQTT_EVENT_DATA, as it's now handled via HTTP
        break;
    default:
        break;
    }
}

static void relay_state_change_handler(int relay_index, bool state)
{
    // Update the UI and hardware for each relay
    switch (relay_index)
    {
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

esp_err_t mqtt_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT client...");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = "mqtt://192.168.1.206",
                .port = 1883,
            },
        },
        .credentials = {
            .username = "mqtt",
            .authentication = {
                .password = "mqtt",
            },
            .client_id = "esp32_office_controller",
        },
        .network = {
            .timeout_ms = 10000,
            .refresh_connection_after_ms = 0,  // Disable auto-refresh
            .disable_auto_reconnect = false,
        },
        .session = {
            .keepalive = 120,  // Increased to 120 seconds to reduce disconnects
            .disable_clean_session = false,
            .last_will = {
                .topic = NULL, // set below to avoid non-const aggregate init ordering issues
            },
        },
        .task = {
            .stack_size = 16384,  // Increased from 8192
        },
        .buffer = {
            .size = 2048,  // Increased buffer for larger messages
            .out_size = 2048,
        },
    };

    // Configure Last Will (publish offline on unexpected disconnect)
    mqtt_cfg.session.last_will.topic = AVAIL_TOPIC;
    mqtt_cfg.session.last_will.msg = "offline";
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = true;

    ESP_LOGI(TAG, "Creating MQTT client (broker: mqtt://192.168.1.206:1883)");
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Registering MQTT event handlers");
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    // Disabled: Subscribing to ALL MQTT topics (#) causes connection overload and disconnects
    // esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_textarea_event_handler, NULL);

    ESP_LOGI(TAG, "Starting MQTT client");
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Setting relay state change callback");
    mqtt_set_relay_callback(relay_state_change_handler);

    ESP_LOGI(TAG, "MQTT initialization complete");
    return ESP_OK;
}

static void mqtt_textarea_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        // Subscribe to all topics (#) to capture all HA events
        int msg_id = esp_mqtt_client_subscribe(mqtt_client, "#", 1);
        ESP_LOGI(TAG, "Textarea handler: Subscribed to all MQTT topics (#), msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DATA:
    {
        char topic[256] = {0};
        char data[256] = {0};
        char event_text[1024] = {0};  // Increased size to prevent truncation
        int tlen = event->topic_len < 255 ? event->topic_len : 255;
        int dlen = event->data_len < 255 ? event->data_len : 255;
        memcpy(topic, event->topic, tlen); topic[tlen] = '\0';
        memcpy(data, event->data, dlen); data[dlen] = '\0';

        // Log ALL MQTT messages (for debugging)
        ESP_LOGI(TAG, "Textarea handler received MQTT data: topic='%s', payload='%s'", topic, data);

        // Log ALL MQTT messages to the text area (no filtering for motion)
        // Format: [TOPIC]: PAYLOAD
        snprintf(event_text, sizeof(event_text), "[%s]: %s", topic, data);
        lcd_append_motion_event(event_text);

        // Still trigger camera fetch only for motion events (if desired)
        bool is_motion_event = (strstr(topic, "motion") != NULL || strstr(topic, "binary_sensor") != NULL);
        if (is_motion_event) {
            extern void camera_client_fetch_image(void);
            //camera_client_fetch_image();
        }
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
    case MQTT_EVENT_SUBSCRIBED:
    case MQTT_EVENT_UNSUBSCRIBED:
    case MQTT_EVENT_PUBLISHED:
    case MQTT_EVENT_ERROR:
    case MQTT_EVENT_BEFORE_CONNECT:
    case MQTT_EVENT_DELETED:
        // These events are handled by the main mqtt_event_handler, silently ignore here
        break;
    default:
        // Only log truly unexpected events
        ESP_LOGW(TAG, "Textarea handler: Unexpected MQTT event ID=%ld", (long)event_id);
        break;
    }
}
