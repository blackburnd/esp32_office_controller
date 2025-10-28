# Component library changes: sdmmc SPI CRC handling (CMD59) and how to re-apply

This document explains why we patched the ESP-IDF sdmmc component for SDSPI cards that don't support CRC enable/disable (CMD59), exactly what changed, and how to re-apply the change after updating ESP-IDF.

## Why this change was needed

- Symptom: When initializing an SD card over SPI (SDSPI), initialization failed with error code `0x106` and logs like:
  - `sdmmc_sd: sdmmc_init_spi_crc: sdmmc_send_cmd_crc_on_off returned 0x106`
  - `vfs_fat_sdmmc: sdmmc_card_init failed (0x106)`
- Meaning: `0x106` is `ESP_ERR_NOT_SUPPORTED`. In SPI mode, enabling CRC for data transfers via CMD59 is optional and many cards simply don’t support it. Returning `ESP_ERR_NOT_SUPPORTED` is a valid outcome for CMD59 on some cards.
- Impact: The default logic treated this as a blocking error and aborted SD init, preventing mount even though data transfers can proceed without SPI CRC.

In our hardware (SDSPI with CH422G-controlled CS), we observed exactly this case: the card rejects CMD59 (reported by the driver as `ESP_ERR_NOT_SUPPORTED`). We want to continue initialization and mount the card without CRC.

## What changed

File (under your ESP-IDF checkout):

- `${IDF_PATH}/components/sdmmc/sdmmc_sd.c`

Function:

- `esp_err_t sdmmc_init_spi_crc(sdmmc_card_t* card)`

Behavior change:

- If `sdmmc_send_cmd_crc_on_off(card, true)` returns `ESP_ERR_NOT_SUPPORTED`, we now:
  - Log a warning (not an error), and
  - Return `ESP_OK`, continuing initialization without SPI CRC.
- For any other error, we still log as error and return the error.

Additionally (cleanup):

- If a prior local change introduced a literal `_0x106` comparison, replace it with `ESP_ERR_NOT_SUPPORTED` (from `esp_err.h`). This avoids undefined symbol issues and uses the proper error macro.

### Minimal diff (illustrative)

Note: This shows the intent of the change; line numbers may differ across IDF versions.

```diff
diff --git a/components/sdmmc/sdmmc_sd.c b/components/sdmmc/sdmmc_sd.c
--- a/components/sdmmc/sdmmc_sd.c
+++ b/components/sdmmc/sdmmc_sd.c
@@
 esp_err_t sdmmc_init_spi_crc(sdmmc_card_t* card)
 {
     assert(host_is_spi(card));
     esp_err_t err = sdmmc_send_cmd_crc_on_off(card, true);
     if (err != ESP_OK) {
-        ESP_LOGE(TAG, "%s: sdmmc_send_cmd_crc_on_off returned 0x%x", __func__, err);
-        if (err == ESP_ERR_NOT_SUPPORTED)
-            err = ESP_OK;
-        return err;
+        if (err == ESP_ERR_NOT_SUPPORTED) {
+            ESP_LOGW(TAG, "%s: CRC on/off (CMD59) not supported in SPI mode; continuing without CRC", __func__);
+            return ESP_OK;
+        }
+        ESP_LOGE(TAG, "%s: sdmmc_send_cmd_crc_on_off returned 0x%x", __func__, err);
+        return err;
     }
     return ESP_OK;
 }
```

If you see code comparing `err == _0x106`, replace `_0x106` with `ESP_ERR_NOT_SUPPORTED`.

## How to re-apply after ESP-IDF updates

Patching ESP-IDF directly is quick but gets overwritten when you update IDF. Choose one of these approaches:

1) Quick manual edit (fastest)

- Open `${IDF_PATH}/components/sdmmc/sdmmc_sd.c`, find `sdmmc_init_spi_crc`, and apply the conditional/LOGW change above.
- Rebuild bootloader/app and flash.

1) Use a Git patch file (repeatable)

- Save the patch provided below as `documents/patches/0001-sdmmc-spi-cmd59-unsupported-continue.patch`.
- Apply it after updating IDF:

  ```bash
  git -C "$IDF_PATH" apply \
    \/Users\/danielblackburn\/Documents\/HomeAssistantControllers\/esp32_office_controller\/documents\/patches\/0001-sdmmc-spi-cmd59-unsupported-continue.patch
  ```

- Rebuild and flash.

1) Component override (advanced)

- ESP-IDF supports overriding components via `EXTRA_COMPONENT_DIRS` with a local copy of `sdmmc`. This is heavier since `sdmmc` depends on other internals—prefer the small patch file unless you need to maintain deeper forks.

## Verify the fix

- On first SDSPI init, you may see:
  - `W sdmmc_sd: sdmmc_init_spi_crc: CRC on/off (CMD59) not supported in SPI mode; continuing without CRC`
- Then successful card info and mount:
  - `Name: <label>`
  - `Type: SD/SDHC`
  - `Speed: 400.00 kHz (limit: 400.00 kHz)` (during bring-up)
  - `... /sdcard mounted` and capacity lines.

## Related project config tweaks (context)

Additional project-level settings that complement the component change:

Not component patches, but helpful for this project:

- Flash size: set to 16MB to match hardware and remove boot warning.
- TLS robustness: enable mbedTLS external memory allocator and use a larger input buffer (e.g., 16–32KB) to avoid `-0x7100` on large TLS records.

These are tracked in `sdkconfig.defaults` and can be re-applied via `idf.py menuconfig` or by regenerating `sdkconfig` from defaults.

## Apply / build / run

```bash
# Optional: apply the sdmmc patch to the current IDF checkout
git -C "$IDF_PATH" apply \
  \/Users\/danielblackburn\/Documents\/HomeAssistantControllers\/esp32_office_controller\/documents\/patches\/0001-sdmmc-spi-cmd59-unsupported-continue.patch

# Reconfigure (pick 16MB flash; enable mbedTLS external allocator), then build/flash/monitor
idf.py menuconfig
idf.py fullclean build flash -p /dev/cu.wchusbserial59720521801 monitor
```

## Notes

- SPI-mode CRC is optional per SD spec. It’s safe to proceed without it when the card indicates `command not supported` for CMD59.
- Keep this document and the patch file under version control. After updating ESP-IDF, re-apply the patch before building.
