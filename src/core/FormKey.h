#pragma once

namespace sst::core {
    // Load-order independent identity of a form: owning plugin file name + local form ID
    // (24-bit for full plugins, 12-bit for ESL-flagged/light plugins).
    struct FormKey {
        std::string   plugin;
        RE::FormID    localId = 0;

        bool IsValid() const { return !plugin.empty(); }
        std::string ToString() const;  // "Skyrim.esm|0x13746"

        // Returns an empty key for dynamic (0xFF) forms and forms without an owning file.
        static FormKey From(const RE::TESForm* a_form);
        // Resolves against the current load order; nullptr if the plugin is absent or the id unknown.
        RE::TESForm* Resolve() const;
        template <class T>
        T* Resolve() const
        {
            auto form = Resolve();
            return form ? form->As<T>() : nullptr;
        }

        friend void to_json(nlohmann::json& j, const FormKey& k);
        friend void from_json(const nlohmann::json& j, FormKey& k);
    };
}
