# Testing

There is no conformance harness yet (it is on the [ROADMAP](../ROADMAP.md)).
What exists are two checks anyone can run, both headless: the dry run, and a
scripted run from the intro into a zoned planet. Neither opens a window on
your screens: `--headless` runs on a private desktop that is never shown, and
mutes the sound.

## The dry run

```
build\afterlife.exe
```

Maps `ALIFE.EXE`'s image, binds its imports and stops. Expected:
`[bind] 0x00400000: 108 native, 0 guest, 13 shimmed, 0 unresolved`, then
`(dry run: mapped and bound; --run enters 0x004B1007)`. Any `unresolved` is a
missing shim or a lift that did not cover a call target.

## Into a zoned planet

```
build\afterlife.exe --headless --run --watchdog 100 --key 0x1B@12 --click 103,40@20 ^
    --click 103,40@26 --click 93,228@60 --click 93,228@64 --click 35,105@70 ^
    --click 360,60,450,95@74 --shot 84 zoned.bmp
```

Escape skips the intro; the clicks pick Start a New Game and Easy, zoom in
twice, pick the green zone and drag it onto Heaven. It passes when
`zoned.bmp` shows the new zone and the money below its starting figure
([the screenshot](screenshots/zoned.png) is this run). The times are seconds
from the start; on a slow machine, push them later.

`--watchdog` ends the run and says where the game was, so a hang shows up as
an address rather than a run that never ends. `--record out.mp4` keeps every
frame (ffmpeg on `PATH`), and `--native-trace` logs each call into Windows.
