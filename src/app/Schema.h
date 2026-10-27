#pragma once

#include "core/FormKey.h"

// In-memory mirror of the export JSON (schema 1). Every form reference is a core::FormKey.
// Serialization is explicit (no std::optional) so unknown keys are ignored and missing keys default.
namespace sst::schema {
    using core::FormKey;
    using json = nlohmann::json;

    struct Tint {
        std::uint32_t index = 0;        // position in the player's tint mask list
        std::uint32_t type = 0;         // TintMask type (6 = skin tone, 7 = war paint, ...)
        std::string   texture;          // mask texture path, used to match masks across races/presets
        std::string   color = "#000000";
        float         alpha = 0.0f;
        std::uint32_t preset = 0;       // only used for TESNPC::Layer tints (NPCs); 0 for the player
    };

    struct Appearance {
        std::vector<FormKey>  headParts;
        std::vector<float>    faceMorphs;
        std::vector<int>      faceParts;
        FormKey               hairColor;
        std::string           bodyTint = "#FFFFFF";
        std::vector<Tint>     tints;
    };

    struct SkillState {
        float         level = 0.0f;
        float         xp = 0.0f;
        float         threshold = 0.0f;
        std::uint32_t legendary = 0;
    };

    struct Progression {
        std::uint32_t                     level = 1;
        float                             xp = 0.0f;
        float                             xpThreshold = 0.0f;
        int                               perkPoints = 0;
        std::map<std::string, SkillState> skills;
        std::map<std::string, float>      attributes;
        float                             dragonSouls = 0.0f;
        std::vector<FormKey>              perks;
    };

    struct Favorites {
        std::vector<FormKey>              spells;
        std::vector<std::optional<FormKey>> hotkeys;  // index = hotkey slot
    };

    struct Magic {
        std::vector<FormKey> spells;
        std::vector<FormKey> shouts;
        std::vector<FormKey> wordsKnown;
        std::vector<FormKey> wordsUnlocked;
        Favorites            favorites;
        FormKey              equippedRight;  // spell in right hand (weapons are in inventory.equippedSlot)
        FormKey              equippedLeft;
        FormKey              equippedShout;
    };

    struct CustomEffect {
        FormKey effect;
        float   mag = 0.0f;
        float   dur = 0.0f;
        float   area = 0.0f;
    };

    struct Item {
        FormKey                   form;
        int                       count = 1;
        float                     health = 0.0f;   // 0 = untempered
        float                     charge = -1.0f;  // <0 = n/a
        int                       soul = 0;
        std::string               name;            // custom display name, empty = none
        std::string               equippedSlot;    // "", "right", "left", "worn"
        bool                      favorite = false;
        int                       hotkey = -1;
        FormKey                   enchantment;     // non-dynamic enchantment attached via extra data
        std::vector<CustomEffect> customEnchant;   // effects of a player-made (dynamic) enchantment
    };

    struct Unresolvable {
        std::string formID;
        std::string name;
        std::string reason;
    };

    struct Inventory {
        int                       gold = 0;
        std::vector<Item>         items;
        std::vector<Unresolvable> unresolvable;
    };

    struct Character {
        std::string name;
        FormKey     race;
        bool        female = false;
        float       height = 1.0f;
        float       weight = 50.0f;
        Appearance  appearance;
        Progression progression;
        Magic       magic;
        Inventory   inventory;
    };

    struct Source {
        std::string              exportedAt;
        std::string              saveName;
        std::string              playerName;
        std::vector<std::string> loadOrder;
    };

    struct Export {
        int         schema = 1;
        std::string toolVersion;
        std::string runtime;
        Source      source;
        Character   character;
        json        world = json::object();  // M1b/M2 fill this
    };

    // --- helpers -----------------------------------------------------------------------------
    template <class T>
    void Get(const json& j, const char* key, T& out)
    {
        if (auto it = j.find(key); it != j.end() && !it->is_null()) {
            it->get_to(out);
        }
    }

    inline void Put(json& j, const char* key, const FormKey& k)
    {
        if (k.IsValid()) {
            j[key] = k;
        }
    }

