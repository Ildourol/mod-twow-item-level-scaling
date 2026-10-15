/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#ifndef _ITEM_SCALING_COMMON_H
#define _ITEM_SCALING_COMMON_H

#include "Common.h"
#include "ItemPrototype.h"
#include <functional>
#include <tuple>
#include <cstdint>

enum ItemScalingMethod : uint8
{
    SCALING_METHOD_DYNAMIC = 0,
    SCALING_METHOD_FIXED   = 1
};

enum RequiredLevelPolicy : uint8
{
    REQ_POLICY_TARGET               = 0,
    REQ_POLICY_PLAYER               = 1,
    REQ_POLICY_TARGET_CAPPED_PLAYER = 2
};

enum class RandomSuffixScalingMode : uint8
{
    Skip = 0,
    Bake = 1
};

constexpr uint8 ITEM_SCALING_GENERATOR_REVISION = 2;

struct VariantKey
{
    uint32 baseEntry{0};
    uint8 targetEffectiveLevel{0};
    uint16 targetItemLevel{0};
    uint8 formulaVersion{0};
    uint8 generatorRevision{0};
    uint8 requiredLevel{0};
    int32 randomPropertyId{0};

    bool operator==(VariantKey const& other) const
    {
        return baseEntry == other.baseEntry &&
               targetEffectiveLevel == other.targetEffectiveLevel &&
               targetItemLevel == other.targetItemLevel &&
               formulaVersion == other.formulaVersion &&
               generatorRevision == other.generatorRevision &&
               requiredLevel == other.requiredLevel &&
               randomPropertyId == other.randomPropertyId;
    }

    bool operator<(VariantKey const& other) const
    {
        return std::tie(baseEntry, targetEffectiveLevel, targetItemLevel, formulaVersion,
                        generatorRevision, requiredLevel, randomPropertyId) <
               std::tie(other.baseEntry, other.targetEffectiveLevel, other.targetItemLevel,
                        other.formulaVersion, other.generatorRevision, other.requiredLevel, other.randomPropertyId);
    }
};

struct VariantKeyHash
{
    std::size_t operator()(VariantKey const& k) const noexcept
    {
        std::size_t h = 0;
        auto hash_combine = [&h](std::size_t val)
        {
            h ^= val + 0x9e3779b9 + (h << 6) + (h >> 2);
        };

        hash_combine(std::hash<uint32>{}(k.baseEntry));
        hash_combine(std::hash<uint8>{}(k.targetEffectiveLevel));
        hash_combine(std::hash<uint16>{}(k.targetItemLevel));
        hash_combine(std::hash<uint8>{}(k.formulaVersion));
        hash_combine(std::hash<uint8>{}(k.generatorRevision));
        hash_combine(std::hash<uint8>{}(k.requiredLevel));
        hash_combine(std::hash<int32>{}(k.randomPropertyId));
        return h;
    }
};

#endif // _ITEM_SCALING_COMMON_H
