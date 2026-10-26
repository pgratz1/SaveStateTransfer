# SaveStateTransfer — SkyrimVR character + world-state export/import mod

## Context

Proteus exports a *character* (appearance, skills, perks, spells, inventory) between saves but leaves the *world* alone: quest progress, faction standing, bounties, globals, cleared dungeons, house ownership, who is dead. Carrying a mid-game character into a rebuilt mod list today means replaying dozens of hours or hand-writing hundreds of `setstage` console lines. Copying the save itself breaks when plugins differ.

SaveStateTransfer ("SST", working name) closes the gap: one SKSE DLL exports character **and** world state to a JSON file whose every form reference is load-order independent (`plugin name + local form ID`), and replays it into any other save, including a new game on a different mod list, skipping and reporting whatever no longer resolves.

Decisions already made with the user:
- SkyrimVR first (1.4.15, SKSEVR 2.0.12, VR Address Library 0.194 installed in FUS), built on **CommonLibSSE-NG** so the same DLL can run on SE/AE later.
- **Own character export**, no Proteus dependency.
- **Personal tool first**, structured for a later MCM + Nexus release. Hotkey/trigger-file/console driven, JSON files, minimal UI.
- World state in **two milestones**, schema covers both from day one:
  - M1: quests (ordered stage replay so fragments fire), factions + ranks, crime gold per faction, globals, cleared locations, player-home ownership (via quests/globals).
  - M2: unique-NPC dead/disabled state, persistent-reference enable state.
- Repo at `~/ExtraSpace/SaveStateTransfer`, public GitHub repo (MIT) with a Windows Actions build as fallback and future release build.

Environment facts (surveyed):
- Dev machine: Ubuntu/KDE Wayland. Game via Proton Experimental + MO2 portable at `/home/pgratz/ExtraSpace/FUS`; mods are `mods/<Name>/`, enabled via `profiles/<profile>/modlist.txt`. Headset-free launch: `/home/pgratz/ExtraSpace/FUS/launch-fus-play.sh "<profile>"`.
- SKSE logs: `/home/pgratz/ExtraSpace/SteamLibrary/steamapps/compatdata/611670/pfx/drive_c/users/steamuser/Documents/My Games/Skyrim VR/SKSE/`. Crash Logger VR installed (Crashlogs under the same SKSE dir); MO2 dumps in `FUS/crashDumps/`.
- FUS provides: po3 Papyrus Extender VR, PapyrusUtil VR, JContainers VR, ConsoleUtilVR, FISSES, MCM Helper 1.5, SkyUI VR, Skyrim VR Tools, Engine Fixes VR, Papyrus Tweaks NG, VRIK, Skyrim VR ESL Support (286 plugins enabled, ESL-flagged esp heavy). **Not** present: RaceMenu/NiOverride (appearance = vanilla TESNPC face data), Proteus, Creation Kit, Papyrus compiler, vanilla script sources.
- C++ on Linux: cmake 4.2, clang/clang-cl 21 (+ llvm-lib/rc/mt), rust/cargo, python 3.14. Missing: ninja, lld-link, MSVC CRT/SDK (xwin), vcpkg. ⇒ **DLL-only, Papyrus-free, ESP-free design**; Papyrus becomes unavoidable only for a real MCM with buttons (M3).

## Architecture

Single `SaveStateTransfer.dll` (CommonLibSSE-NG, `ENABLE_SKYRIM_VR=ON` plus SE/AE, runtime branch via `REL::Module::IsVR()` only where needed). No SKSE co-save: the JSON file is the only artifact, zero save footprint.

