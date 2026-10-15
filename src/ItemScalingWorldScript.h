/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#ifndef _ITEM_SCALING_WORLD_SCRIPT_H
#define _ITEM_SCALING_WORLD_SCRIPT_H

#include "ItemScalingCommon.h"
#include "ScriptObjects.h"

class ItemScalingWorldScript : public WorldScript
{
public:
    ItemScalingWorldScript();

    void OnBeforeWorldInitialized() override;
    void OnLoadCustomDatabaseTable() override;
    void OnAfterConfigLoad(bool reload) override;
    void OnUpdate(uint32 diff) override;
};

void AddItemScalingWorldScripts();

#endif // _ITEM_SCALING_WORLD_SCRIPT_H
