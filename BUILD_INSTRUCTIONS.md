# ESP32-S3 Touch LCD Build Instructions

## Build Process

This project uses the **ESP-IDF VS Code Extension** for building, flashing, and monitoring.

### Important: Build Method
- **DO NOT** use terminal commands like `idf.py build` or `idf.py flash`
- **USE** the ESP-IDF VS Code extension command: **"ESP-IDF: Build, Flash and Monitor"**

### How to Build, Flash and Monitor:

1. Open the project in VS Code
2. Press `Ctrl+Shift+P` (or `Cmd+Shift+P` on Mac) to open the command palette
3. Type: `ESP-IDF: Build, Flash and Monitor`
4. Select the command and press Enter
5. The extension will automatically:
   - Build the project
   - Flash the firmware to the ESP32-S3
   - Start the serial monitor

### Alternative Access:
- Use the ESP-IDF extension buttons in the VS Code status bar
- Look for the "Build, Flash and Monitor" button in the ESP-IDF extension interface

### Project Details:
- **Target**: ESP32-S3
- **Board**: Waveshare ESP32-S3-Touch-LCD-7 (800x480 RGB LCD)
- **Framework**: ESP-IDF v5.5
- **Graphics**: LVGL v8.4.0

### Current Status:
- LCD panel initialization working
- CH422G I2C expander configured (GPIO19=SDA, GPIO20=SCL)
- RGB data pins configured for 800x480 display
- LVGL direct initialization (bypassing ESP LVGL port)
- Test pattern UI with colored stripes

### Key Files:
- `main/lcd.c` - LCD and LVGL initialization
- `main/lcd_st7262_main.cpp` - Main application and UI
- `main/hardware.h` - Pin definitions
- `lv_conf.h` - LVGL configuration

### Troubleshooting:
- If build fails, ensure ESP-IDF extension is properly configured
- Check that the correct target (ESP32-S3) is selected
- Verify USB connection to the development board
- Use "ESP-IDF: Full Clean" if needed to clear build cache

---
*Last updated: September 23, 2025*