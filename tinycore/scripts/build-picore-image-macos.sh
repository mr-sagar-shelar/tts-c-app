#!/bin/sh

set -eu

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
ARTIFACT_DIR="${ARTIFACT_DIR:-$ROOT_DIR/build/tinycore/artifacts}"
WORK_DIR="${WORK_DIR:-$ROOT_DIR/build/tinycore/image}"
TC_VERSION="${TC_VERSION:-15.x}"
TC_ARCH="${TC_ARCH:-armhf}"
TCE_NAME="${TCE_NAME:-tce}"
RELEASE_BASE_URL_DEFAULT="http://tinycorelinux.net/${TC_VERSION}/${TC_ARCH}/releases/RPi/"
EXT_BASE_URL_DEFAULT="http://repo.tinycorelinux.net/${TC_VERSION}/${TC_ARCH}/tcz/"
RELEASE_BASE_URL="${RELEASE_BASE_URL:-$RELEASE_BASE_URL_DEFAULT}"
EXT_BASE_URL="${EXT_BASE_URL:-$EXT_BASE_URL_DEFAULT}"
AUDIO_MODULES_EXT="${AUDIO_MODULES_EXT:-}"
OUTPUT_IMAGE="${OUTPUT_IMAGE:-$WORK_DIR/piCore-sai-custom-${TC_VERSION}-${TC_ARCH}.img}"
RELEASE_BASE_URL_RESOLVED=""
EXT_BASE_URL_RESOLVED=""
RELEASE_IMAGE_DISCOVERED=""
AUDIO_MODULES_EXT_DISCOVERED=""
RELEASE_VERSION_RESOLVED=""

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

resolve_extension_name() {
    ext="$1"

    if curl -fsI "${EXT_BASE_URL}${ext}" >/dev/null 2>&1; then
        printf '%s\n' "$ext"
        return 0
    fi

    case "$ext" in
        SDL2.tcz)
            if curl -fsI "${EXT_BASE_URL}sdl2.tcz" >/dev/null 2>&1; then
                printf '%s\n' "sdl2.tcz"
                return 0
            fi
            ;;
        sdl2.tcz)
            if curl -fsI "${EXT_BASE_URL}SDL2.tcz" >/dev/null 2>&1; then
                printf '%s\n' "SDL2.tcz"
                return 0
            fi
            ;;
    esac

    return 1
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

extract_hdiutil_value() {
    plist_file="$1"
    key="$2"
    index=0

    while :; do
        value="$(/usr/libexec/PlistBuddy -c "Print :system-entities:${index}:${key}" "$plist_file" 2>/dev/null || true)"
        if [ -n "$value" ]; then
            printf '%s\n' "$value"
            return 0
        fi
        next_exists="$(/usr/libexec/PlistBuddy -c "Print :system-entities:${index}" "$plist_file" 2>/dev/null || true)"
        [ -n "$next_exists" ] || break
        index=$((index + 1))
    done

    return 1
}

extract_boot_mount() {
    plist_file="$1"
    index=0

    while :; do
        entity_exists="$(/usr/libexec/PlistBuddy -c "Print :system-entities:${index}" "$plist_file" 2>/dev/null || true)"
        [ -n "$entity_exists" ] || break

        content_hint="$(/usr/libexec/PlistBuddy -c "Print :system-entities:${index}:content-hint" "$plist_file" 2>/dev/null || true)"
        mount_point="$(/usr/libexec/PlistBuddy -c "Print :system-entities:${index}:mount-point" "$plist_file" 2>/dev/null || true)"

        case "$content_hint" in
            DOS_FAT_32|Windows_FAT_32|MS-DOS*)
                if [ -n "$mount_point" ]; then
                    printf '%s\n' "$mount_point"
                    return 0
                fi
                ;;
        esac

        index=$((index + 1))
    done

    return 1
}

