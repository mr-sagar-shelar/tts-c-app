#!/bin/sh

/usr/local/bin/sai-storage-init >/tmp/sai-storage-init.log 2>&1 || true

printf 'Applying GNU Unifont console font\n' > /tmp/sai-console-font.log
if command -v setfont >/dev/null 2>&1 && [ -r /usr/local/share/consolefonts/sai-unifont.psf.gz ]; then
    setfont /usr/local/share/consolefonts/sai-unifont.psf.gz >> /tmp/sai-console-font.log 2>&1 || true
else
    printf 'setfont or bundled Unifont console font not available\n' >> /tmp/sai-console-font.log
fi

/usr/local/bin/sai-audio-init >/tmp/sai-audio-init.log 2>&1 || true

if ! pgrep -f '/usr/local/bin/sai-platform-service' >/dev/null 2>&1; then
    /usr/local/bin/sai-platform-service >/tmp/sai-platform-service.log 2>&1 &
fi

if ! pgrep -f '/usr/local/bin/sai-autostart' >/dev/null 2>&1; then
    su tc -s /bin/sh -c 'setsid /usr/local/bin/sai-autostart </dev/tty1 >/dev/tty1 2>&1 &' >/dev/null 2>&1
fi
