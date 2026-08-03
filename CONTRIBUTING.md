# Contributing to Loon Firmware

Thanks for helping improve Loon. The project aims to remain a quiet, reliable
MeshCore repeater first and a small autonomous bot second.

## Before opening an issue

- Use the latest supported release or current source.
- Confirm the board and firmware filename.
- Keep a recovery image available.
- Remove passwords, private keys, private channel credentials, precise private
  locations, and unrelated message content from logs and screenshots.
- Check whether the problem also occurs with the corresponding upstream
  MeshCore repeater firmware.

Use the bug-report template for failures and the feature-request template for
new behaviour. Security problems belong in private vulnerability reporting,
not public issues.

## Pull requests

- Base Loon changes on `loon/main`.
- Keep Loon-specific changes isolated from MeshCore core code where practical.
- Preserve normal repeater behaviour and airtime priority.
- Avoid dynamic allocation during normal operation.
- Bound buffers, queues, histories, and persisted data.
- Document new public and administration commands.
- Do not commit generated firmware, local configuration, credentials, or device
  identity files.
- Do not add unsolicited public transmissions.
- Include the hardware and test procedure used.

Large features should begin with an issue describing the mesh benefit, airtime
cost, privacy impact, memory cost, failure behaviour, and why existing MeshCore
functionality is insufficient.

## Builds and releases

Source changes do not imply a firmware release. Maintainer approval is required
before compiling release artifacts, tagging a version, or publishing binaries.

## Upstream relationship

Protocol and generally useful core fixes should be proposed to
[meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore) when
appropriate. Loon-specific bot behaviour belongs in this repository.
