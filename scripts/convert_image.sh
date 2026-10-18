#!/bin/bash

mkdir -p converted_landscape

echo "Starting conversion to 320x240 landscape..."

for img in *.jpg *.jpeg *.png *.webp; do
    [ -e "$img" ] || continue

    filename=$(basename -- "$img")
    name="${filename%.*}"
    output="converted_landscape/${name}.jpg"

    if [ -e "$output" ]; then
        echo "Already exists, skipping: $filename -> ${name}.jpg"
        continue
    fi

    echo "Processing: $filename -> ${name}.jpg"

    ffmpeg -nostdin -loglevel error -i "$img" \
        -vf "scale=320:240:force_original_aspect_ratio=increase,crop=320:240" \
        -pix_fmt yuvj420p -q:v 2 "$output" -y
done

echo "Done! Copy the images from the 'converted_landscape' folder to your SD card."
# Copyright (c) 2026 by WillowWilson78-hub
