/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingLootScript.h"
#include "Creature.h"
#include "GameObject.h"
#include "Item.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "ItemScalingRegistry.h"
#include "ItemScalingLive.h"
#include "ItemScalingSafety.h"
#include "ItemScalingTarget.h"
#include "Log.h"
#include "LootMgr.h"
#include "Maps/Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptObjects.h"
#include "WorldSession.h"
#include <algorithm>

static bool IsBotPlayer(Player const* player)
{
    if (!player)
    {
        return false;
    }
    WorldSession const* session = player->GetSession();
    if (!session)
    {
        return true;
    }
    return session->IsHeadless();
}

static bool IsValidDropSource(Creature const* creature)
{
    if (!creature)
    {
        return false;
    }
    if (creature->GetCreatureType() == CREATURE_TYPE_CRITTER || creature->IsTotem() || creature->IsPet())
    {
        return false;
    }
    if (CreatureInfo const* cInfo = creature->GetCreatureInfo())
    {
        if (cInfo->type == CREATURE_TYPE_CRITTER)
        {
            return false;
        }
    }
    return true;
}

static uint8 GetHighestEligibleRealPlayerLevel(Map const* map)
{
    if (!map)
    {
        return 0;
    }

    uint8 highestRealNonGM = 0;
    uint8 highestRealGM = 0;
    uint8 highestBot = 0;

    for (auto const& ref : map->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsInWorld())
        {
            continue;
        }

        bool isBot = IsBotPlayer(player);
        bool isGM = player->IsGameMaster();
        uint8 lvl = player->GetLevel();

        if (isBot)
        {
            if (lvl > highestBot)
            {
                highestBot = lvl;
            }
        }
        else if (isGM)
        {
            if (lvl > highestRealGM)
            {
                highestRealGM = lvl;
            }
        }
        else
        {
            if (lvl > highestRealNonGM)
            {
                highestRealNonGM = lvl;
            }
        }
    }

    // 1. Non-GM real human players take top priority
    if (highestRealNonGM > 0)
    {
        return highestRealNonGM;
    }

    // 2. Real GM players take priority if enabled or if no non-GM real player exists
    if (highestRealGM > 0 && (sItemScalingConfig->IncludeGameMasters || !sItemScalingConfig->RealPlayersOnly || highestBot == 0))
    {
        return highestRealGM;
    }

    // 3. Fallback to highest bot level so all-bot parties or bot-dungeon runs scale properly
    if (highestBot > 0)
    {
        return highestBot;
    }

    // 4. Final fallback for GM
    if (highestRealGM > 0)
    {
        return highestRealGM;
    }

    return 0;
}

static bool IsEligibleInstanceMap(Map const* map)
{
    if (!map || !map->IsDungeon())
    {
        return false;
    }

    if (map->IsBattleGround())
    {
        return false;
    }

    if (sItemScalingConfig->IsMapExcluded(map->GetId()))
    {
        return false;
    }

    if (map->IsRaid())
    {
        if (!sItemScalingConfig->ScaleRaids)
        {
            return false;
        }
    }
    else
    {
        if (!sItemScalingConfig->ScaleDungeons)
        {
            return false;
        }
    }

    return true;
}

Player* ItemScalingLootScript::GetEligibleOwner(Map const* map)
{
    if (!map)
    {
        return nullptr;
    }

    Player* realNonGMOwner = nullptr;
    Player* realGMOwner = nullptr;
    Player* botOwner = nullptr;

    for (auto const& ref : map->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (!player || !player->IsInWorld())
        {
            continue;
        }

        bool isBot = IsBotPlayer(player);
        bool isGM = player->IsGameMaster();

        if (isBot)
        {
            if (!botOwner || botOwner->GetLevel() < player->GetLevel())
            {
                botOwner = player;
            }
        }
        else if (isGM)
        {
            if (!realGMOwner || realGMOwner->GetLevel() < player->GetLevel())
            {
                realGMOwner = player;
            }
        }
        else
        {
            if (!realNonGMOwner || realNonGMOwner->GetLevel() < player->GetLevel())
            {
                realNonGMOwner = player;
            }
        }
    }

    // 1. Non-GM real human players take top priority
    if (realNonGMOwner)
    {
        return realNonGMOwner;
    }

    // 2. Real GM players take priority if enabled or if no non-GM real player exists
    if (realGMOwner && (sItemScalingConfig->IncludeGameMasters || !sItemScalingConfig->RealPlayersOnly || !botOwner))
    {
        return realGMOwner;
    }

    // 3. Fallback to bot owner for all-bot groups or bot testing
    if (botOwner)
    {
        return botOwner;
    }

    // 4. Final fallback
    if (realGMOwner)
    {
        return realGMOwner;
    }

    return nullptr;
}

