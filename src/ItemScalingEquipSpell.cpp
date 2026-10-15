/*
 * Copyright (C) 2026+ Turtle WoW Module Contributors
 * mod-twow-item-level-scaling
 */

#include "ItemScalingEquipSpell.h"
#include "ItemScalingCommon.h"
#include "ItemScalingConfig.h"
#include "ItemScalingFormula.h"
#include "Spells/SpellMgr.h"
#include "Database/DBCStructure.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{
    // Canonical fallback ladders extracted from Turtle WoW client/server Spell.dbc.
    // Each pair is {Magnitude, SpellID}.
    static const std::pair<int32, uint32> s_FAMILY_MANA_REGEN[] = {
        {1, 21359u}, {2, 21360u}, {3, 21361u}, {4, 21362u}, {5, 21363u},
        {6, 18379u}, {7, 21364u}, {8, 18378u}, {9, 21365u}, {10, 20959u},
        {11, 21366u}, {12, 21635u}, {13, 21638u}, {14, 21640u}, {15, 21643u}, {16, 23212u}
    };

    static const std::pair<int32, uint32> s_FAMILY_ARMOR_PENETRATION[] = {
        {3, 48022u}, {5, 48023u}, {7, 48024u}, {9, 48025u}, {10, 48026u},
        {12, 48027u}, {15, 48028u}, {20, 48029u}, {25, 48030u}, {30, 48031u},
        {35, 48032u}, {40, 48033u}, {50, 48034u}, {75, 48035u}, {100, 48036u},
        {150, 48037u}, {200, 48038u}, {250, 48039u}
    };

    static const std::pair<int32, uint32> s_FAMILY_SPELL_DAMAGE[] = {
        {1, 12019u}, {2, 9393u}, {3, 46100u}, {4, 9394u}, {5, 9395u}, {6, 9396u},
        {7, 9397u}, {8, 25109u}, {9, 9415u}, {11, 9416u}, {12, 9417u}, {13, 9342u},
        {14, 9343u}, {15, 9344u}, {16, 25110u}, {17, 58191u}, {18, 9346u}, {19, 14254u},
        {20, 14799u}, {21, 14248u}, {22, 15714u}, {23, 30780u}, {24, 25111u}, {25, 15715u},
        {26, 18049u}, {27, 14054u}, {28, 14127u}, {29, 13881u}, {30, 22747u}, {31, 58192u},
        {32, 17367u}, {33, 18050u}, {34, 18052u}, {35, 14055u}, {36, 25113u}, {37, 18054u},
        {38, 58193u}, {39, 18055u}, {40, 18056u}, {41, 18057u}, {42, 18058u}, {43, 17280u},
        {44, 17493u}, {45, 58194u}, {46, 28264u}, {47, 24196u}, {48, 58195u}, {49, 28360u},
        {50, 58196u}, {51, 28767u}, {52, 58197u}, {53, 26142u}, {54, 58198u}, {55, 58199u},
        {56, 23732u}, {57, 23213u}, {58, 58200u}, {59, 26155u}, {60, 26158u}, {61, 58201u},
        {62, 58202u}, {63, 58225u}, {64, 23730u}, {65, 58203u}, {66, 58204u}, {67, 58205u},
        {68, 26227u}, {69, 58206u}, {70, 58207u}, {71, 23929u}, {72, 26395u}, {73, 58208u},
        {74, 58209u}, {75, 58210u}, {76, 26460u}, {77, 58211u}, {78, 58212u}, {79, 58213u},
        {80, 26459u}, {81, 58214u}, {82, 58215u}, {83, 58216u}, {84, 23728u}, {85, 28687u},
        {86, 58217u}, {87, 58218u}, {88, 58219u}, {89, 58220u}, {90, 58221u}, {91, 51824u},
        {92, 58222u}, {93, 58223u}, {94, 58224u}, {95, 28693u}, {113, 28841u}, {126, 28792u},
        {128, 51067u}, {132, 28530u}, {140, 51065u}, {150, 28141u}, {180, 51066u}, {840, 24243u}
    };

    static const std::pair<int32, uint32> s_FAMILY_HEALING[] = {
        {2, 7675u}, {4, 7676u}, {7, 7677u}, {8, 41650u}, {9, 7678u}, {11, 7679u},
        {13, 7680u}, {15, 7681u}, {18, 9406u}, {20, 9407u}, {22, 9408u}, {24, 23796u},
        {26, 9315u}, {29, 9316u}, {30, 25067u}, {31, 9317u}, {33, 9318u}, {35, 18029u},
        {37, 18030u}, {40, 18031u}, {42, 18032u}, {44, 17371u}, {46, 18033u}, {48, 18034u},
        {51, 18035u}, {53, 15696u}, {55, 22748u}, {57, 18037u}, {59, 18038u}, {62, 18039u},
        {64, 18040u}, {66, 18041u}, {68, 18042u}, {70, 18043u}, {73, 18044u}, {75, 18045u},
        {77, 18046u}, {79, 18047u}, {81, 18048u}, {84, 17320u}, {90, 26154u}, {92, 23593u},
        {106, 26225u}, {110, 26690u}, {121, 26228u}, {134, 29369u}, {143, 26461u},
        {161, 28686u}, {178, 28736u}, {187, 28150u}, {238, 28805u}, {280, 28151u}, {300, 28152u}
    };

    static const std::pair<int32, uint32> s_FAMILY_ATTACK_POWER[] = {
        {2, 9136u}, {4, 9137u}, {6, 9138u}, {8, 9139u}, {10, 9140u}, {12, 9141u},
        {14, 9142u}, {15, 20732u}, {16, 9329u}, {18, 9330u}, {20, 9331u}, {22, 9332u},
        {24, 14027u}, {26, 9334u}, {28, 9335u}, {30, 9336u}, {32, 15807u}, {34, 15806u},
        {36, 14089u}, {38, 15808u}, {40, 14049u}, {42, 15809u}, {44, 15810u}, {46, 15811u},
        {48, 9333u}, {50, 14056u}, {52, 15812u}, {54, 15813u}, {56, 15814u}, {58, 15815u},
        {60, 14052u}, {62, 15816u}, {64, 15817u}, {66, 15818u}, {68, 15819u}, {70, 15820u},
        {72, 15821u}, {74, 15823u}, {76, 15824u}, {78, 15825u}, {80, 15826u}, {82, 15827u},
        {84, 15828u}, {86, 15829u}, {88, 15830u}, {90, 15831u}, {92, 15832u}, {98, 28840u},
        {100, 28735u}, {112, 45435u}, {126, 45436u}, {140, 26153u}, {154, 24994u},
        {168, 45437u}, {182, 45438u}, {196, 45439u}, {200, 18060u}, {210, 45440u},
        {224, 45441u}, {238, 45442u}, {252, 45443u}, {266, 45444u}, {280, 24697u},
        {294, 45446u}, {305, 28717u}, {308, 45447u}, {322, 45448u}, {336, 51131u},
        {350, 51132u}, {364, 51133u}, {378, 51134u}, {392, 51135u}, {406, 51136u},
        {434, 51137u}, {448, 51064u}, {462, 51138u}, {476, 51139u}, {490, 51140u},
        {504, 51141u}, {574, 24694u}, {600, 28154u}
    };

    static const std::pair<int32, uint32> s_FAMILY_RANGED_ATTACK_POWER[] = {
        {2, 21426u}, {5, 21427u}, {7, 21428u}, {10, 21429u}, {12, 21430u}, {14, 21431u},
        {17, 21432u}, {19, 21433u}, {22, 21434u}, {24, 21435u}, {26, 21436u}, {29, 21437u},
        {31, 21438u}, {34, 21439u}, {36, 21440u}, {38, 21441u}, {41, 21442u}, {43, 21443u},
        {46, 21444u}, {48, 21445u}, {50, 21446u}, {53, 21447u}, {55, 21448u}, {58, 21449u},
        {60, 21013u}, {62, 21450u}, {65, 21451u}, {67, 21452u}, {70, 21453u}, {72, 21454u},
        {74, 21455u}, {77, 21456u}, {79, 21457u}, {82, 21458u}, {84, 21459u}, {86, 21460u},
        {89, 21461u}, {91, 21462u}
    };

    static const std::pair<int32, uint32> s_FAMILY_HEALTH_REGEN[] = {
        {1, 21587u}, {2, 21345u}, {3, 21592u}, {4, 21347u}, {5, 21596u}, {6, 21348u},
        {7, 21600u}, {8, 20885u}, {9, 21604u}, {10, 21349u}, {11, 21608u}, {12, 21109u},
        {13, 21612u}, {14, 21350u}, {15, 21616u}, {16, 23210u}
    };

    static const std::pair<int32, uint32> s_FAMILY_DEFENSE[] = {
        {1, 7511u}, {2, 7514u}, {3, 7515u}, {4, 7517u}, {5, 7518u}, {6, 13384u},
        {7, 13385u}, {8, 13387u}, {9, 13388u}, {10, 13390u}, {11, 18185u}, {12, 21408u},
        {13, 14249u}, {14, 21410u}, {15, 18196u}, {16, 21412u}, {17, 13389u}, {18, 21413u},
        {19, 21414u}, {20, 21416u}, {21, 17513u}, {22, 21418u}, {23, 21419u}, {24, 21421u},
        {25, 21422u}, {67, 15804u}, {80, 24775u}, {120, 24774u}
    };

    static const std::pair<int32, uint32> s_FAMILY_BLOCK_VALUE[] = {
        {1, 21539u}, {10, 23172u}, {12, 25036u}, {14, 23515u}, {15, 23203u}, {18, 22912u},
        {19, 23731u}, {21, 28112u}, {23, 22852u}, {24, 23181u}, {27, 23516u}, {30, 23562u},
        {32, 28325u}
    };

    static const std::pair<int32, uint32> s_FAMILY_HOLY_DAMAGE[] = {
        {1, 21499u}, {3, 21500u}, {4, 21501u}, {6, 21502u}, {7, 21503u}, {9, 21504u},
        {10, 21505u}, {11, 21506u}, {13, 21507u}, {14, 21508u}, {16, 21509u}, {17, 21510u},
        {19, 21511u}, {20, 21512u}, {21, 21513u}, {23, 21514u}, {24, 21515u}, {26, 21516u},
        {27, 21517u}, {29, 21518u}, {30, 21519u}, {31, 21520u}, {33, 21521u}, {34, 21522u},
        {36, 21523u}, {37, 21524u}, {39, 21525u}, {40, 21526u}, {41, 21527u}, {43, 21528u},
        {44, 21529u}, {46, 21530u}, {47, 21531u}, {49, 21532u}, {50, 21533u}, {51, 21534u},
        {53, 21535u}, {54, 21536u}, {69, 56529u}
    };

    static const std::pair<int32, uint32> s_FAMILY_FIRE_DAMAGE[] = {
        {1, 7683u}, {3, 7684u}, {4, 7685u}, {6, 7686u}, {7, 7687u}, {9, 7688u},
        {10, 7689u}, {11, 9399u}, {13, 9400u}, {14, 9401u}, {16, 9294u}, {17, 9295u},
        {19, 9296u}, {20, 9297u}, {21, 9298u}, {23, 17747u}, {24, 17866u}, {26, 17867u},
        {27, 17868u}, {29, 13830u}, {30, 17869u}, {31, 17870u}, {33, 17871u}, {34, 17872u},
        {36, 17873u}, {37, 17874u}, {39, 17684u}, {40, 17875u}, {41, 17876u}, {43, 17878u},
        {44, 17879u}, {46, 17880u}, {47, 17881u}, {49, 17882u}, {50, 17884u}, {51, 17885u},
        {53, 17886u}, {54, 17887u}, {57, 26405u}, {59, 26694u}, {60, 26695u}, {61, 26696u},
        {63, 26697u}, {64, 26698u}, {66, 26699u}, {67, 26700u}, {69, 26701u}, {70, 26702u}, {72, 26703u}
    };

    static const std::pair<int32, uint32> s_FAMILY_NATURE_DAMAGE[] = {
        {1, 7690u}, {3, 7691u}, {4, 7692u}, {6, 7693u}, {7, 7694u}, {9, 7695u},
        {10, 7696u}, {11, 9409u}, {14, 9411u}, {16, 9357u}, {17, 9358u}, {19, 9359u},
        {20, 9360u}, {21, 9361u}, {23, 17987u}, {24, 17988u}, {26, 17989u}, {27, 17990u},
        {29, 17819u}, {30, 17991u}, {33, 17993u}, {34, 16638u}, {36, 17994u}, {37, 17995u},
        {39, 17996u}, {40, 17997u}, {41, 17998u}, {43, 17999u}, {44, 18000u}, {46, 18001u},
        {47, 18002u}, {49, 18003u}, {50, 18004u}, {51, 18005u}, {53, 18006u}, {54, 18007u}
    };

    static const std::pair<int32, uint32> s_FAMILY_FROST_DAMAGE[] = {
        {1, 7697u}, {3, 7698u}, {4, 7699u}, {6, 7700u}, {7, 7701u}, {9, 7702u},
        {10, 7703u}, {11, 9402u}, {13, 9403u}, {14, 9404u}, {16, 9304u}, {17, 9305u},
        {19, 9306u}, {20, 9307u}, {21, 9308u}, {23, 17889u}, {24, 17890u}, {26, 17891u},
        {27, 17892u}, {29, 13831u}, {30, 17893u}, {31, 17894u}, {33, 17895u}, {34, 17896u},
        {36, 17897u}, {37, 17898u}, {39, 17899u}, {40, 17900u}, {41, 17901u}, {43, 17902u},
        {44, 17903u}, {46, 17905u}, {47, 17906u}, {49, 17907u}, {50, 17908u}, {51, 17909u},
        {54, 17911u}, {56, 17910u}, {57, 26717u}, {59, 26718u}, {60, 26719u}, {61, 26720u},
        {63, 26721u}, {64, 26722u}, {66, 26723u}, {67, 26724u}, {69, 26725u}, {70, 26726u},
        {72, 24667u}, {132, 24666u}
    };

    static const std::pair<int32, uint32> s_FAMILY_SHADOW_DAMAGE[] = {
        {1, 7704u}, {3, 7705u}, {4, 7706u}, {6, 7707u}, {7, 7708u}, {9, 7710u},
        {10, 7709u}, {11, 9412u}, {13, 9413u}, {14, 9414u}, {16, 9324u}, {17, 9325u},
        {19, 9326u}, {20, 9327u}, {21, 9328u}, {23, 14793u}, {24, 14794u}, {26, 18008u},
        {27, 18009u}, {29, 18010u}, {30, 18011u}, {31, 18012u}, {33, 18013u}, {34, 18014u},
        {36, 18015u}, {37, 18016u}, {39, 18017u}, {40, 18018u}, {41, 18019u}, {43, 18020u},
        {44, 18021u}, {46, 18022u}, {47, 18023u}, {49, 18024u}, {50, 18025u}, {51, 18026u},
        {53, 18027u}, {54, 18028u}, {56, 26728u}, {57, 26729u}, {59, 26730u}, {60, 26731u},
        {61, 26732u}, {63, 26733u}, {64, 23594u}, {66, 26734u}, {67, 26735u}, {69, 23265u},
        {70, 26736u}, {72, 26737u}
    };

    static const std::pair<int32, uint32> s_FAMILY_ARCANE_DAMAGE[] = {
        {1, 13590u}, {3, 13591u}, {4, 13592u}, {6, 13593u}, {7, 13594u}, {9, 13595u},
        {10, 13596u}, {11, 13597u}, {13, 13598u}, {14, 13599u}, {16, 13601u}, {17, 13602u},
        {19, 13603u}, {20, 13604u}, {21, 13605u}, {23, 17821u}, {26, 17823u}, {27, 17824u},
        {29, 17825u}, {30, 17826u}, {31, 17827u}, {33, 17828u}, {34, 17829u}, {36, 17830u},
        {37, 17832u}, {39, 17837u}, {40, 17838u}, {41, 17839u}, {43, 17840u}, {44, 17841u},
        {46, 17842u}, {47, 17844u}, {49, 17845u}, {50, 17846u}, {51, 17847u}, {53, 17848u},
        {56, 26704u}, {57, 26705u}, {58, 17849u}, {59, 26706u}, {60, 26707u}, {61, 26708u},
        {63, 26709u}, {64, 26710u}, {66, 26711u}, {67, 26712u}, {69, 26713u}, {70, 26714u}, {72, 26715u}
    };

    struct EquipLadderStore
    {
        std::mutex mutex;
        bool initialized{false};
        std::vector<std::pair<int32, uint32>> ladders[EQUIP_SPELL_FAMILY_COUNT];
        std::unordered_map<uint32, EquipSpellFamily> spellToFamily;
        std::unordered_map<uint32, int32> spellToValue;

        template <std::size_t N>
        void LoadCanonical(EquipSpellFamily family, std::pair<int32, uint32> const (&table)[N])
        {
            auto& ladder = ladders[family];
            ladder.reserve(ladder.size() + N);
            for (std::size_t i = 0; i < N; ++i)
            {
                ladder.emplace_back(table[i]);
                spellToFamily[table[i].second] = family;
                spellToValue[table[i].second] = table[i].first;
            }
        }
    };

    static EquipLadderStore s_store;

    static void EnsureCanonicalLoaded(EquipLadderStore& store)
    {
        if (store.initialized)
            return;

        store.LoadCanonical(EQUIP_SPELL_FAMILY_MANA_REGEN, s_FAMILY_MANA_REGEN);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_ARMOR_PENETRATION, s_FAMILY_ARMOR_PENETRATION);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_SPELL_DAMAGE, s_FAMILY_SPELL_DAMAGE);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_HEALING, s_FAMILY_HEALING);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_ATTACK_POWER, s_FAMILY_ATTACK_POWER);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_RANGED_ATTACK_POWER, s_FAMILY_RANGED_ATTACK_POWER);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_HEALTH_REGEN, s_FAMILY_HEALTH_REGEN);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_DEFENSE, s_FAMILY_DEFENSE);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_BLOCK_VALUE, s_FAMILY_BLOCK_VALUE);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_HOLY_DAMAGE, s_FAMILY_HOLY_DAMAGE);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_FIRE_DAMAGE, s_FAMILY_FIRE_DAMAGE);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_NATURE_DAMAGE, s_FAMILY_NATURE_DAMAGE);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_FROST_DAMAGE, s_FAMILY_FROST_DAMAGE);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_SHADOW_DAMAGE, s_FAMILY_SHADOW_DAMAGE);
        store.LoadCanonical(EQUIP_SPELL_FAMILY_ARCANE_DAMAGE, s_FAMILY_ARCANE_DAMAGE);

        for (uint32 f = 1; f < EQUIP_SPELL_FAMILY_COUNT; ++f)
        {
            auto& ladder = store.ladders[f];
            std::sort(ladder.begin(), ladder.end(),
                [](std::pair<int32, uint32> const& a, std::pair<int32, uint32> const& b)
                {
                    if (a.first != b.first)
                        return a.first < b.first;
                    return a.second < b.second;
                });
            ladder.erase(std::unique(ladder.begin(), ladder.end(),
                [](std::pair<int32, uint32> const& a, std::pair<int32, uint32> const& b)
                {
                    return a.first == b.first;
                }), ladder.end());
        }

        store.initialized = true;
    }
}

