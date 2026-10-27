#include "character/Character.h"

namespace sst::character {
    using core::FormKey;

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
        if (npc->tintLayers) {
            for (auto* layer : *npc->tintLayers) {
                if (!layer) {
                    continue;
                }
                schema::Tint t;
                t.index = layer->tintIndex;
                t.color = ColorToHex(layer->tintColor);
                t.alpha = layer->interpolationValue / 100.0f;
                t.preset = layer->preset;
                ap.tints.push_back(t);
            }
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
        if (npc->tintLayers) {
            // Reuse existing layer objects where possible; allocate extras on the game heap.
            auto& layers = *npc->tintLayers;
            while (layers.size() > ap.tints.size()) {
                layers.pop_back();
            }
            for (std::size_t i = 0; i < ap.tints.size(); ++i) {
                RE::TESNPC::Layer* layer = i < layers.size() ? layers[i] : nullptr;
                if (!layer) {
                    layer = new RE::TESNPC::Layer();
                    if (i < layers.size()) {
                        layers[i] = layer;
                    } else {
                        layers.push_back(layer);
                    }
                }
                layer->tintIndex = static_cast<std::uint16_t>(ap.tints[i].index);
                layer->tintColor = HexToColor(ap.tints[i].color);
                layer->tintColor.alpha = 0;
                layer->interpolationValue = static_cast<std::uint16_t>(std::lround(ap.tints[i].alpha * 100.0f));
                layer->preset = static_cast<std::uint16_t>(ap.tints[i].preset);
            }
            a_report.Count("appearance", "tints", static_cast<int>(ap.tints.size()));
        }
        a_player->DoReset3D(true);
    }
}
