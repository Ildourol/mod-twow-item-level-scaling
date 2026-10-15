/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#ifndef _ITEM_SCALING_LOOT_SCRIPT_H
#define _ITEM_SCALING_LOOT_SCRIPT_H

#include "ItemScalingCommon.h"
#include "ScriptObjects.h"

class Map;
class Creature;
struct CreatureInfo;
namespace ItemScalingTarget { struct Input; }

class ItemScalingLootScript
{
public:
    static Player* GetEligibleOwner(Map const* map);

    static bool ResolveTargetLevels(Map const* map, Player const* lootOwner,
        CreatureInfo const* sourceOverride, Creature const* creature,
        uint8& outRequestedTarget, uint8& outBracketedTarget, uint8& outHighestRealPlayerLevel,
        bool prewarm = false, ItemScalingTarget::Input* outTargetInput = nullptr);

    static void PrepareLoot(Loot* loot, Player* owner, Creature const* creature = nullptr,
        CreatureInfo const* sourceOverride = nullptr, bool prewarm = false);
};

void AddItemScalingLootScripts();

#endif // _ITEM_SCALING_LOOT_SCRIPT_H
