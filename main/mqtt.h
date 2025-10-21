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

// Shared defines for HA and weather
#define HA_BASE_URL "http://192.168.1.206:8123"
#define HA_ACCESS_TOKEN "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiIyZjI0M2Y3YzZjNDY0Y2ExOTk4YWIzYTM2NDdmMTNkYyIsImlhdCI6MTc1OTk0MzM0MiwiZXhwIjoyMDc1MzAzMzQyfQ.CywPwRlHLE9Oilufp_iRjSByLAjn6huRtx0Omk7vGyE"
#define WEATHER_ENTITY_ID "weather.forecast_home"
#define OPENWEATHERKEY "f629bfa41962d9cfb3842b40d2ca7a5c"
#define WEATHER_CITY "Boynton Beach"

#ifdef __cplusplus
}
#endif

#endif // MQTT_H