static uint8 ResolveEffectiveCreatureLevel(
    Map const* map,
    uint8 playerLevel,
    Creature const* creature,
    CreatureInfo const* cInfo)
{
    if (creature)
    {
        uint8 liveLevel = creature->GetLevel();
        if (cInfo)
        {
            if (liveLevel != cInfo->level_max && liveLevel > 0)
            {
                return liveLevel;
            }
        }
        else if (liveLevel > 0)
        {
            return liveLevel;
        }
    }

    uint8 bossOffset = 0;
    if (map)
    {
        bool isRaidBoss = false;
        bool isDungeonBoss = false;

        if (creature)
        {
            isRaidBoss = map->IsRaid() && (creature->IsWorldBoss() || (cInfo && (cInfo->rank >= CREATURE_ELITE_WORLDBOSS || (cInfo->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND))));
            isDungeonBoss = map->IsDungeon() && (creature->IsWorldBoss() || (cInfo && cInfo->rank >= CREATURE_ELITE_ELITE));
        }
        else if (cInfo)
        {
            isRaidBoss = map->IsRaid() && (cInfo->rank >= CREATURE_ELITE_WORLDBOSS ||
                         (cInfo->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND));
            isDungeonBoss = map->IsDungeon() && (cInfo->rank >= CREATURE_ELITE_ELITE ||
                            (cInfo->flags_extra & CREATURE_FLAG_EXTRA_INSTANCE_BIND));
        }

        if (isRaidBoss)
        {
            uint32 raidCeil = sConfig.GetIntDefault("AutoBalance.LevelScaling.DynamicLevel.Ceiling.Raids", 3);
            bossOffset = static_cast<uint8>(raidCeil > 0 ? raidCeil : 3);
        }
        else if (isDungeonBoss)
        {
            uint32 dungCeil = sConfig.GetIntDefault("AutoBalance.LevelScaling.DynamicLevel.Ceiling.Dungeons", 2);
            bossOffset = static_cast<uint8>(dungCeil > 0 ? dungCeil : 2);
        }
    }

    if (creature)
    {
        uint8 liveLevel = creature->GetLevel();
        if (cInfo && cInfo->level_max >= playerLevel)
        {
            return liveLevel;
        }
    }

    return playerLevel + bossOffset;
}

