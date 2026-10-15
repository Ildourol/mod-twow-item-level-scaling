/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingConfig.h"
#include "ItemScalingVariance.h"
#include "Log.h"
#include "Maps/Map.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <random>
#include <sstream>

ItemScalingConfig* ItemScalingConfig::instance()
{
    static ItemScalingConfig instance;
    return &instance;
}

static uint32 ResolveMapMaxPlayers(Map const* map)
{
    if (!map)
        return 5;

    if (map->IsDungeon())
    {
        DungeonMap const* dMap = reinterpret_cast<DungeonMap const*>(map);
        uint32 maxP = dMap->GetMaxPlayers();
        if (maxP > 0)
            return maxP;
    }

    return map->IsRaid() ? 40 : 5;
}

VarianceWeights ItemScalingConfig::ParseVarianceWeights(std::string const& configStr)
{
    return ItemScalingVariance::ParseWeights(configStr);
}

int8 ItemScalingConfig::RollVarianceDelta(VarianceWeights const& weights, float randomRoll)
{
    if (!weights.HasAny())
        return 0;

    if (randomRoll < 0.0f)
    {
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dist(0.0f, 100.0f);
        randomRoll = dist(gen);
    }

    return ItemScalingVariance::RollDelta(weights, randomRoll);
}

void ItemScalingConfig::ParseItemScalingDynamicOverrides(std::string const& configStr)
{
    DynamicOverrides.clear();
    if (configStr.empty())
        return;

    std::stringstream ss(configStr);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        std::stringstream ts(token);
        uint32 mapId = 0;
        int32 ceiling = -1;
        int32 floor = -1;
        if (ts >> mapId >> ceiling >> floor)
        {
            DynamicOverrides[mapId] = {ceiling, floor};
        }
    }
}

void ItemScalingConfig::ParseAutoBalanceDynamicOverrides(std::string const& configStr)
{
    if (configStr.empty())
        return;

    std::stringstream ss(configStr);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        std::stringstream ts(token);
        uint32 mapId = 0;
        int32 skipHigher = 0;
        int32 skipLower = 0;
        int32 ceiling = -1;
        int32 floor = -1;
        if (ts >> mapId >> skipHigher >> skipLower >> ceiling >> floor)
        {
            DynamicOverrides[mapId] = {ceiling, floor};
        }
        else
        {
            std::stringstream ts2(token);
            if (ts2 >> mapId >> ceiling >> floor)
            {
                DynamicOverrides[mapId] = {ceiling, floor};
            }
        }
    }
}

