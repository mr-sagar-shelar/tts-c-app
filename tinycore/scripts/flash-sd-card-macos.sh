#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
    printf 'Usage: %s /path/to/custom.img diskN\n' "$0" >&2
    exit 1
fi

IMAGE_PATH="$1"
DISK_NAME="$2"

[ -f "$IMAGE_PATH" ] || {
    printf 'Image not found: %s\n' "$IMAGE_PATH" >&2
    exit 1
}

case "$DISK_NAME" in
    disk*) ;;
    *)
        printf 'Second argument must be a raw macOS disk identifier like disk4\n' >&2
        exit 1
        ;;
esac

diskutil unmountDisk "/dev/$DISK_NAME"
sudo dd if="$IMAGE_PATH" of="/dev/r$DISK_NAME" bs=4m
sync
diskutil eject "/dev/$DISK_NAME"

printf 'Flashed %s to /dev/%s\n' "$IMAGE_PATH" "$DISK_NAME"
