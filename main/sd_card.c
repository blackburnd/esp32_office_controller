#include "sd_card.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "base/esp_io_expander.h"
#include "port/esp_io_expander_ch422g.h"

static const char *TAG = "sd_card";

static sdmmc_card_t *card = NULL;
static esp_io_expander_handle_t io_expander = NULL;
static bool mounted = false;

esp_err_t sd_card_init(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "Initializing CH422G I/O expander for SD card control...");

    // Step 1: Initialize I2C bus (should already be initialized for touch, but safe to check)
    // The I2C bus is shared with GT911 touchscreen

    // Step 2: Create CH422G I/O expander instance
    ret = esp_io_expander_new_i2c_ch422g(CH422G_I2C_NUM, CH422G_I2C_ADDR, &io_expander);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CH422G I/O expander: %s", esp_err_to_name(ret));
        return ret;
    }

    // Step 3: CRITICAL - Set all CH422G pins to correct states
    // This is what was missing before - we MUST keep LCD backlight ON!

    // Set all pins as outputs
    ESP_LOGI(TAG, "Configuring CH422G pins...");
    for (int i = 0; i < 8; i++) {
        esp_io_expander_set_dir(io_expander, (1ULL << i), IO_EXPANDER_OUTPUT);
    }

    // Set LCD backlight HIGH (keep display on!)
    ESP_LOGI(TAG, "Setting LCD backlight HIGH to keep display on");
    esp_io_expander_set_level(io_expander, (1ULL << CH422G_EXIO_LCD_BL), 1);

    // Set LCD reset HIGH (normal operation)
    esp_io_expander_set_level(io_expander, (1ULL << CH422G_EXIO_LCD_RST), 1);

    // Set touchscreen reset HIGH (normal operation)
    esp_io_expander_set_level(io_expander, (1ULL << CH422G_EXIO_TP_RST), 1);

    // Set SD_CS HIGH (deselected initially)
    ESP_LOGI(TAG, "Setting SD_CS HIGH (deselected)");
    esp_io_expander_set_level(io_expander, (1ULL << CH422G_EXIO_SD_CS), 1);

    vTaskDelay(pdMS_TO_TICKS(100));  // Give hardware time to stabilize

    // Step 4: Initialize SPI bus for SD card
    ESP_LOGI(TAG, "Initializing SPI bus for SD card...");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_PROBING;  // Start slow (400kHz)

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI_PIN,
        .miso_io_num = SD_MISO_PIN,
        .sclk_io_num = SD_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        esp_io_expander_del(io_expander);
        io_expander = NULL;
        return ret;
    }

    // Step 5: Configure SD card slot with manual CS control via CH422G
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = GPIO_NUM_NC;  // We control CS via CH422G, not GPIO
    slot_config.host_id = host.slot;

    // Step 6: Mount filesystem
    ESP_LOGI(TAG, "Mounting SD card filesystem...");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // Pull CS LOW to select SD card before mounting
    ESP_LOGI(TAG, "Asserting SD_CS (pulling LOW) to enable SD card...");
    esp_io_expander_set_level(io_expander, (1ULL << CH422G_EXIO_SD_CS), 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set format_if_mount_failed = true.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card: %s", esp_err_to_name(ret));
        }
        // Deselect SD card
        esp_io_expander_set_level(io_expander, (1ULL << CH422G_EXIO_SD_CS), 1);
        spi_bus_free(host.slot);
        esp_io_expander_del(io_expander);
        io_expander = NULL;
        return ret;
    }

    mounted = true;

    // Print card info
    sdmmc_card_print_info(stdout, card);

    ESP_LOGI(TAG, "SD card mounted successfully at %s", SD_MOUNT_POINT);

    return ESP_OK;
}

void sd_card_deinit(void)
{
    if (mounted) {
        esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
        mounted = false;
        card = NULL;
        ESP_LOGI(TAG, "SD card unmounted");
    }

    if (io_expander) {
        // Deselect SD card
        esp_io_expander_set_level(io_expander, (1ULL << CH422G_EXIO_SD_CS), 1);

        // DON'T turn off LCD backlight!
        // Keep LCD_BL, LCD_RST, TP_RST at their current states

        esp_io_expander_del(io_expander);
        io_expander = NULL;
    }

    // Free SPI bus
    spi_bus_free(SPI2_HOST);
}

bool sd_card_is_mounted(void)
{
    return mounted;
}

esp_err_t sd_card_get_capacity(uint64_t *total_mb, uint64_t *used_mb)
{
    if (!mounted || !card) {
        return ESP_ERR_INVALID_STATE;
    }

    FATFS *fs;
    DWORD fre_clust;

    esp_err_t ret = f_getfree("0:", &fre_clust, &fs);
    if (ret != 0) {
        return ESP_FAIL;
    }

    uint64_t total_sectors = (fs->n_fatent - 2) * fs->csize;
    uint64_t free_sectors = fre_clust * fs->csize;

    *total_mb = (total_sectors * fs->ssize) / (1024 * 1024);
    *used_mb = ((total_sectors - free_sectors) * fs->ssize) / (1024 * 1024);

    return ESP_OK;
}
