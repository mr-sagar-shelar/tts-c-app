# Raspberry Pi Terminal Checks

This file lists the terminal commands that Sai currently depends on or closely relates to on Raspberry Pi with TinyCore Linux.

The idea is:

1. run these commands directly on the Raspberry Pi
2. verify each command works in the terminal first
3. only then rebuild and redeploy Sai if needed

That saves time because we can separate app issues from OS or device issues.

## Notes

- Run these commands on the Raspberry Pi itself.
- For Wi-Fi, audio routing, and system volume tests, use a root shell unless noted otherwise.
- Most commands are safe to run repeatedly.
- For commands marked as destructive or disruptive, use caution.
- If a command fails, copy both the command and its output when reporting back.

### Become root first

On TinyCore, many network and device-control commands need root.

Try:

```sh
sudo -i
```

or:

```sh
su
```

## 1. Basic Device And Boot Context

### Check model

```sh
cat /proc/device-tree/model
```

Expected:

- should mention your Raspberry Pi model, for example `Raspberry Pi Zero 2 W`

Actual:
`Raspberry Pi 3 Model B Rev 1.2.`

### Check kernel

```sh
uname -a
```

Expected:

- should show the running TinyCore kernel

Actual:
`Linux box 6.12.25-piCore-v7 #29 SMP Sat Apr 26 13:20:03 EDT 2025 armv71 GNU/Linux`

### Check boot logs created by Sai

```sh
cat /tmp/sai-bootlocal.log
cat /tmp/sai-console-font.log
cat /tmp/sai-audio-init.log
cat /tmp/sai-platform-service.log
cat /tmp/sai-app.log
```

Expected:

- each file should exist after boot
- `sai-console-font.log` should show whether the font loader ran
- `sai-platform-service.log` should show Wi-Fi and volume requests
- `sai-app.log` should show app-side logging for platform and speech startup

## 2. Console Font / Unicode / Braille Rendering

### Check if bundled font exists

```sh
ls -lh /usr/local/share/consolefonts/sai-unifont.psf.gz
```

Actual:
`lrwxrwxrwx 1 root root 67 Jan 1 00:00 /usr/local/share/consolefonts/sai-unifont.psf.gz -> /tmp/tcloop/sai-app/usr/local/share/consolefonts/sai-unifont.psf.gz`

Expected:

- file should exist

### Check available font tools

```sh
which setfont
which loadfont
```

Expected:

- at least one should resolve

Actual:
`/usr/sbin/loadfont`

### Apply bundled font manually with `setfont`

Run only if `setfont` exists:

```sh
setfont /usr/local/share/consolefonts/sai-unifont.psf.gz
```

Expected:

- no error output

Actual:
`setfont: command not found`

### Apply bundled font manually with `loadfont`

Run only if `loadfont` exists:

```sh
loadfont < /usr/local/share/consolefonts/sai-unifont.psf.gz
```

Expected:

- no error output

Actual:
`loadfont: input files: bad length or unsupported font type

If this happens, the bundled file is not in a format BusyBox `loadfont` accepts. Sai needs to ship a different console-font file for TinyCore.

### Verify Unicode rendering

```sh
printf 'Hindi: हिंदी\n'
printf 'Braille dots: • • •\n'
printf 'Unicode braille cells: ⠓⠑⠇⠇⠕\n'
```

Expected:

- Hindi should not appear as broken boxes if the console font supports it
- `•` should appear as a round dot, not a square
- braille cells should appear as braille patterns

If `•` still shows as a square but braille cells display correctly, that points to console-font glyph coverage rather than Sai logic.

## 3. ALSA Audio Detection

### Check ALSA cards

```sh
cat /proc/asound/cards
```

Expected:

- should list at least one sound card

### Check ALSA playback devices

```sh
cat /proc/asound/pcm
```

Expected:

- should list one or more `playback` devices

### Check `aplay` device list

```sh
aplay -l
```

Expected:

- should list playback hardware

### Check mixer controls

```sh
amixer scontrols
```

Expected:

- should list controls such as `Master`, `PCM`, `Speaker`, `HDMI`, or similar

Actual:
`Simple mixer control 'PCM',0

