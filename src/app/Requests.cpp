#include "app/Requests.h"

#include "app/Exporter.h"
#include "app/Importer.h"

namespace sst::app {
    const char* ToString(RequestKind a_kind)
    {
        switch (a_kind) {
        case RequestKind::kExport: return "export";
        case RequestKind::kImport: return "import";
        case RequestKind::kDryRun: return "dryrun";
        }
        return "?";
    }

    void Dispatch(RequestKind a_kind, std::string a_argument, std::string_view a_source)
    {
        logger::info("{} requested (source={}, argument='{}')", ToString(a_kind), a_source, a_argument);
        auto* ui = RE::UI::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) {
            logger::warn("  player not loaded; request ignored");
            return;
        }
        if (ui && ui->GameIsPaused()) {
            logger::warn("  game is paused / menu open; request ignored");
            return;
        }
        switch (a_kind) {
        case RequestKind::kExport:
            RunExport(std::move(a_argument));
            break;
        case RequestKind::kImport:
            RunImport(std::move(a_argument), false);
            break;
        case RequestKind::kDryRun:
            RunImport(std::move(a_argument), true);
            break;
        }
    }
}
