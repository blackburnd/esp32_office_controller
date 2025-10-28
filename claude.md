# ESP32 Office Controller - AI Assistant Instructions

## Project Overview
This is an ESP32-S3 based office controller with a 7" touch LCD display (800x480) that integrates:
- MQTT communication with Home Assistant
- Reolink camera client with image display
- Weather forecast display (OpenWeatherMap API)
- Touch panel controls for 3 relays (Water Valve, Central Vacuum, Vacuum Pump)
- Real-time MQTT event monitoring

## Critical Information

### Hardware
- Board: ESP32-S3 (Waveshare ESP32-S3-Touch-LCD-7.0)
- Display: ST7701 RGB LCD (800x480)
- Touch: GT911 I2C touchscreen
- Memory: Uses PSRAM for framebuffers
- SD Card: TF card slot using SPI (GPIO11/12/13), CS controlled via CH422G I2C expander (GPIO8/9)

### Network Configuration
- WiFi credentials: Configured in wifi.c
- MQTT Broker: 192.168.1.206:1883
- Home Assistant: http://192.168.1.206:8123
- SNTP Server: pool.ntp.org (for time synchronization)

## Code Architecture

### File Structure
```
main/
├── main.c              - Entry point, LCD/touch/LVGL initialization, SNTP setup, SD card init
├── wifi.c/h           - WiFi connection management
├── mqtt.c/h           - MQTT client, relay control, HA discovery
├── lcd.c/h            - LVGL UI creation and updates
├── camera_client.c/h  - Reolink camera HTTP client
├── weather.c/h        - OpenWeatherMap API client with SD card persistence
├── sd_card.c/h        - SD card mount/unmount, CH422G I/O expander control
└── assets.h           - Embedded JPG weather icons
```

### Important Notes

1. **mqtt_relay_client.c is DEPRECATED**
   - This file was refactored into mqtt.c
   - If you see mqtt_relay_client.c, it should be deleted
   - All relay functionality is now in mqtt.c

2. **Duplicate Code Issue**
   - Previous AI corrupted mqtt_relay_client.c by duplicating every line
   - Always check for duplicate includes/code before making changes
   - File structure: `#include <xxx>#include <xxx>` indicates duplication

3. **Time Synchronization is CRITICAL**
   - HTTPS/TLS certificate validation requires accurate system time
   - SNTP initialization happens in main.c:initialize_sntp()
   - Time sync callback sets timezone to EST/EDT
   - Code waits up to 10 seconds for time sync before continuing
   - DO NOT REMOVE the time synchronization code!

4. **Build System**
   - Uses ESP-IDF build system (idf.py)
   - CMakeLists.txt defines sources and embedded PNG assets
   - LVGL PNG decoder is included from components/lvgl__lvgl/

## Common Issues & Fixes

### Issue: Build fails with "file not found" for mqtt_relay_client.c
**Fix**: Delete mqtt_relay_client.c - it's not in CMakeLists.txt and functionality is in mqtt.c

### Issue: Duplicate includes in any .c file
**Fix**: The file is corrupted. Compare with git history and restore clean version.

### Issue: HTTPS requests fail with certificate errors
**Fix**: Ensure SNTP is initialized and system time is set before making HTTPS requests.

### Issue: Weather icons not displaying
**Fix**: Check that PNG assets are embedded in CMakeLists.txt and lv_png_init() is called.

### Issue: SD card mount fails
**Fix**:
- Check that 4GB SD card is inserted in TF slot
- Ensure card is FAT32 formatted
- Verify CH422G I2C communication (GPIO8=SDA, GPIO9=SCL)
- Check serial logs for specific error from `sd_card_init()`

### Issue: Weather data not persisting between reboots
**Fix**:
- Verify SD card is mounted successfully (check logs for "SD card initialized successfully")
- Check that `/sdcard/weather_data.json` file exists after weather fetch
- If file is corrupt, delete it and let system fetch fresh data

## Key Functions

### MQTT (mqtt.c)
- `mqtt_init()` - Initialize MQTT client, register event handlers
- `mqtt_publish_*_state(bool)` - Publish relay states to HA
- `mqtt_set_relay_callback()` - Set callback for relay state changes
- `mqtt_publish_discovery_config()` - Send HA MQTT discovery messages

### LCD/UI (lcd.c)
- `lcd_create_ui()` - Create all LVGL UI elements
- `lcd_update_camera_image()` - Update camera image display
- `lcd_update_weather_forecast()` - Update weather display
- `lcd_start_weather_fetch()` - Trigger weather HTTP fetch
- `lcd_append_motion_event()` - Add event to scrolling log

### Weather (weather.c)
- `weather_fetch_and_display()` - Fetch from OpenWeatherMap and update UI (non-blocking, spawns task)
- `weather_save_to_sd()` - Save weather JSON to SD card after successful fetch
- `weather_load_from_sd()` - Load cached weather data from SD card on startup
- Uses lat/lon: 26.1224, -80.1373 (Boynton Beach, FL)
- Fetches 5-day forecast, groups by day, shows highs/lows
- Weather data persists between reboots via SD card

