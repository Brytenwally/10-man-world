/*
 * 10 Man World - released under the MIT License, see LICENSE.
 */

void AddSC_ten_man_world_dungeon_finder();
void AddSC_ten_man_world_battleground();

// The module folder must be named 10-man-world: AzerothCore calls Add<folder name, '-' -> '_'>Scripts()
void Add10_man_worldScripts()
{
    AddSC_ten_man_world_dungeon_finder();
    AddSC_ten_man_world_battleground();
}
