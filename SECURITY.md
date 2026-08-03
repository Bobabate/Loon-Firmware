# Security policy

## Supported versions

Security fixes are applied to the latest supported Loon release and current
development source. Experimental and diagnostic releases are unsupported.

| Version | Status |
| --- | --- |
| Current source | Supported for review and development |
| All previously published binaries | Withdrawn; do not install |

## Credentials and provisioning

Loon is public source code. Any password, key, or secret compiled into source
or a release must be considered public. Change the administrator password
before deploying a node and never reuse that password elsewhere.

Public and `#test` use standard MeshCore channel credentials. Do not put private
channel credentials, device identities, administrator passwords, or exported
device configuration in issues, logs, screenshots, commits, or build artifacts.

If a credential is accidentally published, rotate it on every affected device.
Removing it from the latest commit does not remove it from Git history or
previously downloaded firmware.

## Reporting a vulnerability

Do not report vulnerabilities in a public issue. Use GitHub private
vulnerability reporting from the repository's **Security** tab. Include the
affected version, hardware, impact, reproduction steps, and whether the issue
is remotely reachable over LoRa, USB, BLE, or Wi-Fi.

For vulnerabilities inherited unchanged from MeshCore, also consult upstream's
[security policy](https://github.com/meshcore-dev/MeshCore/security/policy).

## Scope

In scope:

- Authentication or authorization bypasses
- Exposure of private credentials or message contents
- Crafted radio packets causing memory corruption, persistent crashes, or
  unintended transmission
- Bot loops or amplification that materially increase mesh airtime
- Unsafe firmware-update or settings-migration behaviour

Generally out of scope:

- Radio jamming and unavoidable physical-layer interference
- Attacks requiring unrestricted physical access to an unprotected device
- Regulatory questions unrelated to a security defect
- Unmodified third-party dependencies, which should also be reported upstream

Loon is experimental firmware and comes without a security warranty.
