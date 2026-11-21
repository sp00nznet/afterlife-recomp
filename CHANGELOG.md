# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[SemVer](https://semver.org/).

## [Unreleased]

## [0.1.0] - 2026-11-20

### Changed
- Builds against pcrecomp main; the `pcrecomp-afterlife` branch is merged.
- Brought in line with the other recomps: the README in their order (Getting
  Started, Status, Screenshots, Documentation, Contributors, License), the
  build steps and command line moved to `docs/building.md`, `NOTICE` renamed
  `NOTICE.md` and saying what the MIT license covers and what it doesn't.

### Added
- `CONTRIBUTING.md`, `CONTRIBUTORS.md`, `docs/building.md`, `docs/testing.md`.
- `setup.sh`, which says Linux and macOS are not supported yet and stops.
- `.github/workflows/installer.yml`, the recomp-launcher installer for
  Windows, run by hand only (`workflow_dispatch`).

### Added
- First boot of Afterlife (LucasArts, 1996; the CD's Windows 95 `ALIFE.EXE`)
  on pcrecomp `native32`: lifted whole, 13,158 entries, 0 lift errors, no IDA.
- In game: the intro, the main menu, Start a New Game, a new planet's map of
  Heaven and Hell with its panels, zoom, and zoning (a zone dragged onto a
  plane costs money and changes the tiles).
- `work\ALIFE.INI` served in place of the CD's (640x480, saves in `work\save`).
- `--headless`, `--shot S FILE.bmp` and `--record FILE.mp4` from a shadow copy
  of the game's GDI blits, `--click X,Y@S`, `--click X,Y,X2,Y2@S` (a drag) and
  `--key VK@S` with a host-side cursor, `--mute`.
- `Setup.cmd` (`tools/setup.ps1`), `launcher/launcher.json`, `tools/catalog.py`,
  `docs/bringup.md`.