namespace ItemScalingEquipSpell
{
    void Initialize()
    {
        std::lock_guard<std::mutex> lock(s_store.mutex);
        EnsureCanonicalLoaded(s_store);

        // Scan sSpellMgr for runtime custom / discovered spells
        uint32 maxSpellId = sSpellMgr.GetMaxSpellId();
        uint32 discovered = 0;

        for (uint32 id = 1; id < maxSpellId; ++id)
        {
            SpellEntry const* spell = sSpellMgr.GetSpellEntry(id);
            if (!spell)
                continue;

            // Permanent / passive equip spells only
            if (spell->DurationIndex != 0 && spell->DurationIndex != 21)
                continue;

            EquipSpellFamily family = EQUIP_SPELL_FAMILY_NONE;
            uint32 aura = spell->EffectApplyAuraName[0];
            int32 misc = spell->EffectMiscValue[0];

            switch (aura)
            {
                case SPELL_AURA_MOD_DAMAGE_DONE: // 13
                {
                    switch (misc)
                    {
                        case 126: family = EQUIP_SPELL_FAMILY_SPELL_DAMAGE; break;
                        case 2:   family = EQUIP_SPELL_FAMILY_HOLY_DAMAGE; break;
                        case 4:   family = EQUIP_SPELL_FAMILY_FIRE_DAMAGE; break;
                        case 8:   family = EQUIP_SPELL_FAMILY_NATURE_DAMAGE; break;
                        case 16:  family = EQUIP_SPELL_FAMILY_FROST_DAMAGE; break;
                        case 32:  family = EQUIP_SPELL_FAMILY_SHADOW_DAMAGE; break;
                        case 64:  family = EQUIP_SPELL_FAMILY_ARCANE_DAMAGE; break;
                        default:  break;
                    }
                    break;
                }
                case SPELL_AURA_MOD_HEALING_DONE: // 135
                    family = EQUIP_SPELL_FAMILY_HEALING;
                    break;
                case SPELL_AURA_MOD_POWER_REGEN: // 85
                    if (misc == 0)
                        family = EQUIP_SPELL_FAMILY_MANA_REGEN;
                    break;
                case SPELL_AURA_MOD_TARGET_RESISTANCE: // 123
                    if (misc == 1)
                        family = EQUIP_SPELL_FAMILY_ARMOR_PENETRATION;
                    else if (misc == 126 || misc == 2 || misc == 4 || misc == 8 || misc == 16 || misc == 32 || misc == 64)
                        family = EQUIP_SPELL_FAMILY_SPELL_PENETRATION;
                    break;
                case SPELL_AURA_MOD_ATTACK_POWER: // 99
                    if (misc == 0)
                        family = EQUIP_SPELL_FAMILY_ATTACK_POWER;
                    break;
                case SPELL_AURA_MOD_RANGED_ATTACK_POWER: // 124
                    if (misc == 0)
                        family = EQUIP_SPELL_FAMILY_RANGED_ATTACK_POWER;
                    break;
                case SPELL_AURA_MOD_HEALTH_REGEN_PERCENT: // 161
                    if (misc == 0)
                        family = EQUIP_SPELL_FAMILY_HEALTH_REGEN;
                    break;
                case SPELL_AURA_MOD_SKILL: // 30
                    if (misc == 95) // SKILL_DEFENSE
                        family = EQUIP_SPELL_FAMILY_DEFENSE;
                    break;
                case SPELL_AURA_MOD_SHIELD_BLOCKVALUE: // 158
                    if (misc == 0)
                        family = EQUIP_SPELL_FAMILY_BLOCK_VALUE;
                    break;
                default:
                    break;
            }

            if (family == EQUIP_SPELL_FAMILY_NONE)
                continue;

            int32 val = std::abs(spell->EffectBasePoints[0] + 1);
            if (val <= 0)
                continue;

            std::string const& sname = spell->SpellName[0];

            // Filter out non-item spells: must contain expected keywords
            bool legitimate = false;
            switch (family)
            {
                case EQUIP_SPELL_FAMILY_SPELL_DAMAGE:
                    legitimate = (sname.find("Spell Dam") != std::string::npos || sname.find("Spell Damage") != std::string::npos);
                    break;
                case EQUIP_SPELL_FAMILY_HEALING:
                    legitimate = (sname.find("Healing") != std::string::npos);
                    break;
                case EQUIP_SPELL_FAMILY_MANA_REGEN:
                    legitimate = (sname.find("Mana Regen") != std::string::npos);
                    break;
                case EQUIP_SPELL_FAMILY_ARMOR_PENETRATION:
                    legitimate = (sname.find("Armor Penetration") != std::string::npos);
                    break;
                case EQUIP_SPELL_FAMILY_ATTACK_POWER:
                    legitimate = (sname.find("Attack Power") != std::string::npos);
                    break;
                case EQUIP_SPELL_FAMILY_RANGED_ATTACK_POWER:
                    legitimate = (sname.find("Attack Power Ranged") != std::string::npos || sname.find("Ranged Attack Power") != std::string::npos);
                    break;
                case EQUIP_SPELL_FAMILY_HEALTH_REGEN:
                    legitimate = (sname.find("Vitality") != std::string::npos || sname.find("Health Regen") != std::string::npos);
                    break;
                case EQUIP_SPELL_FAMILY_DEFENSE:
                    legitimate = (sname.find("Defense") != std::string::npos);
                    break;
                case EQUIP_SPELL_FAMILY_BLOCK_VALUE:
                    legitimate = (sname.find("Block Value") != std::string::npos);
                    break;
                case EQUIP_SPELL_FAMILY_HOLY_DAMAGE:
                case EQUIP_SPELL_FAMILY_FIRE_DAMAGE:
                case EQUIP_SPELL_FAMILY_NATURE_DAMAGE:
                case EQUIP_SPELL_FAMILY_FROST_DAMAGE:
                case EQUIP_SPELL_FAMILY_SHADOW_DAMAGE:
                case EQUIP_SPELL_FAMILY_ARCANE_DAMAGE:
                    legitimate = (sname.find("Dam ") != std::string::npos || sname.find("Damage") != std::string::npos);
                    break;
                default:
                    legitimate = true;
                    break;
            }

            if (!legitimate)
                continue;

            s_store.ladders[family].emplace_back(val, id);
            s_store.spellToFamily[id] = family;
            s_store.spellToValue[id] = val;
            ++discovered;
        }

        // Sort and deduplicate all family ladders
        for (uint32 f = 1; f < EQUIP_SPELL_FAMILY_COUNT; ++f)
        {
            auto& ladder = s_store.ladders[f];
            std::sort(ladder.begin(), ladder.end(),
                [](std::pair<int32, uint32> const& a, std::pair<int32, uint32> const& b)
                {
                    if (a.first != b.first)
                        return a.first < b.first;
                    return a.second < b.second;
                });
            ladder.erase(std::unique(ladder.begin(), ladder.end(),
                [](std::pair<int32, uint32> const& a, std::pair<int32, uint32> const& b)
                {
                    return a.first == b.first;
                }), ladder.end());
        }

        sLog.outString("[ItemScaling] Initialized equip spell scaling ladders (%u dynamic spells discovered across %u families).",
            discovered, static_cast<uint32>(EQUIP_SPELL_FAMILY_COUNT - 1));
    }

