#include "character/Character.h"

namespace sst::character {
    using core::FormKey;

    namespace {
        // RE::TintMask is only forward-declared in CommonLibSSE-NG 3.7.0. Layout per SKSE64 (GameFormComponents.h).
        struct TintMaskView {
            RE::TESTexture* texture;   // 00
            RE::Color       color;     // 08 (alpha byte unused)
            float           alpha;     // 0C
            std::uint32_t   tintType;  // 10
            std::uint32_t   pad14;     // 14
        };
        static_assert(sizeof(TintMaskView) == 0x18);
        constexpr std::uint32_t kTintTypeSkinTone = 6;

        TintMaskView*       AsView(RE::TintMask* a_mask) { return reinterpret_cast<TintMaskView*>(a_mask); }
        const TintMaskView* AsView(const RE::TintMask* a_mask) { return reinterpret_cast<const TintMaskView*>(a_mask); }
    }

    std::string ColorToHex(const RE::Color& c)
    {
        return std::format("#{:02X}{:02X}{:02X}", c.red, c.green, c.blue);
    }

    RE::Color HexToColor(const std::string& a_hex)
    {
        RE::Color c;
        if (a_hex.size() == 7 && a_hex[0] == '#') {
            const auto v = std::stoul(a_hex.substr(1), nullptr, 16);
            c.red = static_cast<std::uint8_t>((v >> 16) & 0xFF);
            c.green = static_cast<std::uint8_t>((v >> 8) & 0xFF);
            c.blue = static_cast<std::uint8_t>(v & 0xFF);
        }
        return c;
    }

    void ExportAppearance(RE::PlayerCharacter* a_player, schema::Character& a_out, core::Report& a_report)
    {
        auto* npc = a_player->GetActorBase();
        if (!npc) {
            a_report.Error("appearance", "player", "no actor base");
            return;
        }
        a_out.name = a_player->GetName() ? a_player->GetName() : "";
        a_out.race = FormKey::From(a_player->GetRace());
        a_out.female = npc->IsFemale();
        a_out.height = npc->height;
        a_out.weight = npc->weight;

        auto& ap = a_out.appearance;
        for (std::int8_t i = 0; i < npc->numHeadParts; ++i) {
            if (auto* part = npc->headParts[i]) {
                const auto key = FormKey::From(part);
                if (key.IsValid()) {
                    ap.headParts.push_back(key);
                } else {
                    a_report.Skipped("appearance", std::format("head part 0x{:08X}", part->GetFormID()), "dynamic form");
                }
            }
        }
        if (npc->faceData) {
            ap.faceMorphs.assign(std::begin(npc->faceData->morphs), std::end(npc->faceData->morphs));
            ap.faceParts.assign(std::begin(npc->faceData->parts), std::end(npc->faceData->parts));
        }
        if (npc->headRelatedData && npc->headRelatedData->hairColor) {
            ap.hairColor = FormKey::From(npc->headRelatedData->hairColor);
        }
        ap.bodyTint = ColorToHex(npc->bodyTintColor);
        // The player's face tints live on PlayerCharacter (not TESNPC::tintLayers, which is for NPCs).
        auto* maskList = a_player->GetTintList();
        const auto& masks = maskList ? *maskList : RE::BSTArray<RE::TintMask*>{};
        for (std::uint32_t i = 0; i < masks.size(); ++i) {
            const auto* mask = AsView(masks[i]);
            if (!mask) {
                continue;
            }
            schema::Tint t;
            t.index = i;
            t.type = mask->tintType;
            t.texture = mask->texture ? mask->texture->textureName.c_str() : "";
            t.color = ColorToHex(mask->color);
            t.alpha = mask->alpha;
            ap.tints.push_back(std::move(t));
        }
        a_report.Count("appearance", "headParts", static_cast<int>(ap.headParts.size()));
        a_report.Count("appearance", "tints", static_cast<int>(ap.tints.size()));
    }

