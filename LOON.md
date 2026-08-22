# Loon Firmware

Loon Firmware is a standalone MeshCore repeater-bot for the original Heltec
WiFi LoRa 32 V3 (8 MB flash, no PSRAM required), both Heltec T114 variants,
the Seeed SenseCAP Solar Node P1/P1 Pro, and RAK4631 repeaters including the
WisMesh Repeater Mini. It starts from current upstream MeshCore and keeps
repeater operation as the highest priority.

## Version 0.1 scope

- Heltec WiFi LoRa 32 V3, display-equipped and displayless Heltec T114, and
  RAK4631 repeater targets, plus one shared target for the Seeed SenseCAP Solar
  Node P1 and P1 Pro.
- Clean/full installs use ordinary upstream MeshCore radio defaults and retain
  MeshCore's standard public development administrator password. Provision the
  radio for the intended network and change the password before deployment.
- Release filenames identify only the firmware version, board, and image type;
  region/preset names are not included.
- Normal MeshCore repeating and authenticated repeater administration.
- `!ping` on Public and `#test` through the per-channel ping controls.
- `!help` and `!about` on `#test` only.
- `!roll` for one six-sided die on `#test` only.
- Persistent authenticated `loon.roll on|off` and `loon.rps on|off` controls;
  the RPS setting applies to both the single and three-round commands. Both
  settings default to off on a clean installation.
- Scheduled announcements independently configurable for Public and `#test`.
- Announcement cadence: off, hourly, or daily.
- OLED repeater diagnostics.
- Busy-channel deferral, deduplication, loop prevention, and rate limiting.
- Authenticated Wi-Fi OTA for firmware maintenance.
- Optional one-way MeshCore-to-Discord webhook bridge on ESP32 hardware.

## Discord webhook bridge

On ESP32 targets such as the Heltec V3, Loon can forward one explicitly
selected MeshCore channel to one Discord webhook. Each MeshCore message becomes
one Discord message, with the MeshCore sender as the webhook username and the
original message text. The optional path display adds a formatted line containing
the exact inbound path IDs. Loon ping replies label this as the request path, and
direct receptions are shown as `direct`. It defaults to off. Discord mentions
are disabled.

The bridge is disabled until Wi-Fi and a webhook URL are configured. It uses a
bounded 32-message RAM queue, retries failed deliveries, and drops the oldest
entry if the queue fills. Repeater operation continues when Wi-Fi or Discord is
unavailable. HTTPS uses the ESP32 `setInsecure()` mode, matching the proven
reference implementation: traffic is encrypted, but the server certificate is
not verified.

Only one named channel can be selected. There is deliberately no `all` option.
The selected standard channel is derived from its name, such as `Public`,
`#test`, or `#jokes`.

## Ping response

Routed:

```text
Loon: 🏓 @[your-name] | Path A41C72>19B003>CE821F | RSSI -106 | SNR 6.5 | Busy 8%
```

Direct:

```text
Loon: 🏓 @[your-name] | Path direct | RSSI -91 | SNR 10.2 | Busy 3%
```

The path is the actual inbound hop-hash sequence. Incoming 1-, 2-, and 3-byte
hash widths are preserved for display; Loon defaults to 3-byte response paths.

## Announcement

With no custom message configured:

```text
Loon: ONLINE | Up 6d04h | RX 582 | Repeated 143 | Busy 5%
```

When configured, a custom message replaces the status text completely. Empty
or cleared custom text restores the compact status announcement. Custom text
whose first non-space character is `!` is rejected to prevent bot loops.

Daily is the recommended cadence. Public transmission is disabled until it is
deliberately enabled after testing.

## Initial defaults

| Feature | Public | #test |
| --- | --- | --- |
| Ping | Off | Off |
| Announcement | Off | Off |

Additional defaults:

- One ping response per accepted request.
- No boot announcement.
- No catch-up announcement after reboot.
- Use valid MeshCore wall-clock time; do not guess it.
- Add 0-30 seconds of announcement jitter.
- Defer bot traffic above 20% channel utilization.
- Repeater traffic always takes priority.

## Development boundaries

Loon code is implemented independently. Loon-specific code should be isolated
from the MeshCore core wherever practical so upstream updates remain easy to
merge.

No credentials, channel keys, private keys, device configuration dumps, or
build artifacts containing secrets belong in Git.

## Build

Install PlatformIO, then run:

```sh
./build_loon_heltec_v3.sh
./build_loon_rak4631_repeater_mini.sh
```

The build environments are `Loon_heltec_v3_repeater` and
`Loon_RAK4631_repeater_mini`. The RAK target retains the official MeshCore
RAK4631 repeater feature set and adds Loon without replacing that target.

## Administration

The Loon settings are available through the existing authenticated repeater
CLI:

```text
loon
loon.ping.public on|off
loon.ping.test on|off
loon.announce.public off|hourly|daily
loon.announce.test off|hourly|daily
loon.announce.message [text|clear]
loon.daily.hour 0..23
loon.timezone -720..840
loon.busy.threshold 0..100
wifi.status
wifi.ssid [name]
wifi.pwd [password|clear]
wifi.webhook.path on|off
wifi.webhook.channel [Public|#channel]
wifi.webhook [https://discord.com/api/webhooks/...|test|clear]
wifi.connect
```

`loon.timezone` is the UTC offset in minutes. Toronto is `-300` in standard
time and `-240` during daylight time. The Heltec V3 has no battery-backed RTC,
so announcements wait until MeshCore provides a valid clock.

Like upstream MeshCore, Loon compiles a public development administrator
password into clean-install firmware for initial provisioning. Change it
during device provisioning and never reuse it elsewhere.
Wi-Fi passwords and webhook URLs are stored in device preferences and are not
printed by status commands. Treat a Discord webhook URL as a password and
rotate it if it is exposed.
