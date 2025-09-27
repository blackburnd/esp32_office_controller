#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <esp_log.h>
#include <esp_err.h>
#include <mqtt_client.h>
#include "mqtt_relay_client.h"

#define NUM_RELAYS 3
static const char* relay_ids[NUM_RELAYS] = { "vacuum_pump", "central_vac", "water_valve" };
static bool relay_states[NUM_RELAYS] = {false};
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;
static relay_state_change_callback_t relay_state_cb = NULL;
static const char* TAG = "mqtt_relay_client";

void set_relay_state_change_callback(relay_state_change_callback_t cb) {
    relay_state_cb = cb;
}

static void handle_command_message(const char* topic, const char* data, int data_len) {
    if (!topic || !data || data_len <= 0) return;
    for (int i = 0; i < NUM_RELAYS; i++) {
        char expected_topic[64];
        snprintf(expected_topic, sizeof(expected_topic), "%s/cmd", relay_ids[i]);
        if (strcmp(topic, expected_topic) == 0) {
            bool new_state = (strncmp(data, "ON", 2) == 0);
            relay_states[i] = new_state;
            if (relay_state_cb) relay_state_cb(i, new_state);
            ESP_LOGI(TAG, "Relay %d set to %s", i+1, new_state ? "ON" : "OFF");
            return;
        }
    }
    ESP_LOGW(TAG, "Unknown relay topic: %s", topic);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: {
        mqtt_connected = true;
        char topic[64];
        for (int i = 0; i < NUM_RELAYS; i++) {
            snprintf(topic, sizeof(topic), "%s/cmd", relay_ids[i]);
            esp_mqtt_client_subscribe(mqtt_client, topic, 1);
        }
        ESP_LOGI(TAG, "MQTT connected and subscribed");
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGI(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA: {
        char topic[256] = {0};
        char data[256] = {0};
        int tlen = event->topic_len < 255 ? event->topic_len : 255;
        int dlen = event->data_len < 255 ? event->data_len : 255;
        memcpy(topic, event->topic, tlen); topic[tlen] = '\0';
        memcpy(data, event->data, dlen); data[dlen] = '\0';
        handle_command_message(topic, data, dlen);
        break;
    }
    default:
        break;
    }
}

esp_err_t mqtt_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://192.168.1.206",
        .broker.address.port = 1883,
        .credentials.client_id = "esp32_office_controller",
        .credentials.username = "mqtt",
        .credentials.authentication.password = "mqtt",
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) return ESP_FAIL;
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    mqtt_connected = (err == ESP_OK);
    return err;
}

void mqtt_publish_relay_state(int relay_index, bool state) {
    if (!mqtt_connected || relay_index < 1 || relay_index > NUM_RELAYS) return;
    char topic[64];
    snprintf(topic, sizeof(topic), "%s/state", relay_ids[relay_index - 1]);
    const char* payload = state ? "ON" : "OFF";
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
}