### SD Card (sd_card.c)
- `sd_card_init()` - Initialize CH422G I/O expander and mount SD card via SPI
- `sd_card_deinit()` - Unmount SD card and free resources
- `sd_card_is_mounted()` - Check if SD card is currently mounted
- `sd_card_get_capacity()` - Get SD card total and used space in MB
- **Hardware**: Uses SPI2 (GPIO11=MOSI, GPIO12=CLK, GPIO13=MISO)
- **CS Control**: SD card CS pin controlled via CH422G EXIO4 (I2C at GPIO8/9)
- **Mount Point**: /sdcard
- **Current Use**: Caches weather data as `/sdcard/weather_data.json`
- **Future Use**: Can store camera snapshots, MQTT logs, config files

### Time Sync (main.c)
- `initialize_sntp()` - Configure and start SNTP client
- `time_sync_notification_cb()` - Called when time is synchronized
- Waits up to 10 seconds for initial time sync before continuing

## Build & Flash

### IMPORTANT: Build Environment Setup

**The user builds this project using VSCode GUI tools (ESP-IDF extension), NOT command-line idf.py.**

When making changes:

- Always use **absolute paths** from project root: `/Users/danielblackburn/Documents/HomeAssistantControllers/esp32_office_controller/`
- The build system expects proper ESP-IDF environment to be configured
- Paths in CMakeLists.txt and includes must be correct relative to project structure
- Component paths are relative to the components/ directory

### Directory Structure Requirements

```text
esp32_office_controller/
├── main/                    # Main source files
│   ├── CMakeLists.txt      # MUST list all .c files in SOURCES
│   ├── *.c/*.h             # Source files
│   └── assets.h            # Generated from embedded PNGs
├── components/             # ESP-IDF components
│   └── lvgl__lvgl/         # LVGL library
├── static/png/             # PNG assets (embedded at build time)
└── sdkconfig              # ESP-IDF configuration
```

### Build Commands (if using CLI)

**Note**: User prefers VSCode ESP-IDF extension GUI for building.

```bash
# Build only (requires ESP-IDF environment)
idf.py build

# Build and flash
idf.py flash

# Monitor serial output
idf.py monitor

# Full workflow
idf.py build flash monitor
```

### Path-Related Issues

1. **CMakeLists.txt SOURCES must match actual files**
   - Only list files that exist in main/ directory
   - Remove references to deleted files (e.g., mqtt_relay_client.c)

2. **Include paths must be correct**
   - LVGL PNG: `"extra/libs/png/lv_png.h"` (not `"src/extra/libs/png/lv_png.h"`)
   - Local headers: `"mqtt.h"` `"lcd.h"` etc.
   - System headers: `<esp_log.h>` `<freertos/FreeRTOS.h>` etc.

3. **Embedded assets**
   - PNG files in `../static/png/*.png` are embedded via EMBED_FILES
   - Accessible as `extern const uint8_t filename_png_start[]`
   - Must be listed in CMakeLists.txt ASSETS section

## Development Guidelines

1. **Always check git status before major refactors**
   - Use `git diff` to see what changed
   - Don't remove code without understanding its purpose

2. **Test time-critical features**
   - Weather fetch (HTTPS)
   - Camera fetch (HTTPS if using TLS)
   - Certificate validation requires accurate time

3. **LVGL thread safety**
   - Always wrap LVGL calls with `lvgl_port_lock(0)` / `lvgl_port_unlock()`
   - UI updates from HTTP callbacks must use locks

4. **Memory constraints**
   - ESP32-S3 has limited RAM
   - Large buffers (weather, camera) should be static or in PSRAM
   - Weather buffer: 32KB
   - Camera buffer: Defined in camera_client.c

## Future Improvements
- [ ] Add appropriate weather icons to weather page
- [ ] Add button to fetch radar where the refresh button is, the refresh button should not be needed when functioning properly.
- [ ] Add proper humidity reading (currently shows "N/A")
- [ ] Implement periodic weather refresh (currently disabled)
- [ ] Add error recovery for failed HTTP requests
- [ ] Improve motion event filtering in MQTT textarea handler
- [ ] Add battery/power status display

## Credentials & API Keys

### MQTT

- Username: mqtt
- Password: mqtt

### Home Assistant

- Access Token: Stored in `sdkconfig.defaults` as `CONFIG_HA_ACCESS_TOKEN`
- Base URL: Stored in `sdkconfig.defaults` as `CONFIG_HA_BASE_URL`

### OpenWeatherMap

- API Key: Stored in `sdkconfig.defaults` as `CONFIG_OPENWEATHER_API_KEY`

### Camera (Reolink)

- Snapshot URL with credentials: Stored in `sdkconfig.defaults` as `CONFIG_CAMERA_SNAPSHOT_URL`

**IMPORTANT SECURITY NOTES:**

- All credentials are stored in `sdkconfig.defaults` which IS committed to git
- The generated `sdkconfig` and `sdkconfig.old` files are gitignored (do NOT commit these)
- These credentials persist through fullclean/rebuilds because they're in sdkconfig.defaults
- For production, consider using environment variables or a secrets management system

## Last Known Issues (as of build)

None - mqtt_relay_client.c corruption has been resolved.
