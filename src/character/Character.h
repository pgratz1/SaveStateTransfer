#pragma once

#include "app/Schema.h"
#include "core/Report.h"

// Each sub-module fills/applies its slice of schema::Character. All functions run on the game thread.
namespace sst::character {
    void ExportAppearance(RE::PlayerCharacter* a_player, schema::Character& a_out, core::Report& a_report);
    void ImportAppearance(RE::PlayerCharacter* a_player, const schema::Character& a_in, core::Report& a_report);

    void ExportProgression(RE::PlayerCharacter* a_player, schema::Progression& a_out, core::Report& a_report);
    void ImportProgression(RE::PlayerCharacter* a_player, const schema::Progression& a_in, core::Report& a_report);

    void ExportMagic(RE::PlayerCharacter* a_player, schema::Magic& a_out, core::Report& a_report);
    void ImportMagic(RE::PlayerCharacter* a_player, const schema::Magic& a_in, core::Report& a_report);

    void ExportInventory(RE::PlayerCharacter* a_player, schema::Inventory& a_out, core::Report& a_report);
    void ImportInventory(RE::PlayerCharacter* a_player, const schema::Inventory& a_in, core::Report& a_report);

    // shared helpers
    std::string ColorToHex(const RE::Color& a_color);
    RE::Color   HexToColor(const std::string& a_hex);
}
