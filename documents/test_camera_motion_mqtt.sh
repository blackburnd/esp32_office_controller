#!/bin/bash
# Test MQTT camera motion events manually

echo "Testing MQTT camera motion events for ESP32 TFT panel"
echo "This will publish test motion events to each camera channel"
echo ""

# MQTT broker details
MQTT_HOST="192.168.1.206"
MQTT_PORT="1883"
MQTT_USER="mqtt"
MQTT_PASS="mqtt"

# Test each camera channel
for channel in {0..7}; do
    camera_names=("North Driveway" "Front Door" "South Driveway" "East Driveway" "North Canal" "West Canal" "Tiki" "South Yard")
    camera_name="${camera_names[$channel]}"
    
    echo "Testing Camera $channel ($camera_name)"
    
    # Publish motion ON
    echo "  Publishing: ON"
    mosquitto_pub -h "$MQTT_HOST" -p "$MQTT_PORT" \
                  -u "$MQTT_USER" -P "$MQTT_PASS" \
                  -t "esp32_office_controller/camera/$channel/motion" \
                  -m "ON" -q 1
    
    # Wait 3 seconds
    sleep 3
    
    # Publish motion OFF
    echo "  Publishing: OFF"
    mosquitto_pub -h "$MQTT_HOST" -p "$MQTT_PORT" \
                  -u "$MQTT_USER" -P "$MQTT_PASS" \
                  -t "esp32_office_controller/camera/$channel/motion" \
                  -m "OFF" -q 1
    
    echo "  Done - Check ESP32 monitor for logs"
    echo ""
    
    # Wait 2 seconds before next camera
    sleep 2
done

echo "Test complete! Check your ESP32 TFT panel to see if buttons changed color."
echo "Red = Motion detected, Blue = Normal"
