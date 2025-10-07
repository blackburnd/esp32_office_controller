#ifndef MQTT_H
#define MQTT_H
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*relay_state_change_callback_t)(int relay_index, bool state);

esp_err_t mqtt_init(void);
void mqtt_set_relay_callback(relay_state_change_callback_t cb);
bool mqtt_is_connected(void);
esp_err_t mqtt_publish_central_vacuum_state(bool state);
esp_err_t mqtt_publish_water_valve_state(bool state);
esp_err_t mqtt_publish_vacuum_pump_state(bool state);


#ifdef __cplusplus
}
#endif

#endif // MQTT_H
