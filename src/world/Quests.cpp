#include "world/World.h"

#include "core/Config.h"
#include "core/ConsoleExec.h"

namespace sst::world {
    using core::FormKey;

    namespace {
        std::vector<std::uint32_t> DoneStages(const RE::TESQuest* a_quest)
        {
            std::vector<std::uint32_t> out;
            if (a_quest->executedStages) {
                for (const auto& stage : *a_quest->executedStages) {
                    out.push_back(stage.data.index);
                }
            }
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());
            return out;
        }

        bool HasStage(const RE::TESQuest* a_quest, std::uint32_t a_stage)
        {
            if (a_quest->executedStages) {
                for (const auto& stage : *a_quest->executedStages) {
                    if (stage.data.index == a_stage) {
                        return true;
                    }
                }
            }
            return false;
        }

        std::string Hex(const RE::TESForm* a_form) { return std::format("{:X}", a_form->GetFormID()); }
    }

    void ExportQuests(schema::World& a_out, core::Report& a_report)
    {
        auto* dh = RE::TESDataHandler::GetSingleton();
        for (auto* quest : dh->GetFormArray<RE::TESQuest>()) {
            if (!quest) {
                continue;
            }
            const bool running = quest->IsRunning();
            const bool completed = quest->IsCompleted();
            const bool stopped = quest->IsStopped();
            auto done = DoneStages(quest);
            // Only quests that have ever progressed are worth carrying.
            if (!running && !completed && done.empty()) {
                continue;
            }
            const auto key = FormKey::From(quest);
            if (!key.IsValid()) {
                continue;
            }
            schema::QuestState q;
            q.form = key;
            q.editorId = quest->GetFormEditorID() ? quest->GetFormEditorID() : "";
            q.running = running;
            q.completed = completed;
            q.stopped = stopped;
            q.startsEnabled = quest->StartsEnabled();
            q.storyManager = quest->eventID != RE::QuestEvent::kNone && static_cast<std::uint32_t>(quest->eventID) != 0;
            q.type = static_cast<int>(quest->GetType());
            q.currentStage = quest->GetCurrentStageID();
            q.doneStages = std::move(done);
            for (auto* obj : quest->objectives) {
                if (obj) {
                    q.objectives.push_back({ obj->index, static_cast<int>(obj->state.get()) });
                }
            }
            a_out.quests.push_back(std::move(q));
        }
        a_report.Count("quests", "exported", static_cast<int>(a_out.quests.size()));
    }

    void PlanQuests(const schema::World& a_in, core::Sequencer& a_seq, core::Report& a_report, bool a_dryRun)
    {
        const auto& cfg = core::Config::Get();
        const auto frames = cfg.framesBetweenStages;
        int planned = 0, skipped = 0;

        for (const auto& q : a_in.quests) {
            const std::string label = q.editorId.empty() ? q.form.ToString() : q.editorId;
            auto* quest = q.form.Resolve<RE::TESQuest>();
            if (!quest) {
                a_report.Missing("quests", label, "quest not in load order");
                continue;
            }
            // Default filter: radiant/story-manager and miscellaneous quests are not replayable; already-ahead
            // start-game-enabled quests are left alone.
            const auto* rule = FindQuestRule(q.editorId);
            std::string mode = rule ? rule->mode : "replay";
            if (!rule) {
                if (q.storyManager) {
                    mode = "skip";
                } else if (q.type == static_cast<int>(RE::QUEST_DATA::Type::kMiscellaneous)) {
                    mode = "skip";
                }
            }
            if (mode == "skip") {
                a_report.Skipped("quests", label, rule ? rule->reason : (q.storyManager ? "story-manager quest" : "miscellaneous quest"));
                ++skipped;
                continue;
            }
            if (q.startsEnabled && quest->GetCurrentStageID() >= q.currentStage && (quest->IsRunning() || quest->IsCompleted())) {
                a_report.Skipped("quests", label, "start-game-enabled quest already at or past exported stage");
                ++skipped;
                continue;
            }

            std::vector<std::uint32_t> stages = q.doneStages;
            if (mode == "finalOnly") {
                stages.clear();
                if (q.currentStage > 0) {
                    stages.push_back(q.currentStage);
                }
            } else if (mode == "onlyStages" && rule) {
                std::vector<std::uint32_t> filtered;
                for (auto s : q.doneStages) {
                    if (std::find(rule->onlyStages.begin(), rule->onlyStages.end(), s) != rule->onlyStages.end()) {
                        filtered.push_back(s);
                    }
                }
                stages = std::move(filtered);
            }

            for (auto stage : stages) {
                if (HasStage(quest, stage)) {
                    continue;  // target already ran this stage (possibly via another quest's fragment)
                }
                a_report.Action("quests", label, std::format("setstage {}", stage));
                ++planned;
                if (!a_dryRun) {
                    a_seq.Add([quest, stage, label] {
                        if (!HasStage(quest, stage)) {
                            if (core::Config::Get().logImportSteps) {
                                logger::info("quest {}: setstage {}", label, stage);
                            }
                            core::ConsoleExec(std::format("setstage {} {}", Hex(quest), stage));
                        }
                    }, frames);
                }
            }
            if (q.completed) {
                a_report.Action("quests", label, "completequest");
                if (!a_dryRun) {
                    a_seq.Add([quest, label] {
                        if (!quest->IsCompleted()) {
                            core::ConsoleExec("completequest " + Hex(quest));
                        }
                    }, frames);
                }
            } else if (q.stopped && !q.running) {
                a_report.Action("quests", label, "stopquest");
                if (!a_dryRun) {
                    a_seq.Add([quest] { if (quest->IsRunning()) { core::ConsoleExec("stopquest " + Hex(quest)); } }, frames);
                }
            }
            // Objective display states last, once the stages have run.
            if (!a_dryRun && q.running) {
                a_seq.Add([quest, objectives = q.objectives] {
                    const auto id = Hex(quest);
                    for (const auto& o : objectives) {
                        switch (o.state) {
                        case 1: core::ConsoleExec(std::format("setobjectivedisplayed {} {} 1", id, o.index)); break;
                        case 2: core::ConsoleExec(std::format("setobjectivecompleted {} {} 1", id, o.index)); break;
                        case 3: core::ConsoleExec(std::format("setobjectivecompleted {} {} 1", id, o.index)); core::ConsoleExec(std::format("setobjectivedisplayed {} {} 1", id, o.index)); break;
                        case 4: core::ConsoleExec(std::format("setobjectivefailed {} {} 1", id, o.index)); break;
                        case 5: core::ConsoleExec(std::format("setobjectivefailed {} {} 1", id, o.index)); core::ConsoleExec(std::format("setobjectivedisplayed {} {} 1", id, o.index)); break;
                        default: break;
                        }
                    }
                }, 0);
            }
            if (q.editorId.starts_with("HousePurchase") || q.editorId.starts_with("BYOH")) {
                a_report.Verify("quests", label, "player-home quest: check ownership/keys after import");
            }
        }
        a_report.Count("quests", "stepsPlanned", planned);
        a_report.Count("quests", "questsSkipped", skipped);
    }
}
