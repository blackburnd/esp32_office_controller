#include "wifi.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include <string.h>

#include "lcd.h"

#define WIFI_SSID "Sanctuary"
#define WIFI_PASSWORD "tikifire"

static const char *TAG = "wifi";

void wifi_init_sta(void)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished. SSID:%s", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void wifi_register_event_handler(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
}

void wifi_get_status(char *ssid, size_t ssid_len, char *ip, size_t ip_len)
{
    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK)
    {
        strncpy(ssid, (const char *)wifi_cfg.sta.ssid, ssid_len);
    }
    else
    {
        strncpy(ssid, "<unknown>", ssid_len);
    }
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
    {
        snprintf(ip, ip_len, "%d.%d.%d.%d", IP2STR(&ip_info.ip));
    }
    else
    {
        strncpy(ip, "0.0.0.0", ip_len);
    }
}
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    char ssid[33] = {0};
    char ip[16] = {0};
    if ((event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED))
    {
        wifi_get_status(ssid, sizeof(ssid), ip, sizeof(ip));
        lcd_update_wifi_status(ssid, ip);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_get_status(ssid, sizeof(ssid), ip, sizeof(ip));
        lcd_update_wifi_status(ssid, ip);
        // Initialize MQTT only after WiFi has IP
        extern esp_err_t mqtt_init(void);
        static bool mqtt_started = false;
        if (!mqtt_started) {
            mqtt_init();
            mqtt_started = true;
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        lcd_update_wifi_status("<unknown>", "0.0.0.0");
    }
}

void wifi_is_connected(bool *connected) {
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        *connected = true;
    } else {
        *connected = false;
    }
}

// Update WiFi status icon, SSID, and IP in the UI


// Update Home Assistant status icon and IP

