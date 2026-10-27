#include "character/Character.h"

#include "core/ConsoleExec.h"

namespace sst::character {
    using core::FormKey;

    namespace {
        const RE::BGSEquipSlot* HandSlot(bool a_left)
        {
            auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
            return dom ? dom->GetObject<RE::BGSEquipSlot>(a_left ? RE::DEFAULT_OBJECT::kLeftHandEquip : RE::DEFAULT_OBJECT::kRightHandEquip) : nullptr;
        }
    }

    void ExportMagic(RE::PlayerCharacter* a_player, schema::Magic& a_out, core::Report& a_report)
    {
        for (auto* spell : a_player->GetActorRuntimeData().addedSpells) {
            if (!spell) {
                continue;
            }
            const auto key = FormKey::From(spell);
            if (key.IsValid()) {
                a_out.spells.push_back(key);
            } else {
                a_report.Skipped("magic", std::format("spell 0x{:08X} '{}'", spell->GetFormID(), spell->GetName()), "dynamic form");
            }
        }

        auto* dh = RE::TESDataHandler::GetSingleton();
        for (auto* shout : dh->GetFormArray<RE::TESShout>()) {
            if (!shout) {
                continue;
            }
            bool anyWord = false;
            for (const auto& v : shout->variations) {
                if (v.word && v.word->GetKnown()) {
                    const auto key = FormKey::From(v.word);
                    if (key.IsValid()) {
                        a_out.wordsKnown.push_back(key);
                        anyWord = true;
                    }
                }
            }
            if (anyWord || shout->GetKnown()) {
                const auto key = FormKey::From(shout);
                if (key.IsValid()) {
                    a_out.shouts.push_back(key);
                }
            }
        }
        // The engine does not expose "unlocked" separately through CommonLib; treat known words as unlocked.
        a_out.wordsUnlocked = a_out.wordsKnown;

        if (auto* fav = RE::MagicFavorites::GetSingleton()) {
            for (auto* f : fav->spells) {
                const auto key = FormKey::From(f);
                if (key.IsValid()) {
                    a_out.favorites.spells.push_back(key);
                }
            }
            for (auto* f : fav->hotkeys) {
                const auto key = FormKey::From(f);
                a_out.favorites.hotkeys.emplace_back(key.IsValid() ? std::optional<FormKey>(key) : std::nullopt);
            }
        }

        if (auto* right = a_player->GetEquippedObject(false); right && right->Is(RE::FormType::Spell)) {
            a_out.equippedRight = FormKey::From(right);
        }
        if (auto* left = a_player->GetEquippedObject(true); left && left->Is(RE::FormType::Spell)) {
            a_out.equippedLeft = FormKey::From(left);
        }
        if (auto* shout = a_player->GetCurrentShout()) {
            a_out.equippedShout = FormKey::From(shout);
        }
        a_report.Count("magic", "spells", static_cast<int>(a_out.spells.size()));
        a_report.Count("magic", "shouts", static_cast<int>(a_out.shouts.size()));
        a_report.Count("magic", "words", static_cast<int>(a_out.wordsKnown.size()));
    }

    void ImportMagic(RE::PlayerCharacter* a_player, const schema::Magic& a_in, core::Report& a_report)
    {
        int added = 0;
        for (const auto& key : a_in.spells) {
            auto* spell = key.Resolve<RE::SpellItem>();
            if (!spell) {
                a_report.Missing("magic", key.ToString(), "spell");
                continue;
            }
            if (!a_player->HasSpell(spell)) {
                a_player->AddSpell(spell);
                ++added;
            }
        }
        a_report.Count("magic", "spellsAdded", added);

        int words = 0;
        for (const auto& key : a_in.wordsKnown) {
            auto* word = key.Resolve<RE::TESWordOfPower>();
            if (!word) {
                a_report.Missing("magic", key.ToString(), "word of power");
                continue;
            }
            const auto id = std::format("{:X}", word->GetFormID());
            core::ConsoleExec("teachword " + id);
            core::ConsoleExec("unlockword " + id);
            ++words;
        }
        a_report.Count("magic", "wordsTaught", words);

        if (auto* fav = RE::MagicFavorites::GetSingleton()) {
            for (const auto& key : a_in.favorites.spells) {
                if (auto* form = key.Resolve()) {
                    fav->SetFavorite(form);
                }
            }
            for (std::size_t i = 0; i < a_in.favorites.hotkeys.size() && i < fav->hotkeys.size(); ++i) {
                const auto& h = a_in.favorites.hotkeys[i];
                if (h) {
                    if (auto* form = h->Resolve()) {
                        fav->hotkeys[i] = form;
                    }
                }
            }
        }

        auto* equip = RE::ActorEquipManager::GetSingleton();
        if (equip) {
            if (auto* spell = a_in.equippedRight.Resolve<RE::SpellItem>()) {
                equip->EquipSpell(a_player, spell, HandSlot(false));
            }
            if (auto* spell = a_in.equippedLeft.Resolve<RE::SpellItem>()) {
                equip->EquipSpell(a_player, spell, HandSlot(true));
            }
            if (auto* shout = a_in.equippedShout.Resolve<RE::TESShout>()) {
                equip->EquipShout(a_player, shout);
            }
        }
    }
}
