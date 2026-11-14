# Bring-up notes

What Afterlife needed, in the order it turned up. Each item says where the fix
lives: in this repository's host (`src/runtime/host.c`), or in pcrecomp
(now on `main`), when another game would hit it too.

## The copy of the game

The copy used here is a raw `MODE1/2352` BIN/CUE image of the US CD. 7-Zip does not read a raw 2352-byte image; keeping bytes 16..2064 of each
sector gives a plain ISO, which it does. The CD has:

- `ALIFE.EXE`: the Windows 95 build (PE32, MSVC, imports only `KERNEL32`,
  `USER32`, `GDI32`, `WINMM`, `comdlg32`). **This is the one recompiled.**
- `ALIFEDOS.EXE`: the MS-DOS build (DOS/4GW, with `IMUSE.EXE` and
  `UVCONFIG.EXE`).
- `ALIFE.000` (148 MB), `FULL.000`, `MEDIUM.000`: the resource files;
  `ALIFE.INI` says where they are.

No copy protection: no SafeDisc, SecuROM or driver, no CD check beyond
`ALIFE.INI`'s `CD Path`.

## Lifting

One module at `0x00400000`. The catalog (`disasm32`) finds 13,142 functions,
99.6% of `.text`; the lift has 0 errors and 21 instructions as no-ops (`int`,
`fnstenv`/`fldenv`, `insd`, in data decoded as code).

- **Jump-table arms in a neighbour's body.** The window procedure is one big
  `switch`, and the catalog cut it into three functions. The piece at
  `0x004CBD5B` holds `jmp [eax*4 + 0x4CBE7B]`, whose arms (`0x004CBCBB` ...)
  sit in the piece before it and are not entries of anything, so the first key
  press stopped at `ITAIL: unresolved VA 0x004CBCBB`. Fixed in pcrecomp
  (`drive32.lift_image`): every jump-table arm outside the body that jumps
  through the table becomes an entry of its own.

## The display: ALIFE.INI

`Resolution = MAX` in the CD's `ALIFE.INI` makes the game take the desktop's
size (1804x972 on the private desktop, with the 640x480 picture in the middle),
and `Save games = C:\ALIFE` writes outside the project. The host answers the
game's `CreateFileA("ALIFE.INI")` with `work\ALIFE.INI`, which it writes on the
first run: `Resolution = 640x480`, saves in `work\save`, the rest as on the CD.

At start-up the game says it wants a 256-color desktop (a message box the host
answers OK). It runs at 32 bits. New zones draw as a grey dither, not in their sin's
color. The likely cause is palette animation (`AnimatePalette`), which at 32
bits reaches no pixel of a DIB section; Windows' 256-color compatibility mode
(`native32_require_color_depth(8)`) changed nothing on the private desktop.
Not chased further, and not yet compared with the original running.

## Screenshots and recording

The game draws with plain GDI: 8-bit DIB sections, `BitBlt`/`StretchBlt` to
the window, the map view and the panels as separate blits, and only what
changed. On the private desktop of `--headless` a copy of the window is black,
so the host wraps `BitBlt` and `StretchBlt` and repeats every copy to the
window into a 640x480 32-bit shadow (GDI does the palette). `--shot` writes
the shadow; a still screen blits nothing, so a shot taken then writes the
shadow as it is.

## Input

The game reads window messages and `GetCursorPos`; it has no DirectInput. On
the private desktop `SetCursorPos` would move the user's real pointer, so the
host keeps the cursor itself (`GetCursorPos`/`SetCursorPos` shims) and
`--click` sets it, then posts `WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/`WM_LBUTTONUP`.
`--click X,Y,X2,Y2@S` drags in ten steps, which is how a zone is laid.

## Sound

Plain `waveOut`. `--headless` sets `PCRECOMP_MUTE=1` and pcrecomp's
`native32_init` mutes the process's audio session.
