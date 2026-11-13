# Roadmap

## Next
- **Zone colors**: new zones draw grey on a 32-bit desktop (the game's
  palette animation does not reach a DIB section there). Either Windows' 8-bit
  compatibility mode or the host mapping `AnimatePalette` onto the DIB's
  color table.
- **Play a year**: roads, gates, souls arriving, the advisors, saving and loading.
- **A conformance harness** (REPO_RULES section 9): dry-run bind count, the
  main menu and a zoned planet, each checked from `--shot`.
- **A farm build**: `netlab build afterlife-recomp` (recipe added, not run).
- **pcrecomp-afterlife into pcrecomp main**, then Setup, the launcher and netlab on `main`.

## Later
- **Linux and macOS**: a Wine route (clang-cl or mingw cross-build, as in
  redalert2-recomp's `setup.sh --wine`), tried and tested before `setup.sh` offers it.
- Full screen at 640x480 on a real desktop.
- The MS-DOS build (`ALIFEDOS.EXE`, DOS/4GW) is out of scope: the Windows build
  is the same game.
