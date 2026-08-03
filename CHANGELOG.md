# Changelog

All notable Loon-specific changes are recorded here. Upstream MeshCore changes
are tracked in the upstream repository.

## Unreleased

## 0.1.3 - 2026-08-02

- Withdraw all early binaries after their development administrator password
  became public; rotate credentials on affected devices.
- Add `!help` with the public command list.
- Add `!about` with firmware version and the standard MeshCore owner message.
- Add `!roll` and bounded `NdM` dice notation on `#test` only.
- Generalize bot-command rate limiting beyond ping.
- Improve public-project security and contribution documentation.

## 0.1.2 - 2026-08-02

Field-test prerelease.

- Add configurable scheduled announcement text.
- Use compact status when custom announcement text is empty.
- Remove `Errors` from the status announcement.
- Migrate Loon 0.1.1 settings.

## 0.1.1 - 2026-08-02

Hardware-tested baseline.

- Add autonomous `!ping` on Public and `#test`.
- Report inbound hop hashes, RSSI, SNR, and Busy percentage.
- Add scheduled hourly/daily announcements.
- Add persistent Loon configuration and airtime protection.

## 0.1.0 - 2026-08-02

- Initial development build; withdrawn because its factory image did not boot
  reliably on the test hardware.
