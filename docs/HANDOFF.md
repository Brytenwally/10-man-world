# 10 Man World: session handoff

State of the work as of 2026-10-15, for a new Claude session. `CLAUDE.md` holds the standing rules; this file holds
the history, the decisions behind the code and the open threads.

## The owner and how they work

- Runs the server on **Windows**, builds with **Visual Studio 2026 Insiders** (generator "Visual Studio 18 2026",
  MSVC 14.52) and CMake GUI, uses Git Bash for scripts. Not a git expert: give exact copy-paste commands and say
  what each does. Downloads code as ZIPs at times (watch for `-master` / `-<branch>` suffixes on folder names).
- Slow PC: a full build takes ~55 minutes and ran out of memory with default parallelism. They build themselves;
  **do not run builds in the cloud unless asked** (they stopped one). Small compile checks of changed module files
  are fine and useful.
- Tests in game themselves and reports back. Claude cannot test in game; say so.
- Wants changes to this module pushed **straight to `master`** (no branches/PRs here).

## Repositories

| Repo | Role |
| --- | --- |
| [Brytenwally/mod-10-man-world](https://github.com/Brytenwally/mod-10-man-world) (`master`) | This module. Folder must be `modules/mod-10-man-world`. |
| [Brytenwally/azerothcore-wotlk-coa](https://github.com/Brytenwally/azerothcore-wotlk-coa) (`main`) | The owner's core fork (Conquest of Azeroth, CoA). Updated from upstream from time to time; last checked at `dbb02f53`. |
| [jealous-sound/azerothcore-wotlk-coa](https://github.com/jealous-sound/azerothcore-wotlk-coa) | Upstream of the fork. Public; its issues can be read via the web pages, not the GitHub API (not attached). |
| [jealous-sound/mod-playerbots](https://github.com/jealous-sound/mod-playerbots) (branch `coa`) | Playerbots for CoA, used by the owner. Its folder must be `modules/mod-playerbots` (loader `Addmod_playerbotsScripts`). |

The core's never-commit rule: core changes live only in `patches/mod-10-man-world-core.patch`. Commits to the core
repo's `claude/...` branch are not wanted.

## What the module does (src/)

- `TenManWorldDungeonFinder.cpp`: a raid leader (raid of up to `TenManWorld.DungeonFinder.MaxMembers`, default 10)
  joining the Dungeon Finder for a dungeon/random dungeon skips role check and queue. Members are checked like a
  party, a dungeon is picked with the core's `GetCompatibleDungeons` + `FilterCooldownDungeons`, the raid becomes an
  LFG group via `LFGMgr::MakeNewGroup` and is teleported in. Random rewards, cooldown, buff and teleport work as
  normal. `OnPlayerInstancePlayerLimit` lets LFG raids exceed a dungeon's player cap.
- `TenManWorldBattleground.cpp`: raids of up to `TenManWorld.Battleground.MaxMembers` (default 10, never above the
  Random BG template's team size) may queue for Random Battleground (`OnBattlegroundMaxGroupSize`). With
  `LeaderQueuesRaid = 1`, the leader's plain "Join Battle" queues the raid (`OnPlayerBattlegroundJoinAsGroup`), but
  only for raids within the limit.
- `TenManWorldCreatureModifiers.cpp`: health/damage multipliers by area and rank (defaults: world normal 4x/3x,
  world rare/elite/boss 2.5x/2x, dungeon normal 2.5x/2x, dungeon elite/boss 2x/1.5x). Health in
  `OnCreatureSelectLevel` (multiplies the existing create-health, keeps the health percentage); damage in the three
  damage hooks, only from hostile creatures to players/pets, never on positive (heal) periodic ticks. Raids, BGs,
  arenas and scripted private instances excluded. Needs no core change.
- `TenManWorldLoader.cpp`: `Addmod_10_man_worldScripts()`.
- Config prefix `TenManWorld.*`; everything reloadable with `.reload config` (health applies to newly spawned
  creatures).

## The core patch (patches/)

Exposes six `LFGMgr` members (`GetDungeonsByRandom`, `GetDungeonType`, `GetCompatibleDungeons`,
`SetSelectedDungeons`, `SetState`, `MakeNewGroup`) and adds three hooks: `PlayerScript::OnPlayerInstancePlayerLimit`
(InstanceMap::CannotEnter), `GroupScript::OnBattlegroundMaxGroupSize` (Group::CanJoinBattlegroundQueue),
`PlayerScript::OnPlayerBattlegroundJoinAsGroup` (HandleBattlemasterJoinOpcode). The hook enum entries sit right after
`PLAYERHOOK_CAN_ENTER_MAP` / `GROUPHOOK_CAN_GROUP_JOIN_BATTLEGROUND_QUEUE`, not at the end of the lists, so hooks
upstream appends do not conflict. `apply.sh` is idempotent and finds the core three levels up.

When the owner says the patch fails after an upstream update: fetch `main`, try `git apply --check`, then
`git apply --3way`, resolve, regenerate with `git diff`, verify on a clean worktree (apply twice), lint with
`apps/codestyle/codestyle-cpp.py`, push.

## History and decisions worth knowing

- Raising the instance cap through the DBC/`sMapDifficultyMap` was rejected: it broke the "deaths in 5-player
  dungeons" statistic. Hence the `OnPlayerInstancePlayerLimit` hook.
- `CoA.CreatureScaling` (enabled in `coa.conf.dist`: world 2.5x/2x, dungeon 2.5x/1.5x) multiplies on top of this
  module, so open-world normal mobs end at ~10x health / 6x damage. The owner keeps both on deliberately (CoA classes
  are stronger); they may lower this module's values later. Destiny Weaver per-character level scaling carries the
  multipliers through correctly.
- Random dungeons outside the character's level range and unscaled creature loot are the fork's design (Classic
  dungeons' min level lowered to `TargetLevelMin`; Destiny Weaver scales creatures, not loot; Dungeon Spoils is the
  level-scaled reward), not caused by this module.
- In the owner's DBCs a `MaxPlayers` of 0 means "no limit" (they changed that themselves). Edited CSVs: Random
  Battleground and the other battlegrounds `MaxGroupSize` 5 -> 10 in BattlemasterList; MapDifficulty only had one
  5 (Culling of Stratholme difficulty 2) -> 10.
- A `coa-bots` branch of the fork turned out to be an outdated fork; the module targets `main` only.

## Open threads

- **"Join as Group" for Random BG fails with "Group is too large"** although the server and client DBCs were edited.
  The server never sends that text (`LANG_BG_GROUP_TOO_LARGE` is unused; oversize returns the silent
  `ERR_BATTLEGROUND_NONE`), so the client refuses before sending. Likely causes: another custom MPQ with
  `DBFilesClient\BattlemasterList.dbc` outranks the owner's patch, or the Ascension PvP UI addon hard-codes the size.
  The owner will check their patches. "Join Battle" with `LeaderQueuesRaid = 1` works meanwhile.
- **Quests in a raid** need `Quests.IgnoreRaid = 1` in the core's `worldserver.conf` (documented in the README).
- **Dungeon Finder from a raid:** whether the 3.3.5 client lets a raid leader click Join has not been reported yet.
- **Upstream issue #7690** (Riverpaw Genocide, no yellow markers under targets): no server cause found; the markers
  come from the client's `Ascension_NamePlates` addon. Separately, quest 17011's kill counts (10/8/12) do not match
  its text (8/12/10); left unchanged because the data came from Ascension's client cache. Waiting on an in-game
  comparison by the owner.
- Ideas the owner may come back to: a GitHub Actions Windows build that uploads binaries; level-scaled boss loot
  (left to the core developers).

## Owner's local setup notes

- Server folder `D:\build\bin\Release` with `Data\` (dbc, maps, vmaps, mmaps); `DataDir = "./Data"`.
- Build tips that fixed real failures: `cmake --build . --config Release --target worldserver -- /m:1
  /p:CL_MPCount=2` (out of heap space), short source/build paths without spaces (rc.exe "filename too long"),
  use **Build**, not **Rebuild**, for incremental builds; module folder names must match their loader.
- Portable MySQL scripts were written for them (start/stop/console `.bat`, `my.ini`, `init_acore.sql` creating
  user `acore`/`acore` and the three databases); and a `rename_copia_dist` `.bat`/`.sh` that turns
  `<name> - Copia.dist` into `<name>`. These live on their PC, not in a repo.

## Cloud session setup (if a build or compile check is needed)

```sh
apt-get install -y libboost-all-dev libmysqlclient-dev libssl-dev libreadline-dev zlib1g-dev libbz2-dev ninja-build
git clone --depth 1 https://github.com/Brytenwally/azerothcore-wotlk-coa core
git clone https://github.com/Brytenwally/mod-10-man-world core/modules/mod-10-man-world
cd core && bash modules/mod-10-man-world/patches/apply.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAPPS_BUILD=world-only \
  -DSCRIPTS=static -DMODULES=static -DTOOLS_BUILD=none
ninja -C build modules/CMakeFiles/modules.dir/mod-10-man-world/src/<File>.cpp.o   # quick check of one file
```

A full build is ~45 minutes on 4 cores and needs ~20 GB of disk; check `df` first.
