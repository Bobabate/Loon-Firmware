# Start Here

This page is a simple map of the Loon Firmware repository. You do not need to
understand GitHub's folder structure to download or operate Loon.

## I want to install or update Loon

Open the **[Releases page](https://github.com/Bobabate/Loon-Firmware/releases)**.
That is where finished, installable firmware files live.

Do not use the green **Code** button. It downloads source code for developers,
not the normal firmware installation file.

For installation and recovery guidance, see
[Flashing and updating](README.md#flashing-and-updating).

## I want to configure Loon

Open [Loon administration commands](README.md#loon-administration-commands).
It lists the settings available through USB serial or authenticated MeshCore
Remote Management.

## I want to see what Loon can do

- [Features](README.md#features)
- [Bot commands](README.md#bot-commands)
- [Changes by version](CHANGELOG.md)

## I want to understand the files

The repository combines the upstream MeshCore project with Loon's additions.
That is why the Code page contains many folders. Most are inherited MeshCore
source and should remain in their upstream locations.

The files most relevant to Loon are:

- `README.md` — main project homepage, installation, commands, and configuration
- `LOON.md` — concise description of Loon-specific behaviour
- `CHANGELOG.md` — changes made in each Loon version
- `examples/simple_repeater/MyMesh.cpp` — primary Loon repeater-bot logic
- `examples/simple_repeater/MyMesh.h` — Loon repeater-bot declarations and settings
- `build_loon_heltec_v3.sh` — Heltec V3 build helper
- `build_loon_rak4631_repeater_mini.sh` — RAK4631 build helper

## GitHub terms in plain language

- **Repository:** the complete Loon project
- **Branch:** one working line of the project; Loon currently uses `loon/main`
- **Commit:** a saved source-code checkpoint
- **Tag:** a label attached to a particular checkpoint
- **Release:** a published version with firmware files to download
- **Source code:** the editable project files, not the firmware you install

When in doubt, return to the
**[Releases page](https://github.com/Bobabate/Loon-Firmware/releases)** to find
installable firmware.