    // --- serializers -------------------------------------------------------------------------
    inline void to_json(json& j, const Tint& t) { j = { { "index", t.index }, { "type", t.type }, { "texture", t.texture }, { "color", t.color }, { "alpha", t.alpha }, { "preset", t.preset } }; }
    inline void from_json(const json& j, Tint& t) { Get(j, "index", t.index); Get(j, "type", t.type); Get(j, "texture", t.texture); Get(j, "color", t.color); Get(j, "alpha", t.alpha); Get(j, "preset", t.preset); }

    inline void to_json(json& j, const Appearance& a)
    {
        j = { { "headParts", a.headParts }, { "faceMorphs", a.faceMorphs }, { "faceParts", a.faceParts }, { "bodyTint", a.bodyTint }, { "tints", a.tints } };
        Put(j, "hairColor", a.hairColor);
    }
    inline void from_json(const json& j, Appearance& a)
    {
        Get(j, "headParts", a.headParts); Get(j, "faceMorphs", a.faceMorphs); Get(j, "faceParts", a.faceParts);
        Get(j, "hairColor", a.hairColor); Get(j, "bodyTint", a.bodyTint); Get(j, "tints", a.tints);
    }

    inline void to_json(json& j, const SkillState& s) { j = { { "level", s.level }, { "xp", s.xp }, { "threshold", s.threshold }, { "legendary", s.legendary } }; }
    inline void from_json(const json& j, SkillState& s) { Get(j, "level", s.level); Get(j, "xp", s.xp); Get(j, "threshold", s.threshold); Get(j, "legendary", s.legendary); }

    inline void to_json(json& j, const Progression& p)
    {
        j = { { "level", p.level }, { "xp", p.xp }, { "xpThreshold", p.xpThreshold }, { "perkPoints", p.perkPoints },
              { "skills", p.skills }, { "attributes", p.attributes }, { "dragonSouls", p.dragonSouls }, { "perks", p.perks } };
    }
    inline void from_json(const json& j, Progression& p)
    {
        Get(j, "level", p.level); Get(j, "xp", p.xp); Get(j, "xpThreshold", p.xpThreshold); Get(j, "perkPoints", p.perkPoints);
        Get(j, "skills", p.skills); Get(j, "attributes", p.attributes); Get(j, "dragonSouls", p.dragonSouls); Get(j, "perks", p.perks);
    }

    inline void to_json(json& j, const Favorites& f)
    {
        j = { { "spells", f.spells } };
        auto& hk = j["hotkeys"] = json::array();
        for (const auto& h : f.hotkeys) {
            if (h) {
                hk.push_back(*h);
            } else {
                hk.push_back(nullptr);
            }
        }
    }
    inline void from_json(const json& j, Favorites& f)
    {
        Get(j, "spells", f.spells);
        if (auto it = j.find("hotkeys"); it != j.end() && it->is_array()) {
            for (const auto& h : *it) {
                if (h.is_null()) {
                    f.hotkeys.emplace_back(std::nullopt);
                } else {
                    f.hotkeys.emplace_back(h.get<FormKey>());
                }
            }
        }
    }

    inline void to_json(json& j, const Magic& m)
    {
        j = { { "spells", m.spells }, { "shouts", m.shouts }, { "wordsKnown", m.wordsKnown }, { "wordsUnlocked", m.wordsUnlocked }, { "favorites", m.favorites } };
        Put(j, "equippedRight", m.equippedRight); Put(j, "equippedLeft", m.equippedLeft); Put(j, "equippedShout", m.equippedShout);
    }
    inline void from_json(const json& j, Magic& m)
    {
        Get(j, "spells", m.spells); Get(j, "shouts", m.shouts); Get(j, "wordsKnown", m.wordsKnown); Get(j, "wordsUnlocked", m.wordsUnlocked);
        Get(j, "favorites", m.favorites); Get(j, "equippedRight", m.equippedRight); Get(j, "equippedLeft", m.equippedLeft); Get(j, "equippedShout", m.equippedShout);
    }