    void ImportAppearance(RE::PlayerCharacter* a_player, const schema::Character& a_in, core::Report& a_report)
    {
        auto* npc = a_player->GetActorBase();
        if (!npc) {
            a_report.Error("appearance", "player", "no actor base");
            return;
        }
        // Race / sex first: SwitchRace rebuilds the actor, everything else is layered on top.
        auto* race = a_in.race.Resolve<RE::TESRace>();
        if (!race) {
            a_report.Missing("appearance", a_in.race.ToString(), "race not in load order; keeping current race");
        }
        const bool sexChanged = npc->IsFemale() != a_in.female;
        if (sexChanged) {
            if (a_in.female) {
                npc->actorData.actorBaseFlags.set(RE::ACTOR_BASE_DATA::Flag::kFemale);
            } else {
                npc->actorData.actorBaseFlags.reset(RE::ACTOR_BASE_DATA::Flag::kFemale);
            }
        }
        if (race && (race != a_player->GetRace() || sexChanged)) {
            a_player->SwitchRace(race, true);
            a_report.Count("appearance", "raceSwitched");
        }
        npc->height = a_in.height;
        npc->weight = a_in.weight;

        const auto& ap = a_in.appearance;
        for (const auto& key : ap.headParts) {
            if (auto* part = key.Resolve<RE::BGSHeadPart>()) {
                npc->ChangeHeadPart(part);
                a_report.Count("appearance", "headParts");
            } else {
                a_report.Missing("appearance", key.ToString(), "head part");
            }
        }
        if (npc->faceData) {
            const auto nMorphs = std::min<std::size_t>(ap.faceMorphs.size(), RE::TESNPC::FaceData::Morphs::kTotal);
            for (std::size_t i = 0; i < nMorphs; ++i) {
                npc->faceData->morphs[i] = ap.faceMorphs[i];
            }
            const auto nParts = std::min<std::size_t>(ap.faceParts.size(), RE::TESNPC::FaceData::Parts::kTotal);
            for (std::size_t i = 0; i < nParts; ++i) {
                npc->faceData->parts[i] = ap.faceParts[i];
            }
        } else if (!ap.faceMorphs.empty()) {
            a_report.Skipped("appearance", "faceMorphs", "target actor base has no face data block");
        }
        if (ap.hairColor.IsValid()) {
            if (auto* color = ap.hairColor.Resolve<RE::BGSColorForm>()) {
                npc->SetHairColor(color);
            } else {
                a_report.Missing("appearance", ap.hairColor.ToString(), "hair color");
            }
        }
        npc->bodyTintColor = HexToColor(ap.bodyTint);
        {
            auto* maskList = a_player->GetTintList();
            RE::BSTArray<RE::TintMask*> empty;
            auto& masks = maskList ? *maskList : empty;
            int applied = 0;
            std::vector<bool> used(masks.size(), false);
            for (const auto& t : ap.tints) {
                // Prefer an exact (type, texture) match; fall back to the same index with the same type.
                std::int64_t target = -1;
                for (std::uint32_t i = 0; i < masks.size() && target < 0; ++i) {
                    const auto* m = AsView(masks[i]);
                    if (m && !used[i] && m->tintType == t.type && (m->texture ? m->texture->textureName.c_str() : "") == t.texture) {
                        target = i;
                    }
                }
                if (target < 0 && t.index < masks.size()) {
                    const auto* m = AsView(masks[t.index]);
                    if (m && !used[t.index] && m->tintType == t.type) {
                        target = t.index;
                    }
                }
                if (target < 0) {
                    a_report.Skipped("appearance", std::format("tint type {} '{}'", t.type, t.texture), "no matching tint mask on target");
                    continue;
                }
                used[target] = true;
                auto* m = AsView(masks[target]);
                m->color = HexToColor(t.color);
                m->color.alpha = 0;
                m->alpha = std::clamp(t.alpha, 0.0f, 1.0f);
                if (m->tintType == kTintTypeSkinTone) {
                    RE::NiColorA result;
                    npc->SetSkinFromTint(&result, masks[target], true);
                }
                ++applied;
            }
            a_report.Count("appearance", "tints", applied);
        }
        a_player->DoReset3D(true);
    }
}
