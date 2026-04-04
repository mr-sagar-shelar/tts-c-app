#!/bin/sh

/usr/local/bin/sai-audio-init >/tmp/sai-audio-init.log 2>&1 || true

if ! pgrep -f '/usr/local/bin/sai-autostart' >/dev/null 2>&1; then
    su tc -s /bin/sh -c 'setsid /usr/local/bin/sai-autostart </dev/tty1 >/dev/tty1 2>&1 &' >/dev/null 2>&1
fi
