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

## Crash diagnosis (2026-11-04)

- Crash Logger VR is enabled in FUS but SKSE reports it "disabled, fatal error occurred while loading plugin" under
  Proton, so there are never crash logs. The plugin's own `core/CrashProbe` (vectored exception handler) writes
  `FATAL exception ... last step: ...` plus a probable call stack into `SaveStateTransfer.log` instead.
- Every sequencer step carries a label; with `[Debug] bLogImportSteps=1` the label is logged right before the step
  runs, so the last logged line names the step that crashed.
- First real import (2026-11-04) died with no crash log while the game was paused (headset off), right after all
  console calls failed with "script factory unavailable" (VR: the SE-mapped "factories initialized" flag is false).
  Fixes: character phases moved into the sequencer (nothing runs while paused), console factory fallback, equip no
  longer reuses the ExtraDataList after AddObjectToContainer.

## Gotcha: sizeof(RE::ExtraDataList) == 1

In a multi-runtime NG build `RE::ExtraDataList` (and `BaseExtraList`) have no data members, because the AE 1.6.629+
layout differs; members are reached through `GetData()/GetPresence()/GetLock()` with runtime offsets. So
`RE::calloc<RE::ExtraDataList>(1)` allocates ONE byte. The engine then treats it as a 0x18-byte list and corrupts
whatever follows on the heap (both import freezes in `ExtraDataList::Add` came from this). Always create lists with
`core::NewExtraDataList()` (`src/core/ExtraList.h`), which allocates the engine size; the `heapcheck` lines logged on
save load replay the sequence and must end with `replay OK`.

## Gotcha: never re-queue an SKSE task from inside a task

SKSE drains its task queue until it is empty within one frame, so a task that re-adds itself (the old sequencer's
"wait while paused" / "wait N frames" logic) spins the main thread at 100 % and the game freezes. The sequencer now
has a background pacer thread that posts one step at a time and sleeps between posts (12 ms per "frame", 250 ms
polls while the game is paused).