    inline void to_json(json& j, const CustomEffect& e) { j = { { "effect", e.effect }, { "mag", e.mag }, { "dur", e.dur }, { "area", e.area } }; }
    inline void from_json(const json& j, CustomEffect& e) { Get(j, "effect", e.effect); Get(j, "mag", e.mag); Get(j, "dur", e.dur); Get(j, "area", e.area); }

    inline void to_json(json& j, const Item& i)
    {
        j = { { "form", i.form }, { "count", i.count } };
        if (i.health > 0.0f) j["health"] = i.health;
        if (i.charge >= 0.0f) j["charge"] = i.charge;
        if (i.soul > 0) j["soul"] = i.soul;
        if (!i.name.empty()) j["name"] = i.name;
        if (!i.equippedSlot.empty()) j["equippedSlot"] = i.equippedSlot;
        if (i.favorite) j["favorite"] = true;
        if (i.hotkey >= 0) j["hotkey"] = i.hotkey;
        Put(j, "enchantment", i.enchantment);
        if (!i.customEnchant.empty()) j["customEnchant"] = { { "effects", i.customEnchant } };
    }
    inline void from_json(const json& j, Item& i)
    {
        Get(j, "form", i.form); Get(j, "count", i.count); Get(j, "health", i.health); Get(j, "charge", i.charge); Get(j, "soul", i.soul);
        Get(j, "name", i.name); Get(j, "equippedSlot", i.equippedSlot); Get(j, "favorite", i.favorite); Get(j, "hotkey", i.hotkey);
        Get(j, "enchantment", i.enchantment);
        if (auto it = j.find("customEnchant"); it != j.end()) {
            Get(*it, "effects", i.customEnchant);
        }
    }

    inline void to_json(json& j, const Unresolvable& u) { j = { { "formID", u.formID }, { "name", u.name }, { "reason", u.reason } }; }
    inline void from_json(const json& j, Unresolvable& u) { Get(j, "formID", u.formID); Get(j, "name", u.name); Get(j, "reason", u.reason); }

    inline void to_json(json& j, const Inventory& inv) { j = { { "gold", inv.gold }, { "items", inv.items }, { "unresolvable", inv.unresolvable } }; }
    inline void from_json(const json& j, Inventory& inv) { Get(j, "gold", inv.gold); Get(j, "items", inv.items); Get(j, "unresolvable", inv.unresolvable); }

    inline void to_json(json& j, const Character& c)
    {
        j = { { "name", c.name }, { "race", c.race }, { "female", c.female }, { "height", c.height }, { "weight", c.weight },
              { "appearance", c.appearance }, { "progression", c.progression }, { "magic", c.magic }, { "inventory", c.inventory } };
    }
    inline void from_json(const json& j, Character& c)
    {
        Get(j, "name", c.name); Get(j, "race", c.race); Get(j, "female", c.female); Get(j, "height", c.height); Get(j, "weight", c.weight);
        Get(j, "appearance", c.appearance); Get(j, "progression", c.progression); Get(j, "magic", c.magic); Get(j, "inventory", c.inventory);
    }

    inline void to_json(json& j, const Source& s) { j = { { "exportedAt", s.exportedAt }, { "saveName", s.saveName }, { "playerName", s.playerName }, { "loadOrder", s.loadOrder } }; }
    inline void from_json(const json& j, Source& s) { Get(j, "exportedAt", s.exportedAt); Get(j, "saveName", s.saveName); Get(j, "playerName", s.playerName); Get(j, "loadOrder", s.loadOrder); }

    inline void to_json(json& j, const Export& e)
    {
        j = { { "schema", e.schema },
              { "tool", { { "name", "SaveStateTransfer" }, { "version", e.toolVersion }, { "runtime", e.runtime } } },
              { "source", e.source }, { "character", e.character }, { "world", e.world } };
    }
    inline void from_json(const json& j, Export& e)
    {
        Get(j, "schema", e.schema);
        if (auto it = j.find("tool"); it != j.end()) {
            Get(*it, "version", e.toolVersion); Get(*it, "runtime", e.runtime);
        }
        Get(j, "source", e.source); Get(j, "character", e.character);
        if (auto it = j.find("world"); it != j.end()) e.world = *it;
    }
}
