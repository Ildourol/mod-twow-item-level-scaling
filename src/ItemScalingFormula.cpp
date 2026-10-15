/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingFormula.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingEquipSpell.h"
#include "ItemScalingTarget.h"
#include "Database/DBCStores.h"
#include "Database/DBCStructure.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace ItemScalingFormula
{
    bool LoadStartupCurves(std::string const& /*dataPath*/)
    {
        // On Turtle WoW vMaNGOS, baseline statistical medians are generated directly from
        // the loaded ItemPrototype store by ItemScalingBaseline, so external DBC files
        // are not required.
        return true;
    }

    int8 GetSlotFamily(uint32 inventoryType)
    {
        switch (inventoryType)
        {
            // Family 0: Head, Body/Chest, Legs, 2H Weapon, Robe
            case INVTYPE_HEAD:
            case INVTYPE_BODY:
            case INVTYPE_CHEST:
            case INVTYPE_LEGS:
            case INVTYPE_2HWEAPON:
            case INVTYPE_ROBE:
                return 0;

            // Family 1: Shoulders, Waist, Feet, Hands, Trinket
            case INVTYPE_SHOULDERS:
            case INVTYPE_WAIST:
            case INVTYPE_FEET:
            case INVTYPE_HANDS:
            case INVTYPE_TRINKET:
                return 1;

            // Family 2: Neck, Wrists, Finger, Shield, Cloak, Holdable, Relic
            case INVTYPE_NECK:
            case INVTYPE_WRISTS:
            case INVTYPE_FINGER:
            case INVTYPE_SHIELD:
            case INVTYPE_CLOAK:
            case INVTYPE_HOLDABLE:
            case INVTYPE_RELIC:
                return 2;

            // Family 3: 1H Weapon, Main-Hand, Off-Hand
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_WEAPONOFFHAND:
                return 3;

            // Family 4: Ranged, Thrown, Ranged Right
            case INVTYPE_RANGED:
            case INVTYPE_THROWN:
            case INVTYPE_RANGEDRIGHT:
                return 4;

            default:
                return -1;
        }
    }

    double GetBudgetRatio(uint32 baseIlvl, uint32 targetIlvl, uint32 /*quality*/, uint8 /*slotFamily*/)
    {
        if (baseIlvl == targetIlvl || baseIlvl == 0)
        {
            return 1.0;
        }

        return static_cast<double>(targetIlvl) / static_cast<double>(std::max<uint32>(1, baseIlvl));
    }

    int32 ScaleAdditiveStat(int32 v0, double r, bool preserveNonZero)
    {
        if (v0 == 0)
        {
            return 0;
        }
        if (r <= 0.0)
        {
            return v0;
        }

        double scaled = static_cast<double>(v0) * r;
        int32 v1 = static_cast<int32>(std::clamp(std::round(scaled),
            static_cast<double>(std::numeric_limits<int32>::min()),
            static_cast<double>(std::numeric_limits<int32>::max())));

        if (preserveNonZero && v1 == 0)
        {
            v1 = (v0 > 0) ? 1 : -1;
        }

        return v1;
    }

    uint32 ScaleArmor(ItemPrototype const* baseProto, double rBudget, uint8 l0, uint8 lTarget)
    {
        if (!baseProto || baseProto->Armor == 0)
        {
            return 0;
        }

        if (l0 == lTarget || rBudget <= 0.0)
        {
            return static_cast<uint32>(std::max<int32>(0, baseProto->Armor));
        }

        double scaled = static_cast<double>(baseProto->Armor) * rBudget;
        return static_cast<uint32>(std::clamp(std::round(scaled),
            1.0, static_cast<double>(std::numeric_limits<uint32>::max())));
    }

    void ScaleWeaponDamage(ItemPrototype const* baseProto, ItemPrototype& scaledProto, double rBudget, uint8 l0, uint8 lTarget)
    {
        if (!baseProto)
        {
            return;
        }

        if (l0 == lTarget || rBudget <= 0.0)
        {
            for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
            {
                scaledProto.Damage[i] = baseProto->Damage[i];
            }
            return;
        }

        for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            float min0 = baseProto->Damage[i].DamageMin;
            float max0 = baseProto->Damage[i].DamageMax;
            scaledProto.Damage[i].DamageType = baseProto->Damage[i].DamageType;

            if (min0 <= 0.0f && max0 <= 0.0f)
            {
                scaledProto.Damage[i].DamageMin = 0.0f;
                scaledProto.Damage[i].DamageMax = 0.0f;
                continue;
            }

            float avg0 = (min0 + max0) * 0.5f;
            float spread = (max0 + min0 > 0.0f) ? ((max0 - min0) / (max0 + min0)) : 0.0f;

            float avg1 = avg0 * static_cast<float>(rBudget);
            float min1 = avg1 * (1.0f - spread);
            float max1 = avg1 * (1.0f + spread);

            if (min1 < 1.0f && avg0 > 0.0f)
            {
                min1 = 1.0f;
            }
            if (max1 < min1)
            {
                max1 = min1;
            }

            scaledProto.Damage[i].DamageMin = std::round(min1);
            scaledProto.Damage[i].DamageMax = std::round(max1);
        }
    }

    uint8 CalculateRequiredLevel(ItemPrototype const* baseProto, uint8 targetEffectiveLevel, uint8 highestRealPlayerLevel)
    {
        if (!baseProto)
        {
            return 0;
        }

        switch (sItemScalingConfig->ReqLevelPolicy)
        {
            case REQ_POLICY_PLAYER:
                return std::clamp<uint8>(highestRealPlayerLevel, 1, 80);
            case REQ_POLICY_TARGET:
                return std::clamp<uint8>(targetEffectiveLevel, 1, 80);
            case REQ_POLICY_TARGET_CAPPED_PLAYER:
            default:
                return std::clamp<uint8>(std::min<uint8>(targetEffectiveLevel, highestRealPlayerLevel), 1, 80);
        }
    }

    bool IsScalableEquipment(ItemPrototype const* proto)
    {
        if (!proto)
        {
            return false;
        }

        // Combat equipment only
        if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
        {
            return false;
        }

        int8 family = GetSlotFamily(proto->InventoryType);
        if (family < 0)
        {
            return false;
        }

        // Exclude qualities based on config
        if (!sItemScalingConfig->IsQualityEnabled(proto->Quality))
        {
            return false;
        }

        // Exclude specific configured item IDs
        if (sItemScalingConfig->IsItemExcluded(proto->ItemId))
        {
            return false;
        }

        // Check if item contains at least one scalable combat attribute
        bool hasScalableField = (proto->Armor > 0) ||
                                (proto->Block > 0) ||
                                (proto->Damage[0].DamageMax > 0.0f) ||
                                (proto->HolyRes != 0) ||
                                (proto->FireRes != 0) ||
                                (proto->NatureRes != 0) ||
                                (proto->FrostRes != 0) ||
                                (proto->ShadowRes != 0) ||
                                (proto->ArcaneRes != 0);

        if (hasScalableField)
        {
            return true;
        }

        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (proto->ItemStat[i].ItemStatValue != 0)
            {
                return true;
            }
        }

        if (sItemScalingConfig->ScaleEquipSpells)
        {
            for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            {
                if (proto->Spells[i].SpellId != 0 && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP)
                {
                    if (ItemScalingEquipSpell::IsScalableEquipSpell(proto->Spells[i].SpellId))
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool IsScalableEquipSpell(uint32 spellId)
    {
        return ItemScalingEquipSpell::IsScalableEquipSpell(spellId);
    }

    uint32 FindScaledEquipSpell(uint32 baseSpellId, double rBudget)
    {
        return ItemScalingEquipSpell::FindScaledEquipSpell(baseSpellId, rBudget);
    }

    bool CanBakeRandomProperty(ItemPrototype const& base, int32 randomPropertyId)
    {
        if (randomPropertyId <= 0)
        {
            return false;
        }

        if (base.RandomProperty == 0)
        {
            return false;
        }

        ItemRandomPropertiesEntry const* propEntry = sItemRandomPropertiesStore.LookupEntry(static_cast<uint32>(randomPropertyId));
        if (!propEntry)
        {
            return false;
        }

        std::set<uint32> types;
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (base.ItemStat[i].ItemStatValue != 0)
            {
                types.insert(base.ItemStat[i].ItemStatType);
            }
        }

        bool hasBonus = false;
        for (uint32 k = 0; k < 3; ++k)
        {
            uint32 id = propEntry->enchant_id[k];
            if (!id)
            {
                continue;
            }

            SpellItemEnchantmentEntry const* ench = sSpellItemEnchantmentStore.LookupEntry(id);
            if (!ench)
            {
                return false;
            }

            for (uint32 i = 0; i < 3; ++i)
            {
                uint32 type = ench->type[i];
                if (!type)
                {
                    continue;
                }

                hasBonus = true;
                if (type == 5) // ITEM_ENCHANTMENT_TYPE_STAT
                {
                    if (ench->spellid[i] >= MAX_ITEM_MOD)
                    {
                        return false;
                    }
                    types.insert(ench->spellid[i]);
                }
                else if (type != 4 || ench->spellid[i] < SPELL_SCHOOL_HOLY || ench->spellid[i] > SPELL_SCHOOL_ARCANE)
                {
                    return false; // Procs/spells/on-hit cannot be baked cleanly into raw stats
                }
            }
        }

        return hasBonus && types.size() <= MAX_ITEM_PROTO_STATS;
    }

    bool IsValidBakedTemplate(ItemPrototype const& base, ItemPrototype const& scaled, int32 randomPropertyId)
    {
        if (scaled.ItemLevel == 0 || scaled.ItemLevel > 300 ||
            scaled.RequiredLevel == 0 || scaled.RequiredLevel > 80)
        {
            return false;
        }

        for (auto const& damage : scaled.Damage)
        {
            if (!std::isfinite(damage.DamageMin) || !std::isfinite(damage.DamageMax))
            {
                return false;
            }
        }

        if (randomPropertyId == 0)
        {
            return base.RandomProperty == 0;
        }

        return CanBakeRandomProperty(base, randomPropertyId) && scaled.RandomProperty == 0;
    }

    bool BakeRandomSuffixStats(ItemPrototype& scaledProto, ItemPrototype const* baseProto,
        int32 randomPropertyId, uint16 /*targetItemLevel*/, double rBudget)
    {
        if (!baseProto || !CanBakeRandomProperty(*baseProto, randomPropertyId))
        {
            return false;
        }

        ItemPrototype baked = scaledProto;
        ItemRandomPropertiesEntry const* propEntry = sItemRandomPropertiesStore.LookupEntry(static_cast<uint32>(randomPropertyId));
        if (!propEntry)
        {
            return false;
        }

        char const* suffixName = propEntry->nameSuffix[0];
        if (suffixName && suffixName[0])
        {
            baked.Name1 = baseProto->Name1 + " " + suffixName;
        }

        auto accumulate = [](int32& current, int64 amount, bool resistance)
        {
            int64 sum = int64(current) + amount;
            int64 minimum = resistance ? std::numeric_limits<int16>::min() : std::numeric_limits<int32>::min();
            int64 maximum = resistance ? std::numeric_limits<int16>::max() : std::numeric_limits<int32>::max();
            if (sum < minimum || sum > maximum)
            {
                return false;
            }
            current = static_cast<int32>(sum);
            return true;
        };

        for (uint32 k = 0; k < 3; ++k)
        {
            uint32 id = propEntry->enchant_id[k];
            if (!id)
            {
                continue;
            }

            SpellItemEnchantmentEntry const* ench = sSpellItemEnchantmentStore.LookupEntry(id);
            if (!ench)
            {
                continue;
            }

            for (uint32 i = 0; i < 3; ++i)
            {
                uint32 type = ench->type[i];
                if (!type)
                {
                    continue;
                }

                double amount = std::round(double(ench->amount[i]) * rBudget);
                if (ench->amount[i] != 0 && amount == 0.0 && sItemScalingConfig->PreserveNonZeroStats)
                {
                    amount = ench->amount[i] > 0 ? 1.0 : -1.0;
                }

                if (!std::isfinite(amount) || amount < std::numeric_limits<int32>::min() ||
                    amount > std::numeric_limits<int32>::max())
                {
                    return false;
                }

                int64 value = static_cast<int64>(amount);
                if (!value)
                {
                    continue;
                }

                uint32 stat = ench->spellid[i];
                if (type == 5) // Stat
                {
                    int32 slot = -1;
                    int32 firstEmpty = -1;
                    for (int32 s = 0; s < MAX_ITEM_PROTO_STATS; ++s)
                    {
                        if (baked.ItemStat[s].ItemStatValue != 0 && baked.ItemStat[s].ItemStatType == stat)
                        {
                            slot = s;
                            break;
                        }
                        if (baked.ItemStat[s].ItemStatValue == 0 && firstEmpty == -1)
                        {
                            firstEmpty = s;
                        }
                    }

                    if (slot == -1)
                    {
                        if (firstEmpty == -1)
                        {
                            return false;
                        }
                        slot = firstEmpty;
                        baked.ItemStat[slot].ItemStatType = stat;
                        baked.ItemStat[slot].ItemStatValue = 0;
                    }

                    if (!accumulate(baked.ItemStat[slot].ItemStatValue, value, false))
                    {
                        return false;
                    }
                }
                else if (type == 4) // Resistance
                {
                    int32* resistance = nullptr;
                    switch (stat)
                    {
                        case SPELL_SCHOOL_HOLY: resistance = &baked.HolyRes; break;
                        case SPELL_SCHOOL_FIRE: resistance = &baked.FireRes; break;
                        case SPELL_SCHOOL_NATURE: resistance = &baked.NatureRes; break;
                        case SPELL_SCHOOL_FROST: resistance = &baked.FrostRes; break;
                        case SPELL_SCHOOL_SHADOW: resistance = &baked.ShadowRes; break;
                        case SPELL_SCHOOL_ARCANE: resistance = &baked.ArcaneRes; break;
                        default: return false;
                    }

                    if (!accumulate(*resistance, value, true))
                    {
                        return false;
                    }
                }
            }
        }

        baked.RandomProperty = 0;
        scaledProto = std::move(baked);
        return true;
    }

    uint8 GetNativeReferenceLevel(ItemPrototype const* proto)
    {
        if (!proto)
        {
            return 1;
        }
        return ItemScalingTarget::GetNativeReferenceLevel(
            static_cast<uint8>(proto->RequiredLevel), proto->ItemLevel);
    }

    bool IsNativeTargetMatch(ItemPrototype const* proto, uint8 requestedTarget, uint8 bracketedTarget, uint8 playerLevel)
    {
        if (!proto)
        {
            return false;
        }
        uint8 nativeRef = GetNativeReferenceLevel(proto);
        return ItemScalingTarget::IsNativeTargetMatch(nativeRef, requestedTarget, bracketedTarget, playerLevel);
    }

    ItemPrototype CreateScaledTemplate(ItemPrototype const* baseProto, uint32 newEntry, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 /*formulaVersion*/, uint8 highestRealPlayerLevel, int32 randomPropertyId)
    {
        // 1. Full clone of base item prototype
        ItemPrototype scaledProto = *baseProto;

        // 2. Assign synthetic entry
        scaledProto.ItemId = newEntry;
        scaledProto.Discovered = true;

        uint8 l0 = GetNativeReferenceLevel(baseProto);
        uint8 lTarget = std::clamp<uint8>(targetEffectiveLevel, 1, 80);

        // 3. Assign scaled item level & required level
        scaledProto.ItemLevel = static_cast<uint8>(std::min<uint16>(targetItemLevel, 255));
        scaledProto.RequiredLevel = CalculateRequiredLevel(baseProto, lTarget, highestRealPlayerLevel);

        // 4. Calculate budget growth ratio
        int8 family = GetSlotFamily(baseProto->InventoryType);
        if (family < 0)
        {
            family = 2;
        }

        double rBudget = GetBudgetRatio(baseProto->ItemLevel, targetItemLevel, baseProto->Quality, static_cast<uint8>(family));

        // 5. Scale additive stats
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (baseProto->ItemStat[i].ItemStatValue != 0)
            {
                scaledProto.ItemStat[i].ItemStatType = baseProto->ItemStat[i].ItemStatType;
                scaledProto.ItemStat[i].ItemStatValue = ScaleAdditiveStat(
                    baseProto->ItemStat[i].ItemStatValue,
                    rBudget,
                    sItemScalingConfig->PreserveNonZeroStats
                );
            }
        }

        // 6. Scale armor
        scaledProto.Armor = ScaleArmor(baseProto, rBudget, l0, lTarget);

        // 7. Scale weapon damage
        ScaleWeaponDamage(baseProto, scaledProto, rBudget, l0, lTarget);

        // 8. Scale shield block value
        if (baseProto->Block > 0)
        {
            scaledProto.Block = static_cast<uint32>(std::clamp(std::round(baseProto->Block * rBudget),
                0.0, static_cast<double>(std::numeric_limits<uint32>::max())));
        }

        // 9. Scale resistances
        scaledProto.HolyRes = std::clamp<int32>(ScaleAdditiveStat(baseProto->HolyRes, rBudget, false),
            std::numeric_limits<int16>::min(), std::numeric_limits<int16>::max());
        scaledProto.FireRes = std::clamp<int32>(ScaleAdditiveStat(baseProto->FireRes, rBudget, false),
            std::numeric_limits<int16>::min(), std::numeric_limits<int16>::max());
        scaledProto.NatureRes = std::clamp<int32>(ScaleAdditiveStat(baseProto->NatureRes, rBudget, false),
            std::numeric_limits<int16>::min(), std::numeric_limits<int16>::max());
        scaledProto.FrostRes = std::clamp<int32>(ScaleAdditiveStat(baseProto->FrostRes, rBudget, false),
            std::numeric_limits<int16>::min(), std::numeric_limits<int16>::max());
        scaledProto.ShadowRes = std::clamp<int32>(ScaleAdditiveStat(baseProto->ShadowRes, rBudget, false),
            std::numeric_limits<int16>::min(), std::numeric_limits<int16>::max());
        scaledProto.ArcaneRes = std::clamp<int32>(ScaleAdditiveStat(baseProto->ArcaneRes, rBudget, false),
            std::numeric_limits<int16>::min(), std::numeric_limits<int16>::max());

        // 10. Bake random suffix stats if this is a random property variant
        if (randomPropertyId != 0)
        {
            if (!BakeRandomSuffixStats(scaledProto, baseProto, randomPropertyId, targetItemLevel, rBudget))
            {
                return scaledProto;
            }
        }

        // 11. Compact stats: consolidate non-zero stat rows
        _ItemStat compacted[MAX_ITEM_PROTO_STATS]{};
        uint32 count = 0;
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            if (scaledProto.ItemStat[i].ItemStatValue != 0)
            {
                compacted[count++] = scaledProto.ItemStat[i];
            }
        }
        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            scaledProto.ItemStat[i] = compacted[i];
        }

        // 12. Scale passive on-equip spells
        if (sItemScalingConfig->ScaleEquipSpells && rBudget > 0.0)
        {
            ItemScalingEquipSpell::ScalePrototypeEquipSpells(baseProto, scaledProto, rBudget);
        }

        return scaledProto;
    }
}
