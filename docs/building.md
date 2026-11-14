# Building and the command line

`Setup.cmd` (`tools/setup.ps1`) runs exactly the steps below, skipping the ones
already done, and logs everything to `setup.log`. It clones pcrecomp
(`main`) if it is not beside this folder.

Linux and macOS: not yet. The host runs the lifted game inside a 32-bit
Windows process against the real Windows DLLs (pcrecomp `native32`), and the
Wine route has not been tried; `setup.sh` says so and stops.

## Step by step

Prerequisites: Windows 10/11, **Python 3.10+** (`py -3 --version`), **git**,
**Visual Studio 2022** (or its Build Tools) with *Desktop development with
C++*, ffmpeg on `PATH` for `--record` only, and pcrecomp cloned **beside**
this repository (`main`):

```
some-folder\
  pcrecomp\            <- git clone https://github.com/sp00nznet/pcrecomp
  afterlife-recomp\    <- this repository
```

1. `py -3 -m pip install --user pefile capstone`
2. `mklink /J game E:\` (the CD, or the folder you copied it to)
3. `py -3 tools\catalog.py` (about 3 minutes). Expected:
   `[*] Functions: 13142 ...`
4. `py -3 run_lift.py`, then `build.cmd`. Expected from the lift:
   `13158 functions in the dispatch table, lift errors 0`.
5. `build\afterlife.exe`. Expected:
   `[bind] 0x00400000: 108 native, 0 guest, 13 shimmed, 0 unresolved`, then
   `(dry run: mapped and bound; --run enters 0x004B1007)`.
6. `build\afterlife.exe --run`.

The first `--run` writes `work\ALIFE.INI` (640x480, saves in `work\save`),
which the game reads instead of the CD's copy.

The usual trip-ups: `python` opening the Microsoft Store (use `py -3`, or turn
the alias off in Settings); a new Python not on `PATH` until a new window; a
pcrecomp older than the current `main` (Setup names what is missing).

## The command line

```
build\afterlife.exe --run
build\afterlife.exe --headless --run --watchdog 100 --key 0x1B@12 --click 103,40@20 ^
    --click 103,40@26 --click 93,228@60 --click 93,228@64 --click 35,105@70 ^
    --click 360,60,450,95@74 --shot 84 zoned.bmp
```

That second line skips the intro, picks Start a New Game and Easy, zooms in
twice, picks the green zone and drags a zone onto Heaven.

| Flag | |
|---|---|
| `--run` | enter the game (without it: map, bind and stop) |
| `--headless` | a private desktop that is never shown (works over RDP); muted unless `PCRECOMP_MUTE=0` |
| `--mute` | mute the audio session (`PCRECOMP_MUTE=1`; Windows remembers it per program, `PCRECOMP_MUTE=0` restores it) |
| `--shot S FILE.bmp` | the screen at S seconds (up to 16) |
| `--record FILE.mp4` | every blit to the window, to ffmpeg |
| `--click X,Y@S` | a left click at window coordinates X,Y at S seconds |
| `--click X,Y,X2,Y2@S` | a left-button drag from X,Y to X2,Y2 |
| `--key VK@S` | a key (virtual-key code) at S seconds |
| `--watchdog S` | stop after S seconds and say where the game was |
| `--game DIR` | the game folder (default `game`) |
| `--native-trace`, `--callbacks` | one line per call into Windows, or back from it |

## Build options

 `PCRECOMP` (Python) and `-DPCRECOMP=` (CMake, through
`set CMAKE_ARGS=...`) point at another toolkit checkout. The generated C
builds at `/Od` in about a minute. On the farm: `netlab build afterlife-recomp`.
What the game needed, in order, is [docs/bringup.md](docs/bringup.md).
