#include "core/ConsoleExec.h"

namespace sst::core {
    namespace {
        RE::IFormFactory* ScriptFactory()
        {
            static RE::IFormFactory* cached = nullptr;
            static bool              probed = false;
            if (probed) {
                return cached;
            }
            probed = true;
            const auto [table, initialized] = RE::IFormFactory::GetFormFactories();
            auto* script = table ? table[static_cast<std::uint32_t>(RE::FormType::Script)] : nullptr;
            auto* quest = table ? table[static_cast<std::uint32_t>(RE::FormType::Quest)] : nullptr;
            logger::info("console: form factory table {} initialized={} script={} quest={}", static_cast<const void*>(table),
                initialized, static_cast<const void*>(script), static_cast<const void*>(quest));
            if (!script) {
                logger::error("console: no Script form factory in the table");
                return nullptr;
            }
            // On VR the "factories initialized" flag mapped from the SE address library is not reliable; trust the
            // table entry itself when it reports the expected form type.
            if (!initialized) {
                const auto type = script->GetFormType();
                logger::warn("console: initialized flag is false; table entry reports form type {}", static_cast<int>(type));
                if (type != RE::FormType::Script) {
                    return nullptr;
                }
            }
            cached = script;
            return cached;
        }
    }

    bool ConsoleExec(const std::string& a_command, RE::TESObjectREFR* a_target)
    {
        auto* factory = ScriptFactory();
        auto* form = factory ? factory->Create() : nullptr;
        auto* script = form ? form->As<RE::Script>() : nullptr;
        if (!script) {
            logger::error("console: cannot create script for '{}'", a_command);
            return false;
        }
        script->SetCommand(a_command);
        script->CompileAndRun(a_target);
        delete script;
        return true;
    }
}
