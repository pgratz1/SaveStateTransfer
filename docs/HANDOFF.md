# Handoff — 2026-11-04

## State
* Milestones M0 (toolchain/plugin), M1a (character), M1b (world: quests/factions/globals/locations) are **built,
  installed in both FUS profiles ("FUS RO DAH ..." and "FUS (Basic)"), and dry-run verified**. The real import has
  NOT been run yet.
* Latest RO DAH export with correct quest stage data: `exports/Adventurer_20261105-014439.json` (level 28 Breton
  "Adventurer", 902 quests, 36 face tints). Dry run against the Basic Helgen save planned 61 quests / 425 stage
  steps, 12 factions, 94 globals, 7 locations; 831 refs missing because Spellsiphon/Sentinel/Mysticism/Adamant/
  Blade&Blunt/Extended Guard Dialogue are disabled in Basic (expected).
* Installed build = git HEAD (quests skip stopped scene quests by default; MQ201 finalOnly override).

## Next step (the import test)
1. `~/ExtraSpace/FUS/launch-fus-play.sh "FUS (Basic)"`, load the save made outside Helgen.
2. `~/ExtraSpace/SaveStateTransfer/tools/sst.sh import` then keep the headset ON (replay pauses while the game is paused).
3. Wait for the "imported" notification (~30 s), inspect character/journal/inventory, make a NEW save, quit.
4. Read `tools/sst.sh log 200` and `tools/sst.sh report`; verify in-game via console: `GetStage MQ105`, `player.getlevel`,
   `player.getav destruction`, `GetGlobalValue ...`, `IsLocationCleared`. Then re-export from the imported game and diff.

## Known gotchas
* SkyrimVR.exe often lingers after quit; `tools/sst.sh kill` ends it (install.sh refuses while it runs).
* `install.sh` also refuses if the *invoking command line* contains the process names (pgrep -f self-match).
* Game pauses when the headset is off; export/dry run work paused, the world import sequencer waits.
* `TESQuest::executedStages` is NOT the stage history; `waitingStages` is the stage table, flags bit0 = done.
* `PlayerCharacter::GetTintList()` is null on VR; tints read at SKSEVR offsets 0x1208/0x1220.
* Validator (`tools/validate_export.py --target-plugins`) under-predicts missing plugins: plugins.txt lists plugins of
  disabled mods; should also consult modlist.txt (TODO).

## After the import works
* M2: unique NPC dead/disabled + persistent ref enable state (plan in docs/PLAN.md).
* Cleanups: remove core/BootTrace from release builds, delete src/world/QuestDiag.cpp, validator modlist awareness.
