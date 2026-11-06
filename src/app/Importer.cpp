#include "app/Importer.h"

#include "app/Schema.h"
#include "character/Character.h"
#include "core/Sequencer.h"
#include "world/World.h"
#include "core/Config.h"
#include "core/Paths.h"
#include "core/Report.h"

namespace {
    constexpr std::size_t kItemsPerStep = 25;
}

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
        auto exPtr = std::make_shared<schema::Export>();
        auto& ex = *exPtr;
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

        auto report = std::make_shared<core::Report>();
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto stem = path->stem().string();
        const auto reportPath = core::Paths::Reports() / (stem + (a_dryRun ? ".dryrun.report.json" : ".import.report.json"));
        const auto fileName = path->filename().string();

        // The sequencer must outlive this call; one import at a time.
        static core::Sequencer seq;
        if (seq.IsRunning()) {
            logger::warn("import: previous import still running; request ignored");
            return false;
        }
        seq = core::Sequencer{};

        if (a_dryRun) {
            DryRunCharacter(ex.character, *report);
        } else {
            // Character phases run as sequencer steps too: the sequencer only advances while the game is unpaused
            // (in VR the headset must be on), and each phase is logged / handed to the crash probe before it runs.
            seq.Add([player, exPtr, report] { character::ImportAppearance(player, exPtr->character, *report); }, 2, "character appearance");
            seq.Add([player, exPtr, report] { character::ImportProgression(player, exPtr->character.progression, *report); }, 0, "character progression");
            seq.Add([player, exPtr, report] { character::ImportMagic(player, exPtr->character.magic, *report); }, 0, "character magic");
            const auto nItems = ex.character.inventory.items.size();
            for (std::size_t begin = 0; begin == 0 || begin < nItems; begin += kItemsPerStep) {
                const auto end = std::min(nItems, begin + kItemsPerStep);
                seq.Add([player, exPtr, report, begin, end] {
                    character::ImportInventoryRange(player, exPtr->character.inventory, *report, begin, end);
                }, 0, std::format("character inventory {}-{} of {}", begin + 1, end, nItems));
            }
        }
        // World: quests first (fragments may touch globals/factions), then factions, globals, locations.
        world::PlanQuests(ex.world, seq, *report, a_dryRun);
        world::PlanFactions(player, ex.world, seq, *report, a_dryRun);
        world::PlanGlobals(ex.world, seq, *report, a_dryRun);
        world::PlanLocations(ex.world, seq, *report, a_dryRun);

        if (a_dryRun) {
            report->Write(reportPath);
            report->LogSummary("dry run");
            RE::DebugNotification(std::format("SaveStateTransfer: dry run done for {}", fileName).c_str());
            return true;
        }
        // Remember where the player stands: quest stage fragments may teleport them (College of Winterhold stages
        // move the player into the courtyard, for example); the finish step moves them back.
        RE::ObjectRefHandle homeMarker;
        if (core::Config::Get().restorePosition) {
            if (auto* xmarker = RE::TESForm::LookupByID<RE::TESBoundObject>(0x3B)) {  // XMarker
                if (auto marker = player->PlaceObjectAtMe(xmarker, false)) {
                    homeMarker = marker->CreateRefHandle();
                }
            }
        }
        report->LogSummary("import planned (character + world queued)");
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
            logger::info("import: game is paused; steps start when it is unpaused (headset on, no menu open)");
        }
        seq.Add([report, reportPath, fileName, homeMarker, player] {
            if (auto marker = homeMarker.get()) {
                if (player->GetParentCell() != marker->GetParentCell() || player->GetPosition().GetDistance(marker->GetPosition()) > 64.0f) {
                    logger::info("import: moving the player back to where the import started");
                    player->MoveTo(marker.get());
                }
                marker->Disable();
                marker->SetDelete(true);
            }
            report->Write(reportPath);
            report->LogSummary("import complete");
            report->LogMissingByPlugin();
            RE::DebugNotification(std::format("SaveStateTransfer: imported {}", fileName).c_str());
        }, 0, "finish");
        seq.Start("world import");
        return true;
    }
}
