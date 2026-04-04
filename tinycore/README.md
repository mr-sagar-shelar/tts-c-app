# Tiny Core / piCore build for Raspberry Pi Zero 2 W

This folder turns the current repository into a reproducible piCore appliance build.

The flow is:

1. Cross-build the app and Flite for `armhf` inside Docker on macOS.
2. Package the result as a Tiny Core extension: `sai-app.tcz`.
3. Create a `mydata.tgz` containing the boot-time autostart hook.
4. Download a stock piCore Raspberry Pi image and inject:
   - `sai-app.tcz`
   - required runtime extensions
   - `mydata.tgz`
   - boot config snippets for console/audio startup
5. Optionally flash the generated image to an SD card from macOS.

## Why this layout

- The app is installed read-only into `/usr/local/share/sai-base`.
- Writable state lives in the Tiny Core `tce` area, so user settings, downloaded voices, notes, and sample files persist across reboots.
- Boot starts the app automatically on `tty1`, which keeps keyboard navigation working exactly like the current terminal UI.

## Files

- `docker/armhf-cross.Dockerfile`
  Builds `sai`, packages `sai-app.tcz`, and emits `mydata.tgz`.
- `scripts/build-artifacts.sh`
  Runs the Docker build on macOS/Linux and exports the Tiny Core artifacts.
- `scripts/build-picore-image-macos.sh`
  Downloads a stock piCore Raspberry Pi image, injects app/extensions, and writes a custom image.
- `scripts/flash-sd-card-macos.sh`
  Writes the generated custom image to an SD card on macOS.
- `overlay/opt/bootlocal.sh`
  Tiny Core boot hook that starts audio and launches the app at boot.
- `rootfs/usr/local/bin/*`
  Launch helpers included in the custom `sai-app.tcz`.

## Runtime extension set

The app itself shells out to a few external tools, so the image preloads:

- `SDL2.tcz`
- `alsa.tcz`
- `alsa-utils.tcz`
- `alsa-plugins.tcz`
- `curl.tcz`
- `unzip.tcz`
- `mpg123.tcz`
- `ca-certificates.tcz`

The image build script also tries to auto-add a matching `alsa-modules-...tcz` for the chosen piCore release. If the repo layout changes, set `AUDIO_MODULES_EXT` manually when running the script.

## Audio assumptions

- Raspberry Pi Zero 2 W has no built-in 3.5mm audio jack.
- The boot config enables Raspberry Pi audio and forces HDMI audio.
- For spoken output, use one of:
  - mini-HDMI audio
  - a USB audio adapter
  - a PWM/I2S audio HAT that exposes an ALSA device

The default `asound.conf` points to card `0`. If your device enumerates differently, update the generated image or override `/usr/local/etc/asound.conf`.

## macOS commands

Build Tiny Core artifacts:

```sh
./tinycore/scripts/build-artifacts.sh
```

Build a custom piCore image:

```sh
./tinycore/scripts/build-picore-image-macos.sh
```

Write that image to an SD card:

```sh
./tinycore/scripts/flash-sd-card-macos.sh build/tinycore/image/piCore-sai-custom.img disk4
```

`disk4` is an example only. Always confirm the target with `diskutil list` first.

## Sources used

- Tiny Core installation guidance: [piCore Prepare SD-Card](https://wiki.tinycorelinux.net/doku.php?id=picore:prepsd)
- Tiny Core extension packaging guidance: [Creating Extensions](https://wiki.tinycorelinux.net/doku.php?id=wiki:creating_extensions)
- Tiny Core extension repo examples including `SDL2`, `alsa`, `curl`, `unzip`, and `mpg123`: [armv7/armhf extension listings](https://tinycorelinux.net/9.x/armv7/tcz/)
