/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingWorldScript.h"
#include "ItemScalingConfig.h"
#include "ItemScalingEquipSpell.h"
#include "ItemScalingRegistry.h"
#include "ItemScalingLive.h"
#include "Log.h"

ItemScalingWorldScript::ItemScalingWorldScript()
    : WorldScript("ItemScalingWorldScript", {
        WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED,
        WORLDHOOK_ON_LOAD_CUSTOM_DATABASE_TABLE,
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_UPDATE
    })
{
}

void ItemScalingWorldScript::OnLoadCustomDatabaseTable()
{
    sItemScalingConfig->Load();
    if (!sItemScalingRegistry->IsDbSynchronized())
    {
        sItemScalingRegistry->OnLoadCustomDatabaseTable();
    }
}

void ItemScalingWorldScript::OnBeforeWorldInitialized()
{
    sLog.outString(">> Initializing mod-twow-item-level-scaling...");

    if (!sItemScalingConfig->Enable)
    {
        sLog.outString(">> mod-twow-item-level-scaling is disabled in configuration.");
        return;
    }

    ItemScalingEquipSpell::Initialize();
    sItemScalingRegistry->Initialize();
    sItemScalingLive->Initialize();
}

void ItemScalingWorldScript::OnUpdate(uint32 diff)
{
    sItemScalingLive->Update(diff);
}

void ItemScalingWorldScript::OnAfterConfigLoad(bool reload)
{
    sItemScalingConfig->Load(reload);
    if (!reload)
    {
        sItemScalingRegistry->OnLoadCustomDatabaseTable();
    }
    else
    {
        sLog.outString("[ItemScaling] Operational configuration reloaded successfully.");
    }
}

void AddItemScalingWorldScripts()
{
    new ItemScalingWorldScript();
}
