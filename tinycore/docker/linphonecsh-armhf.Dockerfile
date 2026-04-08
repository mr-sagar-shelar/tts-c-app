FROM debian:bookworm

RUN dpkg --add-architecture armhf \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        dpkg-dev \
        findutils \
        file \
        binutils-arm-linux-gnueabihf \
        python3 \
        squashfs-tools \
    && apt-get install -y --no-install-recommends \
        linphone-cli:armhf \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work

RUN rm -rf /tmp/linphone-root /out \
    && mkdir -p /tmp/linphone-root/usr/local/bin \
        /tmp/linphone-root/usr/local/lib/linphone \
        /tmp/linphone-root/usr/local/share \
        /out

RUN if [ -x /usr/bin/linphonecsh ]; then \
        cp -a /usr/bin/linphonecsh /tmp/linphone-root/usr/local/lib/linphone/linphonecsh.real; \
    fi \
    && if [ -x /usr/bin/linphonec ]; then \
        cp -a /usr/bin/linphonec /tmp/linphone-root/usr/local/lib/linphone/linphonec.real; \
    fi \
    && if [ -d /usr/share/linphone ]; then \
        cp -a /usr/share/linphone /tmp/linphone-root/usr/local/share/linphone; \
    fi

RUN cat > /tmp/linphone-root/usr/local/bin/linphonecsh <<'EOF'
#!/bin/sh
BASE_DIR=/usr/local/lib/linphone
export LD_LIBRARY_PATH="$BASE_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$BASE_DIR/linphonecsh.real" "$@"
EOF

RUN chmod 0755 /tmp/linphone-root/usr/local/bin/linphonecsh

RUN cat > /tmp/linphone-root/usr/local/bin/linphonec <<'EOF'
#!/bin/sh
BASE_DIR=/usr/local/lib/linphone
export LD_LIBRARY_PATH="$BASE_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$BASE_DIR/linphonec.real" "$@"
EOF

RUN if [ -x /tmp/linphone-root/usr/local/lib/linphone/linphonec.real ]; then \
        chmod 0755 /tmp/linphone-root/usr/local/bin/linphonec; \
    else \
        rm -f /tmp/linphone-root/usr/local/bin/linphonec; \
    fi

RUN python3 - <<'PY'
import os
import shutil
import subprocess
from collections import deque

root = "/tmp/linphone-root/usr/local/lib/linphone"
search_dirs = [
    "/lib/arm-linux-gnueabihf",
    "/usr/lib/arm-linux-gnueabihf",
]
extra_dirs = [
    "/usr/lib/arm-linux-gnueabihf/mediastreamer",
    "/usr/lib/arm-linux-gnueabihf/mediastreamer/plugins",
    "/usr/lib/arm-linux-gnueabihf/liblinphone",
]

for directory in extra_dirs:
    if os.path.isdir(directory):
        for entry in os.listdir(directory):
            source = os.path.join(directory, entry)
            dest = os.path.join(root, entry)
            if os.path.isfile(source) and not os.path.exists(dest):
                shutil.copy2(source, dest)

queue = deque()
seen = set()

for entry in os.listdir(root):
    path = os.path.join(root, entry)
    if os.path.isfile(path):
        queue.append(path)

def needed(path):
    try:
        output = subprocess.check_output(
            ["arm-linux-gnueabihf-readelf", "-d", path],
            text=True,
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        return []
    result = []
    for line in output.splitlines():
        if "(NEEDED)" in line:
            start = line.find("[")
            end = line.find("]", start + 1)
            if start != -1 and end != -1:
                result.append(line[start + 1:end])
    return result

while queue:
    current = queue.popleft()
    if current in seen:
        continue
    seen.add(current)
    for libname in needed(current):
        destination = os.path.join(root, libname)
        if os.path.exists(destination):
            queue.append(destination)
            continue
        for directory in search_dirs:
            candidate = os.path.join(directory, libname)
            if os.path.exists(candidate):
                shutil.copy2(candidate, destination)
                queue.append(destination)
                break
PY

RUN find /tmp/linphone-root -type f \( -perm -111 -o -name '*.so' -o -name '*.so.*' \) \
        -exec arm-linux-gnueabihf-strip --strip-unneeded {} + || true

RUN mksquashfs /tmp/linphone-root /out/linphonecsh.tcz -noappend -all-root \
    && (cd /tmp/linphone-root && find usr -not -type d | sort > /out/linphonecsh.tcz.list)

RUN cat > /out/linphonecsh.tcz.dep <<'EOF'
alsa.tcz
alsa-plugins.tcz
ca-certificates.tcz
EOF

RUN cat > /out/linphonecsh.tcz.info <<'EOF'
Title:          linphonecsh.tcz
Description:    Linphone console SIP client packaged as a piCore extension for Sai VoIP testing.
Version:        5.1.65-4
Author:         Debian VoIP Team, repackaged locally
Original-site:  https://www.linphone.org/
Copying-policy: GPL
Size:           custom
Extension_by:   Codex
Tags:           voip sip linphone console sai
Comments:       Packs linphonecsh, linphonec, shared libraries, and Linphone data files
                into a self-contained Tiny Core extension built from Debian bookworm armhf packages.
                This is intended as a starting point for Raspberry Pi TinyCore testing.
Change-log:     2026/04/08 Initial local package recipe.
Current:        2026/04/08 Initial local package recipe.
EOF