| Module (`src/<dir>`) | Responsibility |
|---|---|
| `core` | `FormKey{plugin, localId}` ⇄ `TESForm*` (ESL-aware); JSON io (nlohmann/json); paths; INI config (SimpleIni); multi-frame `TaskQueue` on `SKSE::GetTaskInterface()`; `Report`; `PapyrusBridge` (call Papyrus natives from C++ via `IVirtualMachine::DispatchStaticCall`, escape hatch for e.g. `Quest.SetObjectiveCompleted`, `Game.TeachWord`) |
| `character` | appearance, level/skills/xp/perks, spells/shouts/words, inventory + equipment + favorites, actor values, dragon souls |
| `world` (M1) | quests (ordered replay + filter/overrides), factions/ranks/crime gold, globals, cleared locations |
| `world` (M2) | unique-NPC dead/disabled, persistent-ref enable state |
| `trigger` | keyboard hotkeys (`BSInputDeviceManager` sink), trigger-file poll thread, (M3) console command |
| `app` | `Exporter`/`Importer` orchestrators, dry-run, report writer |

Repo layout:
```
~/ExtraSpace/SaveStateTransfer/
  CMakeLists.txt  CMakePresets.json  vcpkg.json  LICENSE (MIT)  README.md
  cmake/toolchain-clang-cl-xwin.cmake
  cmake/triplets/x64-windows-clangcl-static-md.cmake
  src/main.cpp                      # SKSEPluginLoad, messaging (kDataLoaded, kPostLoadGame), module init
  src/core/{FormKey,Json,Paths,Config,TaskQueue,Report,PapyrusBridge}.{h,cpp}
  src/character/{Appearance,Progression,Magic,Inventory,CharacterExporter,CharacterImporter}.cpp
  src/world/{Quests,QuestFilter,Factions,Globals,Locations,Actors,References}.cpp
  src/trigger/{Hotkeys,TriggerFile,Console}.cpp
  src/app/{Exporter,Importer,Schema.h}.cpp
  dist/SKSE/Plugins/SaveStateTransfer.ini
  dist/SKSE/Plugins/SaveStateTransfer/overrides/{vanilla,dlc,fus}.json
  tools/{validate_export.py,diff_exports.py,install.sh}  tools/schema/v1.schema.json
  fixtures/                         # sample exports + expected dry-run reports (regression)
  .github/workflows/build.yml
```

