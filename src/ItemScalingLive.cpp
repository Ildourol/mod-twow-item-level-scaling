/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingLive.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "ItemScalingIdentity.h"
#include "ItemScalingLootScript.h"
#include "ItemScalingRegistry.h"
#include "ItemScalingSafety.h"
#include "Chat/Chat.h"
#include "Creature.h"
#include "GameObject.h"
#include "Group.h"
#include "Log.h"
#include "LootMgr.h"
#include "Maps/Map.h"
#include "Maps/MapManager.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Protocol/Opcodes.h"
#include "Player.h"
#include "ScriptObjects.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Database/DBCStores.h"
#include "Database/SQLStorages.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

struct ItemScalingLive::Impl
{
    struct CatalogueSource
    {
        uint32 entry{0};
        bool chest{false};
        std::vector<uint32> items;
    };
    struct Visit
    {
        uint8 level{0};
        uint32 revision{0};
        std::size_t sourceIndex{0};
        std::size_t itemIndex{0};
        std::size_t rollIndex{0};
        uint8 announcedLevel{0};
    };

    mutable std::mutex mutex;
    bool enabled{false};
    uint64 now{0};
    std::unordered_map<uint32, std::vector<CatalogueSource>> catalogue;
    std::unordered_map<uint32, std::vector<int32>> enchantments;
    std::map<std::pair<uint32, uint32>, Visit> visits;

    bool LoadCatalogue();
    void Prewarm(uint32 budget, std::map<std::pair<uint32, uint32>, Visit>& activeVisits);
};

ItemScalingLive* ItemScalingLive::instance()
{
    static ItemScalingLive instance;
    return &instance;
}

ItemScalingLive::ItemScalingLive() : _impl(std::make_unique<Impl>()) {}
ItemScalingLive::~ItemScalingLive() = default;

bool ItemScalingLive::RecoverStagedTemplates()
{
    return true;
}

bool ItemScalingLive::ReserveSlots()
{
    return true;
}

void ItemScalingLive::IncludeOwnedEntries(std::unordered_set<uint32>& /*entries*/) const
{
}

void ItemScalingLive::Initialize()
{
    _impl->enabled = sItemScalingConfig->LiveEnable && sItemScalingRegistry->IsInitialized() &&
        _impl->LoadCatalogue();
}

uint32 ItemScalingLive::Find(VariantKey const& key) const
{
    return sItemScalingRegistry->FindExistingVariantByKey(key);
}

uint32 ItemScalingLive::FindOrRequest(ItemPrototype const& base, VariantKey const& key, uint8 playerLevel)
{
    return sItemScalingRegistry->GenerateAndPersistLiveVariant(&base, key, playerLevel);
}

bool ItemScalingLive::IsReserved(uint32 /*entry*/) const
{
    return false;
}

void ItemScalingLive::BeginLoot(Loot const& /*loot*/, Player const& /*owner*/)
{
}

void ItemScalingLive::TrackLoot(Loot const& /*loot*/, Player const& /*owner*/, std::size_t /*index*/, VariantKey const& /*key*/)
{
}

void ItemScalingLive::CancelSource(uint32 /*map*/, uint32 /*instance*/, ObjectGuid /*source*/)
{
}

bool ItemScalingLive::DeferPacket(WorldSession* /*session*/, WorldPacket const& /*packet*/)
{
    return false;
}

bool ItemScalingLive::PrepareChest(Player* /*player*/, GameObject* /*go*/)
{
    return false;
}

