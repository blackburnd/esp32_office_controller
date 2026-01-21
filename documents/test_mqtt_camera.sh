#!/bin/bash
# Test MQTT camera motion via Home Assistant API

HA_URL="http://192.168.1.206:8123"
HA_TOKEN=$(grep CONFIG_HA_ACCESS_TOKEN ../sdkconfig | cut -d'"' -f2)

echo "Testing Camera Motion MQTT via Home Assistant API"
echo "=================================================="
echo ""

# Test Camera 0 (North Driveway)
echo "Testing Camera 0 (North Driveway) - Sending ON"
curl -s -X POST "$HA_URL/api/services/mqtt/publish" \
  -H "Authorization: Bearer $HA_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "topic": "esp32_office_controller/camera/0/motion",
    "payload": "ON",
    "qos": 1,
    "retain": false
  }' | jq -r '.[] | select(.state) | .state' || echo "Done"

echo "Waiting 3 seconds..."
sleep 3

echo "Sending OFF"
curl -s -X POST "$HA_URL/api/services/mqtt/publish" \
  -H "Authorization: Bearer $HA_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "topic": "esp32_office_controller/camera/0/motion",
    "payload": "OFF",
    "qos": 1,
    "retain": false
  }' | jq -r '.[] | select(.state) | .state' || echo "Done"

echo ""
echo "Check your ESP32 monitor for messages like:"
echo "  I (xxxxx) mqtt: Camera 0 motion: DETECTED"
echo "  I (xxxxx) LCD: Camera 0 motion DETECTED - button highlighted"
echo ""
echo "Also check your TFT panel - the North Driveway button should have turned RED then BLUE"
