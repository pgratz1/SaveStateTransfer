#pragma once

namespace sst::core {
    // Directory layout outside MO2's virtual filesystem so Linux-side tools can read/write it:
    //   <My Games>/Skyrim VR/SaveStateTransfer/{exports,reports,commands}/
    class Paths {
    public:
        static bool Init();  // creates directories; false if the SKSE log dir is unknown
        static const std::filesystem::path& Root();
        static std::filesystem::path Exports();
        static std::filesystem::path Reports();
        static std::filesystem::path Commands();
        // Shipped, read-only files next to the DLL (game CWD = game root).
        static std::filesystem::path IniFile();
        static std::filesystem::path OverridesDir();
    };
}