    bool IsScalableEquipSpell(uint32 spellId)
    {
        if (spellId == 0)
            return false;

        return GetSpellFamily(spellId) != EQUIP_SPELL_FAMILY_NONE;
    }

    EquipSpellFamily GetSpellFamily(uint32 spellId)
    {
        if (spellId == 0)
            return EQUIP_SPELL_FAMILY_NONE;

        {
            std::lock_guard<std::mutex> lock(s_store.mutex);
            EnsureCanonicalLoaded(s_store);
            auto it = s_store.spellToFamily.find(spellId);
            if (it != s_store.spellToFamily.end())
                return it->second;
        }

        SpellEntry const* spell = sSpellMgr.GetSpellEntry(spellId);
        if (!spell)
            return EQUIP_SPELL_FAMILY_NONE;

        if (spell->DurationIndex != 0 && spell->DurationIndex != 21)
            return EQUIP_SPELL_FAMILY_NONE;

        uint32 aura = spell->EffectApplyAuraName[0];
        int32 misc = spell->EffectMiscValue[0];

        switch (aura)
        {
            case SPELL_AURA_MOD_DAMAGE_DONE:
                switch (misc)
                {
                    case 126: return EQUIP_SPELL_FAMILY_SPELL_DAMAGE;
                    case 2:   return EQUIP_SPELL_FAMILY_HOLY_DAMAGE;
                    case 4:   return EQUIP_SPELL_FAMILY_FIRE_DAMAGE;
                    case 8:   return EQUIP_SPELL_FAMILY_NATURE_DAMAGE;
                    case 16:  return EQUIP_SPELL_FAMILY_FROST_DAMAGE;
                    case 32:  return EQUIP_SPELL_FAMILY_SHADOW_DAMAGE;
                    case 64:  return EQUIP_SPELL_FAMILY_ARCANE_DAMAGE;
                    default:  return EQUIP_SPELL_FAMILY_NONE;
                }
            case SPELL_AURA_MOD_HEALING_DONE:
                return EQUIP_SPELL_FAMILY_HEALING;
            case SPELL_AURA_MOD_POWER_REGEN:
                return (misc == 0) ? EQUIP_SPELL_FAMILY_MANA_REGEN : EQUIP_SPELL_FAMILY_NONE;
            case SPELL_AURA_MOD_TARGET_RESISTANCE:
                if (misc == 1)
                    return EQUIP_SPELL_FAMILY_ARMOR_PENETRATION;
                if (misc == 126 || misc == 2 || misc == 4 || misc == 8 || misc == 16 || misc == 32 || misc == 64)
                    return EQUIP_SPELL_FAMILY_SPELL_PENETRATION;
                return EQUIP_SPELL_FAMILY_NONE;
            case SPELL_AURA_MOD_ATTACK_POWER:
                return (misc == 0) ? EQUIP_SPELL_FAMILY_ATTACK_POWER : EQUIP_SPELL_FAMILY_NONE;
            case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
                return (misc == 0) ? EQUIP_SPELL_FAMILY_RANGED_ATTACK_POWER : EQUIP_SPELL_FAMILY_NONE;
            case SPELL_AURA_MOD_HEALTH_REGEN_PERCENT:
                return (misc == 0) ? EQUIP_SPELL_FAMILY_HEALTH_REGEN : EQUIP_SPELL_FAMILY_NONE;
            case SPELL_AURA_MOD_SKILL:
                return (misc == 95) ? EQUIP_SPELL_FAMILY_DEFENSE : EQUIP_SPELL_FAMILY_NONE;
            case SPELL_AURA_MOD_SHIELD_BLOCKVALUE:
                return (misc == 0) ? EQUIP_SPELL_FAMILY_BLOCK_VALUE : EQUIP_SPELL_FAMILY_NONE;
            default:
                return EQUIP_SPELL_FAMILY_NONE;
        }
    }

