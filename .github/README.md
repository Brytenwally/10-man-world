# 10 Man World

A comprehensive AzerothCore module for a full 10-player World of Warcraft experience: a group of ten levels,
quests, runs dungeons and plays battlegrounds together, as one raid.

- **Dungeon Finder:** a raid of up to 10 players joins a dungeon instantly, with no role check and no queue.
- **Random Battleground:** a raid of up to 10 players queues together.
- **Creature modifiers:** open-world and dungeon creatures get more health and damage, so a group of ten finds mobs
  proportionally as difficult as a solo player or a five-player group did before.
- **Questing as a raid:** with one `worldserver.conf` setting (see [Questing in a raid](#questing-in-a-raid)),
  ordinary quests count for the whole raid.

## Dungeon Finder

When the leader of a **raid group** opens the Dungeon Finder, picks a random dungeon (or one or more specific
dungeons) and clicks **Join as Party**:

1. Every member is checked the way the Dungeon Finder normally checks a party: online, not in a battleground,
   no Deserter debuff, no Dungeon Finder cooldown for random dungeons, and the level/gear/attunement locks of the
   chosen dungeons.
2. There is **no role check** and **no queue**. The leader keeps the roles they ticked; everyone else keeps the
   role they last used in the Dungeon Finder, or damage if they never picked one.
3. A dungeon is chosen from those every member can enter (the same selection a normal random dungeon uses,
   including the "recently completed" filter), the raid becomes a Dungeon Finder group and every member is
   teleported in straight away.

From there it behaves like any Dungeon Finder group: random dungeon rewards on the final boss, the Dungeon Finder
cooldown, the "Luck of the Draw" buff, the teleport in/out button and vote kicks.

Parties of five or fewer and raid (Raid Browser) selections are left to the normal Dungeon Finder.

Five-player dungeons normally hold five players. The module raises that limit to `MaxMembers` only for players in
a Dungeon Finder raid; every other group still meets the dungeon's own cap.

## Random Battleground

A raid group of up to `TenManWorld.Battleground.MaxMembers` players (10 by default) can queue together for the
Random Battleground, which otherwise refuses groups bigger than five. Only the size limit changes; the normal
group checks still apply (online, same level bracket and faction, no Deserter, not queued for anything else, not a
Dungeon Finder group), and the raid enters the regular queue. The limit never goes above the Random Battleground
team size (`battleground_template`, 10 players), so a full raid makes up one whole side.

The 3.3.5 client may not offer **Join as Group** to a raid. With `LeaderQueuesRaid` on (the default), the raid
leader's plain **Join Battle** click for Random Battleground queues the whole raid. Specific battlegrounds are
unchanged.

## Creature modifiers

Creature health and damage are multiplied by where the creature lives and by its rank:

| Creatures | Health | Damage |
| --- | --- | --- |
| Open world normal | 4x | 3x |
| Open world rares, elites and bosses | 2.5x | 2x |
| Dungeon non-elites | 2.5x | 2x |
| Dungeon elites and bosses | 2x | 1.5x |

"Dungeon" means five-player dungeons, normal and heroic. Raids, battlegrounds, arenas and the fork's scripted private
instances are not changed. Player pets, totems, triggers, critters and training dummies are never changed; creatures
summoned by other creatures (boss adds) are.

The defaults are chosen so that mobs are now proportionally as difficult as before: ten players face roughly the
challenge one player (open world) or five players (dungeons) faced without the module.

- **Health** is set when a creature spawns, respawns or changes entry, keeping its current health percentage.
- **Damage** is multiplied for everything a hostile creature deals to players and their pets: melee, spells and
  damage over time. Heals are never multiplied, and damage between creatures (for example a mob hitting an escort
  NPC) is unchanged.
- The multipliers stack with this fork's other scalers (`CoA.CreatureScaling`, `CoA.DungeonTuning`, Mythic+).

## Questing in a raid

By default WotLK does not let ordinary (non-raid) quests progress while you are in a raid group: kills, loot and
objectives only count in a party. To let the whole 10-player raid quest together, enable this setting in the core's
`worldserver.conf` (not in the module's config):

```
Quests.IgnoreRaid = 1
```

`Quests.IgnoreRaid` is in the quest section of `worldserver.conf.dist` ("Allow non-raid quests to be completed
while in a raid group"); its default is `0`. Restart the worldserver or run `.reload config` after changing it.

## Configuration

Copy `conf/mod-10-man-world.conf.dist` to `mod-10-man-world.conf` in the server's module config directory.

| Option | Default | Description |
| --- | --- | --- |
| `TenManWorld.DungeonFinder.Enable` | `1` | `0` turns instant raid queueing off; raids then get the usual "too many members" error. Reloadable. |
| `TenManWorld.DungeonFinder.MaxMembers` | `10` | Largest raid allowed to queue, and the dungeon player cap for Dungeon Finder raids (5-40). Reloadable. |
| `TenManWorld.Battleground.Enable` | `1` | `0` keeps the normal group size limit for Random Battleground. Reloadable. |
| `TenManWorld.Battleground.MaxMembers` | `10` | Largest raid allowed to queue for Random Battleground (5-40, never above its team size). Reloadable. |
| `TenManWorld.Battleground.LeaderQueuesRaid` | `1` | The raid leader's plain "Join Battle" for Random Battleground queues the whole raid. Reloadable. |
| `TenManWorld.CreatureModifiers.Enable` | `1` | `0` leaves creature health and damage unchanged. Reloadable. |
| `TenManWorld.CreatureModifiers.<Area>.<Rank>.Health` / `.Damage` | see table above | `World.Normal`, `World.Elite`, `Dungeon.Normal`, `Dungeon.Elite`; `1.0` = unchanged. Reloadable (health applies to creatures spawned afterwards). |

## Installing

The module needs a few small core changes, shipped as `patches/mod-10-man-world-core.patch`. Apply the patch to the
core **before** configuring and compiling; without it the module does not compile.

The folder must be called exactly `mod-10-man-world` (the default clone name): AzerothCore looks for a loader named
after the module folder, and this module's loader is `Addmod_10_man_worldScripts()`.

```sh
cd <azerothcore>/modules
git clone https://github.com/Brytenwally/mod-10-man-world.git
./mod-10-man-world/patches/apply.sh            # or: ./mod-10-man-world/patches/apply.sh /path/to/azerothcore
# then re-run CMake and build as usual
```

On Windows, run `apply.sh` from Git Bash, or apply the patch from the core folder with
`git apply modules/mod-10-man-world/patches/mod-10-man-world-core.patch`. `apply.sh` does nothing if the patch is
already applied.

The patch is made against [Brytenwally/azerothcore-wotlk-coa](https://github.com/Brytenwally/azerothcore-wotlk-coa);
other AzerothCore forks may need it adjusted.

The patch (the creature modifiers need no core change; they use existing hooks):

- makes `GetDungeonsByRandom`, `GetDungeonType`, `GetCompatibleDungeons`, `SetSelectedDungeons`, `SetState` and
  `MakeNewGroup` public in `lfg::LFGMgr`, so the module can drive the normal Dungeon Finder group creation;
- adds `PlayerScript::OnPlayerInstancePlayerLimit`, called from `InstanceMap::CannotEnter`, so a script can change
  an instance's player cap for the player entering it;
- adds `GroupScript::OnBattlegroundMaxGroupSize`, called from `Group::CanJoinBattlegroundQueue`, so a script can
  raise the largest group allowed to queue for a battleground;
- adds `PlayerScript::OnPlayerBattlegroundJoinAsGroup`, called from `WorldSession::HandleBattlemasterJoinOpcode`,
  so a script can decide whether the player's group joins the queue with them.

## License

The module is released under the MIT License (see `LICENSE`). `patches/mod-10-man-world-core.patch` modifies
AzerothCore itself and stays under the core's license, the GNU GPL v2 (or later).

## Known limitations

- Quests only count for a raid when `Quests.IgnoreRaid = 1` is set in `worldserver.conf` (see
  [Questing in a raid](#questing-in-a-raid)).
- A 10-player raid fills one whole Random Battleground side, so it only gets into a new battleground, never into
  one already running with a few free slots; its queue can be longer than a party's.
- With `LeaderQueuesRaid` on, the leader of a raid of up to 10 cannot queue alone for Random Battleground while in
  the raid; leaders of bigger raids queue alone as usual.
