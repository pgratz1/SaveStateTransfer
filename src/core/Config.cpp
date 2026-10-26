#define SI_SUPPORT_IOSTREAMS
#include "core/Config.h"
#include "core/Paths.h"

#include <SimpleIni.h>

namespace sst::core {
    namespace {
        Config g_config;
    }

    const Config& Config::Get() { return g_config; }

    void Config::Load()
    {
        Config c;
        CSimpleIniA ini;
        ini.SetUnicode();
        const auto path = Paths::IniFile();
        if (ini.LoadFile(path.string().c_str()) < 0) {
            logger::warn("{} not found, using defaults", path.string());
            g_config = c;
            return;
        }
        c.exportKey = static_cast<std::uint32_t>(ini.GetLongValue("Hotkeys", "iExportKey", c.exportKey));
        c.importKey = static_cast<std::uint32_t>(ini.GetLongValue("Hotkeys", "iImportKey", c.importKey));
        c.dryRunKey = static_cast<std::uint32_t>(ini.GetLongValue("Hotkeys", "iDryRunKey", c.dryRunKey));
        c.modifierKey = static_cast<std::uint32_t>(ini.GetLongValue("Hotkeys", "iModifierKey", c.modifierKey));
        c.requireDoublePress = ini.GetBoolValue("Hotkeys", "bRequireDoublePress", c.requireDoublePress);
        c.doublePressWindowMs = static_cast<std::uint32_t>(ini.GetLongValue("Hotkeys", "iDoublePressWindowMs", c.doublePressWindowMs));
        c.enableTriggerFiles = ini.GetBoolValue("General", "bEnableTriggerFiles", c.enableTriggerFiles);
        c.triggerPollMs = static_cast<std::uint32_t>(ini.GetLongValue("General", "iTriggerPollMs", c.triggerPollMs));
        c.framesBetweenStages = static_cast<std::uint32_t>(ini.GetLongValue("Import", "iFramesBetweenStages", c.framesBetweenStages));
        c.logImportSteps = ini.GetBoolValue("Debug", "bLogImportSteps", c.logImportSteps);
        g_config = c;
        logger::info("config loaded from {} (export key 0x{:X}, import key 0x{:X}, dry-run key 0x{:X}, modifier 0x{:X})",
            path.string(), c.exportKey, c.importKey, c.dryRunKey, c.modifierKey);
    }
}