### Run audio init manually

```sh
/usr/local/bin/sai-audio-init
cat /tmp/sai-audio-init.log
```

Expected:

- log should show detected ALSA card and playback device
- writing `/tmp/sai-asound.conf` should succeed if a device is found

### Check generated ALSA config

```sh
cat /tmp/sai-asound.conf
```

Expected:

- should point to `hw:<card>,<device>`

### Play a short test tone or sample

If `speaker-test` exists:

```sh
speaker-test -D default -c 2 -t sine -f 440
```

Stop it with `Ctrl+C`.

Expected:

- audible tone on HDMI or the configured output

Actual:
`It plays audion from left and right speakers of HDMI`

If `speaker-test` is not available but `aplay` is:

```sh
aplay /usr/local/share/sai-base/UserSpace/*.wav
```

Expected:

- audible playback if a WAV file exists

## 4. Volume Read / Volume Set

### Read current mixer state

```sh
amixer
```

Expected:

- should show percentage values for available mixer controls

### Read current volume using Sai service logic

```sh
control="$(amixer scontrols 2>/dev/null | sed -n "s/Simple mixer control '\\([^']*\\)'.*/\\1/p" | grep -E '^(Master|PCM)$' | head -n 1)"; echo "$control"; [ -n "$control" ] && amixer sget "$control"
```

Expected:

- should print the chosen control name
- then show current percentage such as `[75%]`

### Set volume to 50%

```sh
control="$(amixer scontrols 2>/dev/null | sed -n "s/Simple mixer control '\\([^']*\\)'.*/\\1/p" | grep -E '^(Master|PCM)$' | head -n 1)"; [ -n "$control" ] && amixer -q sset "$control" 50% unmute
```

### Verify volume changed

```sh
control="$(amixer scontrols 2>/dev/null | sed -n "s/Simple mixer control '\\([^']*\\)'.*/\\1/p" | grep -E '^(Master|PCM)$' | head -n 1)"; [ -n "$control" ] && amixer sget "$control"
```

Expected:

- output should now show around `50%`

### Test through Sai platform service path

```sh
grep -n 'Read system volume\|System volume updated' /tmp/sai-platform-service.log
```

Expected:

- should show entries after Sai attempts volume read or update

If direct `amixer` commands work but Sai still fails, then the app/service path is the issue. If `amixer` itself fails, then the underlying mixer mapping needs adjustment.

## 5. Wi-Fi Interface And Scanning

Note:

- TinyCore may not include the `ip` command, so `ifconfig` is fine here.

### Bring interface up manually

```sh
ifconfig wlan0 up
```

Expected:

- no error output

Actual:
`Operation not permitted` but no error with sudo command

If you get `Operation not permitted`, rerun from a root shell.

### Verify interface state

```sh
ifconfig wlan0
```

Expected:

- interface should be present and marked `UP`

Actual:
`wlan0  Link encap:Ethernet HWaddr B8:78:8F:8A
  UP BROADCAST MULTICAST MTU:1500 Metric:1
  RX packets:0 errors:0 dropped:0 overruns:0 frame:0
  TX packets:0 errors:0 dropped:0 overruns:0 frame:0
  collision:0 txqueuelen:1000
  RX bytes:0 (0.0 B) TX bytes:0 (0.0 B)
  `

### Check wireless info

```sh
iwconfig wlan0
```

Expected:

- should report wireless details, not `no wireless extensions`

Actual:
`wlan0 IEE 802.11 ESSID:off/any 
  Mode: Managed Access Point: Not-Associated T-Power=32 dBm
  Retry short limit:7 RTS thr:off Frament thr:off
  Power Management:on`

### Scan directly

```sh
iwlist wlan0 scan
```

Expected:

- should list nearby cells and ESSIDs

If it says `Network is down`, bring it up first and retry.

If it says scanning is unsupported, the wireless driver/firmware path still needs work.

Actual:
`No scan result`

### Check firmware-related packages or logs

```sh
dmesg | grep -i -E 'brcm|firmware|wlan|wifi'
```

