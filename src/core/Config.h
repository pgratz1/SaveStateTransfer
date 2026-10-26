#pragma once

namespace sst::core {
    struct Config {
        // [Hotkeys] DirectInput scan codes. Defaults: F7 export, F8 import, F9 dry run, Left Shift modifier.
        std::uint32_t exportKey = 0x41;
        std::uint32_t importKey = 0x42;
        std::uint32_t dryRunKey = 0x43;
        std::uint32_t modifierKey = 0x2A;  // 0 = no modifier required
        bool          requireDoublePress = true;
        std::uint32_t doublePressWindowMs = 2000;
        // [General]
        bool          enableTriggerFiles = true;
        std::uint32_t triggerPollMs = 1000;
        // [Import]
        std::uint32_t framesBetweenStages = 3;
        // [Debug]
        bool          logImportSteps = true;

        static const Config& Get();
        static void Load();  // reads Paths::IniFile(); missing file = defaults
    };
}
