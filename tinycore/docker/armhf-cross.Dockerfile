FROM debian:bookworm

RUN dpkg --add-architecture armhf \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        file \
        findutils \
        gcc-arm-linux-gnueabihf \
        g++-arm-linux-gnueabihf \
        binutils-arm-linux-gnueabihf \
        libc6-dev-armhf-cross \
        make \
        rsync \
        squashfs-tools \
        tar \
        unzip \
        zip \
        libsdl2-dev:armhf \
        libasound2-dev:armhf \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

ARG TARGET_PLATFORM=arm-linux-gnueabihf
ENV CC=arm-linux-gnueabihf-gcc
ENV AR=arm-linux-gnueabihf-ar
ENV RANLIB=arm-linux-gnueabihf-ranlib
ENV STRIP=arm-linux-gnueabihf-strip
ENV SDL_CFLAGS=-I/usr/include/SDL2\ -D_REENTRANT
ENV SDL_LIBS=-L/usr/lib/arm-linux-gnueabihf\ -lSDL2

RUN make clean \
    && make \
        ENABLE_MENU_SPEECH=1 \
        CC="$CC" \
        AR="$AR" \
        RANLIB="$RANLIB" \
        TARGET_PLATFORM="$TARGET_PLATFORM" \
        SDL_CFLAGS="$SDL_CFLAGS" \
        SDL_LIBS="$SDL_LIBS"

RUN rm -rf /tmp/sai-package /tmp/mydata /out \
    && mkdir -p /tmp/sai-package /tmp/mydata /out \
    && make install-rootfs \
        DESTDIR=/tmp/sai-package \
        ENABLE_MENU_SPEECH=1 \
        CC="$CC" \
        AR="$AR" \
        RANLIB="$RANLIB" \
        TARGET_PLATFORM="$TARGET_PLATFORM" \
        SDL_CFLAGS="$SDL_CFLAGS" \
        SDL_LIBS="$SDL_LIBS" \
    && install -d /tmp/sai-package/usr/local/bin /tmp/sai-package/usr/local/etc \
    && install -m 0755 tinycore/rootfs/usr/local/bin/sai-audio-init /tmp/sai-package/usr/local/bin/sai-audio-init \
    && install -m 0755 tinycore/rootfs/usr/local/bin/sai-autostart /tmp/sai-package/usr/local/bin/sai-autostart \
    && install -m 0755 tinycore/rootfs/usr/local/bin/sai-launch /tmp/sai-package/usr/local/bin/sai-launch \
    && install -m 0755 tinycore/rootfs/usr/local/bin/sai-restart /tmp/sai-package/usr/local/bin/sai-restart \
    && install -m 0755 tinycore/rootfs/usr/local/bin/sai-storage-init /tmp/sai-package/usr/local/bin/sai-storage-init \
    && install -m 0755 tinycore/rootfs/usr/local/bin/sai-wifi-service /tmp/sai-package/usr/local/bin/sai-wifi-service \
    && install -m 0644 tinycore/overlay/usr/local/etc/asound.conf /tmp/sai-package/usr/local/etc/asound.conf

RUN find /tmp/sai-package -type f \( -perm -111 -o -name '*.so' -o -name '*.so.*' \) -exec "$STRIP" --strip-unneeded {} + || true

RUN mksquashfs /tmp/sai-package /out/sai-app.tcz -noappend -all-root \
    && (cd /tmp/sai-package && find usr -not -type d | sort > /out/sai-app.tcz.list) \
    && cat > /out/sai-app.tcz.dep <<'EOF'
SDL2.tcz
alsa.tcz
alsa-utils.tcz
alsa-plugins.tcz
e2fsprogs.tcz
curl.tcz
unzip.tcz
mpg123.tcz
ca-certificates.tcz
util-linux.tcz
kmaps.tcz
wireless_tools.tcz
wpa_supplicant.tcz
firmware-rpi-wifi.tcz
wireless-KERNEL.tcz
EOF

RUN cat > /out/sai-app.tcz.info <<'EOF'
Title:          sai-app.tcz
Description:    Menu-driven speech-first appliance build of this repository for piCore on Raspberry Pi.
Version:        0.1
Author:         Local project build
Original-site:  https://github.com/
Copying-policy: Mixed
Size:           custom
Extension_by:   Codex
Tags:           speech console accessibility flite sdl2 raspberrypi
Comments:       Includes the root C application, vendored Flite shared libraries,
                boot launch helpers, and default sample content.
                Runtime state is stored in the writable tce area as /tce/sai-data.
                Intended for piCore armhf appliances on Raspberry Pi Zero 2 W.
Change-log:     2026/04/04 Initial local appliance package.
Current:        2026/04/04 Initial local appliance package.
EOF

RUN mkdir -p /tmp/mydata/opt \
    && install -m 0755 tinycore/overlay/opt/bootlocal.sh /tmp/mydata/opt/bootlocal.sh \
    && (cd /tmp/mydata && tar -czf /out/mydata.tgz .) \
    && cp tinycore/extensions/onboot.lst /out/onboot.lst \
    && cp tinycore/config/config.txt.append /out/config.txt.append \
    && cp tinycore/config/cmdline.append /out/cmdline.append
