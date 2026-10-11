#!/bin/bash

for media in *.mp4 *.avi *.mkv *.mov *.webm *.gif; do
    [ -e "$media" ] || continue
    filename=$(basename -- "$media")
    name="${filename%.*}"

    target_dir="anim_${name}"

    if [ -d "$target_dir" ] && [ "$(ls -A "$target_dir"/*.jpg 2>/dev/null)" ]; then
        echo "Skipping: '$media' already exists."
        continue
    fi

    mkdir -p "$target_dir"

    if [[ "$media" == *.gif ]]; then
        echo "Extracting frames from GIF: $media -> $target_dir/"
        ffmpeg -y -i "$media" -fps_mode passthrough -vf "scale=320:240:force_original_aspect_ratio=increase,crop=320:240" -q:v 3 "${target_dir}/%04d.jpg"
    else
        echo "Splitting video into JPG frames: $media -> $target_dir/"
        ffmpeg -y -i "$media" -vf "fps=15,scale=320:240:force_original_aspect_ratio=increase,crop=320:240" -q:v 3 "${target_dir}/%04d.jpg"
    fi

    echo "Done: $target_dir"
done

echo "All done!"
