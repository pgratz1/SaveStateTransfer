#include "core/FormKey.h"

#include "core/LoadOrder.h"

namespace sst::core {
    std::string FormKey::ToString() const
    {
        return std::format("{}|0x{:X}", plugin, localId);
    }

    FormKey FormKey::From(const RE::TESForm* a_form)
    {
        FormKey key;
        if (!a_form) {
            return key;
        }
        const auto id = a_form->GetFormID();
        const auto* owner = LoadOrder::OwnerOf(id);  // nullptr for dynamic (0xFF) and unknown indices
        if (!owner) {
            return key;
        }
        key.plugin = owner->name;
        key.localId = owner->light ? (id & 0xFFF) : (id & 0xFFFFFF);
        return key;
    }

    RE::TESForm* FormKey::Resolve() const
    {
        if (!IsValid()) {
            return nullptr;
        }
        const auto* owner = LoadOrder::FindByName(plugin);
        if (!owner) {
            return nullptr;
        }
        const RE::FormID id = owner->light ? (0xFE000000u | (owner->index << 12) | (localId & 0xFFF)) :
                                             ((owner->index << 24) | (localId & 0xFFFFFF));
        return RE::TESForm::LookupByID(id);
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
