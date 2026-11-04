#include "world/QuestDiag.h"

namespace sst::world {
    namespace {
        class StageDoneCallback : public RE::BSScript::IStackCallbackFunctor {
        public:
            StageDoneCallback(std::string a_quest, std::uint32_t a_stage) : _quest(std::move(a_quest)), _stage(a_stage) {}
            void operator()(RE::BSScript::Variable a_result) override
            {
                if (a_result.IsBool() && a_result.GetBool()) {
                    logger::info("  [vm] {} IsStageDone({}) = true", _quest, _stage);
                }
            }
            bool CanSave() const override { return false; }
            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

        private:
            std::string   _quest;
            std::uint32_t _stage;
        };

        void DumpList(const char* a_name, const void* a_list)
        {
            logger::info("  {} @ {:#x}", a_name, reinterpret_cast<std::uintptr_t>(a_list));
            if (!a_list) {
                return;
            }
            const auto* node = static_cast<const std::uint64_t*>(a_list);
            for (int i = 0; i < 12 && node; ++i) {
                const auto a = node[0];
                const auto b = node[1];
                logger::info("    node{} @ {:#x}: [{:016X}] [{:016X}]  (lo16 of a = {}, lo16 of b = {})", i,
                    reinterpret_cast<std::uintptr_t>(node), a, b, a & 0xFFFF, b & 0xFFFF);
                node = reinterpret_cast<const std::uint64_t*>(b);
                if (reinterpret_cast<std::uintptr_t>(node) < 0x10000) {
                    break;
                }
            }
        }
    }

    void RunQuestDiagnostics()
    {
        auto* dh = RE::TESDataHandler::GetSingleton();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto* policy = vm ? vm->GetObjectHandlePolicy1() : nullptr;
        const std::array<const char*, 5> ids{ "MQ101", "MQ102", "MQ103", "MQ104", "CW03" };
        for (auto* quest : dh->GetFormArray<RE::TESQuest>()) {
            if (!quest || !quest->GetFormEditorID()) {
                continue;
            }
            const std::string_view edid = quest->GetFormEditorID();
            if (std::find(ids.begin(), ids.end(), edid) == ids.end()) {
                continue;
            }
            logger::info("[diag] quest {} 0x{:08X}: running={} completed={} stopped={} currentStage={} alreadyRun={}", edid, quest->GetFormID(),
                quest->IsRunning(), quest->IsCompleted(), quest->IsStopped(), quest->GetCurrentStageID(), quest->alreadyRun);
            DumpList("executedStages", quest->executedStages);
            DumpList("waitingStages", quest->waitingStages);
            if (vm && policy) {
                const auto handle = policy->GetHandleForObject(static_cast<RE::VMTypeID>(RE::TESQuest::FORMTYPE), quest);
                for (std::uint32_t stage = 0; stage <= 1000; stage += 5) {
                    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb{ new StageDoneCallback(std::string(edid), stage) };
                    auto* args = RE::MakeFunctionArguments(static_cast<std::int32_t>(stage));
                    vm->DispatchMethodCall2(handle, "Quest", "IsStageDone", args, cb);
                }
            }
        }
    }
}
