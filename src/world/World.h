#pragma once

#include "app/Schema.h"
#include "core/Report.h"
#include "core/Sequencer.h"

namespace sst::world {
    // Export fills the slices of schema::World. Import "Plan" functions append steps to a Sequencer (so the
    // same code serves dry runs: pass a_dryRun = true to only record actions in the report).
    void ExportQuests(schema::World& a_out, core::Report& a_report);
    void ExportFactions(RE::PlayerCharacter* a_player, schema::World& a_out, core::Report& a_report);
    void ExportGlobals(schema::World& a_out, core::Report& a_report);
    void ExportLocations(schema::World& a_out, core::Report& a_report);

    void PlanQuests(const schema::World& a_in, core::Sequencer& a_seq, core::Report& a_report, bool a_dryRun);
    void PlanFactions(RE::PlayerCharacter* a_player, const schema::World& a_in, core::Sequencer& a_seq, core::Report& a_report, bool a_dryRun);
    void PlanGlobals(const schema::World& a_in, core::Sequencer& a_seq, core::Report& a_report, bool a_dryRun);
    void PlanLocations(const schema::World& a_in, core::Sequencer& a_seq, core::Report& a_report, bool a_dryRun);

    // Quest replay overrides loaded from Data/SKSE/Plugins/SaveStateTransfer/overrides/*.json
    struct QuestRule {
        std::string                pattern;  // editor-ID glob (case-insensitive, '*' and '?')
        std::string                mode;     // skip | finalOnly | onlyStages | replay
        std::vector<std::uint32_t> onlyStages;
        std::string                reason;
        std::string                sourceFile;
    };
    const std::vector<QuestRule>& LoadQuestRules();  // cached after first call
    const QuestRule*              FindQuestRule(std::string_view a_editorId);
    bool                          GlobMatch(std::string_view a_pattern, std::string_view a_text);
}
