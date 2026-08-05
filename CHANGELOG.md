# Changelog

All notable Loon-specific changes are recorded here. Upstream MeshCore changes
are tracked in the upstream repository.

## Unreleased

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
