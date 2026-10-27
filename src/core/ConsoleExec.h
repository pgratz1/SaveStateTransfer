#pragma once

namespace sst::core {
    // Runs a console command exactly as if typed in the in-game console (e.g. "setstage 1F258 20",
    // "teachword 13E09"). Must be called on the game thread. Returns false if the script factory is missing.
    bool ConsoleExec(const std::string& a_command, RE::TESObjectREFR* a_target = nullptr);
}
