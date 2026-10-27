#pragma once

namespace sst::core {
    // Snapshot of the active plugin list with light-plugin (ESL) awareness on every runtime.
    // SE/AE: the game's own compiled file collection. VR: NG 3.7.0 knows nothing about light plugins, so we
    // ask the "Skyrim VR ESL Support" plugin (skyrimvresl.dll) for its compiled collection via its exported
    // GetCompiledFileCollectionExtern(); without it, every plugin is treated as a full plugin (as the engine does).
    class LoadOrder {
    public:
        struct Entry {
            RE::TESFile*  file = nullptr;
            std::string   name;      // as on disk
            bool          light = false;   // loaded as a light plugin (FE index), not merely ESL-flagged
            std::uint32_t index = 0;       // compile index (full) or small-file index (light)
        };

        static void Init();  // call at kDataLoaded
        static bool IsInitialized();
        static bool HasVRESL();
        static std::size_t FullCount();
        static std::size_t LightCount();
        static const std::vector<Entry>& Entries();
        static const Entry* FindByName(std::string_view a_name);   // case-insensitive
        static const Entry* OwnerOf(RE::FormID a_formID);           // by the id's index bits; nullptr for 0xFF/unknown
        static std::vector<std::string> Names();                    // full plugins in load order, then light plugins
    };
}
