# Speedy Eggbert Demo (eGames, 1999): edition source

Target `sb1/eggbert/demo`. The original is `WCSUP.DLL` of the eGames demo package: despite its name it
is the game executable (534,528 bytes, SHA-256
`fc071424356a9a270e8abf9852bfa08e7b592374a269bf28083071bcc9dfe52c`, linked 1999-06-24 15:11:50 UTC).
It is read statically only and never executed. Status and figures: [docs/EGGBERT_DEMO.md](../../docs/EGGBERT_DEMO.md).

This directory is a whole-file overlay on `src/` (`source_overlays` in `targets.json`,
`tools/source_tree.py`). A file here replaces the file of the same name in `src/` for this target only;
every other file comes from `src/` unchanged, compiled with `_EGAMES=1` like Speedy Eggbert 1.

## Why an overlay and not `_DEMO=1`

[EVIDENCE] The demo is the Speedy Eggbert 1 (version 1.1) source built with the original source's own
demo switch: the functions that differ from `sb1/eggbert/en` differ exactly where `src/` already carries
a `_DEMO` arm that came with the source lineage (`#if _DEMO || _VALUE` in `event.cpp`, the demo marker in
`decor.cpp`, `ReadConfig` without the benchmark keys).

[EVIDENCE] `src/` uses the same name `_DEMO` for the June 1998 demo target (`sb1/blupi/demo`), where it
also selects that earlier revision's code: a pixmap class without the true-colour field, the single
`image\` folder, level revision 1.2. The 1999 demo has none of those: its pixmap, `misc.cpp` and image
loading are byte-identical to Speedy Eggbert 1. Setting `_DEMO=1` for this target would therefore select
code its executable does not contain.

So each overlay file is a copy of the `src/` file in which every `_DEMO` / `_VALUE` arm is resolved
into plain code: the demo arm where the 1999 demo's bytes show it (marked `eGames demo (_DEMO):`), the
retail arm everywhere else. `_EGAMES` arms are left as they are.

## Files

| File | What differs from `src/` (function, evidence in the demo executable) |
|---|---|
| `blupi.cpp` | `ReadConfig` ends after `MouseType=`: no `Benchmark=` and `TrueColor=` keys (the strings are absent from the executable). |
| `decblock.cpp` | `DrawInfo` hides the treasure count only for mission 1, not for every tenth mission. |
| `decblupi.cpp` | `BlupiStep`: winning mission 40, not 99, ends the game (`m_term = -2`). |
| `decor.cpp` | `Write` and `CurrentWrite` store the demo marker 1; `Read` rejects an official level (mission below 200) without that marker. The level revision stays 1.3 and the mission title is kept, unlike June 1998. |
| `event.cpp` | Title screen: `Version 1.0` at y 437 and `D E M O`. Buttons shown in their demo state (`SetDemo`) in the lobby, mission-info, player and pause screens. The closing screen loads the demo texts `TX_DEMO_END1..4`. Cheat 0 and the Escape shortcut from a world hub are absent. A failed CD test closes the program. |
| `res/sb1-eggbert-demo.rc` | The demo's own resource script (`tools/make_rc.py`). |
