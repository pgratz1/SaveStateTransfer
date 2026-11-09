# Handoff — 2026-11-08 (work paused)

Read this first, then `docs/DEVELOPMENT.md` (engine/toolchain gotchas) and `docs/PLAN.md` (milestones).

## Where we are

* **M0, M1a (character), M1b (world: quests, factions, globals, cleared locations) are built and working.** The real
  import of the RO DAH level-28 "Adventurer" export into the "FUS (Basic)" Helgen save has completed twice
  (~27 s, 378 actions, 0 errors, no crash).
* **Installed build = git HEAD `aa80645`** in both profiles ("FUS RO DAH (Basic + Appearance + Gameplay)" and
  "FUS (Basic)"); the installed DLL is byte-identical to `build/linux-xwin-release/stage`. Everything up to
  `aa80645` is pushed to `pgratz1/SaveStateTransfer`.
* **`aa80645` has NOT been tested in game.** It holds the fixes for the three problems seen in the second import
  (next section). That re-test is the next thing to do.
* M2 (unique-NPC dead/disabled, persistent-ref enable state) and M3 (console command, MCM, FF-form recreation,
  packaging) are not started.

## Open problems from the second import, with the untested fixes

| Seen in game | Cause (as understood) | Fix in `aa80645` | How to verify |
|---|---|---|---|
| Health and magicka maxima wrong (level was right) | `SetBaseActorValue` does not move the player's max; race/level formula wins | Export stores `GetPermanentActorValue`; import applies `ModActorValue(av, target - permanent)`, refills, and repeats as the last step (`character::ApplyAttributes`) | Log lines `attributes: Health permanent X -> Y (target 260)`; targets are 260 health / 220 magicka. If Y != target the log shows what the engine did |
| One early quest still open in the journal | Probably MQ102 "Before the Storm", which was on the skip list | `overrides/vanilla.json`: MQ102 is now `finalOnly` | Journal after import; `GetStage MQ102` |
| A completed quest still "tracked" and impossible to deselect | Objective states were applied only to running quests, so a displayed objective stayed on a completed quest | Objectives are applied for running AND completed quests, with an explicit `setobjectivedisplayed <idx> 0` for hidden ones | Journal after import |

The user did not name the two quests. If either problem is still there after the re-test, ask which quests they were
and match them against the `quest <editorId> ...` step labels in the log.

Already confirmed working in the second import: perk-point refund for perks whose plugin is absent
(`bRefundMissingPerks`), return to the pre-import position (`bRestorePosition`), natural editor-ID quest order.

Expected misses on Basic (not bugs): spells from MysticismMagic/Adamant/BladeAndBlunt, perks from Sentinel.esp,
globals and quests from other absent plugins. The import log ends with a per-plugin summary of them.

## Next step: the re-test

1. `~/ExtraSpace/FUS/launch-fus-play.sh "FUS (Basic)"`, then **Continue**. The newest save in
   `FUS/profiles/FUS (Basic)/saves/` is `Save2_..._Tamriel_000021_20261104024809_1_1` (level 1, outside Helgen), the
   clean test target. The two post-import saves were moved to `FUS/_linux-port-backup/removed-saves/` so Continue
   does not pick them; do the same with any new post-import save before the next test.
2. `~/ExtraSpace/SaveStateTransfer/tools/sst.sh import` with the headset ON (the sequencer waits while the game is
   paused). It imports the newest export, `exports/Adventurer_20261105-014439.json`.
3. Wait for the "SaveStateTransfer: imported ..." notification. Check health/magicka maxima, the journal, position,
   perk points. Save, quit, then `tools/sst.sh kill` if `SkyrimVR.exe` lingers.
4. Read `tools/sst.sh log 200` (the `attributes:` lines, any `FATAL`/error) and `tools/sst.sh report`
   (`reports/Adventurer_20261105-014439.import.report.json`).

After that passes: re-export from the imported game and compare with `tools/diff_exports.py`; expected differences
are forms from plugins absent in Basic.

## Backlog (after the re-test)

* `tools/validate_export.py --target-plugins` under-predicts missing plugins: `plugins.txt` also lists plugins of
  disabled mods, so it must consult `modlist.txt` too.
* Cleanup: drop `core/BootTrace` from release builds, delete `src/world/QuestDiag.cpp`, gate `core/HeapCheck` behind
  a debug INI switch (it runs on every save load).
* `core/CrashProbe` logs every first-chance access violation in the process as `FATAL`, including ones other mods
  handle themselves. Relabel the idle (no current step) case, or log it only when the process actually dies.
* M2, then M3, per `docs/PLAN.md`.

## Things that will bite again

* **Crash Logger VR does not load under Proton.** `core/CrashProbe` is the only crash information: module + RVA,
  a stack scan, and the last sequencer step. Symbolize our frames with
  `llvm-symbolizer --obj=<dll> --relative-address` (image base 0x180000000).
* **`sizeof(RE::ExtraDataList) == 1` in multi-runtime NG.** Never `new` it; use `core::NewExtraDataList()` (0x18 bytes).
* **Never re-queue an SKSE task from inside a task.** The queue drains until empty in one frame, so it spins the main
  thread. The sequencer is paced by a background thread.
* **`BGSDefaultObjectManager::GetObject` is bad on VR.** Use form IDs (hand equip slots 0x13F42 / 0x13F43).
* The form factory is unavailable while the game is paused; nothing in the import runs while paused. Export and dry
  run do work while paused.
* `TESQuest::executedStages` is NOT the stage history; `waitingStages` is the stage table, flags bit0 = done.
* `PlayerCharacter::GetTintList()` is null on VR; tints are read at SKSEVR offsets 0x1208/0x1220.
* Skyrim.esm 0x1F4 is the hidden "Unarmed" weapon; skipped on export and import. Imported items are not re-equipped
  (user decision).
* `gdb` attach needs sudo (`ptrace_scope=1`), so the user runs it; the game exe on disk is Steam-encrypted, so there
  is no offline disassembly of engine code.
* `tools/install.sh` refuses while MO2 or the game runs (also if the invoking command line itself contains those
  process names), and MO2's `modlist.txt` is CRLF.
* `SkyrimVR.exe` often lingers after quit (an exit crash in another mod, not ours): `tools/sst.sh kill`.

## FUS-side changes made during this work

* Proton Experimental 11 deletes the unmanaged `S:` drive at every launch. `launch-fus-play.sh` now sets
  `PROTON_SET_GAME_DRIVE=1`, `STEAM_COMPAT_INSTALL_PATH` and `STEAM_COMPAT_LIBRARY_PATHS`.
* MO2 rewrites `ModOrganizer.ini` with defaults when it fails to open the instance. The launcher restores
  `_linux-port-backup/ModOrganizer.ini.linux-good` when the "PLAY FUS" executable entry is missing.
* Both are written up in `FUS/LINUX-PORT-NOTES.md` ("2026-11-05: Proton Experimental update dropped the S: drive").

## Last observed state (2026-11-08)

* No import has been run since `aa80645` was installed on 2026-11-05. `commands/` is empty (nothing pending).
* The plugin log is from a 3-minute session on 2026-11-08 (19:02-19:05, 292 plugins, one save loaded, heap self-test
  `replay OK`). It contains two `FATAL` probe entries with `last step: <none>`, neither in our DLL: one in
  `MSVCP140.dll` called from `Subtitles.dll` at startup (the game carried on loading), and one in `tbbmalloc.dll`
  with `EngineFixesVR.dll` on the stack at 19:05:53, the last line of the log. Whether that second one was a
  crash or the usual exit crash is not known.
