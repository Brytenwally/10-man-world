/*
 * 10 Man World - released under the MIT License, see LICENSE.
 */

/*
 * 10 Man World - Dungeon Finder
 *
 * A raid group leader who joins the Dungeon Finder for a random or specific dungeon skips the
 * role check and the queue entirely: the raid is turned into a Dungeon Finder group on the spot
 * and teleported into the dungeon, with the usual random dungeon rewards, cooldown and buff.
 */

#include "Config.h"
#include "Containers.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "PlayerScript.h"
#include "RBAC.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldScript.h"
#include "WorldSession.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

using namespace lfg;

namespace
{
    constexpr char ENABLE_CONFIG_KEY[] = "TenManWorld.DungeonFinder.Enable";
    constexpr char MAX_MEMBERS_CONFIG_KEY[] = "TenManWorld.DungeonFinder.MaxMembers";
    constexpr uint32 DEFAULT_MAX_MEMBERS = 10;

    bool g_enabled = true;
    uint32 g_maxMembers = DEFAULT_MAX_MEMBERS;

    bool IsDungeonSelection(LfgDungeonSet const& dungeons)
    {
        if (dungeons.empty())
            return false;

        for (uint32 dungeonId : dungeons)
        {
            switch (sLFGMgr->GetDungeonType(dungeonId))
            {
                case LFG_TYPE_RANDOM:
                case LFG_TYPE_DUNGEON:
                case LFG_TYPE_HEROIC:
                    break;
                default:
                    return false;
            }
        }

        return true;
    }

    uint8 AssignedRole(uint8 chosenRoles)
    {
        uint8 const roles = chosenRoles & (PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE);
        return roles ? roles : PLAYER_ROLE_DAMAGE;
    }

    LfgJoinResult CheckMember(Player* member, bool isLeader, bool random)
    {
        if (!member->GetSession()->HasPermission(rbac::RBAC_PERM_JOIN_DUNGEON_FINDER))
            return isLeader ? LFG_JOIN_NOT_MEET_REQS : LFG_JOIN_PARTY_NOT_MEET_REQS;

        bool const queuedForBattleground = member->InBattlegroundQueue()
            && !sWorld->getBoolConfig(CONFIG_ALLOW_JOIN_BG_AND_LFG);
        if (member->InBattleground() || queuedForBattleground)
            return LFG_JOIN_USING_BG_SYSTEM;

        if (member->HasAura(LFG_SPELL_DUNGEON_DESERTER))
            return isLeader ? LFG_JOIN_DESERTER : LFG_JOIN_PARTY_DESERTER;

        if (random && member->HasAura(LFG_SPELL_DUNGEON_COOLDOWN))
            return isLeader ? LFG_JOIN_RANDOM_COOLDOWN : LFG_JOIN_PARTY_RANDOM_COOLDOWN;

        return LFG_JOIN_OK;
    }

    void SendGroupFound(Player* member, LfgDungeonSet const& selected, std::string const& comment)
    {
        WorldSession* session = member->GetSession();
        session->SendLfgUpdateParty(LfgUpdateData(LFG_UPDATETYPE_JOIN_QUEUE, selected, comment));
        session->SendLfgUpdateParty(LfgUpdateData(LFG_UPDATETYPE_GROUP_FOUND));
        session->SendLfgUpdatePlayer(LfgUpdateData(LFG_UPDATETYPE_REMOVED_FROM_QUEUE));
        session->SendLfgUpdateParty(LfgUpdateData(LFG_UPDATETYPE_REMOVED_FROM_QUEUE));
    }

    LfgJoinResultData LaunchInstantDungeon(Player* leader, Group* group, uint8 leaderRoles,
        LfgDungeonSet const& dungeons, std::string const& comment)
    {
        LfgJoinResultData joinData;
        ObjectGuid const gguid = group->GetGUID();

        if (group->isLFGGroup() && sLFGMgr->GetState(gguid) != LFG_STATE_FINISHED_DUNGEON)
        {
            joinData.result = LFG_JOIN_PARTY_NOT_MEET_REQS;
            return joinData;
        }

        if (group->GetMembersCount() > g_maxMembers)
        {
            joinData.result = LFG_JOIN_TOO_MUCH_MEMBERS;
            return joinData;
        }

        uint32 randomDungeonId = 0;
        for (uint32 dungeonId : dungeons)
        {
            if (sLFGMgr->GetDungeonType(dungeonId) != LFG_TYPE_RANDOM)
                continue;

            if (dungeons.size() > 1)
            {
                joinData.result = LFG_JOIN_DUNGEON_INVALID;
                return joinData;
            }

            randomDungeonId = dungeonId;
            sScriptMgr->OnPlayerQueueRandomDungeon(leader, randomDungeonId);
        }

        std::vector<Player*> members;
        LfgGuidSet memberGuids;
        LfgRolesMap roles;
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member)
                continue;

            bool const isLeader = member == leader;
            joinData.result = CheckMember(member, isLeader, randomDungeonId != 0);
            if (joinData.result != LFG_JOIN_OK)
                return joinData;