bool ItemScalingLootScript::ResolveTargetLevels(
    Map const* map,
    Player const* lootOwner,
    CreatureInfo const* sourceOverride,
    Creature const* creature,
    uint8& outRequestedTarget,
    uint8& outBracketedTarget,
    uint8& outHighestRealPlayerLevel,
    bool prewarm,
    ItemScalingTarget::Input* outTargetInput)
{
    if (!sItemScalingConfig->Enable || !map || !lootOwner)
    {
        return false;
    }

    if (!IsEligibleInstanceMap(map))
    {
        return false;
    }

    uint8 highestRealPlayerLevel = GetHighestEligibleRealPlayerLevel(map);
    if (highestRealPlayerLevel == 0)
    {
        return false;
    }

    if (highestRealPlayerLevel < sItemScalingConfig->MinLevel ||
        highestRealPlayerLevel > sItemScalingConfig->MaxLevel)
    {
        return false;
    }

    uint8 cMin = 0;
    uint8 cSrc = 0;
    uint8 cMax = 0;
    CreatureInfo const* cInfo = nullptr;

    if (creature)
    {
        if (!IsValidDropSource(creature))
        {
            return false;
        }

        cInfo = creature->GetCreatureInfo();
        if (cInfo)
        {
            cMin = cInfo->level_min > 0 ? cInfo->level_min : 1;
            cSrc = cInfo->level_max > 0 ? cInfo->level_max : cMin;
        }
    }

    if (sourceOverride)
    {
        cInfo = sourceOverride;
        cMin = sourceOverride->level_min > 0 ? sourceOverride->level_min : 1;
        cSrc = sourceOverride->level_max > 0 ? sourceOverride->level_max : cMin;
    }

    if (cSrc == 0)
    {
        cSrc = (creature ? creature->GetLevel() : (map->IsRaid() ? 60 : 20));
        cMin = cSrc;
    }
    if (cMax == 0 || cMax < cSrc)
    {
        cMax = cSrc;
    }

    uint8 baseFloor = sItemScalingConfig->GetDynamicFloor(map);
    uint8 baseCeiling = sItemScalingConfig->GetDynamicCeiling(map);

    int8 floorDelta = 0;
    int8 ceilingDelta = 0;

    if (!prewarm)
    {
        if (sItemScalingConfig->FloorVarianceEnable)
        {
            floorDelta = ItemScalingConfig::RollVarianceDelta(
                sItemScalingConfig->GetDynamicFloorVarianceWeights(map));
        }

        if (sItemScalingConfig->CeilingVarianceEnable)
        {
            ceilingDelta = ItemScalingConfig::RollVarianceDelta(
                sItemScalingConfig->GetDynamicCeilingVarianceWeights(map));
        }
    }

    uint8 effectiveFloor = static_cast<uint8>(std::clamp<int32>(
        static_cast<int32>(baseFloor) + floorDelta, 0, 80));
    uint8 effectiveCeiling = static_cast<uint8>(std::clamp<int32>(
        static_cast<int32>(baseCeiling) + ceilingDelta, 0, 80));

    ItemScalingTarget::Input targetInput;
    targetInput.playerLevel = highestRealPlayerLevel;
    targetInput.creatureMinLevel = cMin;
    targetInput.creatureSourceLevel = cSrc;
    targetInput.instanceMaxLevel = cMax;
    targetInput.observedCreatureLevel = ResolveEffectiveCreatureLevel(map, highestRealPlayerLevel, creature, cInfo);
    targetInput.floor = effectiveFloor;
    targetInput.ceiling = effectiveCeiling;
    targetInput.minLevel = sItemScalingConfig->MinLevel;
    targetInput.maxLevel = sItemScalingConfig->MaxLevel;
    targetInput.dynamic = (sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC);
    targetInput.hasCreature = (creature != nullptr || sourceOverride != nullptr);
    targetInput.realPlayersOnly = sItemScalingConfig->RealPlayersOnly;

    outRequestedTarget = ItemScalingTarget::Resolve(targetInput);
    outBracketedTarget = ItemScalingSafety::Bracket(outRequestedTarget, sItemScalingConfig->MinLevel,
        sItemScalingConfig->MaxLevel, sItemScalingConfig->BracketStep);
    outHighestRealPlayerLevel = highestRealPlayerLevel;

    if (outTargetInput)
    {
        *outTargetInput = targetInput;
    }

    return true;
}

