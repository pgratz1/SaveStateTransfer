#include "world/World.h"

namespace sst::world {
    using core::FormKey;

    void ExportGlobals(schema::World& a_out, core::Report& a_report)
    {
        auto* dh = RE::TESDataHandler::GetSingleton();
        for (auto* global : dh->GetFormArray<RE::TESGlobal>()) {
            if (!global) {
                continue;
            }
            const auto key = FormKey::From(global);
            if (key.IsValid()) {
                a_out.globals.push_back({ key, global->value });
            }
        }
        a_report.Count("globals", "exported", static_cast<int>(a_out.globals.size()));
    }

    void PlanGlobals(const schema::World& a_in, core::Sequencer& a_seq, core::Report& a_report, bool a_dryRun)
    {
        // Applied as one step after the quest replay (fragments write globals too; the export wins).
        std::vector<std::pair<RE::TESGlobal*, float>> changes;
        for (const auto& g : a_in.globals) {
            auto* global = g.form.Resolve<RE::TESGlobal>();
            if (!global) {
                a_report.Missing("globals", g.form.ToString(), "global variable");
                continue;
            }
            if (global->value != g.value) {
                changes.emplace_back(global, g.value);
            }
        }
        a_report.Action("globals", "all", std::format("{} values differ and will be set", changes.size()));
        a_report.Count("globals", "toSet", static_cast<int>(changes.size()));
        if (!a_dryRun) {
            a_seq.Add([changes = std::move(changes)] {
                for (const auto& [global, value] : changes) {
                    global->value = value;
                }
                logger::info("globals: {} values set", changes.size());
            }, 0);
        }
    }
}
