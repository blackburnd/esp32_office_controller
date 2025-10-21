#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_tls.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "camera_client.h"
#include "lcd.h"
#include "sdkconfig.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "wifi.h"
#include <jpeg_decoder.h>
#include <stdlib.h>  // For malloc/free

static const char *TAG = "CAMERA_CLIENT";

// Forward declaration for resize_rgb565
esp_err_t resize_rgb565(const uint16_t *src, int src_w, int src_h, uint16_t *dst, int dst_w, int dst_h);

// Updated URLs for 640x480 resolution
static const char *camera_urls[8] = {
    "http://192.168.1.115/cgi-bin/api.cgi?cmd=Snap&channel=0&user=danielb&password=plastic12&width=640&height=480",  // Camera 1 (North Driveway)
    "http://192.168.1.115/cgi-bin/api.cgi?cmd=Snap&channel=1&user=danielb&password=plastic12&width=640&height=480",  // Camera 2 (Front Door)
    "http://192.168.1.115/cgi-bin/api.cgi?cmd=Snap&channel=2&user=danielb&password=plastic12&width=640&height=480",  // Camera 3 (South Driveway)
    "http://192.168.1.115/cgi-bin/api.cgi?cmd=Snap&channel=3&user=danielb&password=plastic12&width=640&height=480",  // Camera 4
    "http://192.168.1.115/cgi-bin/api.cgi?cmd=Snap&channel=4&user=danielb&password=plastic12&width=640&height=480",  // Camera 5
    "http://192.168.1.115/cgi-bin/api.cgi?cmd=Snap&channel=5&user=danielb&password=plastic12&width=640&height=480",  // Camera 6
    "http://192.168.1.115/cgi-bin/api.cgi?cmd=Snap&channel=6&user=danielb&password=plastic12&width=640&height=480",  // Camera 7
    "http://192.168.1.115/cgi-bin/api.cgi?cmd=Snap&channel=7&user=danielb&password=plastic12&width=640&height=480"   // Camera 8
};

static uint8_t *image_buffer = NULL;
#define IMAGE_BUFFER_SIZE (100 * 1024)

// Updated buffer size for 640x480 RGB565 (640 * 480 * 2 = 614,400 bytes)
#define DECODED_IMAGE_SIZE (640 * 480 * 2)  // 614,400 bytes

// Dynamically allocated buffer for decoded RGB565 image (320x240) in PSRAM
static uint16_t *decoded_image = NULL;

// Placeholder: Removed JPEG data, will fill buffer directly

extern lv_obj_t *camera_img_widget;
static lv_img_dsc_t camera_img_dsc;  // Descriptor for decoded image data

esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    static int output_len;
    switch(evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            output_len = 0;
            break;
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0 && image_buffer != NULL) {
                if (output_len + evt->data_len < IMAGE_BUFFER_SIZE) {
                    memcpy(image_buffer + output_len, evt->data, evt->data_len);
                    output_len += evt->data_len;
                } else {
                    ESP_LOGW(TAG, "Image buffer overflow");
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH, received %d bytes", output_len);
            if (output_len > 0) {
                // Log the response details
                ESP_LOGI(TAG, "JPEG response: %d bytes received", output_len);
                if (output_len >= 10) {
                    ESP_LOGI(TAG, "First 10 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                             image_buffer[0], image_buffer[1], image_buffer[2], image_buffer[3],
                             image_buffer[4], image_buffer[5], image_buffer[6], image_buffer[7],
                             image_buffer[8], image_buffer[9]);
                }
                // Check for JPEG signature
                if (output_len >= 2 && image_buffer[0] == 0xFF && image_buffer[1] == 0xD8) {
                    ESP_LOGI(TAG, "Response appears to be JPEG");
                } else {
                    ESP_LOGW(TAG, "Response does not appear to be JPEG");
                }
                

                // Decode JPEG to RGB565 using old API
                esp_jpeg_image_cfg_t jpeg_cfg = {
                    .indata = image_buffer,
                    .indata_size = output_len,
                    .outbuf = (uint8_t *)decoded_image,
                    .outbuf_size = DECODED_IMAGE_SIZE,
                    .out_format = JPEG_IMAGE_FORMAT_RGB565,
                    .out_scale = JPEG_IMAGE_SCALE_0,
                    .flags = {
                        .swap_color_bytes = 0,  // Changed from 1 to 0 to fix potential color byte order issues
                    }
                };
                esp_jpeg_image_output_t outimg;
                esp_err_t ret = esp_jpeg_decode(&jpeg_cfg, &outimg);
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "JPEG decoded successfully: %ux%u", outimg.width, outimg.height);
                    
                    // Resize from decoded 640x480 to 405x304 directly into decoded_image (since 405*304 < 640*480)
                    esp_err_t resize_ret = resize_rgb565(decoded_image, outimg.width, outimg.height, decoded_image, 405, 304);
                    if (resize_ret == ESP_OK) {
                        // Update descriptor for resized image
                        camera_img_dsc.header.always_zero = 0;
                        camera_img_dsc.header.w = 405;
                        camera_img_dsc.header.h = 304;
                        camera_img_dsc.data_size = 405 * 304 * 2;
                        camera_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
                        camera_img_dsc.data = (const uint8_t *)decoded_image;
                        
                        lvgl_port_lock(0);
                        lv_img_set_src(camera_img_widget, &camera_img_dsc);
                        lv_obj_invalidate(camera_img_widget);
                        lvgl_port_unlock();
                    } else {
                        ESP_LOGE(TAG, "Resize failed: %s", esp_err_to_name(resize_ret));
                    }
                } else {
                    ESP_LOGE(TAG, "JPEG decode failed: %s", esp_err_to_name(ret));
                }
            }
            output_len = 0;
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static TimerHandle_t stream_timer = NULL;
static bool is_streaming = false;

