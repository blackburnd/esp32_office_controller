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
void mqtt_publish_weather_request(void);

// Nest thermostat topics (simple MQTT bridge to HA automations) - temperatures in Fahrenheit
esp_err_t mqtt_publish_nest_setpoint_cool(float setpoint_f);
esp_err_t mqtt_request_nest_state(void);  // Request thermostat data from HA

// Shared defines for HA and weather - use Kconfig values
#define HA_BASE_URL CONFIG_HA_BASE_URL
#define HA_ACCESS_TOKEN CONFIG_HA_ACCESS_TOKEN
#define WEATHER_ENTITY_ID "weather.forecast_home"
#define OPENWEATHERKEY CONFIG_OPENWEATHER_API_KEY
#define WEATHER_CITY "Boynton Beach"

#ifdef __cplusplus
}
#endif

#endif // MQTT_H