void ItemScalingLootScript::PrepareLoot(Loot* loot, Player* lootOwner, Creature const* creature,
    CreatureInfo const* sourceOverride, bool prewarm)
{
    if (!sItemScalingConfig->Enable || !loot || !lootOwner)
    {
        return;
    }

    Map* map = lootOwner->GetMap();
    if (!IsEligibleInstanceMap(map))
    {
        return;
    }

    if (!prewarm)
    {
        for (auto const& itm : loot->items)
        {
            if (sItemScalingRegistry->IsSynthetic(itm.itemid))
            {
                return;
            }
        }
    }

    uint8 requestedTarget = 0;
    uint8 lTarget = 0;
    uint8 highestRealPlayerLevel = 0;
    ItemScalingTarget::Input baseTargetInput;
    if (!ResolveTargetLevels(map, lootOwner, sourceOverride, creature, requestedTarget, lTarget, highestRealPlayerLevel, prewarm, &baseTargetInput))
    {
        return;
    }

    if (sItemScalingConfig->Debug && !prewarm)
    {
        char const* rawMapName = map->GetMapName();
        std::string mapName = rawMapName ? rawMapName : "Dungeon";
        sLog.outString("[ItemScaling] Map %u ('%s') - Player %s (H: %u) - Mode: %s -> Target Level: %u (Requested: %u)",
            map->GetId(), mapName.c_str(), lootOwner->GetName(), highestRealPlayerLevel,
            sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC ? "dynamic" : "fixed",
            lTarget, requestedTarget);
    }

    for (std::size_t i = 0; i < loot->items.size(); ++i)
    {
        LootItem& item = loot->items[i];
        if (item.needs_quest)
        {
            continue;
        }

        if (sItemScalingRegistry->IsSynthetic(item.itemid) || sItemScalingLive->IsReserved(item.itemid))
        {
            continue;
        }

        ItemPrototype const* baseProto = sObjectMgr.GetItemPrototype(item.itemid);
        if (!baseProto || !ItemScalingFormula::IsScalableEquipment(baseProto))
        {
            continue;
        }

        if (baseProto->StartQuest != 0 || baseProto->ScriptId != 0)
        {
            continue;
        }

        if (!sItemScalingConfig->ScaleUniqueItems &&
            (baseProto->MaxCount != 0 || (baseProto->Flags & ITEM_FLAG_UNIQUE_EQUIPPED)))
        {
            continue;
        }

        if (!sItemScalingConfig->IsQualityEnabled(baseProto->Quality))
        {
            if (sItemScalingConfig->Debug && !prewarm)
            {
                sLog.outString("[ItemScaling] Skipped item %u '%s' (Quality %u not enabled)",
                    baseProto->ItemId, baseProto->Name1.c_str(), baseProto->Quality);
            }
            continue;
        }

        if (sItemScalingConfig->IsItemExcluded(baseProto->ItemId))
        {
            if (sItemScalingConfig->Debug && !prewarm)
            {
                sLog.outString("[ItemScaling] Skipped item %u '%s' (Item ID explicitly excluded)",
                    baseProto->ItemId, baseProto->Name1.c_str());
            }
            continue;
        }

        uint8 origRefLevel = ItemScalingFormula::GetNativeReferenceLevel(baseProto);

        if (sItemScalingConfig->PreserveNativeLoot &&
            ItemScalingFormula::IsNativeTargetMatch(baseProto, requestedTarget, lTarget, highestRealPlayerLevel))
        {
            if (sItemScalingConfig->Debug && !prewarm)
            {
                sLog.outString("[ItemScaling] Preserved native item %u '%s' (Native level %u matches target %u/bracket %u/player %u)",
                    baseProto->ItemId, baseProto->Name1.c_str(), origRefLevel, requestedTarget, lTarget, highestRealPlayerLevel);
            }
            continue;
        }

        if (!sItemScalingConfig->ScaleDown && lTarget < origRefLevel)
        {
            if (sItemScalingConfig->Debug && !prewarm)
            {
                sLog.outString("[ItemScaling] Skipped item %u '%s' (ScaleDown disabled: lTarget %u < origRef %u)",
                    baseProto->ItemId, baseProto->Name1.c_str(), lTarget, origRefLevel);
            }
            continue;
        }
        if (!sItemScalingConfig->ScaleUp && lTarget > origRefLevel)
        {
            if (sItemScalingConfig->Debug && !prewarm)
            {
                sLog.outString("[ItemScaling] Skipped item %u '%s' (ScaleUp disabled: lTarget %u > origRef %u)",
                    baseProto->ItemId, baseProto->Name1.c_str(), lTarget, origRefLevel);
            }
            continue;
        }

        uint16 targetIlvl = sItemScalingBaseline->CalculateTargetItemLevel(baseProto, lTarget, origRefLevel);
        if (targetIlvl == 0 || (targetIlvl == baseProto->ItemLevel && lTarget == origRefLevel))
        {
            if (sItemScalingConfig->Debug && !prewarm)
            {
                sLog.outString("[ItemScaling] Item %u '%s' baseline ilvl %u matches target %u (No stat delta)",
                    baseProto->ItemId, baseProto->Name1.c_str(), baseProto->ItemLevel, targetIlvl);
            }
            continue;
        }

        bool const hasRandomProps = (baseProto->RandomProperty != 0);
        if (hasRandomProps && sItemScalingConfig->RandomSuffixMode == RandomSuffixScalingMode::Skip)
        {
            continue;
        }

        int32 randomPropId = 0;
        if (hasRandomProps && sItemScalingConfig->RandomSuffixMode == RandomSuffixScalingMode::Bake)
        {
            randomPropId = item.randomPropertyId;
            if (randomPropId == 0)
            {
                randomPropId = Item::GenerateItemRandomPropertyId(baseProto->ItemId);
            }
            if (randomPropId == 0 || !ItemScalingFormula::CanBakeRandomProperty(*baseProto, randomPropId))
            {
                continue;
            }
        }

        if (sItemScalingConfig->Debug && !prewarm)
        {
            sLog.outString("[ItemScaling] Scaling item %u '%s' (Native Lv %u, Ilvl %u) -> Target Level %u (Ilvl %u)",
                baseProto->ItemId, baseProto->Name1.c_str(), origRefLevel, baseProto->ItemLevel, lTarget, targetIlvl);
        }

        uint32 variantEntry = sItemScalingRegistry->FindOrRequestVariant(
            baseProto,
            lTarget,
            targetIlvl,
            sItemScalingConfig->FormulaVersion,
            highestRealPlayerLevel,
            randomPropId
        );

        if (variantEntry != 0 && variantEntry != item.itemid)
        {
            sLog.outString("[ItemScaling] Scaled loot item %u -> variant %u for '%s'",
                item.itemid, variantEntry, baseProto->Name1.c_str());
            item.itemid = variantEntry;
            if (randomPropId != 0)
            {
                item.randomPropertyId = 0;
            }
        }
    }
}

