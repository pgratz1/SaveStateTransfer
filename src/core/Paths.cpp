#include "core/Paths.h"

namespace sst::core {
    namespace {
        std::filesystem::path g_root;
    }

    bool Paths::Init()
    {
        auto logDir = logger::log_directory();
        if (!logDir) {
            return false;
        }
        g_root = logDir->parent_path() / "SaveStateTransfer";
        std::error_code ec;
        for (const auto& sub : { Exports(), Reports(), Commands() }) {
            std::filesystem::create_directories(sub, ec);
            if (ec) {
                logger::error("could not create {}: {}", sub.string(), ec.message());
            }
        }
        return true;
    }

    const std::filesystem::path& Paths::Root() { return g_root; }
    std::filesystem::path Paths::Exports() { return g_root / "exports"; }
    std::filesystem::path Paths::Reports() { return g_root / "reports"; }
    std::filesystem::path Paths::Commands() { return g_root / "commands"; }
    std::filesystem::path Paths::IniFile() { return std::filesystem::path("Data/SKSE/Plugins/SaveStateTransfer.ini"); }
    std::filesystem::path Paths::OverridesDir() { return std::filesystem::path("Data/SKSE/Plugins/SaveStateTransfer/overrides"); }
}
