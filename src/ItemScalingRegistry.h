/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#ifndef _ITEM_SCALING_REGISTRY_H
#define _ITEM_SCALING_REGISTRY_H

#include "ItemScalingCommon.h"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

class ItemScalingRegistry
{
public:
    static ItemScalingRegistry* instance();

    // Generate durable rows before the core loads item_template.
    void OnLoadCustomDatabaseTable();

    // Validate and index core-loaded templates before gameplay can select variants.
    void Initialize();

    // Hits select immutable templates; misses invoke live generation.
    [[nodiscard]] uint32 FindOrRequestVariant(ItemPrototype const* baseProto, uint8 targetEffectiveLevel,
        uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel, int32 randomPropertyId = 0);

    // Live synchronous generation and direct SQL persistence
    uint32 GenerateAndPersistLiveVariant(ItemPrototype const* baseProto, VariantKey const& key, uint8 highestRealPlayerLevel);

    // Dynamic registration of live variants into in-memory registry
    void RegisterLiveVariant(VariantKey const& key, uint32 entry);

    // Diagnostics and inspection without queuing database requests
    [[nodiscard]] size_t GetIndexedVariantCount() const;
    [[nodiscard]] size_t GetSyntheticEntryCount() const;
    [[nodiscard]] bool IsInitialized() const { return _initialized.load(std::memory_order_acquire); }
    [[nodiscard]] bool IsDbSynchronized() const { return _dbSynchronized; }
    [[nodiscard]] bool IsSynthetic(uint32 entry) const;
    [[nodiscard]] bool IsSyntheticEntry(uint32 entry) const { return IsSynthetic(entry); }
    [[nodiscard]] uint32 FindExistingVariant(ItemPrototype const* baseProto, uint8 targetEffectiveLevel,
        uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel, int32 randomPropertyId = 0) const;
    [[nodiscard]] uint32 FindExistingVariantByKey(VariantKey const& key) const;

private:
    void EnsureSchema();
    bool ValidateSchema();
    bool ResolveSyntheticEntryRange();

    mutable std::mutex _registryMutex;
    std::unordered_map<VariantKey, uint32, VariantKeyHash> _keyToEntry;
    std::unordered_set<uint32> _syntheticEntries;
    std::unordered_set<VariantKey, VariantKeyHash> _committedKeys;
    uint64 _nextSyntheticEntry{0};
    bool _familyCompatible{true};
    bool _dbSynchronized{false};
    std::atomic<bool> _initialized{false};
};

#define sItemScalingRegistry ItemScalingRegistry::instance()

#endif // _ITEM_SCALING_REGISTRY_H
