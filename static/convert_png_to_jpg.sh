#!/bin/bash

# Define the static directory path
STATIC_DIR="/Users/danielblackburn/Documents/HomeAssistantControllers/esp32_office_controller/static"
PNG_DIR="$STATIC_DIR/png"
JPG_DIR="$STATIC_DIR/jpg"

# Check if png directory exists
if [ ! -d "$PNG_DIR" ]; then
    echo "Error: PNG directory not found at $PNG_DIR"
    exit 1
fi

# Create jpg directory if it doesn't exist
if [ ! -d "$JPG_DIR" ]; then
    mkdir -p "$JPG_DIR"
    echo "Created jpg directory at $JPG_DIR"
else
    echo "jpg directory already exists at $JPG_DIR"
fi

# Convert all PNG files to JPG
count=0
for png_file in "$PNG_DIR"/*.png; do
    if [ -f "$png_file" ]; then
        filename=$(basename "$png_file" .png)
        jpg_file="$JPG_DIR/${filename}.jpg"
        
        # Use ImageMagick to convert PNG to JPG and scale up 4x
        convert "$png_file" -scale 400% -quality 85 "$jpg_file"
        
        if [ $? -eq 0 ]; then
            echo "✓ Converted: $filename.png → ${filename}.jpg"
            ((count++))
        else
            echo "✗ Failed to convert: $filename.png"
        fi
    fi
done

echo ""
echo "Conversion complete! $count PNG files converted to JPG."
echo "JPG files are located in: $JPG_DIR"