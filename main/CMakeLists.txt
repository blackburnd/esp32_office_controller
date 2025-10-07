cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "camera_client.c" "main.c" "lcd.c" "mqtt.c" "wifi.c" "mqtt_relay_client.c"
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_lvgl_port esp_http_client esp_wifi mqtt esp_event esp_netif esp-tls nvs_flash mbedtls esp_jpeg
    
)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
