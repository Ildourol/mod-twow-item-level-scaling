/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingCommands.h"
#include "ItemScalingBaseline.h"
#include "ItemScalingCommon.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "ItemScalingEquipSpell.h"
#include "ItemScalingRegistry.h"
#include "ItemScalingLive.h"
#include "Chat/Chat.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptObjects.h"
#include <algorithm>
#include <cstring>
#include <string>

static char const* GetPolicyName(RequiredLevelPolicy policy)
{
    switch (policy)
    {
        case REQ_POLICY_TARGET: return "target";
        case REQ_POLICY_PLAYER: return "player";
        case REQ_POLICY_TARGET_CAPPED_PLAYER: return "target-capped-player";
        default: return "unknown";
    }
}

namespace
{
    class ItemScalingCommandScript final : public AllCommandScript
    {
    public:
        ItemScalingCommandScript() : AllCommandScript("ItemScalingCommandScript") {}

        bool CanExecuteCommand(ChatHandler* handler, char const* command, char const* args) override
        {
            if (!command)
            {
                return true;
            }

            if (std::strcmp(command, "itemscaling") != 0 && std::strcmp(command, "mils") != 0)
            {
                return true;
            }

            if (handler && handler->GetSession() && handler->GetSession()->GetSecurity() < SEC_MODERATOR)
            {
                handler->SendSysMessage("You do not have permission to use this command.");
                return false;
            }

            std::string subcmd;
            std::string rest;
            if (args && *args)
            {
                std::string argStr(args);
                auto spacePos = argStr.find(' ');
                if (spacePos != std::string::npos)
                {
                    subcmd = argStr.substr(0, spacePos);
                    rest = argStr.substr(spacePos + 1);
                }
                else
                {
                    subcmd = argStr;
                }
            }

            if (subcmd.empty() || subcmd == "status")
            {
                HandleStatus(handler);
                return false;
            }

            if (subcmd == "reload")
            {
                sItemScalingConfig->Load(true);
                if (handler)
                {
                    handler->SendSysMessage("|cff00ff00[ItemScaling] Configuration reloaded successfully.|r");
                }
                return false;
            }

            if (subcmd == "preview")
            {
                HandlePreview(handler, rest);
                return false;
            }

            if (subcmd == "generate" || subcmd == "create" || subcmd == "test")
            {
                HandleGenerate(handler, rest);
                return false;
            }

            if (handler)
            {
                handler->SendSysMessage("|cff3399ff[ItemScaling] Commands:|r");
                handler->SendSysMessage("  .itemscaling status");
                handler->SendSysMessage("  .itemscaling preview <itemId> [targetLevel]");
                handler->SendSysMessage("  .itemscaling generate <itemId> [targetLevel]");
                handler->SendSysMessage("  .itemscaling reload");
            }
            return false;
        }

