# Loon Firmware

Loon Firmware is a standalone MeshCore repeater-bot for the original Heltec
WiFi LoRa 32 V3 (8 MB flash, no PSRAM required). It starts from current
upstream MeshCore and keeps repeater operation as the highest priority.

## Version 0.1 scope

- Heltec WiFi LoRa 32 V3 only.
- USA/Canada recommended preset: 910.525 MHz, SF7, BW62.5, CR5.
- Release filenames identify only the firmware version, board, and image type;
  region/preset names are not included.
- Normal MeshCore repeating and authenticated repeater administration.
- `!ping` on Public and `#test` through the per-channel ping controls.
- `!help` and `!about` on `#test` only.
- `!roll` for one six-sided die on `#test` only.
- Scheduled announcements independently configurable for Public and `#test`.
- Announcement cadence: off, hourly, or daily.
- OLED repeater diagnostics.
- Busy-channel deferral, deduplication, loop prevention, and rate limiting.
- Authenticated Wi-Fi OTA for firmware maintenance.

## Ping response

Routed:

```text
Loon: Pong @[your-name] | Path A41C72>19B003>CE821F | RSSI -106 | SNR 6.5 | Busy 8%
```

Direct:

```text
Loon: Pong @[your-name] | Path direct | RSSI -91 | SNR 10.2 | Busy 3%
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
| Ping | Off | On |
| Announcement | Off | Daily |

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
```

The build environment is `Loon_heltec_v3_repeater`.

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
```

`loon.timezone` is the UTC offset in minutes. Toronto is `-300` in standard
time and `-240` during daylight time. The Heltec V3 has no battery-backed RTC,
so announcements wait until MeshCore provides a valid clock.

Administrator passwords compiled into source or firmware are public defaults.
Change the password during device provisioning and never reuse it elsewhere.
