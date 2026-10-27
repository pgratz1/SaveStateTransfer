#include "app/Importer.h"

#include "app/Schema.h"
#include "character/Character.h"
#include "core/Paths.h"
#include "core/Report.h"

namespace sst::app {
    namespace {
        std::optional<std::filesystem::path> PickExport(const std::string& a_name)
        {
            const auto dir = core::Paths::Exports();
            std::error_code ec;
            if (!a_name.empty()) {
                auto p = dir / a_name;
                if (!std::filesystem::exists(p, ec)) {
                    p = dir / (a_name + ".json");
                }
                return std::filesystem::exists(p, ec) ? std::optional(p) : std::nullopt;
            }
            std::optional<std::filesystem::path> newest;
            std::filesystem::file_time_type newestTime{};
            for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
                if (e.is_regular_file() && e.path().extension() == ".json") {
                    const auto t = e.last_write_time(ec);
                    if (!newest || t > newestTime) {
                        newest = e.path();
                        newestTime = t;
                    }
                }
            }
            return newest;
        }

        void DryRunCharacter(const schema::Character& c, core::Report& r)
        {
            auto check = [&](std::string_view section, const core::FormKey& k, std::string_view what) {
                if (k.IsValid() && !k.Resolve()) {
                    r.Missing(section, k.ToString(), what);
                }
            };
            check("appearance", c.race, "race");
            for (const auto& k : c.appearance.headParts) check("appearance", k, "head part");
            check("appearance", c.appearance.hairColor, "hair color");
            for (const auto& k : c.progression.perks) check("progression", k, "perk");
            for (const auto& k : c.magic.spells) check("magic", k, "spell");
            for (const auto& k : c.magic.wordsKnown) check("magic", k, "word");
            for (const auto& i : c.inventory.items) {
                check("inventory", i.form, i.name.empty() ? "item" : i.name);
                check("inventory", i.enchantment, "enchantment");
            }
            r.Count("dryrun", "perks", static_cast<int>(c.progression.perks.size()));
            r.Count("dryrun", "spells", static_cast<int>(c.magic.spells.size()));
            r.Count("dryrun", "itemStacks", static_cast<int>(c.inventory.items.size()));
        }
    }

    bool RunImport(std::string a_name, bool a_dryRun)
    {
        const auto path = PickExport(a_name);
        if (!path) {
            logger::error("import: no export found ({})", a_name.empty() ? "exports/ is empty" : a_name);
            RE::DebugNotification("SaveStateTransfer: no export file found");
            return false;
        }
        schema::Export ex;
        try {
            std::ifstream in(*path);
            const auto j = nlohmann::json::parse(in);
            ex = j.get<schema::Export>();
        } catch (const std::exception& e) {
            logger::error("import: cannot parse {}: {}", path->string(), e.what());
            return false;
        }
        if (ex.schema != 1) {
            logger::error("import: unsupported schema {}", ex.schema);
            return false;
        }
        logger::info("{} {} (player '{}', exported {}, {} plugins in source)", a_dryRun ? "dry run of" : "importing", path->filename().string(),
            ex.source.playerName, ex.source.exportedAt, ex.source.loadOrder.size());

        core::Report report;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (a_dryRun) {
            DryRunCharacter(ex.character, report);
        } else {
            character::ImportAppearance(player, ex.character, report);
            character::ImportProgression(player, ex.character.progression, report);
            character::ImportMagic(player, ex.character.magic, report);
            character::ImportInventory(player, ex.character.inventory, report);
        }
        const auto stem = path->stem().string();
        report.Write(core::Paths::Reports() / (stem + (a_dryRun ? ".dryrun.report.json" : ".import.report.json")));
        report.LogSummary(a_dryRun ? "dry run" : "import");
        RE::DebugNotification(std::format("SaveStateTransfer: {} {}", a_dryRun ? "dry run done for" : "imported", path->filename().string()).c_str());
        return true;
    }
}
