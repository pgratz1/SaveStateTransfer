#include "world/World.h"

namespace sst::world {
    using core::FormKey;

    void ExportFactions(RE::PlayerCharacter* a_player, schema::World& a_out, core::Report& a_report)
    {
        std::map<RE::TESFaction*, schema::FactionState> states;
        a_player->VisitFactions([&](RE::TESFaction* a_faction, std::int8_t a_rank) {
            if (a_faction) {
                auto& s = states[a_faction];
                s.form = FormKey::From(a_faction);
                s.rank = a_rank;
            }
            return false;  // keep visiting
        });
        auto* dh = RE::TESDataHandler::GetSingleton();
        for (auto* faction : dh->GetFormArray<RE::TESFaction>()) {
            if (!faction) {
                continue;
            }
            const auto nonViolent = faction->GetCrimeGoldNonViolent();
            const auto violent = faction->GetCrimeGoldViolent();
            if (nonViolent > 0 || violent > 0) {
                auto& s = states[faction];
                if (!s.form.IsValid()) {
                    s.form = FormKey::From(faction);
                }
                s.crimeGold = nonViolent;
                s.crimeGoldViolent = violent;
            }
        }
        for (auto& [faction, s] : states) {
            if (s.form.IsValid()) {
                a_out.factions.push_back(std::move(s));
            }
        }
        a_report.Count("factions", "exported", static_cast<int>(a_out.factions.size()));
    }

    void PlanFactions(RE::PlayerCharacter* a_player, const schema::World& a_in, core::Sequencer& a_seq, core::Report& a_report, bool a_dryRun)
    {
        int planned = 0;
        for (const auto& f : a_in.factions) {
            auto* faction = f.form.Resolve<RE::TESFaction>();
            if (!faction) {
                a_report.Missing("factions", f.form.ToString(), "faction");
                continue;
            }
            const std::string label = faction->GetName() && *faction->GetName() ? faction->GetName() : f.form.ToString();
            if (f.rank >= 0) {
                a_report.Action("factions", label, std::format("rank {}", f.rank));
            }
            if (f.crimeGold > 0 || f.crimeGoldViolent > 0) {
                a_report.Action("factions", label, std::format("crime gold {} / violent {}", f.crimeGold, f.crimeGoldViolent));
            }
            ++planned;
            if (!a_dryRun) {
                a_seq.Add([a_player, faction, f] {
                    if (f.rank >= 0) {
                        a_player->AddToFaction(faction, static_cast<std::int8_t>(std::clamp(f.rank, 0, 127)));
                    }
                    if (f.crimeGold > 0 || f.crimeGoldViolent > 0) {
                        faction->SetCrimeGold(f.crimeGold);
                        faction->SetCrimeGoldViolent(f.crimeGoldViolent);
                    }
                }, 0);
            }
        }
        a_report.Count("factions", "planned", planned);
    }
}
