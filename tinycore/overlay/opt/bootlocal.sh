#!/bin/sh

BOOT_LOG=/tmp/sai-bootlocal.log
FONT_LOG=/tmp/sai-console-font.log
FONT_PATH=/usr/local/share/consolefonts/sai-unifont.psf.gz
FONT_TOOL=""

log() {
    printf '%s\n' "$*" >> "$BOOT_LOG"
}

find_font_tool() {
    for tool in setfont loadfont /usr/bin/setfont /bin/setfont /usr/sbin/setfont /sbin/setfont /usr/bin/loadfont /bin/loadfont; do
        if command -v "$tool" >/dev/null 2>&1; then
            command -v "$tool"
            return 0
        fi
        if [ -x "$tool" ]; then
            printf '%s\n' "$tool"
            return 0
        fi
    done
    return 1
}

: > "$BOOT_LOG"
log "bootlocal start"

/usr/local/bin/sai-storage-init >/tmp/sai-storage-init.log 2>&1 || log "sai-storage-init exited with a non-zero status"

: > "$FONT_LOG"
printf 'Applying GNU Unifont console font\n' >> "$FONT_LOG"
FONT_TOOL="$(find_font_tool || true)"
if [ -n "$FONT_TOOL" ] && [ -r "$FONT_PATH" ]; then
    printf 'Using font tool: %s\n' "$FONT_TOOL" >> "$FONT_LOG"
    case "$(basename "$FONT_TOOL")" in
        loadfont)
            "$FONT_TOOL" < "$FONT_PATH" >> "$FONT_LOG" 2>&1 || printf 'Font tool failed for %s\n' "$FONT_TOOL" >> "$FONT_LOG"
            ;;
        *)
            "$FONT_TOOL" "$FONT_PATH" >> "$FONT_LOG" 2>&1 || printf 'Font tool failed for %s\n' "$FONT_TOOL" >> "$FONT_LOG"
            ;;
    esac
else
    printf 'Console font tool or bundled Unifont console font not available\n' >> "$FONT_LOG"
    printf 'FONT_TOOL=%s\n' "${FONT_TOOL:-missing}" >> "$FONT_LOG"
    printf 'FONT_PATH=%s\n' "$FONT_PATH" >> "$FONT_LOG"
    if [ -r "$FONT_PATH" ]; then
        printf 'Bundled font file exists\n' >> "$FONT_LOG"
    else
        printf 'Bundled font file is missing\n' >> "$FONT_LOG"
    fi
fi

/usr/local/bin/sai-audio-init >/tmp/sai-audio-init.log 2>&1 || log "sai-audio-init exited with a non-zero status"

if ! pgrep -f '/usr/local/bin/sai-platform-service' >/dev/null 2>&1; then
    /usr/local/bin/sai-platform-service >/tmp/sai-platform-service.log 2>&1 &
    log "started sai-platform-service"
fi

if ! pgrep -f '/usr/local/bin/sai-autostart' >/dev/null 2>&1; then
    su tc -s /bin/sh -c 'setsid /usr/local/bin/sai-autostart </dev/tty1 >/dev/tty1 2>&1 &' >/dev/null 2>&1
    log "started sai-autostart"
fi