void ItemScalingConfig::Load(bool reload)
{
    Enable = sConfig.GetBoolDefault("ItemScaling.Enable", true);
    ScaleDungeons = sConfig.GetBoolDefault("ItemScaling.ScaleDungeons", true);
    ScaleRaids = sConfig.GetBoolDefault("ItemScaling.ScaleRaids", true);
    ScaleHeroics = sConfig.GetBoolDefault("ItemScaling.ScaleHeroics", true);

    ScaleHeroicDungeons = sConfig.GetBoolDefault("ItemScaling.Scale.HeroicDungeons", true);
    ScaleRaid10M = sConfig.GetBoolDefault("ItemScaling.Scale.Raid10M", true);
    ScaleRaid10MHeroic = sConfig.GetBoolDefault("ItemScaling.Scale.Raid10MHeroic", true);
    ScaleRaid15M = sConfig.GetBoolDefault("ItemScaling.Scale.Raid15M", true);
    ScaleRaid20M = sConfig.GetBoolDefault("ItemScaling.Scale.Raid20M", true);
    ScaleRaid25M = sConfig.GetBoolDefault("ItemScaling.Scale.Raid25M", true);
    ScaleRaid25MHeroic = sConfig.GetBoolDefault("ItemScaling.Scale.Raid25MHeroic", true);
    ScaleRaid40M = sConfig.GetBoolDefault("ItemScaling.Scale.Raid40M", true);

    ScaleChests = sConfig.GetBoolDefault("ItemScaling.ScaleChests", true);

    std::string methodStr = sConfig.GetStringDefault("ItemScaling.LevelScaling.Method", "dynamic");
    std::transform(methodStr.begin(), methodStr.end(), methodStr.begin(), ::tolower);
    Method = (methodStr == "fixed") ? SCALING_METHOD_FIXED : SCALING_METHOD_DYNAMIC;

    MinLevel = static_cast<uint8>(std::clamp(sConfig.GetIntDefault("ItemScaling.MinLevel", 1), 1, 80));
    MaxLevel = static_cast<uint8>(std::clamp(sConfig.GetIntDefault("ItemScaling.MaxLevel", 60), 1, 80));

    RealPlayersOnly = sConfig.GetBoolDefault("ItemScaling.RealPlayersOnly", true);
    IncludeGameMasters = sConfig.GetBoolDefault("ItemScaling.IncludeGameMasters", true);
    ScaleUp = sConfig.GetBoolDefault("ItemScaling.ScaleUp", true);
    ScaleDown = sConfig.GetBoolDefault("ItemScaling.ScaleDown", true);
    ScaleExistingScalingItems = sConfig.GetBoolDefault("ItemScaling.ScaleExistingScalingItems", false);
    ScaleUniqueItems = sConfig.GetBoolDefault("ItemScaling.ScaleUniqueItems", true);

    ScalePoor = sConfig.GetBoolDefault("ItemScaling.ScalePoor", sConfig.GetBoolDefault("ItemScaling.Quality.Poor", true));
    ScaleNormal = sConfig.GetBoolDefault("ItemScaling.ScaleCommon", sConfig.GetBoolDefault("ItemScaling.Quality.Normal", true));
    ScaleUncommon = sConfig.GetBoolDefault("ItemScaling.ScaleUncommon", sConfig.GetBoolDefault("ItemScaling.Quality.Uncommon", true));
    ScaleRare = sConfig.GetBoolDefault("ItemScaling.ScaleRare", sConfig.GetBoolDefault("ItemScaling.Quality.Rare", true));
    ScaleEpic = sConfig.GetBoolDefault("ItemScaling.ScaleEpic", sConfig.GetBoolDefault("ItemScaling.Quality.Epic", true));
    ScaleLegendary = sConfig.GetBoolDefault("ItemScaling.ScaleLegendary", sConfig.GetBoolDefault("ItemScaling.Quality.Legendary", true));
    ScaleArtifact = sConfig.GetBoolDefault("ItemScaling.ScaleArtifact", sConfig.GetBoolDefault("ItemScaling.Quality.Artifact", true));

    uint8 randomMode = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.RandomSuffix.Mode", 1));
    RandomSuffixMode = (randomMode == 1) ? RandomSuffixScalingMode::Bake : RandomSuffixScalingMode::Skip;

    PreserveNonZeroStats = sConfig.GetBoolDefault("ItemScaling.PreserveNonZeroStats", true);
    ScaleEquipSpells = sConfig.GetBoolDefault("ItemScaling.ScaleEquipSpells", true);

    std::string reqPolicyStr = sConfig.GetStringDefault("ItemScaling.RequiredLevel.Policy", "target");
    std::transform(reqPolicyStr.begin(), reqPolicyStr.end(), reqPolicyStr.begin(), ::tolower);
    if (reqPolicyStr == "player")
        ReqLevelPolicy = REQ_POLICY_PLAYER;
    else if (reqPolicyStr == "target-capped-player")
        ReqLevelPolicy = REQ_POLICY_TARGET_CAPPED_PLAYER;
    else
        ReqLevelPolicy = REQ_POLICY_TARGET;

    PreserveNativeLoot = sConfig.GetBoolDefault("ItemScaling.PreserveNativeLoot", true);

    DynamicFloorDungeons = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Floor.Dungeons", 3));
    DynamicCeilingDungeons = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Ceiling.Dungeons", 0));
    DynamicFloorHeroicDungeons = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Floor.HeroicDungeons", 3));
    DynamicCeilingHeroicDungeons = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Ceiling.HeroicDungeons", 0));

    DynamicFloorRaids = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Floor.Raids", 3));
    DynamicCeilingRaids = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Ceiling.Raids", 0));
    DynamicFloorRaid10M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Floor.Raid10M", 3));
    DynamicCeilingRaid10M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Ceiling.Raid10M", 0));
    DynamicFloorRaid15M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Floor.Raid15M", 3));
    DynamicCeilingRaid15M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Ceiling.Raid15M", 0));
    DynamicFloorRaid20M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Floor.Raid20M", 3));
    DynamicCeilingRaid20M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Ceiling.Raid20M", 0));
    DynamicFloorRaid25M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Floor.Raid25M", 3));
    DynamicCeilingRaid25M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Ceiling.Raid25M", 0));
    DynamicFloorRaid40M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Floor.Raid40M", 3));
    DynamicCeilingRaid40M = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Ceiling.Raid40M", 0));

    FloorVarianceEnable = sConfig.GetBoolDefault("ItemScaling.Dynamic.Floor.Variance.Enable", true);
    CeilingVarianceEnable = sConfig.GetBoolDefault("ItemScaling.Dynamic.Ceiling.Variance.Enable", false);
    VarianceScope = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Dynamic.Variance.Scope", 0));

    FloorVarianceDungeons = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Floor.Variance.Dungeons", "-1:20.0, -2:10.0, -3:5.0"));
    CeilingVarianceDungeons = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Ceiling.Variance.Dungeons", ""));
    FloorVarianceHeroicDungeons = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Floor.Variance.HeroicDungeons", "-1:20.0, -2:10.0, -3:5.0"));
    CeilingVarianceHeroicDungeons = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Ceiling.Variance.HeroicDungeons", ""));
    FloorVarianceRaids = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Floor.Variance.Raids", ""));
    CeilingVarianceRaids = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Ceiling.Variance.Raids", ""));

    FloorVarianceRaid10M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Floor.Variance.Raid10M", ""));
    CeilingVarianceRaid10M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Ceiling.Variance.Raid10M", ""));
    FloorVarianceRaid15M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Floor.Variance.Raid15M", ""));
    CeilingVarianceRaid15M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Ceiling.Variance.Raid15M", ""));
    FloorVarianceRaid20M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Floor.Variance.Raid20M", ""));
    CeilingVarianceRaid20M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Ceiling.Variance.Raid20M", ""));
    FloorVarianceRaid25M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Floor.Variance.Raid25M", ""));
    CeilingVarianceRaid25M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Ceiling.Variance.Raid25M", ""));
    FloorVarianceRaid40M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Floor.Variance.Raid40M", ""));
    CeilingVarianceRaid40M = ParseVarianceWeights(sConfig.GetStringDefault("ItemScaling.Dynamic.Ceiling.Variance.Raid40M", ""));

    ParseItemScalingDynamicOverrides(sConfig.GetStringDefault("ItemScaling.Dynamic.PerInstance", ""));

    UseAutoBalanceSettings = sConfig.GetBoolDefault("ItemScaling.UseAutoBalanceSettings", false);
    if (UseAutoBalanceSettings)
    {
        std::string abMethod = sConfig.GetStringDefault("AutoBalance.LevelScaling.Method", "");
        if (!abMethod.empty())
        {
            std::transform(abMethod.begin(), abMethod.end(), abMethod.begin(), ::tolower);
            Method = (abMethod == "fixed") ? SCALING_METHOD_FIXED : SCALING_METHOD_DYNAMIC;
        }
        DynamicFloorDungeons = static_cast<uint8>(sConfig.GetIntDefault("AutoBalance.LevelScaling.DynamicLevel.Floor.Dungeons", DynamicFloorDungeons));
        DynamicCeilingDungeons = static_cast<uint8>(sConfig.GetIntDefault("AutoBalance.LevelScaling.DynamicLevel.Ceiling.Dungeons", DynamicCeilingDungeons));
        DynamicFloorRaids = static_cast<uint8>(sConfig.GetIntDefault("AutoBalance.LevelScaling.DynamicLevel.Floor.Raids", DynamicFloorRaids));
        DynamicCeilingRaids = static_cast<uint8>(sConfig.GetIntDefault("AutoBalance.LevelScaling.DynamicLevel.Ceiling.Raids", DynamicCeilingRaids));
        
        std::string abPerInstance = sConfig.GetStringDefault("AutoBalance.LevelScaling.DynamicLevel.PerInstance", "");
        if (abPerInstance.empty())
        {
            abPerInstance = sConfig.GetStringDefault("AutoBalance.DynamicLevel.PerInstance", "");
        }
        if (!abPerInstance.empty())
        {
            ParseAutoBalanceDynamicOverrides(abPerInstance);
        }
    }

    std::string syntheticStartStr = sConfig.GetStringDefault("ItemScaling.SyntheticEntry.Start",
        sConfig.GetStringDefault("ItemScaling.SyntheticEntryStart", "auto").c_str());
    if (syntheticStartStr == "auto" || syntheticStartStr == "0")
    {
        AutoSyntheticEntry = true;
        SyntheticEntryStart = 0;
    }
    else
    {
        AutoSyntheticEntry = false;
        char* end = nullptr;
        unsigned long val = strtoul(syntheticStartStr.c_str(), &end, 10);
        SyntheticEntryStart = (val > 0) ? static_cast<uint32>(val) : 100000;
    }

    SyntheticEntryAutoOffset = static_cast<uint32>(sConfig.GetIntDefault("ItemScaling.SyntheticEntry.AutoOffset",
        sConfig.GetIntDefault("ItemScaling.SyntheticEntryAutoOffset", 1000)));
    SyntheticEntryMaximum = static_cast<uint32>(sConfig.GetIntDefault("ItemScaling.SyntheticEntry.Maximum",
        sConfig.GetIntDefault("ItemScaling.SyntheticEntryMaximum", 2000000)));
    BracketStep = static_cast<uint8>(std::clamp(sConfig.GetIntDefault("ItemScaling.BracketStep", 1), 1, 80));
    FormulaVersion = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.FormulaVersion", 1));

    LiveEnable = sConfig.GetBoolDefault("ItemScaling.Live.Enable", true);
    LiveGenerationMode = static_cast<uint8>(sConfig.GetIntDefault("ItemScaling.Live.GenerationMode", 2));
    LiveReservedSlots = static_cast<uint32>(sConfig.GetIntDefault("ItemScaling.Live.ReservedSlots", 4096));
    LiveMaxPendingVariants = static_cast<uint32>(sConfig.GetIntDefault("ItemScaling.Live.MaxPendingVariants", 4096));
    LiveMaxPublishPerTick = static_cast<uint32>(sConfig.GetIntDefault("ItemScaling.Live.MaxPublishPerTick", 64));
    LiveLootWaitTimeoutMs = static_cast<uint32>(sConfig.GetIntDefault("ItemScaling.Live.LootWaitTimeoutMs", 10000));

    ExcludedMapIds.clear();
    std::string excludedMaps = sConfig.GetStringDefault("ItemScaling.ExcludedMapIds", "");
    if (!excludedMaps.empty())
    {
        std::stringstream ss(excludedMaps);
        uint32 id;
        while (ss >> id)
            ExcludedMapIds.insert(id);
    }

    ExcludedItemIds.clear();
    std::string excludedItems = sConfig.GetStringDefault("ItemScaling.ExcludedItemIds", "");
    if (!excludedItems.empty())
    {
        std::stringstream ss(excludedItems);
        uint32 id;
        while (ss >> id)
            ExcludedItemIds.insert(id);
    }

    Debug = sConfig.GetBoolDefault("ItemScaling.Debug", false);
    Announce = sConfig.GetBoolDefault("ItemScaling.Announce", true);

    if (reload)
    {
        sLog.outString("[ItemScaling] Configuration reloaded.");
    }
}

