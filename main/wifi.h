#ifndef WIFI_H
#define WIFI_H
#include "esp_err.h"
#include <stdbool.h>
void wifi_init_sta(void);
void wifi_register_event_handler(void);
void wifi_get_status(char *ssid, size_t ssid_len, char *ip, size_t ip_len);
void wifi_is_connected(bool *connected);  // New function
#include <esp_event.h>
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
#endif // WIFI_H
