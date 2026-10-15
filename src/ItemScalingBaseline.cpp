/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

ItemScalingBaseline* ItemScalingBaseline::instance()
{
    static ItemScalingBaseline instance;
    return &instance;
}

void ItemScalingBaseline::BuildBaseline(std::unordered_set<uint32> const* knownSyntheticEntries)
{
    if (_initialized)
    {
        return;
    }

    ItemPrototypeMap const& itemPrototypes = sObjectMgr.GetItemPrototypeMap();
    bool useStore = !itemPrototypes.empty();

    // Temporary collector: [level][quality][family] -> list of item levels
    std::vector<uint32> buckets[MAX_BASELINE_LEVEL + 1][MAX_BASELINE_QUALITY][MAX_BASELINE_FAMILY];
    uint32 totalProcessed = 0;
    std::unordered_set<uint32> syntheticEntries;
    std::unordered_set<uint32> const* activeSynthetic = knownSyntheticEntries;
    if (!activeSynthetic)
    {
        QueryResult* variants = WorldDatabase.Query("SELECT variant_entry FROM scaled_item_variant");
        if (variants)
        {
            do
            {
                syntheticEntries.insert(variants->Fetch()[0].GetUInt32());
            } while (variants->NextRow());
            delete variants;
        }
        activeSynthetic = &syntheticEntries;
    }

    if (useStore)
    {
        for (auto const& pair : itemPrototypes)
        {
            ItemPrototype const& proto = pair.second;

            if (activeSynthetic->count(proto.ItemId))
                continue;

            if (proto.Class != ITEM_CLASS_WEAPON && proto.Class != ITEM_CLASS_ARMOR)
                continue;

            int8 family = ItemScalingFormula::GetSlotFamily(proto.InventoryType);
            if (family < 0 || family >= MAX_BASELINE_FAMILY)
                continue;

            if (proto.Quality >= MAX_BASELINE_QUALITY)
                continue;

            uint8 lvl = static_cast<uint8>(proto.RequiredLevel);
            if (lvl == 0 || lvl > MAX_BASELINE_LEVEL)
                continue;

            if (proto.ItemLevel == 0)
                continue;

            buckets[lvl][proto.Quality][family].push_back(proto.ItemLevel);
            ++totalProcessed;
        }
    }
    else
    {
        QueryResult* result = WorldDatabase.Query(
            "SELECT entry, class, quality, required_level, item_level, inventory_type "
            "FROM item_template WHERE class IN (2, 4) AND required_level BETWEEN 1 AND 80 AND item_level > 0 "
            "AND entry NOT IN (SELECT variant_entry FROM scaled_item_variant)"
        );

        if (result)
        {
            do
            {
                Field* f = result->Fetch();
                uint32 quality = f[2].GetUInt8();
                uint8 lvl = f[3].GetUInt8();
                uint32 ilvl = f[4].GetUInt16();
                uint32 invType = f[5].GetUInt8();

                if (quality >= MAX_BASELINE_QUALITY || lvl == 0 || lvl > MAX_BASELINE_LEVEL || ilvl == 0)
                {
                    continue;
                }

                int8 family = ItemScalingFormula::GetSlotFamily(invType);
                if (family < 0 || family >= MAX_BASELINE_FAMILY)
                {
                    continue;
                }

                buckets[lvl][quality][family].push_back(ilvl);
                ++totalProcessed;
            } while (result->NextRow());
            delete result;
        }
    }

    // Step 2: Compute medians for populated buckets
    for (uint8 lvl = 1; lvl <= MAX_BASELINE_LEVEL; ++lvl)
    {
        for (uint8 q = 0; q < MAX_BASELINE_QUALITY; ++q)
        {
            for (uint8 f = 0; f < MAX_BASELINE_FAMILY; ++f)
            {
                auto& vec = buckets[lvl][q][f];
                if (vec.empty())
                {
                    _medians[lvl][q][f] = 0.0;
                    continue;
                }

                std::sort(vec.begin(), vec.end());
                size_t n = vec.size();
                if (n % 2 == 1)
                {
                    _medians[lvl][q][f] = static_cast<double>(vec[n / 2]);
                }
                else
                {
                    _medians[lvl][q][f] = static_cast<double>(vec[n / 2 - 1] + vec[n / 2]) / 2.0;
                }
            }
        }
    }

    // Step 3: Interpolate/extrapolate unpopulated cells per (quality, family)
    for (uint8 q = 0; q < MAX_BASELINE_QUALITY; ++q)
    {
        for (uint8 f = 0; f < MAX_BASELINE_FAMILY; ++f)
        {
            uint8 minPop = 0;
            uint8 maxPop = 0;
            for (uint8 lvl = 1; lvl <= MAX_BASELINE_LEVEL; ++lvl)
            {
                if (_medians[lvl][q][f] > 0.0)
                {
                    if (minPop == 0) minPop = lvl;
                    maxPop = lvl;
                }
            }

            if (minPop == 0)
            {
                // Quality/family completely unpopulated, fallback to quality-specific baseline progression
                for (uint8 lvl = 1; lvl <= MAX_BASELINE_LEVEL; ++lvl)
                {
                    double qualityOffset = 0.0;
                    switch (q)
                    {
                        case ITEM_QUALITY_POOR:      qualityOffset = -5.0; break;
                        case ITEM_QUALITY_NORMAL:    qualityOffset = 0.0; break;
                        case ITEM_QUALITY_UNCOMMON:  qualityOffset = 5.0; break;
                        case ITEM_QUALITY_RARE:      qualityOffset = 10.0; break;
                        case ITEM_QUALITY_EPIC:      qualityOffset = 15.0; break;
                        case ITEM_QUALITY_LEGENDARY: qualityOffset = 25.0; break;
                        default:                     qualityOffset = 5.0; break;
                    }
                    double estimatedIlvl = std::max(1.0, static_cast<double>(lvl) + qualityOffset);
                    if (lvl > 60)
                    {
                        double factor = 1.0 + (static_cast<double>(lvl - 60) / 20.0) * 1.5;
                        estimatedIlvl *= factor;
                    }
                    _medians[lvl][q][f] = estimatedIlvl;
                }
                continue;
            }

            // Fill left edge
            for (uint8 lvl = 1; lvl < minPop; ++lvl)
            {
                _medians[lvl][q][f] = _medians[minPop][q][f] * static_cast<double>(lvl) / static_cast<double>(minPop);
            }

            // Fill right edge
            for (uint8 lvl = maxPop + 1; lvl <= MAX_BASELINE_LEVEL; ++lvl)
            {
                _medians[lvl][q][f] = _medians[maxPop][q][f] * static_cast<double>(lvl) / static_cast<double>(maxPop);
            }

            // Fill interior gaps via linear interpolation
            for (uint8 lvl = minPop + 1; lvl < maxPop; ++lvl)
            {
                if (_medians[lvl][q][f] == 0.0)
                {
                    uint8 nextPop = lvl + 1;
                    while (nextPop < maxPop && _medians[nextPop][q][f] == 0.0)
                    {
                        ++nextPop;
                    }
                    uint8 prevPop = lvl - 1;
                    while (prevPop > minPop && _medians[prevPop][q][f] == 0.0)
                    {
                        --prevPop;
                    }

                    double pVal = _medians[prevPop][q][f];
                    double nVal = _medians[nextPop][q][f];
                    double fraction = static_cast<double>(lvl - prevPop) / static_cast<double>(nextPop - prevPop);
                    _medians[lvl][q][f] = pVal + fraction * (nVal - pVal);
                }
            }
        }
    }

    _initialized = true;
    sLog.outString("[ItemScaling] Item level baseline built: %u items processed across %u levels.",
        totalProcessed, MAX_BASELINE_LEVEL);
}

