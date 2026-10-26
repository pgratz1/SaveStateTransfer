#include "core/FormKey.h"

namespace sst::core {
    std::string FormKey::ToString() const
    {
        return std::format("{}|0x{:X}", plugin, localId);
    }

    FormKey FormKey::From(const RE::TESForm* a_form)
    {
        FormKey key;
        if (!a_form || a_form->IsDynamicForm()) {
            return key;
        }
        const auto* file = a_form->GetFile(0);
        if (!file) {
            return key;
        }
        key.plugin = std::string(file->GetFilename());
        key.localId = a_form->GetLocalFormID();
        return key;
    }

    RE::TESForm* FormKey::Resolve() const
    {
        if (!IsValid()) {
            return nullptr;
        }
        auto* dh = RE::TESDataHandler::GetSingleton();
        return dh ? dh->LookupForm(localId, plugin) : nullptr;
    }

    void to_json(nlohmann::json& j, const FormKey& k)
    {
        j = nlohmann::json{ { "plugin", k.plugin }, { "id", std::format("0x{:X}", k.localId) } };
    }

    void from_json(const nlohmann::json& j, FormKey& k)
    {
        k.plugin = j.at("plugin").get<std::string>();
        const auto id = j.at("id").get<std::string>();
        k.localId = static_cast<RE::FormID>(std::stoul(id, nullptr, 16));
    }
}