// Timer callback to fetch a frame
static void stream_timer_callback(TimerHandle_t xTimer) {
    camera_client_fetch_image(2);  // Stream for Camera 2 (Front Door)
}

void camera_client_start_stream(void) {
    if (is_streaming) return;
    is_streaming = true;
    ESP_LOGI(TAG, "Starting camera stream");
    // Create a timer to fetch every 200ms (5 FPS; adjust as needed)
    stream_timer = xTimerCreate("stream_timer", pdMS_TO_TICKS(200), pdTRUE, NULL, stream_timer_callback);
    if (stream_timer != NULL) {
        xTimerStart(stream_timer, 0);
    } else {
        ESP_LOGE(TAG, "Failed to create stream timer");
    }
}

void camera_client_stop_stream(void) {
    if (!is_streaming) return;
    is_streaming = false;
    ESP_LOGI(TAG, "Stopping camera stream");
    if (stream_timer != NULL) {
        xTimerStop(stream_timer, 0);
        xTimerDelete(stream_timer, 0);
        stream_timer = NULL;
    }
}

void camera_client_start(void) {
    image_buffer = (uint8_t *)heap_caps_malloc(IMAGE_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (image_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate image buffer in PSRAM. Trying internal RAM.");
        image_buffer = (uint8_t *)malloc(IMAGE_BUFFER_SIZE);
    }
    if (image_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate image buffer.");
        return;
    }
    
    // Allocate decoded_image in PSRAM
    decoded_image = (uint16_t *)heap_caps_malloc(DECODED_IMAGE_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (decoded_image == NULL) {
        ESP_LOGE(TAG, "Failed to allocate decoded image buffer in PSRAM. Trying internal RAM.");
        decoded_image = (uint16_t *)malloc(DECODED_IMAGE_SIZE);
    }
    if (decoded_image == NULL) {
        ESP_LOGE(TAG, "Failed to allocate decoded image buffer.");
        return;
    }
    
    // Create a 320x240 red placeholder by filling the buffer
    uint16_t red_color = 0xF800;  // RGB565 red
    for (int i = 0; i < 405 * 304; i++) {
        decoded_image[i] = red_color;
    }
    
    // Configure lv_img_dsc_t for the placeholder
    camera_img_dsc.header.always_zero = 0;
    camera_img_dsc.header.w = 405;  // Updated for resized size
    camera_img_dsc.header.h = 304;  // Updated for resized size
    camera_img_dsc.data_size = 405 * 304 * 2;
    camera_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565
    camera_img_dsc.data = (const uint8_t *)decoded_image;
    
    lvgl_port_lock(0);
    lv_img_set_src(camera_img_widget, &camera_img_dsc);
    lv_obj_invalidate(camera_img_widget);
    lvgl_port_unlock();
}

esp_err_t resize_rgb565(const uint16_t *src, int src_w, int src_h, uint16_t *dst, int dst_w, int dst_h) {
    if (!src || !dst || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            int src_x = (x * src_w) / dst_w;
            int src_y = (y * src_h) / dst_h;
            dst[y * dst_w + x] = src[src_y * src_w + src_x];
        }
    }
    return ESP_OK;
}

void camera_client_fetch_image(int camera_index) {
    if (camera_index < 1 || camera_index > 8) {
        ESP_LOGE(TAG, "Invalid camera index: %d", camera_index);
        return;
    }
    const char *url = camera_urls[camera_index - 1];  // 0-based array index
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}