Expected:

- should show the wireless chipset initializing

### Verify Sai service log after scan attempt

```sh
tail -n 50 /tmp/sai-platform-service.log
```

Expected:

- should show request processing and either scan success or a more specific failure

Actual:
`brcmfmac: brcmf_c_process_txcap_blob: no txcap_blob available (err=-2)
brcmfmac: brcmf_c_preinit_dcmds: Firmware: BCM43430/1 wl0: Jul 19 2021 03:24:18 version 7.45.98 (TOB)
brcmfmac: brcmf_cfg80211_set_power_mgmt: power save enabled
`

## 6. Wi-Fi Connect

### Generate WPA config manually

Replace `YOUR_SSID` and `YOUR_PASSWORD`.

```sh
wpa_passphrase "YOUR_SSID" "YOUR_PASSWORD" > /tmp/wpa_test.conf
cat /tmp/wpa_test.conf
```

Expected:

- should create a valid config file

### Bring Wi-Fi up and connect manually

```sh
ifconfig wlan0 up
pkill -f 'wpa_supplicant.*-iwlan0' || true
pkill -f 'udhcpc.*wlan0' || true
wpa_supplicant -B -D nl80211,wext -i wlan0 -c /tmp/wpa_test.conf
udhcpc -b -i wlan0 -p /tmp/udhcpc.wlan0.pid
```

Expected:

- both commands should return without fatal errors

Actual:
nl80211: deinit ifname=wlan0 disabled_11b_rates=0
wlan0: Failed to initialize driver interface
wlan0: CTRL-EVENT-DSCP-POLICY clear_all

### Verify Wi-Fi connection

```sh
iwconfig wlan0
ifconfig wlan0
```

Expected:

- `iwconfig` should show the connected ESSID
- `ifconfig` should show an IP address

## 7. TinyCore Platform Service

### Check service is running

```sh
ps | grep sai-platform-service
```

Expected:

- should show `/usr/local/bin/sai-platform-service`

### Start service manually if needed

```sh
/usr/local/bin/sai-platform-service
```

Expected:

- normally this blocks and runs as a foreground service
- use `Ctrl+C` to stop if started manually in the terminal

### Watch service logs live

```sh
tail -f /tmp/sai-platform-service.log
```

Expected:

- new requests should appear when Sai triggers Wi-Fi or volume actions

## 8. Audio Output Selection

### Check current ALSA defaults

```sh
cat /tmp/sai-asound.conf
echo "$ALSA_CONFIG_PATH"
```

Expected:

- config should exist after `sai-audio-init`

Actual:
`No output is shown`

### Test HDMI-oriented mixer path

```sh
amixer -c 0
```

Expected:

- lets you inspect whether the HDMI-capable card is actually card `0`

If not, Sai may need a different card selection rule for your Pi image.

## 9. Speech And Menu Defaults

### Check saved settings

```sh
grep -E '"speech_mode"|"audio_playback"|"braille_display_size"|"audio_output"' /usr/local/share/sai-base/userSettings.json 2>/dev/null || true
grep -E '"speech_mode"|"audio_playback"|"braille_display_size"|"audio_output"' userSettings.json 2>/dev/null || true
```

Expected:

- for a fresh config the defaults should now be:
  - `speech_mode`: `off`
  - `audio_playback`: `off`
  - `braille_display_size`: `medium`
  - `audio_output`: `hdmi`

Important:

- if an older saved `userSettings.json` already exists, it can override the new defaults

## 10. Power Off

### Check command path

```sh
which poweroff
```

Expected:

- should resolve to a valid binary

### Actual shutdown

Use only when ready:

```sh
poweroff
```

Expected:

- Raspberry Pi should shut down cleanly

## 11. Quick Report Back Template

When reporting results back, this format is enough:

```text
1. Font:
- command:
- output:

2. Audio cards:
- command:
- output:

3. Volume set:
- command:
- output:

4. Wi-Fi scan:
- command:
- output:

5. Wi-Fi connect:
- command:
- output:
```

That will make it much faster to pinpoint what still needs changing.
