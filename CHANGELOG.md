# Changelog

All notable Loon-specific changes are recorded here. Upstream MeshCore changes
are tracked in the upstream repository.

## Unreleased

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
