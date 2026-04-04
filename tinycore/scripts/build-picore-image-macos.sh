#!/bin/sh

set -eu

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-$ROOT_DIR/build/tinycore/artifacts}"
WORK_DIR="${WORK_DIR:-$ROOT_DIR/build/tinycore/image}"
TC_VERSION="${TC_VERSION:-16.x}"
TC_ARCH="${TC_ARCH:-armhf}"
TCE_NAME="${TCE_NAME:-tce}"
RELEASE_BASE_URL="${RELEASE_BASE_URL:-http://tinycorelinux.net/${TC_VERSION}/${TC_ARCH}/releases/RPi/}"
EXT_BASE_URL="${EXT_BASE_URL:-http://repo.tinycorelinux.net/${TC_VERSION}/${TC_ARCH}/tcz/}"
AUDIO_MODULES_EXT="${AUDIO_MODULES_EXT:-}"
OUTPUT_IMAGE="${OUTPUT_IMAGE:-$WORK_DIR/piCore-sai-custom.img}"

require_file() {
    [ -f "$1" ] || {
        printf 'Missing required file: %s\n' "$1" >&2
        exit 1
    }
}

download_if_missing() {
    url="$1"
    dest="$2"
    if [ ! -f "$dest" ]; then
        curl -fL "$url" -o "$dest"
    fi
}

append_unique_lines() {
    src="$1"
    dest="$2"
    [ -f "$src" ] || return 0
    touch "$dest"
    while IFS= read -r line; do
        [ -n "$line" ] || continue
        if ! grep -Fqx "$line" "$dest"; then
            printf '%s\n' "$line" >> "$dest"
        fi
    done < "$src"
}

append_cmdline_tokens() {
    src="$1"
    dest="$2"
    [ -f "$src" ] || return 0
    current="$(cat "$dest")"
    while IFS= read -r token; do
        [ -n "$token" ] || continue
        case " $current " in
            *" $token "*) ;;
            *)
                current="$current $token"
                ;;
        esac
    done < "$src"
    printf '%s\n' "$current" > "$dest"
}

download_extension_tree() {
    ext="$1"
    optional_dir="$2"

    case "$ext" in
        sai-app.tcz)
            cp "$ARTIFACT_DIR/sai-app.tcz" "$optional_dir/"
            cp "$ARTIFACT_DIR/sai-app.tcz.dep" "$optional_dir/"
            cp "$ARTIFACT_DIR/sai-app.tcz.info" "$optional_dir/"
            cp "$ARTIFACT_DIR/sai-app.tcz.list" "$optional_dir/"
            return 0
            ;;
    esac

    download_if_missing "${EXT_BASE_URL}${ext}" "$optional_dir/$ext"
    if curl -fsI "${EXT_BASE_URL}${ext}.dep" >/dev/null 2>&1; then
        download_if_missing "${EXT_BASE_URL}${ext}.dep" "$optional_dir/$ext.dep"
        while IFS= read -r dep; do
            [ -n "$dep" ] || continue
            download_extension_tree "$dep" "$optional_dir"
        done < "$optional_dir/$ext.dep"
    fi
    if curl -fsI "${EXT_BASE_URL}${ext}.md5.txt" >/dev/null 2>&1; then
        download_if_missing "${EXT_BASE_URL}${ext}.md5.txt" "$optional_dir/$ext.md5.txt"
    fi
}

discover_release_image() {
    curl -fsSL "$RELEASE_BASE_URL" \
        | grep -Eo 'piCore[^" ]+\.(img\.gz|zip)' \
        | head -n 1
}

discover_audio_modules_ext() {
    curl -fsSL "$EXT_BASE_URL" \
        | grep -Eo 'alsa-modules-[^" ]+piCore-v7[^" ]*\.tcz' \
        | sort -u \
        | tail -n 1
}

require_file "$ARTIFACT_DIR/sai-app.tcz"
require_file "$ARTIFACT_DIR/mydata.tgz"
require_file "$ARTIFACT_DIR/onboot.lst"
require_file "$ARTIFACT_DIR/config.txt.append"
require_file "$ARTIFACT_DIR/cmdline.append"

