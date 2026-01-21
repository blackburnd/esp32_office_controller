#!/bin/bash
# ESP32 Office Controller - Build and Flash Script

set -e  # Exit on error

echo "========================================="
echo "ESP32 Office Controller Build & Flash"
echo "========================================="

# Navigate to project directory
cd "$(dirname "$0")"

# Source ESP-IDF environment
echo "Sourcing ESP-IDF environment..."
source ~/esp/v5.4.1/esp-idf/export.sh

# Clean build directory for fresh compile
echo "Cleaning build directory..."
rm -rf build/

# Build firmware
echo "Building firmware..."
idf.py build

# Check build success
if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Build successful!"
    echo ""
    
    # Ask user if they want to flash
    read -p "Flash to ESP32 now? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Flashing firmware..."
        idf.py -p /dev/tty.wchusbserial59720521801 flash
        
        echo ""
        echo "✅ Flash complete!"
        echo ""
        
        # Ask if user wants to monitor
        read -p "Start serial monitor? (y/n): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo "Starting monitor... (Press Ctrl+] to exit)"
            idf.py -p /dev/tty.wchusbserial59720521801 monitor
        fi
    fi
else
    echo ""
    echo "❌ Build failed! Check errors above."
    exit 1
fi