double ItemScalingBaseline::GetMedianItemLevel(uint8 level, uint32 quality, uint8 slotFamily) const
{
    level = std::clamp<uint8>(level, 1, MAX_BASELINE_LEVEL);
    quality = std::min<uint32>(quality, MAX_BASELINE_QUALITY - 1);
    slotFamily = std::min<uint8>(slotFamily, MAX_BASELINE_FAMILY - 1);

    double val = _medians[level][quality][slotFamily];
    if (val > 0.0)
    {
        return val;
    }

    return static_cast<double>(level);
}

uint16 ItemScalingBaseline::CalculateTargetItemLevel(ItemPrototype const* baseProto, uint8 targetLevel, uint8 originalRefLevel) const
{
    if (!baseProto)
    {
        return 0;
    }

    uint32 i0 = baseProto->ItemLevel;
    if (i0 == 0)
    {
        i0 = std::max<uint32>(1, baseProto->RequiredLevel);
    }

    uint8 l0 = originalRefLevel;
    if (l0 == 0)
    {
        l0 = static_cast<uint8>(baseProto->RequiredLevel);
    }
    if (l0 == 0)
    {
        l0 = static_cast<uint8>(std::clamp<uint32>(i0, 1, 80));
    }

    uint8 lTarget = std::clamp<uint8>(targetLevel, 1, MAX_BASELINE_LEVEL);
    if (lTarget == l0)
    {
        return static_cast<uint16>(i0);
    }

    int8 family = ItemScalingFormula::GetSlotFamily(baseProto->InventoryType);
    if (family < 0)
    {
        family = 2; // fallback
    }

    uint32 q = std::min<uint32>(baseProto->Quality, MAX_BASELINE_QUALITY - 1);

    double m0 = GetMedianItemLevel(l0, q, static_cast<uint8>(family));
    double m1 = GetMedianItemLevel(lTarget, q, static_cast<uint8>(family));

    double targetIlvl = 0.0;
    if (m0 > 0.0 && m1 > 0.0)
    {
        targetIlvl = std::round(static_cast<double>(i0) * m1 / m0);
    }
    else
    {
        targetIlvl = std::round(static_cast<double>(i0) * static_cast<double>(lTarget) / static_cast<double>(std::max<uint8>(1, l0)));
    }

    uint16 clampedIlvl = static_cast<uint16>(std::clamp<int32>(static_cast<int32>(targetIlvl), 1, 300));
    return clampedIlvl;
}
