#include "character/Character.h"

namespace sst::character {
    using core::FormKey;

    namespace {
        constexpr RE::FormID kGold001 = 0xF;

        const RE::BGSEquipSlot* HandSlot(bool a_left)
        {
            auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
            return dom ? dom->GetObject<RE::BGSEquipSlot>(a_left ? RE::DEFAULT_OBJECT::kLeftHandEquip : RE::DEFAULT_OBJECT::kRightHandEquip) : nullptr;
        }

        void FillFromExtra(RE::TESBoundObject* a_obj, RE::ExtraDataList* a_xList, schema::Item& a_item, core::Report& a_report)
        {
            if (auto* h = a_xList->GetByType<RE::ExtraHealth>()) {
                a_item.health = h->health;
            }
            if (auto* c = a_xList->GetByType<RE::ExtraCharge>()) {
                a_item.charge = c->charge;
            }
            if (auto* e = a_xList->GetByType<RE::ExtraEnchantment>(); e && e->enchantment) {
                const auto key = FormKey::From(e->enchantment);
                if (key.IsValid()) {
                    a_item.enchantment = key;
                } else {
                    for (auto* eff : e->enchantment->effects) {
                        if (!eff || !eff->baseEffect) {
                            continue;
                        }
                        schema::CustomEffect ce;
                        ce.effect = FormKey::From(eff->baseEffect);
                        ce.mag = eff->effectItem.magnitude;
                        ce.dur = static_cast<float>(eff->effectItem.duration);
                        ce.area = static_cast<float>(eff->effectItem.area);
                        if (ce.effect.IsValid()) {
                            a_item.customEnchant.push_back(ce);
                        }
                    }
                    a_report.Count("inventory", "customEnchantments");
                }
            }
            if (auto* s = a_xList->GetByType<RE::ExtraSoul>()) {
                a_item.soul = static_cast<int>(s->soul.get());
            }
            if (const char* shown = a_xList->GetDisplayName(a_obj); shown && a_obj->GetName() && std::string_view(shown) != a_obj->GetName()) {
                a_item.name = shown;
            }
            if (auto* hk = a_xList->GetByType<RE::ExtraHotkey>()) {
                a_item.favorite = true;
                const auto v = static_cast<std::int8_t>(hk->hotkey.underlying());
                if (v >= 0 && v <= 7) {
                    a_item.hotkey = v;
                }
            }
            if (a_xList->HasType(RE::ExtraDataType::kWornLeft)) {
                a_item.equippedSlot = "left";
            } else if (a_xList->HasType(RE::ExtraDataType::kWorn)) {
                a_item.equippedSlot = a_obj->Is(RE::FormType::Weapon) ? "right" : "worn";
            }
        }
    }

    void ExportInventory(RE::PlayerCharacter* a_player, schema::Inventory& a_out, core::Report& a_report)
    {
        auto inventory = a_player->GetInventory();
        for (auto& [obj, pair] : inventory) {
            const auto count = pair.first;
            auto* entry = pair.second.get();
            if (!obj || count <= 0) {
                continue;
            }
            if (obj->GetFormID() == kGold001) {
                a_out.gold = count;
                continue;
            }
            const auto key = FormKey::From(obj);
            if (!key.IsValid()) {
                a_out.unresolvable.push_back({ std::format("0x{:08X}", obj->GetFormID()), obj->GetName() ? obj->GetName() : "", "dynamic form" });
                continue;
            }
            std::int32_t accounted = 0;
            if (entry && entry->extraLists) {
                for (auto* xList : *entry->extraLists) {
                    if (!xList) {
                        continue;
                    }
                    schema::Item item;
                    item.form = key;
                    item.count = std::max(1, xList->GetCount());
                    FillFromExtra(obj, xList, item, a_report);
                    accounted += item.count;
                    a_out.items.push_back(std::move(item));
                }
            }
            if (count - accounted > 0) {
                schema::Item plain;
                plain.form = key;
                plain.count = count - accounted;
                a_out.items.push_back(std::move(plain));
            }
        }
        a_report.Count("inventory", "stacks", static_cast<int>(a_out.items.size()));
        a_report.Count("inventory", "unresolvable", static_cast<int>(a_out.unresolvable.size()));
    }

    void ImportInventory(RE::PlayerCharacter* a_player, const schema::Inventory& a_in, core::Report& a_report)
    {
        auto* equip = RE::ActorEquipManager::GetSingleton();
        if (a_in.gold > 0) {
            if (auto* gold = RE::TESForm::LookupByID<RE::TESBoundObject>(kGold001)) {
                a_player->AddObjectToContainer(gold, nullptr, a_in.gold, nullptr);
            }
        }
        int added = 0;
        for (const auto& item : a_in.items) {
            auto* obj = item.form.Resolve<RE::TESBoundObject>();
            if (!obj) {
                a_report.Missing("inventory", item.form.ToString(), item.name.empty() ? "item" : item.name);
                continue;
            }
            RE::ExtraDataList* xList = nullptr;
            auto extra = [&]() {
                if (!xList) {
                    // NG 3.7.0 declares but does not define ExtraDataList(); the engine constructor only zeroes
                    // the members, so a zeroed game-heap allocation is equivalent.
                    xList = RE::calloc<RE::ExtraDataList>(1);
                }
                return xList;
            };
            if (item.health > 0.0f) {
                extra()->Add(new RE::ExtraHealth(item.health));
            }
            if (item.enchantment.IsValid()) {
                if (auto* ench = item.enchantment.Resolve<RE::EnchantmentItem>()) {
                    extra()->Add(new RE::ExtraEnchantment(ench, static_cast<std::uint16_t>(std::clamp(item.charge, 0.0f, 65535.0f))));
                } else {
                    a_report.Missing("inventory", item.enchantment.ToString(), "enchantment on " + item.form.ToString());
                }
            } else if (item.charge >= 0.0f) {
                auto* c = new RE::ExtraCharge();
                c->charge = item.charge;
                extra()->Add(c);
            }
            if (!item.customEnchant.empty()) {
                a_report.Skipped("inventory", item.name.empty() ? item.form.ToString() : item.name, "player-made enchantment not recreated (M3)");
            }
            if (item.soul > 0) {
                extra()->Add(new RE::ExtraSoul(static_cast<RE::SOUL_LEVEL>(item.soul)));
            }
            if (!item.name.empty()) {
                extra()->Add(new RE::ExtraTextDisplayData(item.name.c_str()));
            }
            if (item.favorite) {
                extra()->Add(new RE::ExtraHotkey(static_cast<RE::ExtraHotkey::Hotkey>(item.hotkey >= 0 ? item.hotkey : -1)));
            }
            a_player->AddObjectToContainer(obj, xList, item.count, nullptr);
            ++added;
            if (equip && !item.equippedSlot.empty()) {
                const RE::BGSEquipSlot* slot = nullptr;
                if (item.equippedSlot == "right") {
                    slot = HandSlot(false);
                } else if (item.equippedSlot == "left") {
                    slot = HandSlot(true);
                }
                equip->EquipObject(a_player, obj, xList, 1, slot, true, false, false, false);
            }
        }
        a_report.Count("inventory", "stacksAdded", added);
        for (const auto& u : a_in.unresolvable) {
            a_report.Skipped("inventory", u.name.empty() ? u.formID : u.name, u.reason + " in source");
        }
    }
}