    int32 GetEquipSpellBaseValue(uint32 spellId)
    {
        if (spellId == 0)
            return 0;

        {
            std::lock_guard<std::mutex> lock(s_store.mutex);
            EnsureCanonicalLoaded(s_store);
            auto it = s_store.spellToValue.find(spellId);
            if (it != s_store.spellToValue.end())
                return it->second;
        }

        SpellEntry const* spell = sSpellMgr.GetSpellEntry(spellId);
        if (!spell)
            return 0;

        return std::abs(spell->EffectBasePoints[0] + 1);
    }

    char const* GetFamilyName(EquipSpellFamily family)
    {
        switch (family)
        {
            case EQUIP_SPELL_FAMILY_SPELL_DAMAGE:         return "Spell Damage & Healing";
            case EQUIP_SPELL_FAMILY_HEALING:              return "Healing Done";
            case EQUIP_SPELL_FAMILY_MANA_REGEN:           return "Mana Regen (MP5)";
            case EQUIP_SPELL_FAMILY_ARMOR_PENETRATION:    return "Armor Penetration";
            case EQUIP_SPELL_FAMILY_ATTACK_POWER:         return "Attack Power";
            case EQUIP_SPELL_FAMILY_RANGED_ATTACK_POWER:  return "Ranged Attack Power";
            case EQUIP_SPELL_FAMILY_HEALTH_REGEN:         return "Health Regen (HP5)";
            case EQUIP_SPELL_FAMILY_DEFENSE:              return "Defense Rating";
            case EQUIP_SPELL_FAMILY_BLOCK_VALUE:          return "Shield Block Value";
            case EQUIP_SPELL_FAMILY_HOLY_DAMAGE:          return "Holy Spell Damage";
            case EQUIP_SPELL_FAMILY_FIRE_DAMAGE:          return "Fire Spell Damage";
            case EQUIP_SPELL_FAMILY_NATURE_DAMAGE:        return "Nature Spell Damage";
            case EQUIP_SPELL_FAMILY_FROST_DAMAGE:         return "Frost Spell Damage";
            case EQUIP_SPELL_FAMILY_SHADOW_DAMAGE:        return "Shadow Spell Damage";
            case EQUIP_SPELL_FAMILY_ARCANE_DAMAGE:        return "Arcane Spell Damage";
            case EQUIP_SPELL_FAMILY_SPELL_PENETRATION:    return "Spell Penetration";
            default:                                      return "None";
        }
    }

