/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingCommon.h"

void AddItemScalingWorldScripts();
void AddItemScalingLootScripts();
void AddItemScalingLiveScripts();
void AddItemScalingCommandScripts();

void Addmod_twow_item_level_scalingScripts()
{
    AddItemScalingWorldScripts();
    AddItemScalingLootScripts();
    AddItemScalingLiveScripts();
    AddItemScalingCommandScripts();
}
