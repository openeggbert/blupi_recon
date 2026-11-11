# Speedy Eggbert SE Valueware v1.0 (eGames, 1999): edition source

Target `sb1/eggbert/value`. The original is `SpeedyEggbertSEValueware_v1.0_EN.exe` (534,528 bytes,
SHA-256 `2cf7fa8f1ebb5aba1ea31ae805b788f67f224a0b1e0aef314f8e889568800ee3`, linked 1999-06-24
15:08:23 UTC). It is read statically only and never executed. Status and figures:
[docs/EGGBERT_VALUE.md](../../docs/EGGBERT_VALUE.md).

This directory is a whole-file overlay on `src/` (`source_overlays` in `targets.json`,
`tools/source_tree.py`). A file here replaces the file of the same name in `src/` for this target only;
every other file comes from `src/` unchanged, compiled with `_EGAMES=1` like Speedy Eggbert 1.

## Why an overlay

[EVIDENCE] Valueware is the Speedy Eggbert 1 (version 1.1) source built with the original source's
`_VALUE` switch. `src/` carries that switch only where the source lineage kept it (`#if _DEMO || _VALUE`
in `event.cpp`); `def.h` does not define it, and the level-file arms in `decor.cpp` are written with
`_DEMO`, the name the June 1998 demo target uses for an earlier source revision. Each overlay file is a
copy of the `src/` file in which every `_DEMO` / `_VALUE` arm is resolved into plain code: the Valueware
arm where this executable's bytes show it (marked `eGames Valueware (_VALUE):`), the retail arm
everywhere else.

[EVIDENCE] Valueware shares the Speedy Eggbert Demo's restricted screens and its level-file marker, but
not everything: it reads the `Benchmark=` and `TrueColor=` keys, keeps the full game's closing texts and
the Escape shortcut, and ends the game on a different condition. The two overlays are therefore separate
files, not one shared set.

## Files

| File | What differs from `src/` (function, evidence in the Valueware executable) |
|---|---|
| `decblupi.cpp` | `BlupiStep`: winning mission 1 ends the game (`m_term = -2` instead of 99). |
| `decor.cpp` | `Write` and `CurrentWrite` store the marker 1; `Read` rejects an official level (mission below 200) without that marker. |
| `event.cpp` | Title screen: `Version 1.0` at y 437 and `VALUEWARE`. Buttons shown in their demo state (`SetDemo`) in the lobby, mission-info, player and pause screens. Cheat 0 is absent. |
| `res/sb1-eggbert-value.rc` | Valueware's own resource script (`tools/make_rc.py`). |
