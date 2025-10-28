#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

// SD card pin definitions (SPI)
#define SD_MOSI_PIN 11
#define SD_MISO_PIN 13
#define SD_CLK_PIN  12

// CH422G I2C I/O Expander settings
#define CH422G_I2C_ADDR 0x24
#define CH422G_I2C_NUM  I2C_NUM_0

// CH422G EXIO pin assignments (from Waveshare schematic)
#define CH422G_EXIO_TP_RST  0  // Touchscreen reset
#define CH422G_EXIO_LCD_BL  1  // LCD backlight
#define CH422G_EXIO_LCD_RST 2  // LCD reset
#define CH422G_EXIO_SD_CS   4  // SD card chip select
#define CH422G_EXIO_USB_SEL 3  // USB select

// SD card mount point
#define SD_MOUNT_POINT "/sdcard"

/**
 * @brief Initialize SD card
 *
 * Initializes the CH422G I/O expander and mounts the SD card filesystem.
 * CRITICAL: Sets LCD_BL HIGH to keep display backlight on!
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_card_init(void);

/**
 * @brief Unmount SD card and free resources
 */
void sd_card_deinit(void);

/**
 * @brief Check if SD card is mounted
 *
 * @return true if mounted, false otherwise
 */
bool sd_card_is_mounted(void);

/**
 * @brief Get SD card capacity information
 *
 * @param total_mb Pointer to store total capacity in MB
 * @param used_mb Pointer to store used capacity in MB
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sd_card_get_capacity(uint64_t *total_mb, uint64_t *used_mb);

#endif // SD_CARD_H
