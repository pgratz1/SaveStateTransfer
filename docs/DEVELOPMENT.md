# Development loop

## Build options
* **CI (always works):** push to `main`; the `build` workflow uploads `SaveStateTransfer-<sha>` (staged mod folder).
  `gh run download <run-id> -D <empty dir>` then `tools/install.sh <dir>/SaveStateTransfer-<sha>`.
* **Local cross-compile:** needs an xwin splat at `~/xwin` (one-time, accepts Microsoft's license):
  `~/.cargo/bin/xwin --accept-license splat --output ~/xwin --include-debug-libs`
  then `cmake --preset linux-xwin-release && cmake --build --preset linux-xwin-release && tools/install.sh`.

## Test loop (FUS, Linux)
1. `tools/install.sh [stage] [profile]` with MO2 and the game closed (it edits the profile's modlist.txt).
2. Start Steam with `steam-nvidia`, start SteamVR with the headset, then
   `~/ExtraSpace/FUS/launch-fus-play.sh "FUS RO DAH (Basic + Appearance + Gameplay)"`.
3. Log: `~/ExtraSpace/SteamLibrary/steamapps/compatdata/611670/pfx/drive_c/users/steamuser/Documents/My Games/Skyrim VR/SKSE/SaveStateTransfer.log`
4. Headset-free triggers once a save is loaded:
   `touch ".../My Games/Skyrim VR/SaveStateTransfer/commands/export.cmd"` (or `import.cmd`, `dryrun.cmd`; first line = optional file name).
5. Hotkeys (keyboard, via WayVR or a real keyboard): LShift+F7 export, LShift+F8 import, LShift+F9 dry run; press twice within 2 s.
6. Crashes: `.../My Games/Skyrim VR/SKSE/Crashlogs/` (Crash Logger VR), `~/ExtraSpace/FUS/crashDumps/`.

## M0 acceptance (what the log must show)
* `SaveStateTransfer v0.1.0 loading; runtime 1.4.15.0 (VR)`
* `data loaded: N full plugins, M light plugins`, `round trip Skyrim.esm|0x13746 -> ... -> OK`, three `ESL round trip ... OK`
* `hotkeys installed`, `command-file watcher polling ...`
* after `touch export.cmd`: `export requested (source=command file ...)`

## Runtime gotchas (learned in M0)
* The Proton prefix ships msvcp140.dll 14.00.24215 (2016). STL headers >= 14.40 need `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR`
  or the first `std::mutex` lock faults (seen in `REL::Module::get()`); applied globally in CMakeLists.txt.
* CommonLibSSE-NG 3.7.0 reports 0 light plugins on VR. `core/LoadOrder` queries skyrimvresl's
  `GetCompiledFileCollectionExtern()` instead. Form ownership is derived from the form id's index bits, never from
  `TESForm::GetFile(0)` (which returned a late override for Skyrim.esm records).
* M0 accepted 2026-10-26: 138 full + 154 light plugins, all FormKey round trips OK.
