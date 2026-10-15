/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#ifndef _ITEM_SCALING_IDENTITY_H
#define _ITEM_SCALING_IDENTITY_H

#include "ItemScalingCommon.h"
#include "ItemScalingEquipSpell.h"

// DBC-enforced identity fields for Turtle WoW ItemPrototype.
struct ItemScalingIdentity
{
    uint32 itemClass{0};
    uint32 subClass{0};
    int32 material{0};
    uint32 displayId{0};
    uint32 inventoryType{0};
    uint32 sheath{0};

    static bool CanCapture(ItemPrototype const& item)
    {
        int32 mat = static_cast<int16>(item.Material);
        return item.Class <= 255 && item.SubClass <= 255 && item.InventoryType <= 255 && item.Sheath <= 255 &&
               mat >= -128 && mat <= 127;
    }

    static ItemScalingIdentity Capture(ItemPrototype const& item)
    {
        return {item.Class, item.SubClass, static_cast<int16>(item.Material), item.DisplayInfoID,
                item.InventoryType, item.Sheath};
    }

    bool Matches(ItemPrototype const& item) const
    {
        return item.Class == itemClass && item.SubClass == subClass &&
               static_cast<int16>(item.Material) == material && item.DisplayInfoID == displayId &&
               item.InventoryType == inventoryType && item.Sheath == sheath;
    }

    // Synthetic IDs skip core DBC validation. Withhold rows whose inherited loot semantics differ.
    static bool CompatibleLootMetadata(ItemPrototype const& base, ItemPrototype const& variant, bool baked = false)
    {
        if (base.Quality != variant.Quality || base.Flags != variant.Flags ||
            base.MaxCount != variant.MaxCount || base.StartQuest != variant.StartQuest ||
            base.RequiredSkill != variant.RequiredSkill || base.AllowableClass != variant.AllowableClass ||
            base.AllowableRace != variant.AllowableRace || base.Bonding != variant.Bonding ||
            base.Stackable != variant.Stackable)
        {
            return false;
        }

        if (baked)
        {
            if (variant.RandomProperty != 0)
                return false;
        }
        else
        {
            if (base.RandomProperty != variant.RandomProperty)
                return false;
        }

        for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (base.Spells[i].SpellId != variant.Spells[i].SpellId)
            {
                if (base.Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP &&
                    variant.Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP &&
                    ItemScalingEquipSpell::IsScalableEquipSpell(base.Spells[i].SpellId) &&
                    ItemScalingEquipSpell::IsScalableEquipSpell(variant.Spells[i].SpellId) &&
                    ItemScalingEquipSpell::GetSpellFamily(base.Spells[i].SpellId) == ItemScalingEquipSpell::GetSpellFamily(variant.Spells[i].SpellId))
                {
                    continue;
                }

                return false;
            }
        }

        return true;
    }

    // Used only on module-owned temporary templates before their SQL insert.
    void Apply(ItemPrototype& item) const
    {
        item.Class = itemClass;
        item.SubClass = subClass;
        item.Material = static_cast<uint32>(material);
        item.DisplayInfoID = displayId;
        item.InventoryType = inventoryType;
        item.Sheath = sheath;
    }
};

#endif // _ITEM_SCALING_IDENTITY_H