namespace
{
    class ItemScalingUnitScript : public UnitScript
    {
    public:
        ItemScalingUnitScript() : UnitScript("ItemScalingUnitScript", {UNITHOOK_ON_UNIT_DEATH}) {}

        void OnUnitDeath(Unit* unit, Unit* killer) override
        {
            if (!unit || unit->GetTypeId() != TYPEID_UNIT)
            {
                return;
            }

            Creature* creature = static_cast<Creature*>(unit);
            if (!creature->IsInWorld() || creature->loot.empty())
            {
                return;
            }

            Map* map = creature->GetMap();
            if (!map || !map->IsDungeon() || map->IsBattleGround())
            {
                return;
            }

            Player* owner = nullptr;
            if (killer)
            {
                owner = killer->GetCharmerOrOwnerPlayerOrPlayerItself();
            }
            if (!owner && creature->GetLootRecipient())
            {
                owner = creature->GetLootRecipient();
            }
            if (!owner)
            {
                owner = ItemScalingLootScript::GetEligibleOwner(map);
            }

            if (owner)
            {
                ItemScalingLootScript::PrepareLoot(&creature->loot, owner, creature);
            }
        }
    };

    class ItemScalingSessionScript : public WorldSessionScript
    {
    public:
        explicit ItemScalingSessionScript(WorldSession* session) : m_session(session) {}

        void OnLoot(ObjectGuid guid, LootType /*lootType*/) override
        {
            if (!sItemScalingConfig->Enable || !sItemScalingConfig->ScaleChests || !m_session)
            {
                return;
            }

            Player* player = m_session->GetPlayer();
            if (!player || !player->IsInWorld())
            {
                return;
            }

            Map* map = player->GetMap();
            if (!map || !map->IsDungeon() || map->IsBattleGround())
            {
                return;
            }

            if (guid.IsGameObject())
            {
                if (GameObject* go = map->GetGameObject(guid))
                {
                    if (go->GetGoType() == GAMEOBJECT_TYPE_CHEST && !go->loot.empty())
                    {
                        ItemScalingLootScript::PrepareLoot(&go->loot, player, nullptr, nullptr);
                    }
                }
            }
        }

    private:
        WorldSession* m_session;
    };

    class ItemScalingPlayerScript : public PlayerScript
    {
    public:
        ItemScalingPlayerScript() : PlayerScript("ItemScalingPlayerScript", {
            PLAYERHOOK_ON_CREATURE_KILL,
            PLAYERHOOK_ON_LOGIN,
            PLAYERHOOK_ON_LOGOUT
        }) {}

        void OnCreatureKill(Player* killer, Creature* killed) override
        {
            if (!killer || !killed || killed->loot.empty())
            {
                return;
            }

            Map* map = killer->GetMap();
            if (!map || !map->IsDungeon() || map->IsBattleGround())
            {
                return;
            }

            ItemScalingLootScript::PrepareLoot(&killed->loot, killer, killed);
        }

        void OnLogin(Player* player) override
        {
            if (player && player->GetSession())
            {
                player->GetSession()->AddScript("ItemScalingSessionScript", new ItemScalingSessionScript(player->GetSession()));
            }
        }

        void OnLogout(Player* player) override
        {
            if (player && player->GetSession())
            {
                WorldSessionScript* script = player->GetSession()->GetScriptByName("ItemScalingSessionScript");
                if (script)
                {
                    player->GetSession()->scripts.erase("ItemScalingSessionScript");
                    delete script;
                }
            }
        }
    };
}

void AddItemScalingLootScripts()
{
    new ItemScalingUnitScript();
    new ItemScalingPlayerScript();
}
