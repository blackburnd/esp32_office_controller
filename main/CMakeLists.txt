cmake_minimum_required(VERSION 3.16)

set(SOURCES
    main.c
    wifi.c
    mqtt.c
    lcd.c
    camera_client.c
    weather.c
    sd_card.c
)

# Embed JPG assets from static/jpg/
set(ASSETS
    ../static/jpg/clear-day.jpg
    ../static/jpg/clear-night.jpg
    ../static/jpg/cloudy.jpg
    ../static/jpg/cloudy-1-day.jpg
    ../static/jpg/cloudy-1-night.jpg
    ../static/jpg/cloudy-2-day.jpg
    ../static/jpg/cloudy-2-night.jpg
    ../static/jpg/cloudy-3-day.jpg
    ../static/jpg/cloudy-3-night.jpg
    ../static/jpg/dust.jpg
    ../static/jpg/fog.jpg
    ../static/jpg/fog-day.jpg
    ../static/jpg/fog-night.jpg
    ../static/jpg/frost.jpg
    ../static/jpg/frost-day.jpg
    ../static/jpg/frost-night.jpg
    ../static/jpg/hail.jpg
    ../static/jpg/haze.jpg
    ../static/jpg/haze-day.jpg
    ../static/jpg/haze-night.jpg
    ../static/jpg/hurricane.jpg
    ../static/jpg/isolated-thunderstorms.jpg
    ../static/jpg/isolated-thunderstorms-day.jpg
    ../static/jpg/isolated-thunderstorms-night.jpg
    ../static/jpg/rain-and-sleet-mix.jpg
    ../static/jpg/rain-and-snow-mix.jpg
    ../static/jpg/rainy-1.jpg
    ../static/jpg/rainy-1-day.jpg
    ../static/jpg/rainy-1-night.jpg
    ../static/jpg/rainy-2.jpg
    ../static/jpg/rainy-2-day.jpg
    ../static/jpg/rainy-2-night.jpg
    ../static/jpg/rainy-3.jpg
    ../static/jpg/rainy-3-day.jpg
    ../static/jpg/rainy-3-night.jpg
    ../static/jpg/scattered-thunderstorms.jpg
    ../static/jpg/scattered-thunderstorms-day.jpg
    ../static/jpg/scattered-thunderstorms-night.jpg
    ../static/jpg/severe-thunderstorm.jpg
    ../static/jpg/snow-and-sleet-mix.jpg
    ../static/jpg/snowy-1.jpg
    ../static/jpg/snowy-1-day.jpg
    ../static/jpg/snowy-1-night.jpg
    ../static/jpg/snowy-2.jpg
    ../static/jpg/snowy-2-day.jpg
    ../static/jpg/snowy-2-night.jpg
    ../static/jpg/snowy-3.jpg
    ../static/jpg/snowy-3-day.jpg
    ../static/jpg/snowy-3-night.jpg
    ../static/jpg/thunderstorms.jpg
    ../static/jpg/tornado.jpg
    ../static/jpg/tropical-storm.jpg
    ../static/jpg/wind.jpg
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "."
    EMBED_FILES ${ASSETS}  # Embeds the JPGs
    REQUIRES lvgl esp_lvgl_port esp_http_client esp_wifi mqtt esp_event esp_netif esp-tls nvs_flash esp_jpeg json fatfs driver esp32_io_expander
)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