Runtime file locations:
- Read-only config shipped in the mod folder: `Data/SKSE/Plugins/SaveStateTransfer.ini`, `Data/SKSE/Plugins/SaveStateTransfer/overrides/*.json`.
- Read/write (outside MO2's virtual FS so Linux tools see them): `<prefix>/.../Documents/My Games/Skyrim VR/SaveStateTransfer/{exports,reports,commands}/`, derived from `SKSE::log::log_directory()->parent_path()`.

## JSON schema v1 (compact)

```jsonc
{
  "schema": 1,
  "tool": { "name": "SaveStateTransfer", "version": "0.1.0", "runtime": "VR 1.4.15" },
  "source": { "exportedAt": "...", "saveName": "...", "playerName": "...", "loadOrder": ["Skyrim.esm", "..."] },  // diagnostics only
  "character": {
    "name": "...", "race": {"plugin":"Skyrim.esm","id":"0x13746"}, "female": true, "height": 1.0, "weight": 40.0,
    "appearance": { "headParts": [FormKey], "faceMorphs": [19 floats], "faceParts": [ints],
                    "hairColor": FormKey, "bodyTint": "#RRGGBB", "tints": [{"index":1,"color":"#..","alpha":0.6,"preset":0}] },
    "progression": { "level": 31, "xp": 1250.0, "xpThreshold": 3100.0, "perkPoints": 2,
                     "skills": { "OneHanded": {"level":62,"xp":120.5,"threshold":1800.0,"legendary":0} },
                     "attributes": {"Health":250,"Magicka":180,"Stamina":150}, "dragonSouls": 7,
                     "perks": [FormKey] },                                     // rank-chain order
    "magic": { "spells": [FormKey], "shouts": [FormKey], "wordsKnown": [FormKey], "wordsUnlocked": [FormKey],
               "favorites": { "spells": [FormKey], "hotkeys": [null, FormKey, ...] } },
    "inventory": { "gold": 15230,
                   "items": [{ "form": FormKey, "count": 1, "health": 1.24, "charge": 300.0, "soul": 0, "name": "...",
                               "equippedSlot": "right", "favorite": true,
                               "customEnchant": { "effects": [{"effect": FormKey, "mag": 25, "dur": 0, "area": 0}] } }],
                   "unresolvable": [{"formID":"0xFF00123A","name":"...","reason":"dynamic form"}] }
  },
  "world": {
    "quests": [{ "form": FormKey, "editorId": "MQ103", "running": true, "completed": false, "stopped": false,
                 "currentStage": 60, "doneStages": [10,20,30,40,60],
                 "objectives": [{"index":10,"state":"completed"},{"index":20,"state":"displayed"}] }],
    "factions": [{ "form": FormKey, "rank": 2, "crimeGold": 40, "crimeGoldViolent": 0 }],
    "globals": [{ "form": FormKey, "value": 1.0 }],
    "locationsCleared": [FormKey],
    "actors": [{ "form": FormKey, "dead": true, "disabled": false }],          // M2
    "references": [{ "form": FormKey, "enabled": false }]                     // M2, only where state != plugin default
  }
}
```
Rules: `FormKey = {"plugin": "<file>", "id": "0x<local>"}`; local id is 24-bit for full plugins, 12-bit for ESL-flagged. Unknown keys ignored on import. `schema` bumps only on breaking changes. Report file `<export>.report.json` mirrors sections with `skipped[]`, `missing[]`, `overridden[]`, `errors[]`, `verify[]`, each with `reason`.

## Key design decisions

- **Form identity**: owner = `form->GetFile(0)`; `localId = file->IsLight() ? id & 0xFFF : id & 0xFFFFFF`. Resolve with `TESDataHandler::GetSingleton()->LookupForm(localId, plugin)` (case-insensitive plugin match). `IsDynamicForm()` (0xFF) forms never become FormKeys. Verify ESL round-trip in M0.
- **Enumeration**: `TESDataHandler::GetFormArray<TESQuest|TESGlobal|TESFaction|BGSLocation>()`; `TESForm::GetAllForms()` (locked map) for M2 actors (unique TESNPC base) and persistent refs. No pre-scan tool needed.
- **Triggers**: hotkeys from INI (default F7 export / F8 import, modifier required, second press within 2 s to confirm) via `BSInputDeviceManager::AddEventSink`; plus a 1 s poll thread watching `commands/export.cmd|import.cmd|dryrun.cmd` (optional content = export filename) posting work via `AddTask`. Console command `sst export|import|dryrun <file>` in M3 by replacing an unused command found with `SCRIPT_FUNCTION::LocateConsoleCommand`.
- **Import pacing**: `Importer` is a state machine on `TaskQueue`; each `SetStage` is its own task, queue yields `iFramesBetweenStages` frames (default 3) so Papyrus fragments run. Preconditions: player loaded, no menu open, not in combat; recommend interior cell + fresh save.
- **Apply order**: race/sex/appearance → progression → magic → inventory → quests (replay) → factions → globals (after quests, fragments write globals) → locations → M2 actors/refs → objective display states → report.
- **Quest replay**: for each quest passing the filter, if `running || completed`: `Start()` if needed; for each `doneStages` ascending: skip if target already has it done (guards against cascades), else `SetStage`; then ensure completing stage set, or `Stop()` only for `stopped && !completed`. Objective states last via `PapyrusBridge`.
- **Quest default filter** (export records everything, filter applies at import): skip if (a) story-manager event quest (radiant/instanced), (b) `QuestType == kMiscellaneous`, (c) Start-Game-Enabled and target already at ≥ exported stage, (d) override `"mode":"skip"`. Overrides: `skip`, `onlyStages:[...]`, `finalOnly:true`, editor-ID glob patterns. Seed `overrides/vanilla.json` with MQ101 (`finalOnly`), MQ102, DLC1/DLC2 init quests, `DialogueFavorGeneric`, `WI*`, `DGIntimidate*`, `BQ*`, `CR*`. Dry-run writes the full planned action list without touching the game.
- **Player homes**: no separate mechanism; purchase state is quests + globals. Report lists `HousePurchase*`/`BYOH*` outcomes under `verify[]`.
- **FF forms** (custom enchants, player potions): exported into `unresolvable[]` with names in M1a; recreated in M3 via `BGSCreatedObjectManager` + `ExtraDataList::SetEnchantment`.
- **JSON library**: nlohmann/json (safe under clang-cl cross-compile).

## Milestones and acceptance criteria

**M0 — Toolchain + hello world** (1–2 days)
- Linux cross-build produces `SaveStateTransfer.dll`; `tools/install.sh` copies DLL + `dist/` to `/home/pgratz/ExtraSpace/FUS/mods/SaveStateTransfer/` and inserts `+SaveStateTransfer` into the chosen profile's `modlist.txt` if absent (MO2 closed; no `plugins.txt` change, no esp).
- Launch via `launch-fus-play.sh`; `SKSE/SaveStateTransfer.log` shows version, runtime 1.4.15, a `kDataLoaded` line with plugin count (286) and one resolved form from an ESL-flagged plugin.
- Hotkey press and `commands/export.cmd` both log "export requested". GitHub Actions produces the same DLL as an artifact.

**M1a — Character** (~1 week)
- Export from an RO DAH save writes `exports/<player>_<timestamp>.json` passing `validate_export.py`.
- Import into a fresh game on "FUS (Basic)": `player.getlevel`, `player.getav onehanded`, `player.hasperk`, spell list, gold, equipped gear match; face visibly the same (head parts, tints, weight); report lists every item whose plugin is absent in Basic.
- Round-trip: re-export from the imported game; `diff_exports.py` shows only expected diffs (FF items, missing-plugin items).

**M1b — World M1** (1–2 weeks)
- Dry-run report lists planned actions; nothing in the vanilla override list is replayed with intermediate stages.
- After import: `GetStage` for MQ/C00/etc. match; `player.getcrimegold` per faction matches; 10 sampled `GetGlobalValue` match; 5 sampled `IsLocationCleared` true; Breezehome ownership consistent after replay (`verify[]`).
- A 60-hour save imports without crash in under ~2 min game time; Papyrus log shows no replay-attributable stack dumps.

**M2 — NPC and reference state** (~1 week)
- Export has `actors[]` for every unique-NPC actor in memory (dead/disabled) and `references[]` only for non-FF refs whose enable state differs from plugin default and which have no enable parent.
- After import, sampled dead NPCs are dead; sampled refs match; nothing touched a ref from a missing plugin (`missing[]`).

**M3 — Polish (stub)**: console command; overrides library for FUS plugins; custom-enchant recreation; MCM Helper page (INI-backed settings + toggle-as-button, needs a tiny esp with MCM Helper's `MCM_ConfigBase`; real buttons = first Papyrus); Nexus FOMOD packaging (single DLL for VR/SE/AE).

## Build toolchain

Primary — Linux cross-compile:
1. `sudo apt install ninja-build lld-21` (symlink `lld-link`); `cargo install xwin`; clone + bootstrap vcpkg to `~/vcpkg`.
2. `xwin --accept-license splat --output ~/xwin --include-debug-libs` (CRT + SDK with case-fixing symlinks).
3. `cmake/toolchain-clang-cl-xwin.cmake`: `CMAKE_SYSTEM_NAME Windows`, `AMD64`, `clang-cl`/`lld-link`/`llvm-rc`/`llvm-mt`; flags `--target=x86_64-pc-windows-msvc /vctoolsdir ~/xwin/crt /winsdkdir ~/xwin/sdk /winsdkversion <ver>`; `/std:c++latest /permissive- /Zc:preprocessor /EHsc`; libpaths for `crt/lib/x86_64`, `sdk/lib/um/x86_64`, `sdk/lib/ucrt/x86_64`.
4. CommonLibSSE-NG via CPM/FetchContent at a pinned tag with `ENABLE_SKYRIM_VR/SE/AE=ON`, `SKSE_SUPPORT_XBYAK=OFF`. Leaf deps (spdlog/fmt, binary_io, rapidcsv, DirectXMath, DirectXTK) and ours (nlohmann-json, simpleini) via vcpkg manifest with overlay triplet `x64-windows-clangcl-static-md` (chainloads the toolchain; `VCPKG_CRT_LINKAGE dynamic`, `VCPKG_LIBRARY_LINKAGE static`). Caveat: Linux-host → `x64-windows*` is community-unsupported; DirectXTK is the likely fight. **Time-box one day**, then fall back.
5. `CMakePresets.json`: `linux-xwin-release`, `windows-msvc-release` (CI). Post-build stages `build/stage/`.

Fallback / release — GitHub Actions `windows-latest`: `lukka/run-vcpkg` + `lukka/run-cmake` with the MSVC preset; artifact `SaveStateTransfer-<sha>.zip` (DLL + `dist/`). On Linux: `gh run download -n ... -D <empty dir>` then `tools/install.sh <dir>`.

## Test strategy / verification

- **Source saves**: `FUS/profiles/FUS RO DAH (Basic + Appearance + Gameplay)/saves/` (~348 pairs, "Adventurer"). Pick early (Bleak Falls), mid (College), latest.
- **Target**: new game on "FUS (Basic)" (fewer mods → exercises missing-plugin paths). Reach an interior, save, drop `commands/dryrun.cmd` → inspect report → `commands/import.cmd`. Hotkeys for in-headset use.
- **Checks**: report JSON (`missing[]` ⊆ plugins absent in Basic); SKSE log; console sanity (`GetStage`, `player.getav`, `player.getcrimegold`, `GetGlobalValue`, `IsLocationCleared`, `player.hasperk`); visual appearance; round-trip `diff_exports.py`.
- **Linux tooling** (run with `python3 -I`): `validate_export.py` (jsonschema + FormKey format + predicts `missing[]` against a target profile's `plugins.txt`), `diff_exports.py`; `fixtures/` regression test asserting dry-run action lists unchanged when filter code changes.
- **Crashes**: Crash Logger VR output in the SKSE dir; `[Debug] bLogImportSteps=1` so the last logged step names the offending quest/item.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| VR address IDs differ from SE | CommonLibSSE-NG + VR Address Library; only NG-wrapped relocations with VR entries; M0 verifies with real calls. |
| Fragments misbehave on replay | Dry-run; default filter; overrides library; `IsStageDone` guard; frame yielding; interior + save first; `verify[]` for manual follow-up. |
| FF forms | Never FormKeys; `unresolvable[]`; M3 recreation. |
| Modded items missing in target | Resolve-or-skip + `missing[]`; validator predicts before launch. |
| Papyrus VM overload on import | Multi-frame queue, `iFramesBetweenStages`; Papyrus Tweaks NG present. |
| ESL local IDs in VR | NG's VR ESL integration; explicit M0 round-trip; plugin name always stored. |
| Proton paths / case | Write under `My Games/Skyrim VR/SaveStateTransfer/`; case-insensitive plugin compare; ASCII names; wide paths. |
| clang-cl + xwin build fails | One-day time-box → GitHub Actions. |
| Hotkey misfires in VR | Modifier + confirm press; trigger files when headset-free. |
| Race/sex change vs VRIK | Apply appearance first, `DoReset3D`; document possible VRIK recalibration. |

## Critical files
- `src/core/FormKey.cpp` — ESL-aware identity; everything depends on it.
- `src/world/Quests.cpp`, `src/world/QuestFilter.cpp` — replay, filter, overrides.
- `src/app/Importer.cpp`, `src/core/TaskQueue.cpp` — multi-frame pipeline, ordering, report.
- `cmake/toolchain-clang-cl-xwin.cmake`, `.github/workflows/build.yml` — primary and fallback builds.
- Install target: `/home/pgratz/ExtraSpace/FUS/mods/SaveStateTransfer/SKSE/Plugins/SaveStateTransfer.dll`.
