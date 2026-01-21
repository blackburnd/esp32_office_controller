# Camera Motion Detection Setup Guide

## Overview
This guide explains how to set up motion detection from your Reolink cameras to the ESP32 TFT panel.

## Camera Channel Mapping

| Channel | Camera Name    | ESP32 Button Variable       | Automation ID                  |
|---------|----------------|----------------------------|--------------------------------|
| 0       | North Driveway | `fetch_image_button`       | `camera_motion_north_driveway` |
| 1       | Front Door     | `camera_front_door_btn`    | `camera_motion_front_door`     |
| 2       | South Driveway | `camera_north_canal_btn`   | `camera_motion_south_driveway` |
| 3       | East Driveway  | `camera3_btn`              | `camera_motion_east_driveway`  |
| 4       | North Canal    | `camera_south_yard_btn`    | `camera_motion_north_canal`    |
| 5       | West Canal     | `camera_west_canal_btn`    | `camera_motion_west_canal`     |
| 6       | Tiki           | `camera_tiki_btn`          | `camera_motion_tiki`           |
| 7       | South Yard     | `camera8_btn`              | `camera_motion_south_yard`     |

## Finding Your Actual Reolink Entity IDs

The automations in `automations.yaml` use placeholder entity IDs. You need to replace them with your actual Reolink motion sensor entities.

### Method 1: Using Home Assistant UI

1. Go to **Developer Tools** → **States**
2. Search for "motion" or "reolink" in the filter box
3. Look for entities like:
   - `binary_sensor.reolink_*_motion`
   - `binary_sensor.*_person_detection`
   - `binary_sensor.*_motion_detected`
4. Note down the entity IDs for each camera

### Method 2: Using Device Pages

1. Go to **Settings** → **Devices & Services**
2. Click on **Reolink** integration
3. Click on each camera device
4. Look for the motion detection binary sensor
5. Copy the entity ID

### Method 3: Using YAML Configuration

If you have access to your Home Assistant configuration files:

```bash
# SSH into your Home Assistant instance
# Look for Reolink entities in:
cat /config/.storage/core.entity_registry
```

## Updating the Automations

Once you have your entity IDs, edit `documents/automations.yaml` and replace the placeholders:

**Current (placeholder):**
```yaml
- id: camera_motion_north_driveway
  alias: "Camera Motion: North Driveway → MQTT"
  trigger:
    - platform: state
      entity_id: binary_sensor.north_driveway_motion  # <-- UPDATE THIS
```

**Example (actual):**
```yaml
- id: camera_motion_north_driveway
  alias: "Camera Motion: North Driveway → MQTT"
  trigger:
    - platform: state
      entity_id: binary_sensor.reolink_driveway_north_motion  # <-- Your actual entity
```

## Common Entity ID Patterns

Reolink cameras typically use these patterns:
- `binary_sensor.reolink_{camera_name}_motion`
- `binary_sensor.{camera_name}_motion_detected`
- `binary_sensor.{camera_name}_person_detection`

## Testing the Setup

### 1. Reload Automations
After updating `automations.yaml`:
- Go to **Developer Tools** → **YAML**
- Click **Automations** reload button
- Or run: `automation.reload` service

### 2. Monitor MQTT Topics
Use the MQTT dev tool to watch for motion events:
- Go to **Developer Tools** → **MQTT**
- Click **Listen to a topic**
- Enter: `esp32_office_controller/camera/+/motion`
- Trigger motion on a camera and verify the message appears

### 3. Check ESP32 Logs
Monitor the ESP32 serial output for:
```
I (12345) mqtt: Camera 0 motion: DETECTED
I (12346) LCD: Camera 0 motion DETECTED - button highlighted
```

### 4. Visual Verification
- Go to the Cameras tab on your ESP32 TFT panel
- Trigger motion on a camera
- The corresponding button should turn RED
- When motion clears, it should return to BLUE

## MQTT Topic Structure

- **Command Topics:** `esp32_office_controller/camera/{channel}/motion`
- **Payload:** `ON` (motion detected) or `OFF` (motion cleared)
- **QoS:** 1 (at least once delivery)
- **Retained:** false (state should not persist across restarts)

## Troubleshooting

### Motion not detected on panel
1. Check automation is enabled in HA
2. Verify MQTT broker is running
3. Check ESP32 MQTT connection status
4. Use MQTT listen tool to verify messages are published
5. Check ESP32 serial logs for subscription confirmations

### Wrong button highlighted
1. Verify camera channel mapping matches your setup
2. Check channel number in camera_client.c matches button layout
3. Review `get_camera_button_by_channel()` in lcd.c

### Button stays red after motion clears
1. Check if HA automation sends "OFF" payload
2. Verify binary sensor returns to "off" state
3. Check ESP32 logs for "motion CLEARED" messages

## Color Coding

- **Normal (Blue):** `lv_color_lighten(COLOR_BLUE, LV_OPA_20)` with darker gradient
- **Motion Detected (Red):** `COLOR_MOTION_DETECTED` = `0xFF4444` with darker gradient

## Next Steps

1. Update all 8 entity IDs in `automations.yaml`
2. Reload automations in Home Assistant
3. Rebuild and flash ESP32 firmware (if you made code changes)
4. Test each camera by triggering motion
5. Verify button colors change appropriately

## Reference Files

- **Automations:** `documents/automations.yaml` (lines 621-740)
- **MQTT Handler:** `main/mqtt.c` (handle_camera_motion function)
- **UI Updates:** `main/lcd.c` (lcd_set_camera_button_motion function)
- **Camera URLs:** `main/camera_client.c` (camera_urls array)