    uint32 FindScaledEquipSpell(uint32 baseSpellId, double rBudget)
    {
        if (baseSpellId == 0 || rBudget <= 0.0)
            return baseSpellId;

        EquipSpellFamily family = GetSpellFamily(baseSpellId);
        if (family == EQUIP_SPELL_FAMILY_NONE)
            return baseSpellId;

        int32 baseVal = GetEquipSpellBaseValue(baseSpellId);
        if (baseVal <= 0)
            return baseSpellId;

        int32 targetVal = ItemScalingFormula::ScaleAdditiveStat(baseVal, rBudget, sItemScalingConfig->PreserveNonZeroStats);
        if (targetVal == baseVal)
            return baseSpellId;

        std::lock_guard<std::mutex> lock(s_store.mutex);
        EnsureCanonicalLoaded(s_store);

        auto const& ladder = s_store.ladders[family];
        if (ladder.empty())
            return baseSpellId;

        if (targetVal <= ladder.front().first)
            return ladder.front().second;

        if (targetVal >= ladder.back().first)
            return ladder.back().second;

        auto it = std::lower_bound(ladder.begin(), ladder.end(), targetVal,
            [](std::pair<int32, uint32> const& a, int32 val)
            {
                return a.first < val;
            });

        if (it == ladder.begin())
            return it->second;
        if (it == ladder.end())
            return ladder.back().second;

        auto prev = it - 1;
        int32 diffCur = std::abs(it->first - targetVal);
        int32 diffPrev = std::abs(prev->first - targetVal);

        return (diffCur < diffPrev) ? it->second : prev->second;
    }

    void ScalePrototypeEquipSpells(ItemPrototype const* baseProto, ItemPrototype& scaledProto, double rBudget)
    {
        if (!baseProto || rBudget <= 0.0)
            return;

        for (uint32 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            uint32 baseSpellId = baseProto->Spells[i].SpellId;
            if (baseSpellId == 0)
                continue;

            if (baseProto->Spells[i].SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
                continue;

            uint32 scaledSpellId = FindScaledEquipSpell(baseSpellId, rBudget);
            if (scaledSpellId != 0)
            {
                scaledProto.Spells[i].SpellId = scaledSpellId;
            }
        }
    }
}
