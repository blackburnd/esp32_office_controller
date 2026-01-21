#!/bin/bash
# Script to find Reolink camera motion sensor entity IDs in Home Assistant

# Extract HA credentials from sdkconfig
HA_URL=$(grep CONFIG_HA_BASE_URL ../sdkconfig | cut -d'"' -f2)
HA_TOKEN=$(grep CONFIG_HA_ACCESS_TOKEN ../sdkconfig | cut -d'"' -f2)

echo "Fetching entities from Home Assistant..."
echo "URL: $HA_URL"
echo ""

# Fetch all states and filter for motion sensors
curl -s -H "Authorization: Bearer $HA_TOKEN" \
     -H "Content-Type: application/json" \
     "$HA_URL/api/states" | \
     python3 -c "
import json
import sys

data = json.load(sys.stdin)

print('=== All Binary Sensors (motion, camera, etc.) ===')
print('')

motion_sensors = []
for entity in data:
    entity_id = entity.get('entity_id', '')
    # Filter for binary sensors that might be motion/camera related
    if 'binary_sensor' in entity_id:
        attributes = entity.get('attributes', {})
        friendly_name = attributes.get('friendly_name', entity_id)
        device_class = attributes.get('device_class', 'none')
        state = entity.get('state', 'unknown')
        
        # Focus on motion, occupancy, or camera-related sensors
        if any(keyword in entity_id.lower() or keyword in friendly_name.lower() 
               for keyword in ['motion', 'camera', 'reolink', 'driveway', 'door', 'canal', 'tiki', 'yard']):
            motion_sensors.append({
                'entity_id': entity_id,
                'name': friendly_name,
                'device_class': device_class,
                'state': state
            })

if motion_sensors:
    for sensor in sorted(motion_sensors, key=lambda x: x['entity_id']):
        print(f\"Entity ID: {sensor['entity_id']}\")
        print(f\"  Name: {sensor['name']}\")
        print(f\"  Device Class: {sensor['device_class']}\")
        print(f\"  Current State: {sensor['state']}\")
        print('')
else:
    print('No motion sensors found!')
    print('')
    print('All binary sensors:')
    for entity in data:
        if 'binary_sensor' in entity.get('entity_id', ''):
            print(f\"  {entity['entity_id']}\")
"

echo ""
echo "=== Existing automations using these entity IDs ==="
echo ""
grep -A2 "entity_id: 95e87c95620bf88cce05566a0120e9ea" ../documents/automations.yaml | head -10
grep -A2 "entity_id: 72e642ca7065b03f6dbc344e84cedd7c" ../documents/automations.yaml | head -10