bool ItemScalingConfig::IsQualityEnabled(uint32 quality) const
{
    switch (quality)
    {
        case ITEM_QUALITY_POOR:      return ScalePoor;
        case ITEM_QUALITY_NORMAL:    return ScaleNormal;
        case ITEM_QUALITY_UNCOMMON:  return ScaleUncommon;
        case ITEM_QUALITY_RARE:      return ScaleRare;
        case ITEM_QUALITY_EPIC:      return ScaleEpic;
        case ITEM_QUALITY_LEGENDARY: return ScaleLegendary;
        case ITEM_QUALITY_ARTIFACT:  return ScaleArtifact;
        default:                     return false;
    }
}

bool ItemScalingConfig::IsMapExcluded(uint32 mapId) const
{
    return ExcludedMapIds.find(mapId) != ExcludedMapIds.end();
}

bool ItemScalingConfig::IsItemExcluded(uint32 itemId) const
{
    return ExcludedItemIds.find(itemId) != ExcludedItemIds.end();
}

uint8 ItemScalingConfig::GetDynamicFloor(bool isRaid, bool /*isHeroic*/, uint32 /*expansion*/, uint32 maxPlayers, uint32 mapId) const
{
    if (mapId > 0)
    {
        auto it = DynamicOverrides.find(mapId);
        if (it != DynamicOverrides.end() && it->second.floor >= 0)
            return static_cast<uint8>(it->second.floor);
    }

    if (isRaid)
    {
        if (maxPlayers <= 10) return DynamicFloorRaid10M;
        if (maxPlayers <= 15) return DynamicFloorRaid15M;
        if (maxPlayers <= 20) return DynamicFloorRaid20M;
        if (maxPlayers <= 25) return DynamicFloorRaid25M;
        if (maxPlayers > 25)  return DynamicFloorRaid40M;
        return DynamicFloorRaids;
    }

    return DynamicFloorDungeons;
}