download_extension_tree() {
    ext="$1"
    optional_dir="$2"
    repo_ext="$ext"

    case "$ext" in
        alsa-modules-KERNEL.tcz)
            if [ -z "$AUDIO_MODULES_EXT" ]; then
                discover_audio_modules_ext || true
                AUDIO_MODULES_EXT="${AUDIO_MODULES_EXT_DISCOVERED:-}"
                if [ -n "$EXT_BASE_URL_RESOLVED" ]; then
                    EXT_BASE_URL="$EXT_BASE_URL_RESOLVED"
                fi
            fi
            if [ -n "$AUDIO_MODULES_EXT" ]; then
                ext="$AUDIO_MODULES_EXT"
                repo_ext="$AUDIO_MODULES_EXT"
            else
                printf 'Unable to resolve Tiny Core extension: %s from %s\n' "alsa-modules-KERNEL.tcz" "$EXT_BASE_URL" >&2
                return 1
            fi
            ;;
    esac

    case "$ext" in
        sai-app.tcz)
            cp "$ARTIFACT_DIR/sai-app.tcz" "$optional_dir/"
            cp "$ARTIFACT_DIR/sai-app.tcz.dep" "$optional_dir/"
            cp "$ARTIFACT_DIR/sai-app.tcz.info" "$optional_dir/"
            cp "$ARTIFACT_DIR/sai-app.tcz.list" "$optional_dir/"
            return 0
            ;;
    esac

    repo_ext="$(resolve_extension_name "$ext")" || {
        printf 'Unable to resolve Tiny Core extension: %s from %s\n' "$ext" "$EXT_BASE_URL" >&2
        return 1
    }
    download_if_missing "${EXT_BASE_URL}${repo_ext}" "$optional_dir/$repo_ext"
    if [ "$repo_ext" != "$ext" ] && [ ! -e "$optional_dir/$ext" ]; then
        cp "$optional_dir/$repo_ext" "$optional_dir/$ext"
    fi
    if curl -fsI "${EXT_BASE_URL}${repo_ext}.dep" >/dev/null 2>&1; then
        download_if_missing "${EXT_BASE_URL}${repo_ext}.dep" "$optional_dir/$repo_ext.dep"
        if [ "$repo_ext" != "$ext" ] && [ ! -e "$optional_dir/$ext.dep" ]; then
            cp "$optional_dir/$repo_ext.dep" "$optional_dir/$ext.dep"
        fi
        while IFS= read -r dep; do
            [ -n "$dep" ] || continue
            download_extension_tree "$dep" "$optional_dir"
        done < "$optional_dir/$repo_ext.dep"
    fi
    if curl -fsI "${EXT_BASE_URL}${repo_ext}.md5.txt" >/dev/null 2>&1; then
        download_if_missing "${EXT_BASE_URL}${repo_ext}.md5.txt" "$optional_dir/$repo_ext.md5.txt"
        if [ "$repo_ext" != "$ext" ] && [ ! -e "$optional_dir/$ext.md5.txt" ]; then
            cp "$optional_dir/$repo_ext.md5.txt" "$optional_dir/$ext.md5.txt"
        fi
    fi
}

discover_release_image() {
    for base_url in \
        "$RELEASE_BASE_URL" \
        "http://tinycorelinux.net/17.x/${TC_ARCH}/releases/RPi/" \
        "http://tinycorelinux.net/16.x/${TC_ARCH}/releases/RPi/" \
        "http://tinycorelinux.net/15.x/${TC_ARCH}/releases/RPi/" \
        "http://tinycorelinux.net/14.x/${TC_ARCH}/releases/RPi/"
    do
        image="$(curl -fsSL "$base_url" 2>/dev/null \
            | grep -Eo 'piCore[^" ]+\.(img\.gz|zip)' \
            | head -n 1 || true)"
        if [ -n "$image" ]; then
            RELEASE_BASE_URL_RESOLVED="$base_url"
            RELEASE_VERSION_RESOLVED="$(printf '%s\n' "$base_url" | sed -E 's#.*tinycorelinux.net/([^/]+)/.*#\1#')"
            RELEASE_IMAGE_DISCOVERED="$image"
            return 0
        fi
    done

    return 1
}

discover_audio_modules_ext() {
    for base_url in \
        "$EXT_BASE_URL" \
        "http://repo.tinycorelinux.net/17.x/${TC_ARCH}/tcz/" \
        "http://repo.tinycorelinux.net/16.x/${TC_ARCH}/tcz/" \
        "http://repo.tinycorelinux.net/15.x/${TC_ARCH}/tcz/" \
        "http://repo.tinycorelinux.net/14.x/${TC_ARCH}/tcz/"
    do
        ext="$(curl -fsSL "$base_url" 2>/dev/null \
            | grep -Eo 'alsa-modules-[^" ]+piCore-v7[^" ]*\.tcz' \
            | sort -u \
            | tail -n 1 || true)"
        if [ -n "$ext" ]; then
            EXT_BASE_URL_RESOLVED="$base_url"
            AUDIO_MODULES_EXT_DISCOVERED="$ext"
            return 0
        fi
    done

    return 1
}

require_file "$ARTIFACT_DIR/sai-app.tcz"
require_file "$ARTIFACT_DIR/mydata.tgz"
require_file "$ARTIFACT_DIR/onboot.lst"
require_file "$ARTIFACT_DIR/config.txt.append"
require_file "$ARTIFACT_DIR/cmdline.append"

