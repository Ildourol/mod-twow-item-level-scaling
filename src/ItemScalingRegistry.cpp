/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingRegistry.h"
#include "ItemScalingSafety.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "ItemScalingIdentity.h"
#include "ItemScalingLive.h"
#include "ItemScalingSnapshot.h"
#include "Database/DatabaseEnv.h"
#include "Database/QueryResult.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "World.h"
#include <cstddef>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

ItemScalingRegistry* ItemScalingRegistry::instance()
{
    static ItemScalingRegistry instance;
    return &instance;
}

namespace
{
    VariantKey ReadKey(Field* fields)
    {
        return {
            fields[0].GetUInt32(),
            fields[1].GetUInt8(),
            fields[2].GetUInt16(),
            fields[3].GetUInt8(),
            fields[4].GetUInt8(),
            fields[5].GetUInt8(),
            fields[6].GetInt32()
        };
    }

    bool ValidKey(VariantKey const& key)
    {
        return key.baseEntry != 0 && key.targetEffectiveLevel >= 1 && key.targetEffectiveLevel <= 80 &&
            key.targetItemLevel >= 1 && key.targetItemLevel <= 300 && key.formulaVersion != 0 &&
            key.generatorRevision != 0 && key.requiredLevel >= 1 && key.requiredLevel <= 80;
    }

    std::string const KeyColumns =
        "base_entry,target_effective_level,target_item_level,formula_version,generator_revision,required_level,random_property_id";
    std::string const IdentityColumns =
        "base_class,base_subclass,base_material,base_displayid,base_inventory_type,base_sheath";

    ItemScalingIdentity ReadIdentity(Field* fields)
    {
        return {
            fields[0].GetUInt32(),
            fields[1].GetUInt32(),
            fields[2].GetInt32(),
            fields[3].GetUInt32(),
            fields[4].GetUInt32(),
            fields[5].GetUInt32()
        };
    }
}