uint8 ItemScalingConfig::GetDynamicCeiling(bool isRaid, bool /*isHeroic*/, uint32 /*expansion*/, uint32 maxPlayers, uint32 mapId) const
{
    if (mapId > 0)
    {
        auto it = DynamicOverrides.find(mapId);
        if (it != DynamicOverrides.end() && it->second.ceiling >= 0)
            return static_cast<uint8>(it->second.ceiling);
    }

    if (isRaid)
    {
        if (maxPlayers <= 10) return DynamicCeilingRaid10M;
        if (maxPlayers <= 15) return DynamicCeilingRaid15M;
        if (maxPlayers <= 20) return DynamicCeilingRaid20M;
        if (maxPlayers <= 25) return DynamicCeilingRaid25M;
        if (maxPlayers > 25)  return DynamicCeilingRaid40M;
        return DynamicCeilingRaids;
    }

    return DynamicCeilingDungeons;
}

uint8 ItemScalingConfig::GetDynamicFloor(Map const* map) const
{
    if (!map) return DynamicFloorDungeons;
    uint32 maxP = ResolveMapMaxPlayers(map);
    return GetDynamicFloor(map->IsRaid(), false, 0, maxP, map->GetId());
}

uint8 ItemScalingConfig::GetDynamicCeiling(Map const* map) const
{
    if (!map) return DynamicCeilingDungeons;
    uint32 maxP = ResolveMapMaxPlayers(map);
    return GetDynamicCeiling(map->IsRaid(), false, 0, maxP, map->GetId());
}