void ItemScalingLive::EnterMap(Map const& map, Player* player)
{
    if (!sItemScalingConfig->Enable || !map.IsDungeon() || map.IsBattleGround() ||
        sItemScalingConfig->IsMapExcluded(map.GetId()))
    {
        return;
    }

    auto key = std::make_pair(map.GetId(), map.GetInstanceId());
    uint8 highestLevel = 0;
    std::string highestPlayerName;
    Player* owner = ItemScalingLootScript::GetEligibleOwner(&map);
    if (owner)
    {
        highestLevel = owner->GetLevel();
        highestPlayerName = owner->GetName();
    }

    bool shouldAnnounceEntry = false;
    bool shouldAnnounceRecalc = false;

    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        auto [it, inserted] = _impl->visits.try_emplace(key, Impl::Visit{});
        auto& visit = it->second;

        if (highestLevel > 0)
        {
            if (visit.announcedLevel == 0)
            {
                visit.announcedLevel = highestLevel;
                visit.level = highestLevel;
                visit.revision = sItemScalingConfig->Revision;
                shouldAnnounceEntry = true;
            }
            else if (highestLevel > visit.announcedLevel)
            {
                visit.announcedLevel = highestLevel;
                visit.level = highestLevel;
                visit.revision = sItemScalingConfig->Revision;
                visit.sourceIndex = 0;
                visit.itemIndex = 0;
                visit.rollIndex = 0;
                shouldAnnounceRecalc = true;
            }
            else if (player && sItemScalingConfig->Announce)
            {
                shouldAnnounceEntry = true;
            }
        }
    }

    if (sItemScalingConfig->Announce && highestLevel > 0)
    {
        char const* rawMapName = map.GetMapName();
        std::string mapName = rawMapName ? rawMapName : "Instance";
        std::string categoryTag = sItemScalingConfig->GetInstanceCategoryDescription(&map);
        uint8 ceiling = sItemScalingConfig->GetDynamicCeiling(&map);
        uint8 floor = sItemScalingConfig->GetDynamicFloor(&map);

        char msg[256];
        if (shouldAnnounceRecalc)
        {
            snprintf(msg, sizeof(msg),
                "|cff00ccff[ItemScaling]|r Player Level Update in %s %s: Loot scaling recalculated to level %u (Highest Player: %s) [Ceiling: +%u, Floor: -%u].",
                mapName.c_str(), categoryTag.c_str(), highestLevel, highestPlayerName.c_str(), ceiling, floor);

            for (auto const& ref : map.GetPlayers())
            {
                if (Player* p = ref.GetSource())
                {
                    if (p->IsInWorld() && p->GetSession() && !p->GetSession()->IsHeadless())
                    {
                        ChatHandler(p->GetSession()).SendSysMessage(msg);
                    }
                }
            }
        }
        else if (shouldAnnounceEntry && player && player->IsInWorld() && player->GetSession() && !player->GetSession()->IsHeadless())
        {
            snprintf(msg, sizeof(msg),
                "|cff00ccff[ItemScaling]|r Entering %s %s: Loot scaling active for level %u (Highest Player: %s) [Ceiling: +%u, Floor: -%u].",
                mapName.c_str(), categoryTag.c_str(), highestLevel, highestPlayerName.c_str(), ceiling, floor);

            ChatHandler(player->GetSession()).SendSysMessage(msg);
        }
    }
}

