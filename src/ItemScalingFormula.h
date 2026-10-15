/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#ifndef _ITEM_SCALING_FORMULA_H
#define _ITEM_SCALING_FORMULA_H

#include "ItemScalingCommon.h"

namespace ItemScalingFormula
{
    // Baseline and curve initialization
    bool LoadStartupCurves(std::string const& dataPath = "");
    [[nodiscard]] int8 GetSlotFamily(uint32 inventoryType);

    [[nodiscard]] double GetBudgetRatio(uint32 baseIlvl, uint32 targetIlvl, uint32 quality = 0, uint8 slotFamily = 0);

    [[nodiscard]] int32 ScaleAdditiveStat(int32 v0, double r, bool preserveNonZero);

    [[nodiscard]] uint32 ScaleArmor(ItemPrototype const* baseProto, double rBudget, uint8 l0, uint8 lTarget);

    void ScaleWeaponDamage(ItemPrototype const* baseProto, ItemPrototype& scaledProto, double rBudget, uint8 l0, uint8 lTarget);

    [[nodiscard]] uint8 CalculateRequiredLevel(ItemPrototype const* baseProto, uint8 targetEffectiveLevel, uint8 highestRealPlayerLevel);

    [[nodiscard]] bool IsScalableEquipment(ItemPrototype const* proto);
    [[nodiscard]] bool IsScalableEquipSpell(uint32 spellId);
    [[nodiscard]] uint32 FindScaledEquipSpell(uint32 baseSpellId, double rBudget);

    [[nodiscard]] bool CanBakeRandomProperty(ItemPrototype const& base, int32 randomPropertyId);
    [[nodiscard]] bool IsValidBakedTemplate(ItemPrototype const& base, ItemPrototype const& scaled, int32 randomPropertyId);
    bool BakeRandomSuffixStats(ItemPrototype& scaledProto, ItemPrototype const* baseProto, int32 randomPropertyId, uint16 targetItemLevel, double rBudget);

    [[nodiscard]] uint8 GetNativeReferenceLevel(ItemPrototype const* proto);
    [[nodiscard]] bool IsNativeTargetMatch(ItemPrototype const* proto, uint8 requestedTarget, uint8 bracketedTarget, uint8 playerLevel = 0);

    [[nodiscard]] ItemPrototype CreateScaledTemplate(ItemPrototype const* baseProto, uint32 newEntry, uint8 targetEffectiveLevel, uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel, int32 randomPropertyId = 0);
}

#endif // _ITEM_SCALING_FORMULA_H
