cmake_minimum_required(VERSION 3.16)

set(SOURCES
    main.c
    wifi.c
    mqtt.c
    lcd.c
    camera_client.c
    weather.c
    ../components/lvgl__lvgl/src/extra/libs/png/lv_png.c
)

# Embed PNG assets from static/png/
set(ASSETS
    ../static/png/clear-day.png
    ../static/png/clear-night.png
    ../static/png/cloudy.png
    ../static/png/cloudy-1-day.png
    ../static/png/cloudy-1-night.png
    ../static/png/cloudy-2-day.png
    ../static/png/cloudy-2-night.png
    ../static/png/cloudy-3-day.png
    ../static/png/cloudy-3-night.png
    ../static/png/dust.png
    ../static/png/fog.png
    ../static/png/fog-day.png
    ../static/png/fog-night.png
    ../static/png/frost.png
    ../static/png/frost-day.png
    ../static/png/frost-night.png
    ../static/png/hail.png
    ../static/png/haze.png
    ../static/png/haze-day.png
    ../static/png/haze-night.png
    ../static/png/hurricane.png
    ../static/png/isolated-thunderstorms.png
    ../static/png/isolated-thunderstorms-day.png
    ../static/png/isolated-thunderstorms-night.png
    ../static/png/rain-and-sleet-mix.png
    ../static/png/rain-and-snow-mix.png
    ../static/png/rainy-1.png
    ../static/png/rainy-1-day.png
    ../static/png/rainy-1-night.png
    ../static/png/rainy-2.png
    ../static/png/rainy-2-day.png
    ../static/png/rainy-2-night.png
    ../static/png/rainy-3.png
    ../static/png/rainy-3-day.png
    ../static/png/rainy-3-night.png
    ../static/png/scattered-thunderstorms.png
    ../static/png/scattered-thunderstorms-day.png
    ../static/png/scattered-thunderstorms-night.png
    ../static/png/severe-thunderstorm.png
    ../static/png/snow-and-sleet-mix.png
    ../static/png/snowy-1.png
    ../static/png/snowy-1-day.png
    ../static/png/snowy-1-night.png
    ../static/png/snowy-2.png
    ../static/png/snowy-2-day.png
    ../static/png/snowy-2-night.png
    ../static/png/snowy-3.png
    ../static/png/snowy-3-day.png
    ../static/png/snowy-3-night.png
    ../static/png/thunderstorms.png
    ../static/png/tornado.png
    ../static/png/tropical-storm.png
    ../static/png/wind.png
)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "." "../components/lvgl__lvgl/src/extra/libs/png"  # <- add png header dir
    EMBED_FILES ${ASSETS}  # Embeds the PNGs
    REQUIRES lvgl esp_lvgl_port esp_http_client esp_wifi mqtt esp_event esp_netif esp-tls nvs_flash esp_jpeg json
)

idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${lvgl_lib} PRIVATE -Wno-format)
