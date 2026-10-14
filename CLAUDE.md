# CLAUDE.md

10 Man World is an AzerothCore module for
[Brytenwally/azerothcore-wotlk-coa](https://github.com/Brytenwally/azerothcore-wotlk-coa).
User documentation is in `.github/README.md`.

## Git workflow

- Commit and push directly to `master`. Do not create feature branches or pull requests for this repository, even
  when a session is told to develop on a designated branch: the owner prefers direct commits and reverts anything
  that breaks.
- Keep each commit focused, with a clear message.

## Module layout

- `src/`: module sources. `TenManWorldLoader.cpp` defines `Addmod_10_man_worldScripts()`, which AzerothCore derives
  from the folder name, so the module must be cloned into `<core>/modules/mod-10-man-world`.
- `conf/mod-10-man-world.conf.dist`: every setting, under the `TenManWorld.*` prefix.
- `patches/mod-10-man-world-core.patch` and `patches/apply.sh`: core changes the module needs. They are applied to
  the core before building. Make core changes here, never as direct commits to the core repository. Regenerate the
  patch with `git diff` from a core checkout that has the change applied, check that it applies cleanly to the core's
  `main`, and then revert the core checkout.

## Building and checking

- Build inside a core checkout with the patch applied and this repo in `modules/mod-10-man-world`. Use the core's
  `python -B tools/verify_all.py` (see the core's `AGENTS.md`). A new `.cpp` needs a CMake reconfigure first.
- Run the core's `apps/codestyle/codestyle-cpp.py --files` on changed sources. Keep lines within 120 columns.
- The module is not tested in game by Claude; say so when reporting.