VarianceWeights ItemScalingConfig::GetDynamicFloorVarianceWeights(bool isRaid, bool /*isHeroic*/, uint32 /*expansion*/, uint32 maxPlayers, uint32 /*mapId*/) const
{
    if (isRaid)
    {
        if (maxPlayers <= 10 && FloorVarianceRaid10M.HasAny()) return FloorVarianceRaid10M;
        if (maxPlayers <= 15 && FloorVarianceRaid15M.HasAny()) return FloorVarianceRaid15M;
        if (maxPlayers <= 20 && FloorVarianceRaid20M.HasAny()) return FloorVarianceRaid20M;
        if (maxPlayers <= 25 && FloorVarianceRaid25M.HasAny()) return FloorVarianceRaid25M;
        if (maxPlayers > 25 && FloorVarianceRaid40M.HasAny())  return FloorVarianceRaid40M;
        if (FloorVarianceRaids.HasAny()) return FloorVarianceRaids;
    }
    return FloorVarianceDungeons;
}

VarianceWeights ItemScalingConfig::GetDynamicCeilingVarianceWeights(bool isRaid, bool /*isHeroic*/, uint32 /*expansion*/, uint32 maxPlayers, uint32 /*mapId*/) const
{
    if (isRaid)
    {
        if (maxPlayers <= 10 && CeilingVarianceRaid10M.HasAny()) return CeilingVarianceRaid10M;
        if (maxPlayers <= 15 && CeilingVarianceRaid15M.HasAny()) return CeilingVarianceRaid15M;
        if (maxPlayers <= 20 && CeilingVarianceRaid20M.HasAny()) return CeilingVarianceRaid20M;
        if (maxPlayers <= 25 && CeilingVarianceRaid25M.HasAny()) return CeilingVarianceRaid25M;
        if (maxPlayers > 25 && CeilingVarianceRaid40M.HasAny())  return CeilingVarianceRaid40M;
        if (CeilingVarianceRaids.HasAny()) return CeilingVarianceRaids;
    }
    return CeilingVarianceDungeons;
}

VarianceWeights ItemScalingConfig::GetDynamicFloorVarianceWeights(Map const* map) const
{
    if (!map) return FloorVarianceDungeons;
    uint32 maxP = ResolveMapMaxPlayers(map);
    return GetDynamicFloorVarianceWeights(map->IsRaid(), false, 0, maxP, map->GetId());
}

VarianceWeights ItemScalingConfig::GetDynamicCeilingVarianceWeights(Map const* map) const
{
    if (!map) return CeilingVarianceDungeons;
    uint32 maxP = ResolveMapMaxPlayers(map);
    return GetDynamicCeilingVarianceWeights(map->IsRaid(), false, 0, maxP, map->GetId());
}

std::string ItemScalingConfig::GetInstanceCategoryDescription(Map const* map) const
{
    if (!map) return "Unknown";
    uint32 maxP = ResolveMapMaxPlayers(map);
    if (map->IsRaid())
    {
        if (maxP <= 10) return "10-Player Raid";
        if (maxP <= 20) return "20-Player Raid";
        if (maxP <= 40) return "40-Player Raid";
        return "Raid";
    }
    return "5-Player Dungeon";
}