            members.push_back(member);
            memberGuids.insert(member->GetGUID());
            roles[member->GetGUID()] = AssignedRole(isLeader ? leaderRoles : sLFGMgr->GetRoles(member->GetGUID()));
        }

        if (members.size() != group->GetMembersCount())
        {
            joinData.result = LFG_JOIN_DISCONNECTED;
            return joinData;
        }

        LfgDungeonSet candidates = randomDungeonId ? sLFGMgr->GetDungeonsByRandom(randomDungeonId) : dungeons;
        for (Player* member : members)
            sLFGMgr->InitializeLockedDungeons(member, group);

        sLFGMgr->GetCompatibleDungeons(candidates, memberGuids, joinData.lockmap, randomDungeonId);
        if (candidates.empty())
        {
            joinData.result = LFG_JOIN_PARTY_NOT_MEET_REQS;
            return joinData;
        }

        LfgDungeonSet const playable = sLFGMgr->FilterCooldownDungeons(candidates, roles);
        uint32 const dungeonId = Acore::Containers::SelectRandomContainerElement(playable);

        LfgDungeonSet selected;
        if (randomDungeonId)
            selected.insert(randomDungeonId);
        else
            selected = dungeons;

        LfgProposal proposal(dungeonId);
        proposal.leader = leader->GetGUID();
        proposal.state = LFG_PROPOSAL_SUCCESS;

        sLFGMgr->SetComment(leader->GetGUID(), comment);
        sLFGMgr->SetLeader(gguid, leader->GetGUID());
        leader->GetSession()->SendLfgJoinResult(joinData);

        for (Player* member : members)
        {
            ObjectGuid const guid = member->GetGUID();
            uint8 const role = roles[guid] | (guid == proposal.leader ? PLAYER_ROLE_LEADER : 0);

            LfgProposalPlayer& proposalPlayer = proposal.players[guid];
            proposalPlayer.role = role;
            proposalPlayer.group = gguid;
            proposalPlayer.accept = LFG_ANSWER_AGREE;

            sLFGMgr->SetGroup(guid, gguid);
            sLFGMgr->AddPlayerToGroup(gguid, guid);
            sLFGMgr->SetSelectedDungeons(guid, selected);
            sLFGMgr->SetRoles(guid, role);
            sLFGMgr->SetState(guid, LFG_STATE_DUNGEON);

            SendGroupFound(member, selected, comment);
        }

        LOG_DEBUG("module", "10 Man World: raid {} led by {} ({} members) sent to dungeon {} (random {}).",
            gguid.ToString(), leader->GetName(), members.size(), dungeonId, randomDungeonId);

        sLFGMgr->MakeNewGroup(proposal);
        return joinData;
    }
}

class TenManWorldDungeonFinderPlayerScript : public PlayerScript
{
public:
    TenManWorldDungeonFinderPlayerScript()
        : PlayerScript("TenManWorldDungeonFinderPlayerScript",
            { PLAYERHOOK_CAN_JOIN_LFG, PLAYERHOOK_ON_INSTANCE_PLAYER_LIMIT }) { }

    bool OnPlayerCanJoinLfg(Player* player, uint8 roles, std::set<uint32>& dungeons,
        std::string const& comment) override
    {
        if (!g_enabled)
            return true;

        Group* group = player->GetGroup();
        if (!group || !group->isRaidGroup() || group->isBGGroup() || group->isBFGroup())
            return true;

        if (!IsDungeonSelection(dungeons) || sLFGMgr->GetState(player->GetGUID()) == LFG_STATE_RAIDBROWSER)
            return true;

        if (group->GetLeaderGUID() != player->GetGUID())
            return false;

        LfgJoinResultData joinData = LaunchInstantDungeon(player, group, roles, dungeons, comment);
        if (joinData.result != LFG_JOIN_OK)
            player->GetSession()->SendLfgJoinResult(joinData);

        return false;
    }

    void OnPlayerInstancePlayerLimit(Player* player, InstanceMap const* map, uint32& maxPlayers) override
    {
        if (!map->IsNonRaidDungeon())
            return;

        Group* group = player->GetGroup();
        if (group && group->isLFGGroup() && group->isRaidGroup())
            maxPlayers = std::max(maxPlayers, g_maxMembers);
    }
};

class TenManWorldDungeonFinderWorldScript : public WorldScript
{
public:
    TenManWorldDungeonFinderWorldScript()
        : WorldScript("TenManWorldDungeonFinderWorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_enabled = sConfigMgr->GetOption<bool>(ENABLE_CONFIG_KEY, true);
        uint32 const maxMembers = sConfigMgr->GetOption<uint32>(MAX_MEMBERS_CONFIG_KEY, DEFAULT_MAX_MEMBERS);
        g_maxMembers = std::clamp<uint32>(maxMembers, MAXGROUPSIZE, MAXRAIDSIZE);
    }
};

void AddSC_ten_man_world_dungeon_finder()
{
    new TenManWorldDungeonFinderPlayerScript();
    new TenManWorldDungeonFinderWorldScript();
}