    private:
        void HandleStatus(ChatHandler* handler)
        {
            if (!handler)
            {
                return;
            }

            handler->SendSysMessage("|cff3399ff=== [Turtle WoW Item Level Scaling Status] ===|r");
            handler->PSendSysMessage("Module State: %s", sItemScalingConfig->Enable ? "|cff00ff00ENABLED|r" : "|cffff0000DISABLED|r");
            handler->PSendSysMessage("Scaling Method: %s", sItemScalingConfig->Method == SCALING_METHOD_DYNAMIC ? "Dynamic" : "Fixed");
            handler->PSendSysMessage("AutoBalance Synergy: %s", sItemScalingConfig->UseAutoBalanceSettings ? "|cff00ff00ACTIVE (Adopting AutoBalance Settings)|r" : "|cffff8000INACTIVE (Standalone Config)|r");
            handler->PSendSysMessage("Dynamic Dungeons: Floor -%u, Ceiling +%u", sItemScalingConfig->DynamicFloorDungeons, sItemScalingConfig->DynamicCeilingDungeons);
            handler->PSendSysMessage("Dynamic Heroic Dungeons: Floor -%u, Ceiling +%u", sItemScalingConfig->DynamicFloorHeroicDungeons, sItemScalingConfig->DynamicCeilingHeroicDungeons);
            handler->PSendSysMessage("Dynamic Raids (Default): Floor -%u, Ceiling +%u", sItemScalingConfig->DynamicFloorRaids, sItemScalingConfig->DynamicCeilingRaids);
            handler->PSendSysMessage("Dynamic Raids (10M / 15M / 20M / 25M / 40M): 10M [-%u, +%u], 15M [-%u, +%u], 20M [-%u, +%u], 25M [-%u, +%u], 40M [-%u, +%u]",
                sItemScalingConfig->DynamicFloorRaid10M, sItemScalingConfig->DynamicCeilingRaid10M,
                sItemScalingConfig->DynamicFloorRaid15M, sItemScalingConfig->DynamicCeilingRaid15M,
                sItemScalingConfig->DynamicFloorRaid20M, sItemScalingConfig->DynamicCeilingRaid20M,
                sItemScalingConfig->DynamicFloorRaid25M, sItemScalingConfig->DynamicCeilingRaid25M,
                sItemScalingConfig->DynamicFloorRaid40M, sItemScalingConfig->DynamicCeilingRaid40M);
            handler->PSendSysMessage("Per-Instance Overrides: %u configured", static_cast<uint32>(sItemScalingConfig->DynamicOverrides.size()));
            handler->PSendSysMessage("Scale Chests & GameObjects: %s", sItemScalingConfig->ScaleChests ? "|cff00ff00ENABLED|r" : "|cffff0000DISABLED|r");
            handler->PSendSysMessage("Preserve Native Loot: %s", sItemScalingConfig->PreserveNativeLoot ? "|cff00ff00ENABLED|r" : "|cffff0000DISABLED|r");
            handler->PSendSysMessage("Directional Scaling: ScaleUp=%s, ScaleDown=%s",
                sItemScalingConfig->ScaleUp ? "Yes" : "No", sItemScalingConfig->ScaleDown ? "Yes" : "No");
            handler->PSendSysMessage("RealPlayersOnly: %s, IncludeGameMasters: %s",
                sItemScalingConfig->RealPlayersOnly ? "Yes" : "No", sItemScalingConfig->IncludeGameMasters ? "Yes" : "No");
            handler->PSendSysMessage("RequiredLevel Policy: %s", GetPolicyName(sItemScalingConfig->ReqLevelPolicy));
            handler->PSendSysMessage("RandomSuffix Mode: %u (0=Skip, 1=Bake)", static_cast<uint32>(sItemScalingConfig->RandomSuffixMode));
            handler->PSendSysMessage("Formula Version: %u, Generator Revision: %u", sItemScalingConfig->FormulaVersion, ITEM_SCALING_GENERATOR_REVISION);
            handler->PSendSysMessage("Database Synchronized: %s", sItemScalingRegistry->IsDbSynchronized() ? "|cff00ff00YES|r" : "|cffff0000NO|r");
            handler->PSendSysMessage("Registry Initialized: %s", sItemScalingRegistry->IsInitialized() ? "|cff00ff00YES|r" : "|cffff0000NO|r");
            handler->PSendSysMessage("In-Memory Scaled Variants: |cff00ff00%u|r", static_cast<uint32>(sItemScalingRegistry->GetIndexedVariantCount()));
            handler->PSendSysMessage("Allocated Synthetic Entries: |cff00ff00%u|r", static_cast<uint32>(sItemScalingRegistry->GetSyntheticEntryCount()));

            auto live = sItemScalingLive->GetDiagnostics();
            handler->PSendSysMessage("Live Scaling: %s | Available slots: %u", live.enabled ? "|cff00ff00Active|r" : "|cffff0000Inactive|r", static_cast<uint32>(live.available));
            handler->PSendSysMessage("Live Variants: Pending=%u Durable=%u Ready=%u | Failures=%u",
                static_cast<uint32>(live.pending), static_cast<uint32>(live.durable), static_cast<uint32>(live.ready), static_cast<uint32>(live.failures));
            handler->SendSysMessage("|cffaaaaaaUsage: .itemscaling preview <itemId> [targetLevel]|r");
        }

