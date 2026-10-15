/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#ifndef _ITEM_SCALING_EQUIP_SPELL_H
#define _ITEM_SCALING_EQUIP_SPELL_H

#include "Common.h"
#include "ItemPrototype.h"

enum EquipSpellFamily : uint8
{
    EQUIP_SPELL_FAMILY_NONE = 0,
    EQUIP_SPELL_FAMILY_SPELL_DAMAGE,
    EQUIP_SPELL_FAMILY_HEALING,
    EQUIP_SPELL_FAMILY_MANA_REGEN,
    EQUIP_SPELL_FAMILY_ARMOR_PENETRATION,
    EQUIP_SPELL_FAMILY_ATTACK_POWER,
    EQUIP_SPELL_FAMILY_RANGED_ATTACK_POWER,
    EQUIP_SPELL_FAMILY_HEALTH_REGEN,
    EQUIP_SPELL_FAMILY_DEFENSE,
    EQUIP_SPELL_FAMILY_BLOCK_VALUE,
    EQUIP_SPELL_FAMILY_HOLY_DAMAGE,
    EQUIP_SPELL_FAMILY_FIRE_DAMAGE,
    EQUIP_SPELL_FAMILY_NATURE_DAMAGE,
    EQUIP_SPELL_FAMILY_FROST_DAMAGE,
    EQUIP_SPELL_FAMILY_SHADOW_DAMAGE,
    EQUIP_SPELL_FAMILY_ARCANE_DAMAGE,
    EQUIP_SPELL_FAMILY_SPELL_PENETRATION,
    EQUIP_SPELL_FAMILY_COUNT
};

namespace ItemScalingEquipSpell
{
    // Initializes runtime ladders, scanning loaded spells and merging with canonical tables.
    void Initialize();

    // Identifies if the given spell ID is a recognized scalable on-equip effect.
    [[nodiscard]] bool IsScalableEquipSpell(uint32 spellId);

    // Returns the equip spell family for a spell ID.
    [[nodiscard]] EquipSpellFamily GetSpellFamily(uint32 spellId);

    // Extracts the base magnitude of the equip effect (e.g. 20 for Armor Pen, 6 for MP5, 15 for Spell Dam).
    [[nodiscard]] int32 GetEquipSpellBaseValue(uint32 spellId);

    // Returns a human-readable family description.
    [[nodiscard]] char const* GetFamilyName(EquipSpellFamily family);

    // Finds the best matching scaled spell ID for a given base spell ID and budget scaling ratio.
    [[nodiscard]] uint32 FindScaledEquipSpell(uint32 baseSpellId, double rBudget);

    // Scales all passive on-equip spells on a prototype in-place.
    void ScalePrototypeEquipSpells(ItemPrototype const* baseProto, ItemPrototype& scaledProto, double rBudget);
}

#endif // _ITEM_SCALING_EQUIP_SPELL_H
