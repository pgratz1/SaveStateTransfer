#include "character/Character.h"

namespace sst::character {
    using core::FormKey;
    using Skill = RE::PlayerCharacter::PlayerSkills::Data::Skill;

    namespace {
        constexpr std::array<const char*, Skill::kTotal> kSkillNames{
            "OneHanded", "TwoHanded", "Archery", "Block", "Smithing", "HeavyArmor", "LightArmor", "Pickpocket", "Lockpicking",
            "Sneak", "Alchemy", "Speech", "Alteration", "Conjuration", "Destruction", "Illusion", "Restoration", "Enchanting" };

        // Skill i (OneHanded = 0) is actor value 6 + i (kOneHanded = 6 ... kEnchanting = 23).
        RE::ActorValue SkillAV(std::size_t a_index) { return static_cast<RE::ActorValue>(6 + a_index); }

        std::optional<std::size_t> SkillIndex(const std::string& a_name)
        {
            for (std::size_t i = 0; i < kSkillNames.size(); ++i) {
                if (a_name == kSkillNames[i]) {
                    return i;
                }
            }
            return std::nullopt;
        }

        constexpr std::array<std::pair<const char*, RE::ActorValue>, 3> kAttributes{ {
            { "Health", RE::ActorValue::kHealth }, { "Magicka", RE::ActorValue::kMagicka }, { "Stamina", RE::ActorValue::kStamina } } };
    }

    void ExportProgression(RE::PlayerCharacter* a_player, schema::Progression& a_out, core::Report& a_report)
    {
        auto* avOwner = a_player->AsActorValueOwner();
        auto* npc = a_player->GetActorBase();
        a_out.level = a_player->GetLevel();
        a_out.perkPoints = a_player->GetGameStatsData().perkCount;

        auto* skills = a_player->GetInfoRuntimeData().skills;
        const auto* data = skills ? skills->data : nullptr;
        if (data) {
            a_out.xp = data->xp;
            a_out.xpThreshold = data->levelThreshold;
        } else {
            a_report.Error("progression", "skills", "player skill data unavailable");
        }
        for (std::size_t i = 0; i < kSkillNames.size(); ++i) {
            schema::SkillState s;
            s.level = avOwner->GetBaseActorValue(SkillAV(i));
            if (data) {
                s.xp = data->skills[i].xp;
                s.threshold = data->skills[i].levelThreshold;
                s.legendary = data->legendaryLevels[i];
            }
            a_out.skills[kSkillNames[i]] = s;
        }
        for (const auto& [name, av] : kAttributes) {
            a_out.attributes[name] = avOwner->GetBaseActorValue(av);
        }
        a_out.dragonSouls = avOwner->GetBaseActorValue(RE::ActorValue::kDragonSouls);

        if (npc && npc->perks) {
            for (std::uint32_t i = 0; i < npc->perkCount; ++i) {
                auto* perk = npc->perks[i].perk;
                if (!perk) {
                    continue;
                }
                const auto key = FormKey::From(perk);
                if (key.IsValid()) {
                    a_out.perks.push_back(key);
                } else {
                    a_report.Skipped("progression", std::format("perk 0x{:08X}", perk->GetFormID()), "dynamic form");
                }
            }
        }
        a_report.Count("progression", "perks", static_cast<int>(a_out.perks.size()));
    }

    void ImportProgression(RE::PlayerCharacter* a_player, const schema::Progression& a_in, core::Report& a_report)
    {
        auto* avOwner = a_player->AsActorValueOwner();
        auto* npc = a_player->GetActorBase();
        auto* skills = a_player->GetInfoRuntimeData().skills;
        auto* data = skills ? skills->data : nullptr;

        if (npc) {
            npc->actorData.level = static_cast<std::uint16_t>(std::clamp<std::uint32_t>(a_in.level, 1, 65535));
        }
        if (data) {
            data->xp = a_in.xp;
            data->levelThreshold = a_in.xpThreshold;
        }
        for (const auto& [name, s] : a_in.skills) {
            const auto idx = SkillIndex(name);
            if (!idx) {
                a_report.Skipped("progression", name, "unknown skill name");
                continue;
            }
            avOwner->SetBaseActorValue(SkillAV(*idx), s.level);
            if (data) {
                data->skills[*idx].level = s.level;
                data->skills[*idx].xp = s.xp;
                data->skills[*idx].levelThreshold = s.threshold;
                data->legendaryLevels[*idx] = s.legendary;
            }
        }
        for (const auto& [name, av] : kAttributes) {
            if (auto it = a_in.attributes.find(name); it != a_in.attributes.end()) {
                avOwner->SetBaseActorValue(av, it->second);
            }
        }
        avOwner->SetBaseActorValue(RE::ActorValue::kDragonSouls, a_in.dragonSouls);

        int added = 0;
        for (const auto& key : a_in.perks) {
            auto* perk = key.Resolve<RE::BGSPerk>();
            if (!perk) {
                a_report.Missing("progression", key.ToString(), "perk");
                continue;
            }
            if (!a_player->HasPerk(perk)) {
                a_player->AddPerk(perk, 0);
                ++added;
            }
        }
        a_player->GetGameStatsData().perkCount = static_cast<std::int8_t>(std::clamp(a_in.perkPoints, 0, 127));
        a_report.Count("progression", "perksAdded", added);
        a_report.Count("progression", "skills", static_cast<int>(a_in.skills.size()));
    }
}