bool ItemScalingLive::Impl::LoadCatalogue()
{
    using Graph = std::unordered_map<uint32, std::vector<std::pair<uint32, int32>>>;
    Graph creatureLoot, chestLoot, references;
    auto load = [](char const* table, Graph& graph)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT entry, item, mincountOrRef FROM %s WHERE ChanceOrQuestChance >= 0", table);
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 entry = fields[0].GetUInt32();
                uint32 item = fields[1].GetUInt32();
                int32 mincountOrRef = fields[2].GetInt32();
                graph[entry].emplace_back(item, mincountOrRef);
            } while (result->NextRow());
            delete result;
        }
    };

    load("creature_loot_template", creatureLoot);
    load("gameobject_loot_template", chestLoot);
    load("reference_loot_template", references);

    auto flatten = [&](Graph const& graph, uint32 lootId)
    {
        std::set<uint32> items;
        std::set<uint32> seen;
        std::vector<std::pair<Graph const*, uint32>> stack{{&graph, lootId}};
        while (!stack.empty())
        {
            auto [current, id] = stack.back();
            stack.pop_back();
            auto it = current->find(id);
            if (it == current->end())
            {
                continue;
            }
            seen.insert(id);
            for (auto const& [itemId, refId] : it->second)
            {
                if (refId < 0)
                {
                    uint32 nested = static_cast<uint32>(-refId);
                    if (!seen.count(nested))
                    {
                        stack.emplace_back(&references, nested);
                    }
                }
                else if (itemId != 0)
                {
                    items.insert(itemId);
                }
            }
        }
        return std::vector<uint32>(items.begin(), items.end());
    };

    catalogue.clear();

    std::string instanceMapFilter;
    for (auto itr = sMapStorage.begin<MapEntry>(); itr < sMapStorage.end<MapEntry>(); ++itr)
    {
        MapEntry const* mapEntry = *itr;
        if (mapEntry && mapEntry->IsDungeon() && !sItemScalingConfig->IsMapExcluded(mapEntry->id))
        {
            if (!instanceMapFilter.empty())
            {
                instanceMapFilter += ", ";
            }
            instanceMapFilter += std::to_string(mapEntry->id);
        }
    }

    if (!instanceMapFilter.empty())
    {
        std::string creatureQuery =
            "SELECT DISTINCT creature.map, creature_template.entry, creature_template.loot_id FROM creature_template "
            "JOIN creature ON creature.id = creature_template.entry "
            "WHERE creature.map IN (" + instanceMapFilter + ") AND creature_template.loot_id <> 0";

        QueryResult* creatures = WorldDatabase.Query(creatureQuery.c_str());
        if (creatures)
        {
            do
            {
                Field* fields = creatures->Fetch();
                uint32 map = fields[0].GetUInt32();
                uint32 entry = fields[1].GetUInt32();
                uint32 lootId = fields[2].GetUInt32();
                auto items = flatten(creatureLoot, lootId);
                if (!items.empty())
                {
                    catalogue[map].push_back({entry, false, std::move(items)});
                }
            } while (creatures->NextRow());
            delete creatures;
        }

        std::string chestQuery =
            "SELECT DISTINCT gameobject.map, gameobject_template.entry, gameobject_template.data1 AS lootId FROM gameobject_template "
            "JOIN gameobject ON gameobject.id = gameobject_template.entry "
            "WHERE gameobject_template.type = 3 AND gameobject.map IN (" + instanceMapFilter + ") AND gameobject_template.data1 <> 0";

        QueryResult* chests = WorldDatabase.Query(chestQuery.c_str());
        if (chests)
        {
            do
            {
                Field* fields = chests->Fetch();
                uint32 map = fields[0].GetUInt32();
                uint32 entry = fields[1].GetUInt32();
                uint32 lootId = fields[2].GetUInt32();
                auto items = flatten(chestLoot, lootId);
                if (!items.empty())
                {
                    catalogue[map].push_back({entry, true, std::move(items)});
                }
            } while (chests->NextRow());
            delete chests;
        }
    }

    enchantments.clear();
    QueryResult* ench = WorldDatabase.Query("SELECT entry, ench FROM item_enchantment_template");
    if (ench)
    {
        do
        {
            Field* fields = ench->Fetch();
            uint32 entry = fields[0].GetUInt32();
            int32 property = fields[1].GetInt32();
            enchantments[entry].push_back(property);
        } while (ench->NextRow());
        delete ench;
    }

    return true;
}

