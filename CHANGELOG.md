# Changelog

All notable Loon-specific changes are recorded here. Upstream MeshCore changes
are tracked in the upstream repository.

## 1.17.1-L9 - 2026-08-25

- Add the inbound repeater-hop count to the optional Discord webhook path line,
  using `Hops: N | Path: ...`. `wifi.webhook.path on|off` controls the complete
  hops-and-path line; direct receptions display `Hops: 0 | Path: direct`.

## 1.17.1-L8 - 2026-08-25

- Remove the `!about` command entirely.
- Put the repeater-hop count before the route in ping replies using
  `Hops: N | Path: ...`. The count includes only the inbound repeater hashes;
  the receiving companion endpoint does not add a hop.

## 1.17.1-L7 - 2026-08-25

- Add an authenticated persistent `loon.help.test on|off` setting that controls
  only `!help` on `#test`. It defaults to off on clean installations and does
  not affect any other command or Public-channel behavior. Application-image
  updates preserve all existing Loon preferences and initialize the new help
  control to off.

## 1.17.1-L6 - 2026-08-22

- Add an authenticated `wifi.webhook.path on|off` setting for exact inbound
  MeshCore path IDs on Heltec V3 Discord webhook posts. It defaults to off and
  preserves existing webhook configuration during application-image updates.
  Loon ping replies identify enabled path details as the request path, and
  direct receptions are labelled `direct`.

## 1.17.1-L5 - 2026-08-21

- Restore authenticated Wi-Fi OTA mode in the Heltec V3 Loon target.
- Remove `meshcore.io` from the Loon boot screen and show the complete Loon
  version and build date on separate readable lines.

## 1.17.1-L4 - 2026-08-21

- Default `!roll`, `!rps`, and `!rps3` to off on clean/full installations.
  Existing saved Loon preferences remain unchanged during application-image
  updates.
- Display clean Loon version names such as `Loon v1.17.1-L4` without a Git
  commit suffix. Manual workflow builds retain their workflow identifier but
  are also clearly identified as Loon firmware.
- Update public project links for the GitHub username `Bobabate`.

## 1.17.1-L3 - 2026-08-20

- Add a shared Loon firmware target and GitHub Actions selections for the Seeed
  SenseCAP Solar Node P1 and P1 Pro, preserving the upstream SenseCAP Solar
  board, radio, GPS, sensor, and power configuration.
- Add Loon firmware targets and GitHub Actions selections for both the
  display-equipped and displayless Heltec T114, preserving each variant's
  upstream MeshCore board, radio, display, and power configuration.
- Make clean/full Loon installations use the ordinary upstream MeshCore radio
  defaults instead of a preloaded USA/Canada preset. Retain MeshCore's standard
  public development administrator password for initial provisioning.
  Application-image updates continue to preserve the device's existing radio
  settings and password.
- Add persistent authenticated `loon.roll on|off` and `loon.rps on|off`
  controls. Disabling RPS covers both RPS commands and cancels an active
  three-round game.

## 1.17.1-L2 - 2026-08-18

- Replace `Pong` with the 🏓 emoji in `!ping` replies.
- Add `!rps` on `#test`, returning a random rock, paper, or scissors emoji.
- Add non-blocking `!rps3` best-of-three games with three random throws at
  10-second intervals, compact round labels, and one active game at a time.

## 1.17.1-L1 - 2026-08-17

- Prevent duplicate Discord webhook posts when Discord accepts a complete request but its HTTP response is lost or malformed.
- Simplify `!roll` replies to a dice emoji followed by the result.
- Replace the Loon normal display's radio parameters with node name, Wi-Fi IP,
  uptime, and current busy percentage.
- Default Public and `#test` pings and scheduled announcements to off for a
  clean/full-image installation. Normal application-image updates preserve
  the device's existing settings.

## 0.2.0-rc3 - 2026-08-17

- Fix Discord webhook forwarding from the standard Public channel by using
  MeshCore's fixed Public channel key instead of deriving a key from its name.

## 0.2.0-rc2 - 2026-08-16

- Add an optional ESP32 MeshCore-to-Discord webhook bridge.
- Forward one explicitly configured standard channel, with no `all` mode.
- Present each MeshCore message as one Discord post using its sender name.
- Store Wi-Fi, webhook, and channel settings in checksummed device preferences.
- Add a bounded RAM queue, retry pacing, Wi-Fi reconnect, safe mention handling,
  and authenticated CLI configuration commands.
- Keep non-ESP32 targets building without webhook support.

## 0.2.0-rc1 - 2026-08-15

- Update the Heltec V3 Loon target to the MeshCore v1.17.1 source revision
  (`d929643`) without changing Loon's commands or bot behaviour.

## 0.1.5 - 2026-08-07

- Prefix every command reply with the configured node name so MeshCore renders
  ping, help, roll, and about under one consistent sender identity.
- Remove the dice emoji from the roll title so it cannot be mistaken for a
  separate sender identity.

- Give Public and `#test` independent custom announcement text.
- Migrate an existing shared custom message to both channels; clearing either
  channel's text restores its compact status announcement.
- Add an additive RAK4631 repeater target for the WisMesh Repeater Mini while
  preserving the complete official MeshCore RAK repeater configuration.
- Add a RAK4631 build helper and document both supported hardware targets.
- Prefix custom scheduled announcements with the configured node name so
  MeshCore renders them as normal group messages.
- Add the node-name prefix without exceeding the encrypted group-text payload.

## 0.1.4 - 2026-08-02

- Restrict `!roll` to one six-sided die.
- Replace the global/60-second cooldowns with a 10-second per-sender cooldown.
- Make `!help` loop-safe by omitting complete command tokens from its response.
- Restrict Public commands to `!ping`; keep help, about, and roll on `#test`.
- Reject custom announcements beginning with `!` and safely clear legacy
  values that could trigger another bot.

## 0.1.3 - 2026-08-02

- Add `!help` with the public command list.
- Add `!about` with firmware version and the standard MeshCore owner message.
- Add `!roll` on `#test` only.
- Generalize bot-command rate limiting beyond ping.
- Improve public-project security and contribution documentation.
