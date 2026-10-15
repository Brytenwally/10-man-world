/*
 * 10 Man World - released under the MIT License, see LICENSE.
 */

/*
 * 10 Man World - Creature modifiers
 *
 * Multiplies the health and damage of creatures by where they live (open world or five-player dungeon) and by
 * their rank (normal, or rare/elite/boss). Health is applied whenever a creature's level is selected (spawn,
 * respawn, entry change); damage is applied to the damage a hostile creature deals to players and their pets,
 * never to its heals.
 * Raids, battlegrounds, arenas and scripted private instances are left alone.
 */

#include "AllCreatureScript.h"
#include "Config.h"
#include "Creature.h"
#include "Log.h"
#include "Map.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "UnitScript.h"
#include "WorldScript.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <string>

namespace
{
    enum class Category : uint8
    {
        WorldNormal,
        WorldElite,
        DungeonNormal,
        DungeonElite,
        Count,
        None = Count
    };

    struct CategoryDefaults
    {
        char const* configName;
        float health;
        float damage;
    };

    constexpr std::array<CategoryDefaults, size_t(Category::Count)> DEFAULTS =
    { {
        { "World.Normal", 4.0f, 3.0f },
        { "World.Elite", 2.5f, 2.0f },
        { "Dungeon.Normal", 2.5f, 2.0f },
        { "Dungeon.Elite", 2.0f, 1.5f },
    } };

    constexpr char CONFIG_PREFIX[] = "TenManWorld.CreatureModifiers.";

    std::atomic<bool> g_enabled = true;
    std::array<std::atomic<float>, size_t(Category::Count)> g_health;
    std::array<std::atomic<float>, size_t(Category::Count)> g_damage;

    bool IsTrainingDummy(Creature const* creature)
    {
        std::string const& script = creature->GetScriptName();
        return script == "npc_training_dummy" || script == "npc_target_dummy";
    }

    bool IsEligible(Creature const* creature)
    {
        return creature && !creature->IsPet() && !creature->IsTotem() && !creature->IsTrigger()
            && !creature->IsCritter() && !creature->GetCharmerOrOwnerPlayerOrPlayerItself()
            && !IsTrainingDummy(creature);
    }

    Category CategoryOf(Creature const* creature)
    {
        if (!g_enabled || !IsEligible(creature))
            return Category::None;

        Map const* map = creature->FindMap();
        if (!map || map->IsBattlegroundOrArena() || map->IsRaid() || map->IsScriptedPrivateInstance())
            return Category::None;

        bool const elite = creature->GetCreatureTemplate()->rank != CREATURE_ELITE_NORMAL;
        if (map->IsDungeon())
            return elite || creature->IsDungeonBoss() ? Category::DungeonElite : Category::DungeonNormal;

        return elite ? Category::WorldElite : Category::WorldNormal;
    }

    uint32 Scaled(uint32 value, float multiplier)
    {
        if (multiplier <= 0.0f || value == 0)
            return value;

        double const scaled = double(value) * multiplier + 0.5;
        if (scaled >= double(std::numeric_limits<uint32>::max()))
            return std::numeric_limits<uint32>::max();

        return scaled < 1.0 ? 1 : uint32(scaled);
    }

    float DamageMultiplier(Unit const* target, Unit const* attacker)
    {
        if (!target || !attacker || !target->GetCharmerOrOwnerPlayerOrPlayerItself())
            return 1.0f;

        Creature const* creature = attacker->ToCreature();
        Category const category = CategoryOf(creature);
        if (category == Category::None || !creature->IsHostileTo(target))
            return 1.0f;

        return g_damage[size_t(category)].load(std::memory_order_relaxed);
    }

    template <typename T>
    void ScaleDamage(T& damage, Unit const* target, Unit const* attacker)
    {
        if (damage <= 0)
            return;

        if (float const multiplier = DamageMultiplier(target, attacker); multiplier != 1.0f)
            damage = T(Scaled(uint32(damage), multiplier));
    }
}

class TenManWorldCreatureModifiersWorldScript : public WorldScript
{
public:
    TenManWorldCreatureModifiersWorldScript()
        : WorldScript("TenManWorldCreatureModifiersWorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_enabled = sConfigMgr->GetOption<bool>(std::string(CONFIG_PREFIX) + "Enable", true);

        for (size_t i = 0; i < DEFAULTS.size(); ++i)
        {
            std::string const key = std::string(CONFIG_PREFIX) + DEFAULTS[i].configName;
            g_health[i] = std::max(0.0f, sConfigMgr->GetOption<float>(key + ".Health", DEFAULTS[i].health));
            g_damage[i] = std::max(0.0f, sConfigMgr->GetOption<float>(key + ".Damage", DEFAULTS[i].damage));
        }

        LOG_INFO("module", "10 Man World: creature modifiers {} (health/damage) world normal {}/{}, world elite {}/{}, "
            "dungeon normal {}/{}, dungeon elite {}/{}.", g_enabled ? "enabled" : "disabled",
            g_health[0].load(), g_damage[0].load(), g_health[1].load(), g_damage[1].load(),
            g_health[2].load(), g_damage[2].load(), g_health[3].load(), g_damage[3].load());
    }
};

class TenManWorldCreatureModifiersHealthScript : public AllCreatureScript
{
public:
    TenManWorldCreatureModifiersHealthScript() : AllCreatureScript("TenManWorldCreatureModifiersHealthScript") { }

    void OnCreatureSelectLevel(CreatureTemplate const* /*cinfo*/, Creature* creature) override
    {
        Category const category = CategoryOf(creature);
        if (category == Category::None)
            return;

        float const multiplier = g_health[size_t(category)].load(std::memory_order_relaxed);
        if (multiplier == 1.0f || multiplier <= 0.0f)
            return;

        uint32 const maxHealth = creature->GetMaxHealth();
        float const healthPct = maxHealth ? float(creature->GetHealth()) / maxHealth : 1.0f;
        uint32 const health = Scaled(creature->GetCreateHealth(), multiplier);
        creature->SetCreateHealth(health);
        creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, float(health));
        creature->UpdateMaxHealth();
        if (creature->IsAlive())
            creature->SetHealth(std::max<uint32>(1, uint32(creature->GetMaxHealth() * healthPct)));
        creature->ResetPlayerDamageReq();
    }
};

class TenManWorldCreatureModifiersDamageScript : public UnitScript
{
public:
    TenManWorldCreatureModifiersDamageScript() : UnitScript("TenManWorldCreatureModifiersDamageScript", true,
        { UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
          UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK }) { }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        ScaleDamage(damage, target, attacker);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        ScaleDamage(damage, target, attacker);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
        SpellInfo const* spellInfo) override
    {
        if (spellInfo && spellInfo->IsPositive())
            return;

        ScaleDamage(damage, target, attacker);
    }
};

void AddSC_ten_man_world_creature_modifiers()
{
    new TenManWorldCreatureModifiersWorldScript();
    new TenManWorldCreatureModifiersHealthScript();
    new TenManWorldCreatureModifiersDamageScript();
}
