/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#ifndef MOD_ITEM_SCALING_SNAPSHOT_H
#define MOD_ITEM_SCALING_SNAPSHOT_H

#include "Objects/ItemPrototype.h"
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace ItemScalingSnapshot
{
    // Hex literals do not depend on SQL escaping, connection checkout, SQL mode or locale.
    inline std::string Text(std::string const& value)
    {
        if (value.empty())
        {
            return "''";
        }

        static char const digits[] = "0123456789abcdef";
        std::string result = "CONVERT(0x";
        for (unsigned char c : value)
        {
            result += digits[c >> 4];
            result += digits[c & 15];
        }
        return result + " USING utf8mb4)";
    }

    // Serialize the complete, normalized RAM ItemPrototype into MySQL INSERT for Turtle WoW's 130 item_template columns.
    inline std::string Insert(ItemPrototype const& item, std::string const& table)
    {
        std::ostringstream columns;
        std::ostringstream values;
        values.imbue(std::locale::classic());
        values << std::setprecision(std::numeric_limits<float>::max_digits10);
        bool first = true;
        auto field = [&](std::string const& name, auto value)
        {
            if (!first)
            {
                columns << ',';
                values << ',';
            }
            first = false;
            columns << '`' << name << '`';
            values << value;
        };

        field("entry", item.ItemId);
        field("class", item.Class);
        field("subclass", item.SubClass);
        field("name", Text(item.Name1));
        field("description", Text(item.Description));
        field("display_id", item.DisplayInfoID);
        field("quality", item.Quality);
        field("flags", item.Flags);
        field("buy_count", item.BuyCount);
        field("buy_price", item.BuyPrice);
        field("sell_price", item.SellPrice);
        field("inventory_type", item.InventoryType);
        field("allowable_class", static_cast<int32>(item.AllowableClass));
        field("allowable_race", static_cast<int32>(item.AllowableRace));
        field("item_level", item.ItemLevel);
        field("required_level", item.RequiredLevel);
        field("required_skill", item.RequiredSkill);
        field("required_skill_rank", item.RequiredSkillRank);
        field("required_spell", item.RequiredSpell);
        field("required_honor_rank", item.RequiredHonorRank);
        field("required_city_rank", item.RequiredCityRank);
        field("required_reputation_faction", item.RequiredReputationFaction);
        field("required_reputation_rank", item.RequiredReputationRank);
        field("max_count", item.MaxCount);
        field("stackable", item.Stackable);
        field("container_slots", item.ContainerSlots);

        for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
        {
            std::string idx = std::to_string(i + 1);
            field("stat_type" + idx, item.ItemStat[i].ItemStatType);
            field("stat_value" + idx, item.ItemStat[i].ItemStatValue);
        }

        field("delay", item.Delay);
        field("range_mod", item.RangedModRange);
        field("ammo_type", item.AmmoType);

        for (uint32 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            std::string idx = std::to_string(i + 1);
            field("dmg_min" + idx, item.Damage[i].DamageMin);
            field("dmg_max" + idx, item.Damage[i].DamageMax);
            field("dmg_type" + idx, item.Damage[i].DamageType);
        }

        field("block", item.Block);
        field("armor", item.Armor);
        field("holy_res", item.HolyRes);
        field("fire_res", item.FireRes);
        field("nature_res", item.NatureRes);
        field("frost_res", item.FrostRes);
        field("shadow_res", item.ShadowRes);
        field("arcane_res", item.ArcaneRes);

        for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            std::string suffix = "_" + std::to_string(i + 1);
            field("spellid" + suffix, item.Spells[i].SpellId);
            field("spelltrigger" + suffix, item.Spells[i].SpellTrigger);
            field("spellcharges" + suffix, item.Spells[i].SpellCharges);
            field("spellppmrate" + suffix, item.Spells[i].SpellPPMRate);
            field("spellcooldown" + suffix, item.Spells[i].SpellCooldown);
            field("spellcategory" + suffix, item.Spells[i].SpellCategory);
            field("spellcategorycooldown" + suffix, item.Spells[i].SpellCategoryCooldown);
        }

        field("bonding", item.Bonding);
        field("page_text", item.PageText);
        field("page_language", item.LanguageID);
        field("page_material", item.PageMaterial);
        field("start_quest", item.StartQuest);
        field("lock_id", item.LockID);
        field("material", static_cast<int16>(item.Material));
        field("sheath", item.Sheath);
        field("random_property", item.RandomProperty);
        field("set_id", item.ItemSet);
        field("max_durability", item.MaxDurability);
        field("area_bound", item.Area);
        field("map_bound", static_cast<int16>(item.Map));
        field("duration", item.Duration);
        field("bag_family", static_cast<int32>(item.BagFamily));
        field("disenchant_id", item.DisenchantID);
        field("food_type", item.FoodType);
        field("min_money_loot", item.MinMoneyLoot);
        field("max_money_loot", item.MaxMoneyLoot);
        field("wrapped_gift", item.WrappedGift);
        field("extra_flags", item.ExtraFlags);
        field("other_team_entry", item.OtherTeamEntry);
        field("script_name", "''");

        return "REPLACE INTO " + table + " (" + columns.str() + ") VALUES (" + values.str() + ")";
    }
}

#endif // MOD_ITEM_SCALING_SNAPSHOT_H