mkdir -p "$WORK_DIR"

RELEASE_IMAGE="${RELEASE_IMAGE:-$(discover_release_image)}"
[ -n "$RELEASE_IMAGE" ] || {
    printf 'Unable to discover a piCore image from %s\n' "$RELEASE_BASE_URL" >&2
    exit 1
}

ARCHIVE_PATH="$WORK_DIR/$RELEASE_IMAGE"
RAW_IMAGE="$WORK_DIR/$(basename "$RELEASE_IMAGE" .gz)"

download_if_missing "${RELEASE_BASE_URL}${RELEASE_IMAGE}" "$ARCHIVE_PATH"

case "$ARCHIVE_PATH" in
    *.img.gz)
        gunzip -c "$ARCHIVE_PATH" > "$RAW_IMAGE"
        ;;
    *.zip)
        unzip -o "$ARCHIVE_PATH" -d "$WORK_DIR" >/dev/null
        RAW_IMAGE="$(find "$WORK_DIR" -maxdepth 1 -name '*.img' | head -n 1)"
        ;;
    *.img)
        cp "$ARCHIVE_PATH" "$RAW_IMAGE"
        ;;
    *)
        printf 'Unsupported release artifact: %s\n' "$ARCHIVE_PATH" >&2
        exit 1
        ;;
esac

cp "$RAW_IMAGE" "$OUTPUT_IMAGE"

ATTACH_OUTPUT="$(hdiutil attach "$OUTPUT_IMAGE")"
DISK_DEV="$(printf '%s\n' "$ATTACH_OUTPUT" | awk '/^\/dev\/disk/ {print $1; exit}')"
BOOT_MOUNT="$(printf '%s\n' "$ATTACH_OUTPUT" | awk '/FAT_32|Windows_FAT_32|DOS_FAT_32/ {print $NF; exit}')"

[ -n "$DISK_DEV" ] || {
    printf 'Failed to attach image: %s\n' "$OUTPUT_IMAGE" >&2
    exit 1
}
[ -n "$BOOT_MOUNT" ] || {
    hdiutil detach "$DISK_DEV" >/dev/null 2>&1 || true
    printf 'Failed to mount FAT boot partition from %s\n' "$OUTPUT_IMAGE" >&2
    exit 1
}

trap 'hdiutil detach "$DISK_DEV" >/dev/null 2>&1 || true' EXIT

OPTIONAL_DIR="$BOOT_MOUNT/$TCE_NAME/optional"
ONBOOT_FILE="$BOOT_MOUNT/$TCE_NAME/onboot.lst"
mkdir -p "$OPTIONAL_DIR"

cp "$ARTIFACT_DIR/mydata.tgz" "$BOOT_MOUNT/mydata.tgz"
cp "$ARTIFACT_DIR/onboot.lst" "$ONBOOT_FILE"

if [ -z "$AUDIO_MODULES_EXT" ]; then
    AUDIO_MODULES_EXT="$(discover_audio_modules_ext || true)"
fi
if [ -n "$AUDIO_MODULES_EXT" ] && ! grep -Fqx "$AUDIO_MODULES_EXT" "$ONBOOT_FILE"; then
    printf '%s\n' "$AUDIO_MODULES_EXT" >> "$ONBOOT_FILE"
fi

while IFS= read -r ext; do
    [ -n "$ext" ] || continue
    download_extension_tree "$ext" "$OPTIONAL_DIR"
done < "$ONBOOT_FILE"

append_unique_lines "$ARTIFACT_DIR/config.txt.append" "$BOOT_MOUNT/config.txt"
append_cmdline_tokens "$ARTIFACT_DIR/cmdline.append" "$BOOT_MOUNT/cmdline.txt"

sync
hdiutil detach "$DISK_DEV" >/dev/null
trap - EXIT

printf 'Custom piCore image created at %s\n' "$OUTPUT_IMAGE"
