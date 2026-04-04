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
- The generated image does not use `copy2fs`, so extensions stay mounted from the SD card instead of being copied wholesale into RAM. That keeps boot lighter and matches your "regular OS on SD card" goal as closely as Tiny Core allows.
- The packaged seed content is intentionally minimal so the app becomes interactive quickly after boot.
- No voices or sample user content are preloaded in the extension. Users download voices after the app starts.

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
- `e2fsprogs.tcz`
- `curl.tcz`
- `unzip.tcz`
- `mpg123.tcz`
- `ca-certificates.tcz`
- `util-linux.tcz`

The image build script also tries to auto-add a matching `alsa-modules-...tcz` for the chosen piCore release. If the repo layout changes, set `AUDIO_MODULES_EXT` manually when running the script.

## Boot and startup behavior

- piCore itself still uses a RAM-based core design, but this build keeps the large pieces on the SD card:
  - extensions are mounted from `tce/optional`
  - user state is persisted in `tce/sai-data`
  - no `copy2fs.flg` is added
- On the first boot after flashing, `sai-storage-init` expands partition 2 to fill the SD card, grows the ext filesystem, updates `cmdline.txt` to use that partition for `tce`, `home`, and `opt`, and then reboots.
- Because of that resize flow, the very first power-on should be treated as a setup boot. Expect up to two automatic reboots before Sai reaches its normal startup path.
- `bootlocal.sh` starts audio setup and then launches Sai on `tty1`.
- `sai-autostart` uses a 1 second default delay to let the boot console settle before input begins.
- On the first spoken prompt after launch, the app says `Sai is ready` before announcing the current menu item.

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

This writes a versioned image such as:

```sh
build/tinycore/image/piCore-sai-custom-15.x-armhf.img
```

Write that image to an SD card:

```sh
./tinycore/scripts/flash-sd-card-macos.sh build/tinycore/image/piCore-sai-custom-15.x-armhf.img disk4
```

`disk4` is an example only. Always confirm the target with `diskutil list` first.

## After The Image Is Ready

1. Build the artifacts:

```sh
./tinycore/scripts/build-artifacts.sh
```

2. Build the custom piCore image:

```sh
TC_VERSION=15.x ./tinycore/scripts/build-picore-image-macos.sh
```

3. Verify the resulting image exists:

```sh
ls -lh build/tinycore/image/piCore-sai-custom-15.x-armhf.img
```

4. Flash it to the SD card:

```sh
diskutil list
./tinycore/scripts/flash-sd-card-macos.sh build/tinycore/image/piCore-sai-custom-15.x-armhf.img disk4
```

5. Safely eject the SD card and insert it into the Raspberry Pi Zero 2 W.

## What The Image Builder Downloads

When you run:

```sh
./tinycore/scripts/build-picore-image-macos.sh
```

the script downloads two groups of files:

1. The base piCore release image archive.
2. Tiny Core extension files needed by the custom image:
   - `.tcz`
   - `.dep`
   - `.md5.txt`

Local storage locations:

- Base piCore release archives and extracted images:
  - `build/tinycore/image/`
- Persistent extension cache reused across builds:
  - `build/tinycore/cache/<release>-<arch>/tcz/`

Examples:

- `build/tinycore/image/piCore-15.0.0.zip`
- `build/tinycore/image/extracted-15.x-armhf/piCore-15.0.0.img`
- `build/tinycore/cache/15.x-armhf/tcz/SDL2.tcz`
- `build/tinycore/cache/15.x-armhf/tcz/alsa.tcz.dep`

## Download Reuse And Optimization

The image builder now avoids re-downloading files when they already exist locally:

- The base piCore archive is reused from `build/tinycore/image/`.
- Tiny Core extensions are reused from `build/tinycore/cache/...`.
- On later runs, cached files are copied into the mounted image instead of being fetched again.

Because of this, the first build downloads the most data and later builds should be much faster.

If you want to force a fresh extension download, remove the cache directory for that release:

```sh
rm -rf build/tinycore/cache/15.x-armhf
```

## Hardware Before First Boot

Connect:

- Raspberry Pi Zero 2 W
- flashed microSD card
- USB keyboard through the appropriate OTG adapter
- audio output device:
  - mini-HDMI audio, or
  - USB audio adapter, or
  - supported I2S/PWM audio hardware
- stable power supply

For the very first boot, use a monitor if possible. The first-boot resize path may reboot automatically and visual feedback helps confirm progress.

## Initial Boot Sequence

On the first boot after flashing, the image performs setup in this order:

1. piCore boots from the SD card.
2. `sai-storage-init` resizes partition 2 to consume the remaining SD card space.
3. The Pi reboots automatically.
4. On the next boot, `sai-storage-init` grows the ext filesystem on partition 2, creates persistent `tce`, `home`, and `opt` directories there, updates `/mnt/mmcblk0p1/cmdline.txt`, and reboots again.
5. On the final boot, piCore uses the expanded partition 2 for persistence and Sai launches automatically on `tty1`.
6. When audio is working and the app is ready, Sai speaks: `Sai is ready`.

If the device is powered off during these first two setup reboots, simply power it on again and let the sequence finish.

## First-Boot Checks

Once the Pi has settled and Sai is running:

- Confirm that the keyboard moves through the menu.
- Confirm that speech output works through the selected audio device.
- If speech is not yet available, check the audio path first, then review:

```sh
cat /tmp/sai-audio-init.log
cat /tmp/sai-storage-init.log
```

- Confirm the persistent storage location:

```sh
cat /opt/.tce_dir
```

The expected result after the first-boot storage setup is a path on `/mnt/mmcblk0p2/tce`.

## Recommended Post-Boot Steps

After the first successful launch:

1. Download the voice files you want from the app's voice management flow.
2. Reboot once manually to confirm the downloaded voices and settings persist.
3. If Wi-Fi is needed later, follow Tiny Core's piCore Wi-Fi guidance and store the related configuration in persistent storage.

## Recovery Notes

If the first-boot storage expansion does not finish cleanly:

- attach HDMI and keyboard
- let the Pi boot fully
- inspect:

```sh
cat /tmp/sai-storage-fdisk.log
cat /tmp/sai-storage-e2fsck.log
cat /tmp/sai-storage-resize2fs.log
cat /tmp/sai-storage-init.log
```

- inspect the stage marker:

```sh
sudo mount /dev/mmcblk0p1 /mnt/mmcblk0p1
cat /mnt/mmcblk0p1/tce/.sai-storage-stage
```

If needed, you can delete that stage file and retry the first-boot expansion flow on the next reboot.

## Sources used

- Tiny Core installation guidance: [piCore Prepare SD-Card](https://wiki.tinycorelinux.net/doku.php?id=picore:prepsd)
- Tiny Core extension packaging guidance: [Creating Extensions](https://wiki.tinycorelinux.net/doku.php?id=wiki:creating_extensions)
- Tiny Core extension repo examples including `SDL2`, `alsa`, `curl`, `unzip`, `util-linux`, and `e2fsprogs`: [armv7/armhf extension listings](https://tinycorelinux.net/9.x/armv7/tcz/)
- Tiny Core persistence guidance for `tce`, `home`, and `opt`: [Persistent Home](https://wiki.tinycorelinux.net/doku.php?id=wiki:persistent_home)
- Tiny Core boot code guidance: [Boot Codes Explained](https://wiki.tinycorelinux.net/doku.php?id=wiki:boot_codes_explained)
