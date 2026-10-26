# SaveStateTransfer

An SKSE plugin for Skyrim VR (and, via CommonLibSSE-NG, SE/AE) that exports a player
character **and** world state (quests, factions, bounties, globals, cleared locations,
unique-NPC state) to a JSON file and imports it into another save game, even one running a
different mod list. Think "Proteus for the character, plus the world".

Form references are stored as `plugin name + local form ID`, so they survive load-order
changes. Anything that no longer resolves is skipped and listed in a report file.

Status: milestone M0 (toolchain + hello-world plugin). See `docs/PLAN.md`.

## Build (Linux cross-compile)

Requirements: clang-cl 21, lld-link, ninja, cmake >= 3.28, `xwin` (`cargo install xwin`) with a
splat at `~/xwin` (`xwin --accept-license splat --output ~/xwin`).

    cmake --preset linux-xwin-release
    cmake --build --preset linux-xwin-release
    tools/install.sh            # copies into the MO2 mods folder and enables the mod

## Build (Windows / CI)

    cmake --preset windows-msvc-release
    cmake --build --preset windows-msvc-release

## Runtime files

* Config: `Data/SKSE/Plugins/SaveStateTransfer.ini`
* Overrides: `Data/SKSE/Plugins/SaveStateTransfer/overrides/*.json`
* Exports / reports / command files: `My Games/Skyrim VR/SaveStateTransfer/{exports,reports,commands}/`

Trigger an export headset-free: `touch ".../My Games/Skyrim VR/SaveStateTransfer/commands/export.cmd"`.

## License

MIT.
