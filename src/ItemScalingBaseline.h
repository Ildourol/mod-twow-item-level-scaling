/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#ifndef _ITEM_SCALING_BASELINE_H
#define _ITEM_SCALING_BASELINE_H

#include "ItemScalingCommon.h"
#include <unordered_set>
#include <vector>

constexpr uint8 MAX_BASELINE_LEVEL = 80;
constexpr uint8 MAX_BASELINE_QUALITY = 7;
constexpr uint8 MAX_BASELINE_FAMILY = 5;

class ItemScalingBaseline
{
public:
    static ItemScalingBaseline* instance();

    void BuildBaseline(std::unordered_set<uint32> const* knownSyntheticEntries = nullptr);

    [[nodiscard]] double GetMedianItemLevel(uint8 level, uint32 quality, uint8 slotFamily) const;

    [[nodiscard]] uint16 CalculateTargetItemLevel(ItemPrototype const* baseProto, uint8 targetLevel, uint8 originalRefLevel) const;

    [[nodiscard]] bool IsInitialized() const { return _initialized; }

private:
    double _medians[MAX_BASELINE_LEVEL + 1][MAX_BASELINE_QUALITY][MAX_BASELINE_FAMILY]{};
    bool _initialized{false};
};

#define sItemScalingBaseline ItemScalingBaseline::instance()

#endif // _ITEM_SCALING_BASELINE_H
