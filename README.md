# Loon Firmware

Loon Firmware is a standalone MeshCore repeater-bot for the Heltec WiFi LoRa
32 V3. It repeats normal MeshCore traffic and adds a small, airtime-conscious
bot for Public and `#test`. It needs no phone, computer, Wi-Fi, or Internet
connection during normal operation.

Loon is based on current upstream
[MeshCore](https://github.com/meshcore-dev/MeshCore). Its bot behaviour was
developed independently and is maintained in this repository.

The name comes from a **loon call**: a compact signal sent across distance and
answered elsewhere. That is the design goal for Loon on the mesh—useful,
recognizable responses without unnecessary chatter.

> [!WARNING]
> Loon is experimental community firmware. Keep a known-working recovery image,
> test on `#test` before enabling Public responses, and change the default
> administrator password before deployment. Flashing custom firmware can make a
> device temporarily unusable and may require recovery over USB.

## Release status

Install firmware only from the current GitHub releases. Change the default
administrator password before deployment.

The current source targets the original Heltec V3 with 8 MB flash and no PSRAM:

- Radio settings: 910.525 MHz, SF7, BW 62.5 kHz, CR 5
- Default node name: `Loon`

Release filenames identify the version, board, and image type. They do not
include a region or preset name.

- Current release: v0.1.4.

## Features

- Normal MeshCore repeater operation remains the priority.
- Public accepts only `!ping`; it follows `loon.ping.public`.
- `#test` accepts `!ping`, `!help`, `!about`, and `!roll`; ping follows
  `loon.ping.test`.
- `!roll` rolls one six-sided die on `#test`; Public ignores it.
- Ping replies show the actual inbound path, RSSI, SNR, and recent channel use.
- Scheduled announcements can be off, hourly, or daily per channel.
- Persistent Loon configuration survives reboot and firmware updates.
- Duplicate/rate protection and busy-channel delay reduce unnecessary airtime.
- OLED shows the normal repeater information.
- Existing authenticated MeshCore remote administration remains available.
- Wi-Fi is off during normal operation; the existing authenticated OTA mode is
  available when deliberately started by an administrator.

## Bot commands

The current source supports these channel commands:

```text
!ping
!roll
!help
!about
```

The command is case-insensitive, tolerates surrounding spaces, and must be the
entire message. Loon ignores its own messages.

On `#test`, `!help` returns:

```text
Loon commands: ping, roll, about. Use the ! prefix.
```

Public does not accept `!help`. The `#test` help response deliberately contains
no complete command tokens, preventing it from triggering Loon or another
exact-match bot.

`!roll` rolls one six-sided die. It is restricted to `#test`.

```text
🎲 @[your-name] | 1d6: 4
```

`!about` returns the firmware version and the standard MeshCore owner message:

```text
Loon v0.1.3 | Operated by your-name
```

Set the owner message through authenticated administration:

```text
set owner.info Operated by your-name
```

If `owner.info` is empty, `!about` returns only the firmware version.

Direct response:

```text
Loon: Pong @[your-name] | Path direct | RSSI -29 | SNR 11.8 | Busy 1%
```

Routed response:

```text
Loon: Pong @[your-name] | Path A41C72>19B003>CE821F | RSSI -106 | SNR 6.5 | Busy 8%
```

`Path` is the inbound route observed by Loon. A directly received request is
shown as `direct`; routed requests show hop hashes separated by `>`. Incoming
1-, 2-, and 3-byte hashes are preserved for display. Replies use 3-byte path
hashes.

Command rate protection:

- Each sender is limited to one accepted bot command every 10 seconds.
- One sender's cooldown does not block other senders.
- Above the configured Busy threshold, the response is progressively delayed.
- Repeater forwarding continues to take priority.

## Scheduled announcements

With no custom message configured, Loon sends the compact status:

```text
Loon: ONLINE | Up 6d04h | RX 582 | Repeated 143 | Busy 5%
```

- `Up`: time since boot.
- `RX`: packets received by the radio.
- `Repeated`: flood packets transmitted by Loon.
- `Busy`: measured TX plus RX airtime over the latest one-minute sample.

When a custom message is configured, it replaces the status completely. Loon
does not add a name prefix because MeshCore already identifies the sender:

```text
Community repeater online — monitoring #test
```

The custom message can contain up to 140 characters. Clearing it restores the
default compact status announcement. Its first non-space character cannot be
`!`; unsafe stored text is cleared during upgrade and the compact status is
used instead.

Hourly announcements occur at the next top of the hour. Daily announcements
use `loon.daily.hour` and `loon.timezone`. Loon adds 0–30 seconds of random
jitter. It waits for a valid MeshCore clock and does not guess wall-clock time.
An announcement is skipped when Busy is 80% or higher; missed announcements
are not replayed later.

The Heltec V3 has no battery-backed real-time clock. Its clock may need to be
synchronized after a complete power loss.

## Defaults

| Setting | Public | `#test` |
| --- | --- | --- |
| Ping | Off | On |
| Announcement | Off | Daily |

Other defaults:

- Daily announcement hour: `09:00`
- Timezone offset: `-300` minutes (Toronto standard time)
- Busy-delay threshold: `20%`
- Maximum busy delay: 120 seconds
- Boot announcement: none

Toronto uses `-300` in standard time and `-240` during daylight time. Loon uses
a fixed offset and does not change it automatically for DST.

## Loon administration commands

Run these through the USB serial console or authenticated MeshCore Remote
Management. Commands with no value display their current setting.

| Command | Purpose |
| --- | --- |
| `loon` | Show all main Loon settings. |
| `loon.ping.public [on\|off]` | Read or change Public ping responses. |
| `loon.ping.test [on\|off]` | Read or change `#test` ping responses. |
| `loon.announce.public [off\|hourly\|daily]` | Read or change Public announcements. |
| `loon.announce.test [off\|hourly\|daily]` | Read or change `#test` announcements. |
| `loon.announce.message [TEXT\|clear]` | Read, set, or clear the custom announcement text. |
| `loon.daily.hour [0..23]` | Read or set the local hour for daily announcements. |
| `loon.timezone [-720..840]` | Read or set the UTC offset in minutes. |
| `loon.busy.threshold [0..100]` | Read or set the percentage where ping delay begins. |

Examples:

```text
loon
loon.ping.public on
loon.ping.test off
loon.announce.public daily
loon.announce.test hourly
loon.announce.message Community repeater online — monitoring #test
loon.daily.hour 21
loon.timezone -240
loon.busy.threshold 25
```

Successful changes reply `OK` and are saved immediately. With no argument,
`loon.announce.message` shows the current message or `off`. Use
`loon.announce.message clear` to return to the default status announcement.

`loon.ping.public` controls only `!ping` on Public. `loon.ping.test` controls
`!ping` on `#test`. The `#test`-only utility commands remain available whenever
the `#test` command channel is enabled.

## Useful inherited MeshCore commands

Loon retains the standard repeater CLI. Commonly useful commands include:

| Command | Purpose |
| --- | --- |
| `ver` | Show firmware version. |
| `board` | Show board information. |
| `clock` | Show the current clock. |
| `clock sync` | Request clock synchronization. |
| `time EPOCH` | Set Unix time from the USB serial console. |
| `stats-packets` | Show packet statistics (USB serial only). |
| `stats-radio` | Show radio statistics (USB serial only). |
| `stats-core` | Show core statistics (USB serial only). |
| `clear stats` | Reset collected statistics. |
| `neighbors` | Show the neighbour table. |
| `discover.neighbors` | Send a neighbour discovery request. |
| `advert` | Send an advertisement. |
| `reboot` | Reboot the device. |
| `start ota` | Start the platform OTA flow when supported. |
| `get NAME` | Read a standard repeater setting. |
| `set NAME VALUE` | Change a standard repeater setting. |

The complete standard repeater configuration surface is inherited from
MeshCore and may change when Loon updates its upstream base. Use
[`config.meshcore.io`](https://config.meshcore.io) for normal provisioning and
the upstream [MeshCore documentation](https://docs.meshcore.io) for standard
settings. Loon-specific commands are the stable interface documented above.

## Flashing and updating

### Updating a working Loon installation

Use the non-merged application image from the latest GitHub release. Keep the
installed working image or official MeshCore firmware available for recovery.
Use release assets from this repository rather than mirrors or old links.

### Recovery

Keep a known-working official Heltec V3 repeater image available. If
Loon fails to boot, restore the official firmware with erase, then layer the
known-working non-merged application image over it without erasing.

## Initial configuration and security

The source uses MeshCore's public development default administrator password
for first provisioning. Change it immediately using the standard MeshCore
administration tools. Never publish deployed administrator passwords, private
channel keys, private keys, or device configuration dumps. A password embedded
in source or a release must be treated as public.

Public and `#test` are the only channels recognized by the Loon bot in v0.1.3.
Their standard channel secrets are embedded so Loon can decrypt commands and
construct replies. Ordinary repeater forwarding does not expose message
content through Loon, and Loon does not retain a message history.

## Building from source

Install PlatformIO, clone the repository, and run:

```sh
./build_loon_heltec_v3.sh
```

The PlatformIO environment is `Loon_heltec_v3_repeater`. The resulting
application image is:

```text
.pio/build/Loon_heltec_v3_repeater/firmware.bin
```

The current build targets an ESP32-S3 Heltec V3 with 8 MB flash and no PSRAM.
Loon-specific code is kept in the repeater application so upstream MeshCore
updates remain manageable.

## Testing checklist

- Device boots and OLED remains stable.
- Repeater advert appears on the mesh.
- `!ping` on `#test` produces one reply.
- `!help` lists the utility commands on `#test` and is ignored on Public.
- `!roll` works on `#test` and is ignored on Public.
- `!about` shows the firmware version and configured owner message.
- A direct ping displays `Path direct`.
- A routed ping displays the observed hop hashes.
- RSSI, SNR, and Busy values are plausible.
- Disabled channels produce no bot response.
- Hourly/daily announcements occur only on enabled channels.
- Repeater traffic continues normally during bot activity.
- No resets occur during extended operation.

## Project status

Loon has booted successfully on a physical Heltec V3, and direct `#test` ping
behaviour has been verified. Routed pings, dice rolling, long-duration
stability, and scheduled announcements remain field-test items.

MeshCore and Loon are MIT-licensed; see [license.txt](license.txt). This fork
retains upstream copyright and attribution.
