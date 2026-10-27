#include "app/Exporter.h"

#include "Version.h"
#include "app/Schema.h"
#include "character/Character.h"
#include "core/LoadOrder.h"
#include "core/Paths.h"
#include "core/Report.h"

namespace sst::app {
    namespace {
        std::string Sanitize(std::string a_name)
        {
            for (auto& ch : a_name) {
                if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.')) {
                    ch = '_';
                }
            }
            return a_name.empty() ? "export" : a_name;
        }

        std::string Timestamp(const char* a_fmt)
        {
            const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
            return std::vformat(a_fmt, std::make_format_args(now));
        }
    }

    std::optional<std::filesystem::path> RunExport(std::string a_name)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return std::nullopt;
        }
        core::Report report;
        schema::Export ex;
        ex.toolVersion = SST_VERSION_STRING;
        ex.runtime = std::format("{} {}", REL::Module::IsVR() ? "VR" : (REL::Module::IsAE() ? "AE" : "SE"), REL::Module::get().version().string("."));
        ex.source.exportedAt = Timestamp("{:%Y-%m-%dT%H:%M:%SZ}");
        ex.source.playerName = player->GetName() ? player->GetName() : "";
        ex.source.loadOrder = core::LoadOrder::Names();

        character::ExportAppearance(player, ex.character, report);
        character::ExportProgression(player, ex.character.progression, report);
        character::ExportMagic(player, ex.character.magic, report);
        character::ExportInventory(player, ex.character.inventory, report);

        if (a_name.empty()) {
            a_name = std::format("{}_{}", ex.source.playerName.empty() ? "player" : ex.source.playerName, Timestamp("{:%Y%m%d-%H%M%S}"));
        }
        a_name = Sanitize(a_name);
        if (a_name.size() < 5 || a_name.substr(a_name.size() - 5) != ".json") {
            a_name += ".json";
        }
        const auto path = core::Paths::Exports() / a_name;
        {
            std::ofstream out(path);
            if (!out) {
                logger::error("could not write {}", path.string());
                return std::nullopt;
            }
            out << nlohmann::json(ex).dump(2);
        }
        report.Write(core::Paths::Reports() / (a_name.substr(0, a_name.size() - 5) + ".report.json"));
        report.LogSummary("export");
        logger::info("export written: {}", path.string());
        RE::DebugNotification(std::format("SaveStateTransfer: exported {}", a_name).c_str());
        return path;
    }
}