void ItemScalingLive::Impl::Prewarm(uint32 budget, std::map<std::pair<uint32, uint32>, Visit>& activeVisits)
{
    if (!enabled || !sItemScalingConfig->Enable || sItemScalingConfig->LiveGenerationMode != 1)
    {
        return;
    }

    for (auto it = activeVisits.begin(); it != activeVisits.end() && budget;)
    {
        Map* map = sMapMgr.FindMap(it->first.first, it->first.second);
        if (!map)
        {
            it = activeVisits.erase(it);
            continue;
        }

        Player* owner = ItemScalingLootScript::GetEligibleOwner(map);
        if (!owner)
        {
            ++it;
            continue;
        }

        uint8 level = owner->GetLevel();
        Visit& visit = it->second;
        if (visit.level != level || visit.revision != sItemScalingConfig->Revision)
        {
            visit = {level, sItemScalingConfig->Revision, 0, 0, 0, visit.announcedLevel};
        }

        auto catalogueIt = catalogue.find(map->GetId());
        if (catalogueIt != catalogue.end())
        {
            auto const& sourcesForMap = catalogueIt->second;
            while (visit.sourceIndex < sourcesForMap.size() && budget)
            {
                auto const& source = sourcesForMap[visit.sourceIndex];
                if (visit.itemIndex >= source.items.size())
                {
                    ++visit.sourceIndex;
                    visit.itemIndex = 0;
                    visit.rollIndex = 0;
                    continue;
                }

                uint32 entry = source.items[visit.itemIndex];
                ItemPrototype const* proto = sObjectMgr.GetItemPrototype(entry);
                if (!proto || !ItemScalingFormula::IsScalableEquipment(proto))
                {
                    ++visit.itemIndex;
                    visit.rollIndex = 0;
                    continue;
                }

                if (sItemScalingConfig->PreserveNativeLoot)
                {
                    CreatureInfo const* sourceCreature = source.chest ? nullptr : sObjectMgr.GetCreatureTemplate(source.entry);
                    uint8 requestedTarget = 0;
                    uint8 bracketedTarget = 0;
                    uint8 highestPlayerLevel = 0;
                    if (ItemScalingLootScript::ResolveTargetLevels(map, owner, sourceCreature, nullptr, requestedTarget, bracketedTarget, highestPlayerLevel))
                    {
                        if (ItemScalingFormula::IsNativeTargetMatch(proto, requestedTarget, bracketedTarget, level))
                        {
                            ++visit.itemIndex;
                            visit.rollIndex = 0;
                            continue;
                        }
                    }
                }

                int32 property = 0;
                bool moreRolls = false;
                if (proto->RandomProperty && sItemScalingConfig->RandomSuffixMode == RandomSuffixScalingMode::Bake)
                {
                    uint32 pool = static_cast<uint32>(proto->RandomProperty);
                    auto rolls = enchantments.find(pool);
                    if (rolls != enchantments.end() && visit.rollIndex < rolls->second.size())
                    {
                        property = rolls->second[visit.rollIndex++];
                        moreRolls = (visit.rollIndex < rolls->second.size());
                    }
                }

                Loot preview(owner);
                LootItem item(entry, 1, property);
                preview.items.push_back(item);

                CreatureInfo const* sourceCreature = source.chest ? nullptr : sObjectMgr.GetCreatureTemplate(source.entry);
                ItemScalingLootScript::PrepareLoot(&preview, owner, nullptr, sourceCreature, true);

                if (!moreRolls)
                {
                    ++visit.itemIndex;
                    visit.rollIndex = 0;
                }
                --budget;
            }
        }
        ++it;
    }
}

void ItemScalingLive::Update(uint32 diff)
{
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        _impl->now += diff;
    }

    if (sItemScalingConfig->LiveGenerationMode == 1)
    {
        std::map<std::pair<uint32, uint32>, Impl::Visit> visits;
        {
            std::lock_guard<std::mutex> lock(_impl->mutex);
            visits.swap(_impl->visits);
        }
        _impl->Prewarm(sItemScalingConfig->LiveMaxPublishPerTick, visits);
        {
            std::lock_guard<std::mutex> lock(_impl->mutex);
            for (auto const& [k, visit] : visits)
            {
                _impl->visits.insert_or_assign(k, visit);
            }
        }
    }
}

ItemScalingLive::Diagnostics ItemScalingLive::GetDiagnostics() const
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    Diagnostics result;
    result.enabled = _impl->enabled;
    size_t count = sItemScalingRegistry->GetSyntheticEntryCount();
    result.available = (sItemScalingConfig->SyntheticEntryMaximum > count)
        ? (sItemScalingConfig->SyntheticEntryMaximum - count) : 0;
    result.ready = sItemScalingRegistry->GetIndexedVariantCount();
    result.durable = count;
    result.pending = 0;
    result.failures = 0;
    return result;
}

namespace
{
    class ItemScalingLiveServerScript : public ServerScript
    {
    public:
        ItemScalingLiveServerScript() : ServerScript("ItemScalingLiveServerScript",
            {SERVERHOOK_CAN_PACKET_RECEIVE, SERVERHOOK_CAN_PACKET_SEND}) { }

        bool CanPacketReceive(WorldSession* /*session*/, WorldPacket const& /*packet*/) override
        {
            return true;
        }

        bool CanPacketSend(WorldSession* /*session*/, WorldPacket const& /*packet*/) override
        {
            return true;
        }
    };

    class ItemScalingLiveMapScript : public AllMapScript
    {
    public:
        ItemScalingLiveMapScript() : AllMapScript("ItemScalingLiveMapScript") { }
        void OnPlayerEnterAll(Map* map, Player* player) override { sItemScalingLive->EnterMap(*map, player); }
        void OnPlayerLeaveAll(Map* map, Player* /*player*/) override { sItemScalingLive->EnterMap(*map, nullptr); }
    };
}

void AddItemScalingLiveScripts()
{
    new ItemScalingLiveServerScript();
    new ItemScalingLiveMapScript();
}
