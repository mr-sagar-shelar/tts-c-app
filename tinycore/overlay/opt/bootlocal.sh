#!/bin/sh

/usr/local/bin/sai-storage-init >/tmp/sai-storage-init.log 2>&1 || true

printf 'Applying US console keymap\n' > /tmp/sai-keymap-init.log
if command -v loadkmap >/dev/null 2>&1 && [ -r /usr/share/kmap/us.kmap ]; then
    loadkmap < /usr/share/kmap/us.kmap >> /tmp/sai-keymap-init.log 2>&1 || true
else
    printf 'loadkmap or /usr/share/kmap/us.kmap not available\n' >> /tmp/sai-keymap-init.log
fi

/usr/local/bin/sai-audio-init >/tmp/sai-audio-init.log 2>&1 || true

if ! pgrep -f '/usr/local/bin/sai-autostart' >/dev/null 2>&1; then
    su tc -s /bin/sh -c 'setsid /usr/local/bin/sai-autostart </dev/tty1 >/dev/tty1 2>&1 &' >/dev/null 2>&1
fi
