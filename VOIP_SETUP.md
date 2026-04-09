# Sai VoIP Setup

This document explains how to enable VoIP calling in Sai so a user can place SIP-based calls from the app.

## Overview

Sai's VoIP menu is designed as a lightweight SIP client controller. The app stores SIP account settings in `userSettings.json` and uses the external `linphonecsh` command-line client to:

- initialize a background SIP client
- register a SIP account
- dial a SIP address, extension, or phone-style target
- hang up the current call
- read registration status

## What You Need

You need three pieces:

1. A SIP server or hosted SIP provider
2. A SIP account on that server
3. `linphonecsh` installed on the Raspberry Pi image that runs Sai

## Recommended Server Options

Sai can work with any SIP-compatible backend. Common choices are:

- Asterisk
- FreeSWITCH
- Kamailio plus a media server
- FusionPBX
- 3CX
- Any hosted SIP trunk or hosted SIP extension provider

For a first setup, Asterisk or FusionPBX is usually the easiest path because they provide simple extension-based calling.

## Minimum SIP Server Requirements

Your VoIP server should provide:

- one SIP account for the Sai device
- SIP registration enabled
- at least one dialable extension or outbound route
- codecs supported by the target device and server
- network access from the Raspberry Pi to the SIP server

## Install the Client on Raspberry Pi / TinyCore

Sai uses `linphonecsh`. Make sure both `linphonecsh` and its runtime dependencies are available in the image.

Typical package names on Linux distributions are:

- `linphone-cli`
- `linphone`
- `linphonec`

On TinyCore you may need to build or package Linphone separately if an extension is not already available.

This repository now includes a Docker-based TinyCore packaging path for that:

- [LINPHONECSH_EXTENSION.md](/Users/sagarshelar/fliteDemo/tts-c-app-codex/tinycore/LINPHONECSH_EXTENSION.md)
- [build-linphonecsh-extension.sh](/Users/sagarshelar/fliteDemo/tts-c-app-codex/tinycore/scripts/build-linphonecsh-extension.sh)

## App Configuration Fields

Sai stores these values in the app configuration:

- `Display Name`
- `SIP Server`
- `SIP Domain`
- `Username`
- `Password`
- `Transport`

### Meaning of Each Field

- `Display Name`: friendly caller label for the account
- `SIP Server`: SIP proxy or registrar host
- `SIP Domain`: SIP domain used when Sai builds `sip:user@domain` targets
- `Username`: account or extension number
- `Password`: SIP account password
- `Transport`: `udp`, `tcp`, or `tls`

## How to Enter the Settings in Sai

From the main menu:

1. Open `VoIP`
2. Enter `Display Name`
3. Enter `SIP Server`
4. Enter `SIP Domain`
5. Enter `Username`
6. Enter `Password`
7. Select `Transport`

After that you can use:

- `Dial SIP Address`
- `Call Contact`
- `Registration Status`
- `Hang Up Call`
- `Show VoIP Config`

## Contact Dialing

`Call Contact` uses the existing contact's `phone` field as the VoIP target. This lets you store:

- a SIP URI like `sip:1001@example.com`
- an extension like `1001`
- a number or provider-specific dial string

If a plain extension or number is stored and `SIP Domain` is configured, Sai will build a target like:

`sip:1001@example.com`

and append the chosen transport where applicable.

## Example Asterisk Configuration

Below is a minimal example for one extension.

### `pjsip.conf`

```ini
[transport-udp]
type=transport
protocol=udp
bind=0.0.0.0

[6001]
type=endpoint
context=internal
disallow=all
allow=ulaw,alaw
auth=6001
aors=6001

[6001]
type=auth
auth_type=userpass
username=6001
password=change_me

[6001]
type=aor
max_contacts=1
```

### `extensions.conf`

```ini
[internal]
exten => 6001,1,Dial(PJSIP/6001,20)
exten => 6002,1,Dial(PJSIP/6002,20)
```

For this example, Sai would typically use:

- `SIP Server`: your Asterisk host, for example `192.168.1.50`
- `SIP Domain`: same host or DNS name
- `Username`: `6001`
- `Password`: `change_me`
- `Transport`: `udp`

## Network Notes

Make sure the Raspberry Pi can reach the SIP server on the required ports.

Common ports:

- SIP UDP/TCP: `5060`
- SIP TLS: `5061`
- RTP media: usually a configurable UDP range

If calls register but you have no audio, that is usually an RTP, NAT, firewall, or codec issue rather than a Sai app issue.

## Current Implementation Notes

Sai currently focuses on:

- account registration
- manual dialing
- contact-based dialing
- call hangup
- status checks

It does not yet provide:

- incoming-call UI
- call history
- conference calling
- DTMF keypad UI
- contact presence
- voicemail UI

Those can be added later on top of the current menu structure.

## Suggested Next Steps

For a production-ready blind-access workflow, the next improvements would be:

1. incoming call announcements with speech feedback
2. answer and reject actions from keyboard shortcuts
3. a keypad for DTMF during active calls
4. recent calls and favorite contacts
5. headset or audio routing configuration per platform
