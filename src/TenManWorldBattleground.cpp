/*
 * 10 Man World - released under the MIT License, see LICENSE.
 */

/*
 * 10 Man World - Random Battleground
 *
 * A raid group of up to TenManWorld.Battleground.MaxMembers players may queue together for the
 * Random Battleground. The usual group checks (level bracket, deserter, faction, other queues)
 * still apply; only the group size cap is raised.
 */

#include "Battleground.h"
#include "Config.h"
#include "Group.h"
#include "GroupScript.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "WorldScript.h"

#include <algorithm>

namespace
{
    constexpr char ENABLE_CONFIG_KEY[] = "TenManWorld.Battleground.Enable";
    constexpr char MAX_MEMBERS_CONFIG_KEY[] = "TenManWorld.Battleground.MaxMembers";
    constexpr char LEADER_QUEUES_RAID_CONFIG_KEY[] = "TenManWorld.Battleground.LeaderQueuesRaid";
    constexpr uint32 DEFAULT_MAX_MEMBERS = 10;

    bool g_enabled = true;
    uint32 g_maxMembers = DEFAULT_MAX_MEMBERS;
    bool g_leaderQueuesRaid = true;

    bool IsQueueableRaid(Group const* group)
    {
        return group && group->isRaidGroup() && !group->isBGGroup() && !group->isBFGroup() && !group->isLFGGroup();
    }
}

class TenManWorldBattlegroundPlayerScript : public PlayerScript
{
public:
    TenManWorldBattlegroundPlayerScript() : PlayerScript("TenManWorldBattlegroundPlayerScript", { PLAYERHOOK_ON_BATTLEGROUND_JOIN_AS_GROUP }) { }

    void OnPlayerBattlegroundJoinAsGroup(Player* player, BattlegroundTypeId bgTypeId, uint8& joinAsGroup) override
    {
        if (!g_enabled || !g_leaderQueuesRaid || joinAsGroup || bgTypeId != BATTLEGROUND_RB)
            return;

        Group const* group = player->GetGroup();
        if (IsQueueableRaid(group) && group->GetLeaderGUID() == player->GetGUID())
            joinAsGroup = 1;
    }
};

class TenManWorldBattlegroundGroupScript : public GroupScript
{
public:
    TenManWorldBattlegroundGroupScript() : GroupScript("TenManWorldBattlegroundGroupScript", { GROUPHOOK_ON_BATTLEGROUND_MAX_GROUP_SIZE }) { }

    void OnBattlegroundMaxGroupSize(Group const* group, Battleground const* bgTemplate, uint32& maxGroupSize) override
    {
        if (!g_enabled || bgTemplate->GetBgTypeID() != BATTLEGROUND_RB || !IsQueueableRaid(group))
            return;

        uint32 const teamSize = bgTemplate->GetMaxPlayersPerTeam();
        maxGroupSize = std::max(maxGroupSize, teamSize ? std::min(g_maxMembers, teamSize) : g_maxMembers);
    }
};

class TenManWorldBattlegroundWorldScript : public WorldScript
{
public:
    TenManWorldBattlegroundWorldScript() : WorldScript("TenManWorldBattlegroundWorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_enabled = sConfigMgr->GetOption<bool>(ENABLE_CONFIG_KEY, true);
        g_maxMembers = std::clamp<uint32>(sConfigMgr->GetOption<uint32>(MAX_MEMBERS_CONFIG_KEY, DEFAULT_MAX_MEMBERS), MAXGROUPSIZE, MAXRAIDSIZE);
        g_leaderQueuesRaid = sConfigMgr->GetOption<bool>(LEADER_QUEUES_RAID_CONFIG_KEY, true);
    }
};

void AddSC_ten_man_world_battleground()
{
    new TenManWorldBattlegroundPlayerScript();
    new TenManWorldBattlegroundGroupScript();
    new TenManWorldBattlegroundWorldScript();
}
