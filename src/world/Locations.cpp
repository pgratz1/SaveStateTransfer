#include "world/World.h"

namespace sst::world {
    using core::FormKey;

    void ExportLocations(schema::World& a_out, core::Report& a_report)
    {
        auto* dh = RE::TESDataHandler::GetSingleton();
        for (auto* loc : dh->GetFormArray<RE::BGSLocation>()) {
            if (loc && loc->IsCleared()) {
                const auto key = FormKey::From(loc);
                if (key.IsValid()) {
                    a_out.locationsCleared.push_back(key);
                }
            }
        }
        a_report.Count("locations", "cleared", static_cast<int>(a_out.locationsCleared.size()));
    }

    void PlanLocations(const schema::World& a_in, core::Sequencer& a_seq, core::Report& a_report, bool a_dryRun)
    {
        std::vector<RE::BGSLocation*> toClear;
        for (const auto& key : a_in.locationsCleared) {
            auto* loc = key.Resolve<RE::BGSLocation>();
            if (!loc) {
                a_report.Missing("locations", key.ToString(), "location");
                continue;
            }
            if (!loc->IsCleared()) {
                toClear.push_back(loc);
            }
        }
        a_report.Action("locations", "all", std::format("{} locations will be marked cleared", toClear.size()));
        a_report.Count("locations", "toClear", static_cast<int>(toClear.size()));
        if (!a_dryRun) {
            a_seq.Add([toClear] {
                for (auto* loc : toClear) {
                    loc->cleared = true;
                    loc->everCleared = true;
                    loc->AddChange(1u << 31);  // BGSLocation::ChangeFlags::kCleared
                }
                logger::info("locations: {} marked cleared", toClear.size());
            }, 0);
        }
    }
}
