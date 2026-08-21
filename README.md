# Loon Firmware

## Start here

**Looking for firmware to install?** Go to
**[Loon Firmware Releases](https://github.com/Bobabate/Loon-Firmware/releases)**.
Do not use GitHub's green **Code** download button; that downloads the source
code, not an installable firmware image.

What do you want to do?

- **Download firmware:** [open Releases](https://github.com/Bobabate/Loon-Firmware/releases)
- **Install or update Loon:** [jump to the installation instructions](#flashing-and-updating)
- **Configure Loon:** [jump to the administration commands](#loon-administration-commands)
- **See Loon's commands:** [jump to bot commands](#bot-commands)
- **Understand this repository:** [read START-HERE.md](START-HERE.md)
- **Review changes by version:** [read the changelog](CHANGELOG.md)

### Code and Releases are different

- The **Code** page contains the project files used to build Loon.
- The **Releases** page contains finished firmware files ready to install.
- Most Loon users never need to open the folders shown on the Code page.
- The many folders and commits come from MeshCore, which Loon is built upon.

---

Loon Firmware is a standalone MeshCore repeater-bot designed to help people
establish and test a mesh. It repeats normal MeshCore traffic and provides
recognizable, airtime-conscious responses that operators can listen for, aim
toward, and use to confirm coverage. Its optional webhook can also feed mesh
activity into Discord, helping communities observe and grow their network.

Loon is based on upstream [MeshCore](https://github.com/meshcore-dev/MeshCore).
Its bot behaviour was developed independently and is maintained in this
repository.

The name comes from a **loon call**—a distinct signal sent across the distance
to find and connect with others.

> [!WARNING]
> Loon is experimental community firmware. Keep a known-working recovery image,
> test on `#test` before enabling Public responses, and change the default
> administrator password before deployment. Flashing custom firmware can make a
> device temporarily unusable and may require recovery over USB.

## Release status

Install firmware only from the current GitHub releases. Change the default
administrator password before deployment.

The current source targets the original Heltec V3 with 8 MB flash and no PSRAM,
both display-equipped and displayless Heltec T114 variants, and the RAK4631. A
shared target also supports the Seeed SenseCAP Solar Node P1 and P1 Pro. A
clean/full installation uses the ordinary upstream MeshCore radio defaults and
retains MeshCore's public development administrator password for initial
provisioning. Provision the radio for the intended network and set a unique
password before deployment. The default node name is `Loon`.

Release filenames identify the version, board, and image type. They do not
include a region or preset name.

- Current release: v1.17.1-L3.

## Features

- Normal MeshCore repeater operation remains the priority.
- Public accepts only `!ping`; it follows `loon.ping.public`.
- `#test` accepts `!ping`, `!help`, `!about`, `!roll`, `!rps`, and `!rps3`; ping follows
  `loon.ping.test`.
- `!roll` rolls one six-sided die on `#test`; Public ignores it.
- `!rps` randomly chooses rock, paper, or scissors on `#test`; Public ignores it.
- `!rps3` sends three random throws 10 seconds apart on `#test`; Public ignores it.
- Authenticated administration can persistently enable or disable dice with
  `loon.roll on|off`, and both RPS modes with `loon.rps on|off`.
- Ping replies show the actual inbound path, RSSI, SNR, and recent channel use.
- Scheduled announcements can be off, hourly, or daily per channel.
- Persistent Loon configuration survives reboot and firmware updates.
- Duplicate/rate protection and busy-channel delay reduce unnecessary airtime.
- OLED shows the normal repeater information.
- Existing authenticated MeshCore remote administration remains available.
- Heltec V3 can forward one selected MeshCore channel to Discord, one message
  per post, using the MeshCore sender as the webhook username.
- Wi-Fi is off during normal operation; the existing authenticated OTA mode is
  available when deliberately started by an administrator. Configuring the
  webhook bridge also enables Wi-Fi station mode.

## Bot commands

The current source supports these channel commands:

```text
!ping
!roll
!rps
!rps3
!help
!about
```

The command is case-insensitive, tolerates surrounding spaces, and must be the
entire message. Loon ignores its own messages.

On `#test`, `!help` returns:

```text
Loon: Commands: ping, roll, rps, rps3, about. Use the ! prefix.
```

Public does not accept `!help`. The `#test` help response deliberately contains
no complete command tokens, preventing it from triggering Loon or another
exact-match bot.

`!roll` rolls one six-sided die. It is restricted to `#test`.

```text
Loon: 🎲 4
```

`!rps` randomly chooses rock, paper, or scissors. It is restricted to `#test`.

```text
Loon: 🪨
```

`!rps3` starts a non-blocking best-of-three game. Loon sends three independent
random throws, 10 seconds apart, beginning 10 seconds after the command. Only
one game can run at a time; additional starts are ignored until it finishes.

```text
Loon: 1/3 🪨
Loon: 2/3 ✂️
Loon: 3/3 📄
```

Authenticated administrators can disable or re-enable the game commands. The
RPS setting controls both `!rps` and `!rps3`; switching it off also cancels a
three-round game already in progress. Settings persist across reboot:

```text
loon.roll off
loon.roll on
loon.rps off
loon.rps on
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
Loon: 🏓 @[your-name] | Path direct | RSSI -29 | SNR 11.8 | Busy 1%
```

Routed response:

```text
Loon: 🏓 @[your-name] | Path A41C72>19B003>CE821F | RSSI -106 | SNR 6.5 | Busy 8%
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

Public and `#test` each have their own custom message. When one is configured,
it replaces that channel's status body while keeping
the node-name prefix required by MeshCore group text:

```text
Loon: Community repeater online — monitoring #test
```

Each custom message can contain up to 140 characters. Clearing either one
restores the default compact status announcement for that channel. Its first non-space character cannot be
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
| Ping | Off | Off |
| Announcement | Off | Off |

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
| `loon.announce.public.message [TEXT\|clear]` | Read, set, or clear the Public custom text. |
| `loon.announce.test.message [TEXT\|clear]` | Read, set, or clear the `#test` custom text. |
| `loon.daily.hour [0..23]` | Read or set the local hour for daily announcements. |
| `loon.timezone [-720..840]` | Read or set the UTC offset in minutes. |
| `loon.busy.threshold [0..100]` | Read or set the percentage where Loon reply delays begin. |
| `wifi.status` | Show Wi-Fi state, IP address, and webhook queue depth. |
| `wifi.ssid [NAME]` | Read or set the Wi-Fi network name. |
| `wifi.pwd [PASSWORD\|clear]` | Set or clear the Wi-Fi password; reading shows only whether it is set. |
| `wifi.webhook.channel [Public\|#CHANNEL]` | Select exactly one standard MeshCore channel to forward. |
| `wifi.webhook [URL\|test\|clear]` | Set, test, or clear the Discord webhook without printing its URL. |
| `wifi.connect` | Request an immediate Wi-Fi connection. |

Loon measures combined receive/transmit airtime over about one minute. Above
`loon.busy.threshold`, it increasingly delays its own ping and command replies,
up to the 120-second maximum at 100% Busy. Normal repeater forwarding is never
delayed by this setting; set the threshold to `100` to disable busy-based reply
delays. Scheduled announcements use the separate 80% Busy skip described above.

Examples:

```text
loon
loon.ping.public on
loon.ping.test off
loon.announce.public daily
loon.announce.test hourly
loon.announce.public.message Community repeater online
loon.announce.test.message Test channel check-in
loon.daily.hour 21
loon.timezone -240
loon.busy.threshold 25
wifi.ssid MyNetwork
wifi.pwd MyPassword
wifi.webhook.channel #test
wifi.webhook https://discord.com/api/webhooks/...
wifi.connect
wifi.webhook test
```

Successful changes reply `OK` and are saved immediately. With no argument,
Each message command shows its current message or `default`. Use `clear` to
return that channel to the default compact status announcement.

`loon.ping.public` controls only `!ping` on Public. `loon.ping.test` controls
`!ping` on `#test`. The `#test`-only utility commands remain available whenever
the `#test` command channel is enabled.

The webhook bridge is optional and currently available on ESP32 targets such
as the Heltec V3. It forwards only the explicitly selected channel; `all` is
deliberately rejected. Messages retain their original text, Discord mentions
are disabled, and a bounded RAM queue retries temporary delivery failures.
Repeater operation continues when Wi-Fi or Discord is unavailable. HTTPS uses
encrypted `setInsecure()` mode, so Discord's certificate is not verified.
Treat the webhook URL as a password and rotate it if exposed.

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

A clean/full Loon installation uses ordinary upstream MeshCore radio defaults
and the same public development administrator password as upstream MeshCore:
`password`. Provision the correct regional and network radio settings, then
replace that password with a unique administrator password using the standard
MeshCore tools before deployment. A normal application-image update does not
replace an existing device's stored radio settings or password. Never publish
deployed administrator passwords, private channel keys, private keys, or device
configuration dumps.

Public and `#test` are the only channels recognized by the Loon bot in v0.1.3.
Their standard channel secrets are embedded so Loon can decrypt commands and
construct replies. Ordinary repeater forwarding does not expose message
content through Loon, and Loon does not retain a message history.

## Building from source

Install PlatformIO, clone the repository, and run:

```sh
./build_loon_heltec_v3.sh
./build_loon_rak4631_repeater_mini.sh
```

The PlatformIO environments are `Loon_heltec_v3_repeater` and
`Loon_RAK4631_repeater_mini`. The resulting firmware images are:

```text
.pio/build/Loon_heltec_v3_repeater/firmware.bin
.pio/build/Loon_RAK4631_repeater_mini/firmware.zip
```

Loon-specific code is kept in the repeater application so upstream MeshCore
updates remain manageable. The RAK environment preserves the official RAK4631
repeater build flags, source set, display support, sensors, power management,
and boosted receive gain.

## Testing checklist

- Device boots and OLED remains stable.
- Repeater advert appears on the mesh.
- `!ping` on `#test` produces one reply.
- `!help` lists the utility commands on `#test` and is ignored on Public.
- `!roll` works on `#test` and is ignored on Public.
- `!rps` works on `#test` and is ignored on Public.
- `!rps3` sends exactly three throws on `#test` and ignores overlapping starts.
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
behaviour has been verified. Routed pings, dice and RPS commands, long-duration
stability, and scheduled announcements remain field-test items.

MeshCore and Loon are MIT-licensed; see [license.txt](license.txt). This fork
retains upstream copyright and attribution.