        void HandlePreview(ChatHandler* handler, std::string const& rest)
        {
            if (!handler)
            {
                return;
            }

            if (rest.empty())
            {
                handler->SendSysMessage("Usage: .itemscaling preview <itemId> [targetLevel]");
                return;
            }

            uint32 itemId = 0;
            uint32 targetLvl = 60;
            auto linkPos = rest.find("|Hitem:");
            if (linkPos != std::string::npos)
            {
                size_t idStart = linkPos + 7;
                size_t idEnd = rest.find(':', idStart);
                if (idEnd != std::string::npos)
                {
                    itemId = static_cast<uint32>(strtoul(rest.substr(idStart, idEnd - idStart).c_str(), nullptr, 10));
                }
                auto linkClose = rest.find("|h|r", linkPos);
                if (linkClose != std::string::npos)
                {
                    std::string rem = rest.substr(linkClose + 4);
                    char* end = nullptr;
                    uint32 lvl = strtoul(rem.c_str(), &end, 10);
                    if (lvl > 0)
                    {
                        targetLvl = lvl;
                    }
                }
            }
            else
            {
                char const* start = rest.c_str();
                char* end = nullptr;
                itemId = strtoul(start, &end, 10);
                if (end && *end)
                {
                    targetLvl = strtoul(end, nullptr, 10);
                    if (targetLvl == 0)
                    {
                        targetLvl = 60;
                    }
                }
            }
            targetLvl = std::clamp<uint32>(targetLvl, 1, 80);

            ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
            if (!proto)
            {
                handler->PSendSysMessage("|cffff0000Item %u not found.|r", itemId);
                return;
            }

            if (!ItemScalingFormula::IsScalableEquipment(proto))
            {
                handler->PSendSysMessage("|cffff8000Item %u '%s' is not scalable equipment.|r", itemId, proto->Name1.c_str());
                return;
            }

            uint8 origRef = ItemScalingFormula::GetNativeReferenceLevel(proto);
            uint16 targetIlvl = sItemScalingBaseline->CalculateTargetItemLevel(proto, static_cast<uint8>(targetLvl), origRef);
            int8 family = ItemScalingFormula::GetSlotFamily(proto->InventoryType);
            double rBudget = ItemScalingFormula::GetBudgetRatio(proto->ItemLevel, targetIlvl, proto->Quality, family < 0 ? 2 : static_cast<uint8>(family));

            handler->PSendSysMessage("|cff3399ff[ItemScaling Preview]|r Item %u '%s'", itemId, proto->Name1.c_str());
            handler->PSendSysMessage("  Native: ReqLevel %u, ItemLevel %u -> Target: Level %u, ItemLevel %u (Ratio: %.3f)",
                proto->RequiredLevel, proto->ItemLevel, targetLvl, targetIlvl, rBudget);

            if (proto->Armor > 0)
            {
                uint32 scaledArmor = ItemScalingFormula::ScaleArmor(proto, rBudget, origRef, static_cast<uint8>(targetLvl));
                handler->PSendSysMessage("  Armor: %u -> %u", proto->Armor, scaledArmor);
            }

            if (proto->Damage[0].DamageMax > 0.0f)
            {
                ItemPrototype dummy = *proto;
                ItemScalingFormula::ScaleWeaponDamage(proto, dummy, rBudget, origRef, static_cast<uint8>(targetLvl));
                handler->PSendSysMessage("  Damage: %.0f-%.0f -> %.0f-%.0f",
                    proto->Damage[0].DamageMin, proto->Damage[0].DamageMax,
                    dummy.Damage[0].DamageMin, dummy.Damage[0].DamageMax);
            }

            for (uint32 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
            {
                if (proto->ItemStat[i].ItemStatValue != 0)
                {
                    int32 scaledStat = ItemScalingFormula::ScaleAdditiveStat(
                        proto->ItemStat[i].ItemStatValue, rBudget, sItemScalingConfig->PreserveNonZeroStats);
                    handler->PSendSysMessage("  Stat %u: %d -> %d",
                        proto->ItemStat[i].ItemStatType, proto->ItemStat[i].ItemStatValue, scaledStat);
                }
            }

            for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
            {
                if (proto->Spells[i].SpellId != 0 && proto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_EQUIP)
                {
                    uint32 scaledSpell = ItemScalingEquipSpell::FindScaledEquipSpell(proto->Spells[i].SpellId, rBudget);
                    int32 baseVal = ItemScalingEquipSpell::GetEquipSpellBaseValue(proto->Spells[i].SpellId);
                    int32 scaledVal = ItemScalingEquipSpell::GetEquipSpellBaseValue(scaledSpell);
                    EquipSpellFamily family = ItemScalingEquipSpell::GetSpellFamily(proto->Spells[i].SpellId);
                    char const* famName = ItemScalingEquipSpell::GetFamilyName(family);

                    if (scaledSpell != 0 && scaledSpell != proto->Spells[i].SpellId)
                    {
                        handler->PSendSysMessage("  Equip Spell [%s]: %d (Spell %u) -> %d (Spell %u)",
                            famName, baseVal, proto->Spells[i].SpellId, scaledVal, scaledSpell);
                    }
                    else
                    {
                        handler->PSendSysMessage("  Equip Spell [%s]: %d (Spell %u) [Unchanged]",
                            famName, baseVal, proto->Spells[i].SpellId);
                    }
                }
            }
        }

        void HandleGenerate(ChatHandler* handler, std::string const& rest)
        {
            if (!handler)
            {
                return;
            }

            if (rest.empty())
            {
                handler->SendSysMessage("Usage: .itemscaling generate <itemId> [targetLevel]");
                return;
            }

            uint32 itemId = 0;
            uint32 targetLvl = 60;
            char const* start = rest.c_str();
            char* end = nullptr;
            itemId = strtoul(start, &end, 10);
            if (end && *end)
            {
                targetLvl = strtoul(end, nullptr, 10);
                if (targetLvl == 0)
                {
                    targetLvl = 60;
                }
            }
            targetLvl = std::clamp<uint32>(targetLvl, 1, 80);

            ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
            if (!proto)
            {
                handler->PSendSysMessage("|cffff0000Item %u not found.|r", itemId);
                return;
            }

            if (!ItemScalingFormula::IsScalableEquipment(proto))
            {
                handler->PSendSysMessage("|cffff8000Item %u '%s' is not scalable equipment.|r", itemId, proto->Name1.c_str());
                return;
            }

            uint8 origRef = ItemScalingFormula::GetNativeReferenceLevel(proto);
            uint16 targetIlvl = sItemScalingBaseline->CalculateTargetItemLevel(proto, static_cast<uint8>(targetLvl), origRef);

            uint32 variantEntry = sItemScalingRegistry->FindOrRequestVariant(
                proto,
                static_cast<uint8>(targetLvl),
                targetIlvl,
                sItemScalingConfig->FormulaVersion,
                static_cast<uint8>(targetLvl),
                0
            );

            if (variantEntry != 0)
            {
                handler->PSendSysMessage("|cff00ff00[ItemScaling] Successfully generated variant %u for '%s' (Base %u) -> Target Lv %u (Ilvl %u)|r",
                    variantEntry, proto->Name1.c_str(), proto->ItemId, targetLvl, targetIlvl);
                if (handler->GetSession() && handler->GetSession()->GetPlayer())
                {
                    handler->GetSession()->GetPlayer()->AddItem(variantEntry, 1);
                }
            }
            else
            {
                handler->PSendSysMessage("|cffff0000[ItemScaling] Failed to generate variant for '%s' (Base %u) at target level %u|r",
                    proto->Name1.c_str(), proto->ItemId, targetLvl);
            }
        }
    };
}

void AddItemScalingCommandScripts()
{
    new ItemScalingCommandScript();
}