void ItemScalingRegistry::EnsureSchema()
{
    WorldDatabase.DirectExecute(
        "CREATE TABLE IF NOT EXISTS `scaled_item_variant` ("
        "  `variant_entry` INT UNSIGNED NOT NULL,"
        "  `base_entry` INT UNSIGNED NOT NULL,"
        "  `target_effective_level` TINYINT UNSIGNED NOT NULL,"
        "  `target_item_level` SMALLINT UNSIGNED NOT NULL,"
        "  `formula_version` TINYINT UNSIGNED NOT NULL,"
        "  `generator_revision` TINYINT UNSIGNED NOT NULL,"
        "  `required_level` TINYINT UNSIGNED NOT NULL,"
        "  `random_property_id` INT NOT NULL DEFAULT 0,"
        "  `base_class` TINYINT UNSIGNED NOT NULL,"
        "  `base_subclass` TINYINT UNSIGNED NOT NULL,"
        "  `base_sound_override_subclass` TINYINT NOT NULL DEFAULT -1,"
        "  `base_material` TINYINT NOT NULL,"
        "  `base_displayid` INT UNSIGNED NOT NULL,"
        "  `base_inventory_type` TINYINT UNSIGNED NOT NULL,"
        "  `base_sheath` TINYINT UNSIGNED NOT NULL,"
        "  `preserve_nonzero_stats` TINYINT UNSIGNED NOT NULL,"
        "  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "  PRIMARY KEY (`variant_entry`),"
        "  UNIQUE KEY `uk_variant_key` (`base_entry`, `target_effective_level`, `target_item_level`, `formula_version`, `generator_revision`, `required_level`, `random_property_id`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;"
    );

    QueryResult* columns = WorldDatabase.Query(
        "SELECT TABLE_NAME,COLUMN_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='scaled_item_variant'");
    if (columns)
    {
        std::set<std::pair<std::string, std::string>> present;
        do
        {
            Field* fields = columns->Fetch();
            present.emplace(fields[0].GetCppString(), fields[1].GetCppString());
        } while (columns->NextRow());
        delete columns;

        struct Migration
        {
            char const* table;
            char const* column;
            char const* sql;
        };
        static Migration const migrations[] = {
            {"scaled_item_variant", "generator_revision", "ALTER TABLE `scaled_item_variant` ADD COLUMN `generator_revision` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `formula_version`;"},
            {"scaled_item_variant", "required_level", "ALTER TABLE `scaled_item_variant` ADD COLUMN `required_level` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `generator_revision`;"},
            {"scaled_item_variant", "random_property_id", "ALTER TABLE `scaled_item_variant` ADD COLUMN `random_property_id` INT NOT NULL DEFAULT 0 AFTER `required_level`;"},
            {"scaled_item_variant", "base_class", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_class` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `random_property_id`;"},
            {"scaled_item_variant", "base_subclass", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_subclass` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_class`;"},
            {"scaled_item_variant", "base_sound_override_subclass", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_sound_override_subclass` TINYINT NOT NULL DEFAULT -1 AFTER `base_subclass`;"},
            {"scaled_item_variant", "base_material", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_material` TINYINT NOT NULL DEFAULT 0 AFTER `base_sound_override_subclass`;"},
            {"scaled_item_variant", "base_displayid", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_displayid` INT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_material`;"},
            {"scaled_item_variant", "base_inventory_type", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_inventory_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_displayid`;"},
            {"scaled_item_variant", "base_sheath", "ALTER TABLE `scaled_item_variant` ADD COLUMN `base_sheath` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `base_inventory_type`;"},
            {"scaled_item_variant", "preserve_nonzero_stats", "ALTER TABLE `scaled_item_variant` ADD COLUMN `preserve_nonzero_stats` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `base_sheath`;"},
        };

        for (auto const& migration : migrations)
        {
            if (!present.count({migration.table, migration.column}))
            {
                WorldDatabase.DirectExecute(migration.sql);
            }
        }
    }

    QueryResult* uk = WorldDatabase.Query(
        "SELECT COUNT(*),COALESCE(SUM(COLUMN_NAME='random_property_id'),0) FROM information_schema.STATISTICS "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key'");
    if (uk)
    {
        uint64 totalIdx = uk->Fetch()[0].GetUInt64();
        uint64 randPropIdx = uk->Fetch()[1].GetUInt64();
        delete uk;

        if (randPropIdx == 0)
        {
            if (totalIdx != 0)
            {
                WorldDatabase.DirectExecute("ALTER TABLE `scaled_item_variant` DROP KEY `uk_variant_key`");
            }
            WorldDatabase.DirectExecute(
                "ALTER TABLE `scaled_item_variant` ADD UNIQUE KEY `uk_variant_key` ("
                "`base_entry`,`target_effective_level`,`target_item_level`,`formula_version`,`generator_revision`,`required_level`,`random_property_id`)"
            );
        }
    }
}

bool ItemScalingRegistry::ValidateSchema()
{
    struct ColumnType
    {
        std::string type;
        bool isUnsigned;
        bool nullable;
    };
    std::unordered_map<std::string, ColumnType> expected = {
        {"variant_entry", {"int", true, false}},
        {"base_entry", {"int", true, false}},
        {"target_effective_level", {"tinyint", true, false}},
        {"target_item_level", {"smallint", true, false}},
        {"formula_version", {"tinyint", true, false}},
        {"generator_revision", {"tinyint", true, false}},
        {"required_level", {"tinyint", true, false}},
        {"random_property_id", {"int", false, false}},
        {"base_class", {"tinyint", true, false}},
        {"base_subclass", {"tinyint", true, false}},
        {"base_sound_override_subclass", {"tinyint", false, false}},
        {"base_material", {"tinyint", false, false}},
        {"base_displayid", {"int", true, false}},
        {"base_inventory_type", {"tinyint", true, false}},
        {"base_sheath", {"tinyint", true, false}},
        {"preserve_nonzero_stats", {"tinyint", true, false}},
        {"created_at", {"timestamp", false, false}}
    };

    QueryResult* columns = WorldDatabase.Query(
        "SELECT COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE FROM information_schema.COLUMNS "
        "WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='scaled_item_variant'");
    if (columns)
    {
        do
        {
            Field* field = columns->Fetch();
            std::string colName = field[0].GetCppString();
            auto it = expected.find(colName);
            if (it == expected.end())
            {
                continue;
            }

            ColumnType const& type = it->second;
            std::string dataType = field[1].GetCppString();
            std::string fullType = field[2].GetCppString();
            bool isNullable = (field[3].GetCppString() == "YES");

            if (dataType != type.type ||
                (fullType.find("unsigned") != std::string::npos) != type.isUnsigned ||
                isNullable != type.nullable)
            {
                sLog.outError("[ItemScaling] Incompatible column scaled_item_variant.%s", colName.c_str());
                delete columns;
                return false;
            }
            expected.erase(it);
        } while (columns->NextRow());
        delete columns;
    }

    if (!expected.empty())
    {
        sLog.outError("[ItemScaling] Missing required scaled_item_variant columns.");
        return false;
    }

    QueryResult* index = WorldDatabase.Query(
        "SELECT COLUMN_NAME,NON_UNIQUE FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='uk_variant_key' ORDER BY SEQ_IN_INDEX");
    std::vector<std::string> indexColumns;
    if (index)
    {
        do
        {
            Field* f = index->Fetch();
            if (f[1].GetUInt8() != 0)
            {
                delete index;
                return false;
            }
            indexColumns.push_back(f[0].GetCppString());
        } while (index->NextRow());
        delete index;
    }

    std::vector<std::string> const keyColumns = {"base_entry", "target_effective_level", "target_item_level",
        "formula_version", "generator_revision", "required_level", "random_property_id"};
    if (indexColumns != keyColumns)
    {
        sLog.outError("[ItemScaling] Incompatible scaled_item_variant identity key index.");
        return false;
    }

    QueryResult* primary = WorldDatabase.Query(
        "SELECT COLUMN_NAME FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME='scaled_item_variant' AND INDEX_NAME='PRIMARY' ORDER BY SEQ_IN_INDEX");
    if (!primary || primary->GetRowCount() != 1 || primary->Fetch()[0].GetCppString() != "variant_entry")
    {
        sLog.outError("[ItemScaling] Invalid committed variant primary key.");
        if (primary) delete primary;
        return false;
    }
    delete primary;

    QueryResult* engines = WorldDatabase.Query(
        "SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() "
        "AND TABLE_NAME IN ('item_template','scaled_item_variant')");
    if (!engines)
    {
        sLog.outError("[ItemScaling] Could not query information_schema.TABLES for item tables.");
        return false;
    }

    bool hasItemTemplate = false;
    bool hasVariant = false;
    std::string itemTemplateEngine;
    std::string variantEngine;

    do
    {
        Field* f = engines->Fetch();
        std::string tableName = f[0].GetCppString();
        std::string engineName = f[1].GetCppString();

        if (tableName == "item_template")
        {
            hasItemTemplate = true;
            itemTemplateEngine = engineName;
        }
        else if (tableName == "scaled_item_variant")
        {
            hasVariant = true;
            variantEngine = engineName;
        }
    } while (engines->NextRow());
    delete engines;

    if (!hasItemTemplate || !hasVariant)
    {
        sLog.outError("[ItemScaling] Required tables missing: item_template (%s) or scaled_item_variant (%s).",
            hasItemTemplate ? "found" : "MISSING", hasVariant ? "found" : "MISSING");
        return false;
    }

    if (itemTemplateEngine != "InnoDB")
    {
        sLog.outString("[ItemScaling] Notice: item_template is using '%s'. Converting to InnoDB for optimal concurrency...",
            itemTemplateEngine.c_str());
        WorldDatabase.DirectExecute("ALTER TABLE item_template ENGINE=InnoDB ROW_FORMAT=DYNAMIC");

        QueryResult* verifyEngine = WorldDatabase.Query(
            "SELECT ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='item_template'");
        if (verifyEngine)
        {
            itemTemplateEngine = verifyEngine->Fetch()[0].GetCppString();
            delete verifyEngine;
        }

        if (itemTemplateEngine == "InnoDB")
        {
            sLog.outString("[ItemScaling] Successfully converted item_template to InnoDB storage engine.");
        }
        else
        {
            sLog.outString("[ItemScaling] Warning: item_template remains '%s' (InnoDB recommended). Proceeding in compatible mode.",
                itemTemplateEngine.c_str());
        }
    }

    if (variantEngine != "InnoDB")
    {
        sLog.outString("[ItemScaling] Converting scaled_item_variant to InnoDB...");
        WorldDatabase.DirectExecute("ALTER TABLE scaled_item_variant ENGINE=InnoDB");
    }

    return true;
}

bool ItemScalingRegistry::ResolveSyntheticEntryRange()
{
    QueryResult* result = WorldDatabase.Query(
        "SELECT COALESCE((SELECT MAX(entry) FROM item_template),0),"
        "COALESCE((SELECT MAX(variant_entry) FROM scaled_item_variant),0)");
    if (!result)
    {
        return false;
    }

    uint64 highestItem = result->Fetch()[0].GetUInt64();
    uint64 highestVariant = result->Fetch()[1].GetUInt64();
    delete result;

    _nextSyntheticEntry = ItemScalingSafety::AllocationStart(highestItem, highestVariant,
        sItemScalingConfig->AutoSyntheticEntry, sItemScalingConfig->SyntheticEntryStart,
        sItemScalingConfig->SyntheticEntryAutoOffset);

    if (_nextSyntheticEntry > sItemScalingConfig->SyntheticEntryMaximum)
    {
        sLog.outDetail("[ItemScaling] Synthetic ID limit reached; pending requests will be retained.");
    }

    sLog.outString("[ItemScaling] Synthetic IDs: highest item %u, highest variant %u, next allocation %u, max %u.",
        static_cast<uint32>(highestItem), static_cast<uint32>(highestVariant),
        static_cast<uint32>(_nextSyntheticEntry), sItemScalingConfig->SyntheticEntryMaximum);
    return true;
}

void ItemScalingRegistry::OnLoadCustomDatabaseTable()
{
    if (_dbSynchronized)
    {
        return;
    }

    EnsureSchema();
    if (!ValidateSchema())
    {
        return;
    }

    if (!ResolveSyntheticEntryRange() || !ItemScalingFormula::LoadStartupCurves(""))
    {
        sLog.outError("[ItemScaling] Startup prerequisites failed; item scaling disabled for this run.");
        return;
    }

    sLog.outString("[ItemScaling] Formula version %u, generator revision %u, target levels %u-%u, bracket %u.",
        sItemScalingConfig->FormulaVersion, ITEM_SCALING_GENERATOR_REVISION,
        sItemScalingConfig->MinLevel, sItemScalingConfig->MaxLevel, sItemScalingConfig->BracketStep);

    QueryResult* family = WorldDatabase.PQuery(
        "SELECT COUNT(*) FROM scaled_item_variant WHERE generator_revision=%u AND formula_version=%u "
        "AND preserve_nonzero_stats<>%u", ITEM_SCALING_GENERATOR_REVISION,
        sItemScalingConfig->FormulaVersion, uint32(sItemScalingConfig->PreserveNonZeroStats));
    if (family)
    {
        _familyCompatible = family->Fetch()[0].GetUInt64() == 0;
        delete family;

        if (!_familyCompatible)
        {
            sLog.outDetail("[ItemScaling] PreserveNonZeroStats conflicts with committed FormulaVersion family.");
        }
    }

    _dbSynchronized = true;
}

void ItemScalingRegistry::Initialize()
{
    if (!_dbSynchronized || _initialized.load(std::memory_order_acquire))
    {
        return;
    }

    QueryResult* count = WorldDatabase.Query("SELECT COUNT(*) FROM scaled_item_variant");
    if (!count)
    {
        return;
    }
    uint64 totalExpected = count->Fetch()[0].GetUInt64();
    delete count;

    std::string query = "SELECT variant_entry," + KeyColumns + "," + IdentityColumns + ",preserve_nonzero_stats FROM scaled_item_variant";
    QueryResult* result = WorldDatabase.Query(query.c_str());
    if (result && totalExpected != result->GetRowCount())
    {
        sLog.outError("[ItemScaling] Could not read complete committed registry; scaling disabled.");
        delete result;
        return;
    }

    uint32 validated = 0;
    uint32 inactiveFamily = 0;
    uint32 missingTemplate = 0;
    uint32 invalid = 0;
    uint32 incompatible = 0;

    if (result)
    {
        std::lock_guard<std::mutex> lock(_registryMutex);
        _keyToEntry.reserve(static_cast<std::size_t>(result->GetRowCount()));
        _syntheticEntries.reserve(static_cast<std::size_t>(result->GetRowCount()));

        do
        {
            Field* fields = result->Fetch();
            uint32 entry = fields[0].GetUInt32();
            VariantKey key = ReadKey(fields + 1);
            _syntheticEntries.insert(entry);
            _committedKeys.insert(key);

            ItemPrototype const* persisted = sObjectMgr.GetItemPrototype(entry);
            ItemPrototype const* base = sObjectMgr.GetItemPrototype(key.baseEntry);

            if (!ValidKey(key) || !entry || entry == key.baseEntry || !base ||
                !ItemScalingIdentity::CanCapture(*base))
            {
                ++invalid;
                continue;
            }

            if (!persisted)
            {
                ++missingTemplate;
                continue;
            }

            const_cast<ItemPrototype*>(persisted)->Discovered = true;

            ItemScalingIdentity const identity = ReadIdentity(fields + 8);
            if (!identity.Matches(*base) || !identity.Matches(*persisted) || fields[14].GetUInt8() > 1)
            {
                ++invalid;
                continue;
            }

            if (persisted->RequiredLevel != key.requiredLevel || persisted->ItemLevel != key.targetItemLevel)
            {
                ++invalid;
                continue;
            }

            if (!ItemScalingIdentity::CompatibleLootMetadata(*base, *persisted, key.randomPropertyId != 0))
            {
                ++incompatible;
                continue;
            }

            ++validated;
            if (key.generatorRevision == ITEM_SCALING_GENERATOR_REVISION &&
                key.formulaVersion == sItemScalingConfig->FormulaVersion &&
                fields[14].GetUInt8() == uint8(sItemScalingConfig->PreserveNonZeroStats))
            {
                _keyToEntry.emplace(key, entry);
            }
            else
            {
                ++inactiveFamily;
            }
        } while (result->NextRow());
        delete result;
    }

    sItemScalingBaseline->BuildBaseline(&_syntheticEntries);
    _initialized.store(true, std::memory_order_release);

    sLog.outString("[ItemScaling] Registry Initialized: %u validated, %u indexed, %u inactive family, %u missing templates, %u invalid.",
        validated, static_cast<uint32>(_keyToEntry.size()), inactiveFamily, missingTemplate, invalid);

    if (missingTemplate)
    {
        sLog.outDetail("[ItemScaling] %u committed variants withheld: item_template rows are missing.", missingTemplate);
    }
}

void ItemScalingRegistry::RegisterLiveVariant(VariantKey const& key, uint32 entry)
{
    std::lock_guard<std::mutex> lock(_registryMutex);
    _keyToEntry[key] = entry;
    _syntheticEntries.insert(entry);
    _committedKeys.insert(key);
}

size_t ItemScalingRegistry::GetIndexedVariantCount() const
{
    std::lock_guard<std::mutex> lock(_registryMutex);
    return _keyToEntry.size();
}

size_t ItemScalingRegistry::GetSyntheticEntryCount() const
{
    std::lock_guard<std::mutex> lock(_registryMutex);
    return _syntheticEntries.size();
}

bool ItemScalingRegistry::IsSynthetic(uint32 entry) const
{
    std::lock_guard<std::mutex> lock(_registryMutex);
    return _syntheticEntries.find(entry) != _syntheticEntries.end();
}

uint32 ItemScalingRegistry::FindExistingVariant(ItemPrototype const* baseProto, uint8 targetEffectiveLevel,
    uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel, int32 randomPropertyId) const
{
    if (!baseProto || !_initialized.load(std::memory_order_acquire))
    {
        return 0;
    }

    uint8 required = ItemScalingFormula::CalculateRequiredLevel(baseProto, targetEffectiveLevel, highestRealPlayerLevel);
    VariantKey key{baseProto->ItemId, targetEffectiveLevel, targetItemLevel, formulaVersion,
        ITEM_SCALING_GENERATOR_REVISION, required, randomPropertyId};

    return FindExistingVariantByKey(key);
}

uint32 ItemScalingRegistry::FindExistingVariantByKey(VariantKey const& key) const
{
    std::lock_guard<std::mutex> lock(_registryMutex);
    auto it = _keyToEntry.find(key);
    return it != _keyToEntry.end() ? it->second : 0;
}

uint32 ItemScalingRegistry::GenerateAndPersistLiveVariant(ItemPrototype const* baseProto, VariantKey const& key, uint8 highestRealPlayerLevel)
{
    if (!baseProto || !sItemScalingConfig->LiveEnable)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(_registryMutex);

    // Re-check under lock in case another thread just added it
    auto it = _keyToEntry.find(key);
    if (it != _keyToEntry.end())
    {
        return it->second;
    }

    if (_nextSyntheticEntry > sItemScalingConfig->SyntheticEntryMaximum)
    {
        sLog.outError("[ItemScaling] Synthetic entry limit (%u) exceeded; cannot generate variant for item %u.",
            sItemScalingConfig->SyntheticEntryMaximum, baseProto->ItemId);
        return 0;
    }

    uint32 newEntry = static_cast<uint32>(_nextSyntheticEntry++);

    ItemPrototype scaledProto = ItemScalingFormula::CreateScaledTemplate(baseProto, newEntry, key.targetEffectiveLevel, key.targetItemLevel, key.formulaVersion, highestRealPlayerLevel, key.randomPropertyId);

    // Direct in-memory prototype registration into sObjectMgr
    auto& prototypeMap = const_cast<ItemPrototypeMap&>(sObjectMgr.GetItemPrototypeMap());
    prototypeMap[newEntry] = scaledProto;

    // Direct SQL persistence: write complete 130-column template into item_template
    std::string itemSql = ItemScalingSnapshot::Insert(scaledProto, "item_template");
    WorldDatabase.DirectExecute(itemSql.c_str());

    // Direct SQL persistence: write variant identity into scaled_item_variant
    ItemScalingIdentity id = ItemScalingIdentity::Capture(*baseProto);
    WorldDatabase.PExecute(
        "INSERT INTO scaled_item_variant (variant_entry, base_entry, target_effective_level, target_item_level, "
        "formula_version, generator_revision, required_level, random_property_id, base_class, base_subclass, "
        "base_sound_override_subclass, base_material, base_displayid, base_inventory_type, base_sheath, preserve_nonzero_stats) "
        "VALUES (%u, %u, %u, %u, %u, %u, %u, %d, %u, %u, %d, %d, %u, %u, %u, %u)",
        newEntry, baseProto->ItemId, key.targetEffectiveLevel, key.targetItemLevel,
        key.formulaVersion, key.generatorRevision, key.requiredLevel, key.randomPropertyId,
        id.itemClass, id.subClass, -1, id.material, id.displayId,
        id.inventoryType, id.sheath, uint32(sItemScalingConfig->PreserveNonZeroStats));

    // Register in memory indexes
    _keyToEntry[key] = newEntry;
    _syntheticEntries.insert(newEntry);
    _committedKeys.insert(key);

    sLog.outString("[ItemScaling] Live generated & saved variant %u for '%s' (Base %u) -> Target Lv %u (Ilvl %u, ReqLv %u)",
        newEntry, baseProto->Name1.c_str(), baseProto->ItemId, key.targetEffectiveLevel, key.targetItemLevel, key.requiredLevel);

    return newEntry;
}

uint32 ItemScalingRegistry::FindOrRequestVariant(ItemPrototype const* baseProto, uint8 targetEffectiveLevel,
    uint16 targetItemLevel, uint8 formulaVersion, uint8 highestRealPlayerLevel, int32 randomPropertyId)
{
    if (!baseProto || !_initialized.load(std::memory_order_acquire))
    {
        return 0;
    }

    if (sItemScalingConfig->PreserveNativeLoot &&
        ItemScalingFormula::IsNativeTargetMatch(baseProto, targetEffectiveLevel, targetEffectiveLevel, highestRealPlayerLevel))
    {
        return 0;
    }

    uint8 required = ItemScalingFormula::CalculateRequiredLevel(baseProto, targetEffectiveLevel, highestRealPlayerLevel);
    VariantKey key{baseProto->ItemId, targetEffectiveLevel, targetItemLevel, formulaVersion,
        ITEM_SCALING_GENERATOR_REVISION, required, randomPropertyId};

    {
        std::lock_guard<std::mutex> lock(_registryMutex);
        if (_syntheticEntries.count(baseProto->ItemId))
        {
            return 0;
        }

        auto it = _keyToEntry.find(key);
        if (it != _keyToEntry.end())
        {
            return it->second;
        }

        if (_committedKeys.count(key))
        {
            return 0;
        }
    }

    if (_familyCompatible && sItemScalingConfig->LiveEnable && ValidKey(key) &&
        ItemScalingIdentity::CanCapture(*baseProto))
    {
        return GenerateAndPersistLiveVariant(baseProto, key, highestRealPlayerLevel);
    }
    else
    {
        sLog.outString("[ItemScaling] FindOrRequestVariant rejected item %u '%s': family=%d live=%d validKey=%d canCapture=%d",
            baseProto->ItemId, baseProto->Name1.c_str(), int(_familyCompatible), int(sItemScalingConfig->LiveEnable),
            int(ValidKey(key)), int(ItemScalingIdentity::CanCapture(*baseProto)));
    }

    return 0;
}
