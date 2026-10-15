#!/bin/bash

FPS="${FPS:-24}"
QUALITY="${QUALITY:-5}"
OUT_DIR="mjpeg"

shopt -s nullglob nocaseglob

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "ERROR: ffmpeg not found. Please install ffmpeg first."
    exit 1
fi

mkdir -p "$OUT_DIR"
converted=0

for media in *.mp4 *.avi *.mkv *.mov *.webm *.gif; do
    [ -f "$media" ] || continue
    filename=$(basename -- "$media")
    name="${filename%.*}"

    target="${OUT_DIR}/${name}.mjpeg"

    if [ -s "$target" ]; then
        echo "Skipping: '$media' already converted -> $target"
        continue
    fi

    echo "Converting: $media -> $target (${FPS} fps, q=${QUALITY})"

    tmp="${target}.tmp"
    if ffmpeg -y -loglevel error -stats -i "$media" \
        -an \
        -vf "fps=${FPS},scale=320:240:force_original_aspect_ratio=increase,crop=320:240" \
        -c:v mjpeg -pix_fmt yuvj420p -q:v "$QUALITY" \
        -f mjpeg "$tmp"; then
        mv -f "$tmp" "$target"
        echo "Done: $target"
        converted=$((converted + 1))
    else
        rm -f "$tmp"
        echo "FAILED: $media"
    fi
done

echo "All done! Converted $converted file(s) into ./$OUT_DIR/"
