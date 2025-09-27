#ifndef WIFI_H
#define WIFI_H
#include "esp_err.h"
void wifi_init_sta(void);
void wifi_register_event_handler(void);
void wifi_get_status(char *ssid, size_t ssid_len, char *ip, size_t ip_len);
#endif // WIFI_H
