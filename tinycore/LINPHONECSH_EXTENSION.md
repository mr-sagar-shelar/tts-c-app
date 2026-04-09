# Building `linphonecsh.tcz` For piCore

This repository now includes a Docker-based helper to build a separate Tiny Core extension for Linphone's console SIP tools.

The goal is to produce a testable extension for Sai VoIP work on Raspberry Pi with TinyCore Linux.

## What It Builds

The build emits these artifacts:

- `linphonecsh.tcz`
- `linphonecsh.tcz.dep`
- `linphonecsh.tcz.info`
- `linphonecsh.tcz.list`

Default output directory:

```sh
build/tinycore/linphonecsh/
```

## Build Command

Run:

```sh
./tinycore/scripts/build-linphonecsh-extension.sh
```

If you want a custom output path:

```sh
OUT_DIR="$PWD/build/tinycore/custom-linphonecsh" ./tinycore/scripts/build-linphonecsh-extension.sh
```

## How The Build Works

The Docker image in [linphonecsh-armhf.Dockerfile](/Users/sagarshelar/fliteDemo/tts-c-app-codex/tinycore/docker/linphonecsh-armhf.Dockerfile) does this:

1. starts from Debian Bookworm
2. enables `armhf` multiarch
3. installs Debian's `linphone-cli:armhf`
4. copies `linphonecsh`, `linphonec`, and Linphone data files into a TinyCore package root
5. gathers the required armhf shared libraries
6. creates a SquashFS Tiny Core extension

## Why This Uses Debian Packages

This is the fastest way to get an `armhf` Linphone console client packaged for TinyCore-style testing without introducing a full native TinyCore build toolchain first.

The current recipe uses Debian Bookworm's `linphone-cli` package for `armhf`, which includes the console SIP client needed by Sai.

## Resulting Paths Inside TinyCore

The extension installs:

- `/usr/local/bin/linphonecsh`
- `/usr/local/bin/linphonec`
- `/usr/local/lib/linphone/*`
- `/usr/local/share/linphone/*`

The `/usr/local/bin/*` commands are wrappers that export `LD_LIBRARY_PATH` before launching the real binaries.

## Using It On The Raspberry Pi

Copy the generated files into:

```sh
/mnt/mmcblk0p2/tce/optional/
```

Then add this line to:

```sh
/mnt/mmcblk0p2/tce/onboot.lst
```

```text
linphonecsh.tcz
```

If you want Sai's main extension metadata to declare the dependency as well, add:

```text
linphonecsh.tcz
```

to:

[sai-app.tcz.dep](/Users/sagarshelar/fliteDemo/tts-c-app-codex/build/tinycore/artifacts/sai-app.tcz.dep)

after you generate the Sai artifacts.

## Quick Validation On TinyCore

After boot:

```sh
which linphonecsh
which linphonec
linphonecsh --help
```

If those work, Sai's VoIP menu should be able to start testing registration and outbound dialing.

## Notes And Limits

- This is a practical packaging path for testing, not yet a fully curated upstream TinyCore extension.
- The dependency list in `linphonecsh.tcz.dep` is intentionally small and may need adjustment depending on your Pi audio setup.
- If the binary starts but audio fails, the likely issue is ALSA or codec/runtime behavior on the target system rather than the extension image itself.
- If Linphone changes package layout in Debian, the Docker recipe may need a refresh.
