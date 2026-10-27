#include "core/ConsoleExec.h"

namespace sst::core {
    bool ConsoleExec(const std::string& a_command, RE::TESObjectREFR* a_target)
    {
        const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
        auto* script = factory ? factory->Create() : nullptr;
        if (!script) {
            logger::error("console: script factory unavailable");
            return false;
        }
        script->SetCommand(a_command);
        script->CompileAndRun(a_target);
        delete script;
        return true;
    }
}