mkdir -p "$WORK_DIR"

if [ -z "${RELEASE_IMAGE:-}" ]; then
    discover_release_image || true
    RELEASE_IMAGE="${RELEASE_IMAGE_DISCOVERED:-}"
fi
[ -n "$RELEASE_IMAGE" ] || {
    printf 'Unable to discover a piCore image from %s\n' "$RELEASE_BASE_URL" >&2
    exit 1
}

if [ -n "$RELEASE_BASE_URL_RESOLVED" ]; then
    RELEASE_BASE_URL="$RELEASE_BASE_URL_RESOLVED"
fi
if [ -n "$RELEASE_VERSION_RESOLVED" ] && [ "$EXT_BASE_URL" = "$EXT_BASE_URL_DEFAULT" ]; then
    EXT_BASE_URL="http://repo.tinycorelinux.net/${RELEASE_VERSION_RESOLVED}/${TC_ARCH}/tcz/"
fi

printf 'Using piCore release %s from %s\n' "$RELEASE_IMAGE" "$RELEASE_BASE_URL"
if [ -n "$RELEASE_VERSION_RESOLVED" ] && [ "$RELEASE_VERSION_RESOLVED" != "$TC_VERSION" ]; then
    printf 'Requested %s but using fallback release %s for %s\n' "$TC_VERSION" "$RELEASE_VERSION_RESOLVED" "$TC_ARCH"
fi

ARCHIVE_PATH="$WORK_DIR/$RELEASE_IMAGE"
EXTRACT_DIR="$WORK_DIR/extracted-${TC_VERSION}-${TC_ARCH}"
RAW_IMAGE=""

download_if_missing "${RELEASE_BASE_URL}${RELEASE_IMAGE}" "$ARCHIVE_PATH"
rm -rf "$EXTRACT_DIR"
mkdir -p "$EXTRACT_DIR"

case "$ARCHIVE_PATH" in
    *.img.gz)
        RAW_IMAGE="$EXTRACT_DIR/$(basename "$RELEASE_IMAGE" .gz)"
        gunzip -c "$ARCHIVE_PATH" > "$RAW_IMAGE"
        ;;
    *.zip)
        unzip -o "$ARCHIVE_PATH" -d "$EXTRACT_DIR" >/dev/null
        RAW_IMAGE="$(find "$EXTRACT_DIR" -maxdepth 2 -name '*.img' | sort | head -n 1)"
        ;;
    *.img)
        RAW_IMAGE="$EXTRACT_DIR/$(basename "$RELEASE_IMAGE")"
        cp "$ARCHIVE_PATH" "$RAW_IMAGE"
        ;;
    *)
        printf 'Unsupported release artifact: %s\n' "$ARCHIVE_PATH" >&2
        exit 1
        ;;
esac

[ -n "$RAW_IMAGE" ] && [ -f "$RAW_IMAGE" ] || {
    printf 'Unable to locate extracted piCore image for %s\n' "$ARCHIVE_PATH" >&2
    exit 1
}

if [ "$RAW_IMAGE" != "$OUTPUT_IMAGE" ]; then
    cp "$RAW_IMAGE" "$OUTPUT_IMAGE"
fi

ATTACH_PLIST="$(mktemp -t sai-hdiutil-attach.XXXXXX.plist)"
hdiutil attach -plist "$OUTPUT_IMAGE" > "$ATTACH_PLIST"
DISK_DEV="$(extract_hdiutil_value "$ATTACH_PLIST" "dev-entry" | head -n 1)"
BOOT_MOUNT="$(extract_boot_mount "$ATTACH_PLIST" || true)"
rm -f "$ATTACH_PLIST"

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
    discover_audio_modules_ext || true
    AUDIO_MODULES_EXT="${AUDIO_MODULES_EXT_DISCOVERED:-}"
fi
if [ -n "$EXT_BASE_URL_RESOLVED" ]; then
    EXT_BASE_URL="$EXT_BASE_URL_RESOLVED"
fi
printf 'Using Tiny Core extensions from %s\n' "$EXT_BASE_URL"
if [ -n "$AUDIO_MODULES_EXT" ] && ! grep -Fqx "$AUDIO_MODULES_EXT" "$ONBOOT_FILE"; then
    printf '%s\n' "$AUDIO_MODULES_EXT" >> "$ONBOOT_FILE"
fi
sed -i '' '/^alsa-modules-KERNEL\.tcz$/d' "$ONBOOT_FILE" 2>/dev/null || true

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
