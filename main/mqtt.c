#include "mqtt.h"
#include "lcd.h"
#include "mqtt.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "mqtt_client.h"

// Forward declaration for the text area MQTT handler
static void mqtt_textarea_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

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
        "\"device\":{"
        "\"identifiers\":[\"esp32_office_controller\"],"
        "\"name\":\"ESP32 Office Controller\","
        "\"manufacturer\":\"YourNameOrBrand\","
        "\"model\":\"ESP32-S3 + ST7701\""
        "}"
        "}";
    esp_mqtt_client_publish(mqtt_client, vacuum_pump_config_topic, vacuum_pump_config_payload, 0, 1, 1);
}

// Call this after MQTT connects, e.g. in MQTT_EVENT_CONNECTED:
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        esp_mqtt_client_subscribe(mqtt_client, "water_valve/cmd", 1);
        esp_mqtt_client_subscribe(mqtt_client, "central_vac/cmd", 1);
        esp_mqtt_client_subscribe(mqtt_client, "vacuum_pump/cmd", 1);
        mqtt_publish_discovery_config();

        ESP_LOGI(TAG, "MQTT connected");
        lcd_update_ha_status(true, "192.168.1.206");
        break;
    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGI(TAG, "MQTT disconnected");
        lcd_update_ha_status(false, NULL);
        break;
    case MQTT_EVENT_DATA:
        handle_command_message(event->topic, event->data, event->data_len);
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
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://192.168.1.206",
        .broker.address.port = 1883,
        .credentials.username = "mqtt",
        .credentials.authentication.password = "mqtt",
        .credentials.client_id = "esp32_office_controller"};
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client)
        return ESP_FAIL;
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_textarea_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    mqtt_set_relay_callback(relay_state_change_handler);
    return err;
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
    default:
        // Log unhandled MQTT events (e.g., MQTT_EVENT_DISCONNECTED, MQTT_EVENT_ERROR, etc.)
        ESP_LOGW(TAG, "Textarea handler: Unhandled MQTT event ID=%ld", (long)event_id);
        break;
    }
}
