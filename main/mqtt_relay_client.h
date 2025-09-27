#ifndef MQTT_RELAY_CLIENT_H
#define MQTT_RELAY_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_RELAYS 3

// Callback function type for state changes
typedef void (*relay_state_change_callback_t)(int relay_index, bool state);

esp_err_t mqtt_init(void);
void set_relay_state_change_callback(relay_state_change_callback_t callback);
void mqtt_publish_relay_state(int relay_index, bool state);

#ifdef __cplusplus
}
#endif

#endif // MQTT_RELAY_CLIENT_H
