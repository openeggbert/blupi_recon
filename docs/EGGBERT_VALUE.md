# Speedy Eggbert SE Valueware v1.0 (eGames, 1999)

Target `sb1/eggbert/value`, added 2026-11-11. The original is
`SpeedyEggbertSEValueware_v1.0_EN.exe`, registered at
`assets/downloads/eggbert_value/SpeedyEggbertSEValueware_v1.0_EN.exe` (534,528 bytes, SHA-256
`2cf7fa8f1ebb5aba1ea31ae805b788f67f224a0b1e0aef314f8e889568800ee3`, PE link time 1999-06-24 15:08:23
UTC). It is read statically and never executed. Its restrictions are reproduced as they are; nothing
removes or bypasses them.

Source: the whole-file overlay [games/sb1-eggbert-value/](../games/sb1-eggbert-value/README.md) on
`src/`, with `_EGAMES=1`. Inventory: [oracle/sb1-eggbert-value/](../oracle/sb1-eggbert-value/README.md).
All figures below were measured on 2026-11-11 with VC5 SP3 (compiler) and VC5 RTM LINK 5.00.7022.

## What Valueware is

- [EVIDENCE] **The same compile session as Speedy Eggbert 1 and the Speedy Eggbert Demo.** It was
  linked first of the three (15:08:23; demo 15:11:50; Speedy Eggbert 1 15:13:35), with the same linker
  (5.00, no Rich header, a separate `.idata`), and has the same file size and imports. Its `.text` is
  208 bytes longer than Speedy Eggbert 1's (175,459) and its `.data` 32 bytes longer (335,096).
- [EVIDENCE] **The same source with the original `_VALUE` switch on.** `src/` carries that switch
  where the source lineage kept it (`#if _DEMO || _VALUE` in `event.cpp`). With those arms taken, and
  three further changes read from the bytes, every changed function is byte-identical except
  `ChangePhase` and `TreatEventBase`, which differ only as the unchanged retail residues do.
- [EVIDENCE] **The build path is `F:\Profil\sound.cpp`:** all 28 functions of `sound.cpp` are
  STRICT with that path.

## What Valueware changes against Speedy Eggbert 1

All [EVIDENCE], each read from Valueware's bytes and reproduced by the overlay:

| Function | Change | Also in the demo? |
|---|---|---|
| `CDecor::BlupiStep` (decblupi.cpp) | Winning mission 1 sets `m_term = -2` (end of game) instead of 99. | No: the demo ends after mission 40. |
| `CDecor::Write`, `CurrentWrite` (decor.cpp) | Level and save files carry the marker 1. | Yes |
| `CDecor::Read` (decor.cpp) | An official level without the marker is rejected. | Yes |
| `CEvent::DrawButtons` (event.cpp) | Title screen: `Version 1.0` at y 437 and `VALUEWARE` in gold at y 450. | The demo draws `D E M O` and loads other closing texts. |
| `CEvent::NetAdjustLobbyButtons`, `ChangePhase` (event.cpp) | Buttons in their demo state (`SetDemo`) on the lobby, mission-info, player and pause screens. | Yes |
| `CEvent::TreatEventBase` (event.cpp) | Cheat 0 (build official missions) does nothing. | Yes; the demo also drops an Escape shortcut. |
| `CButton::SetDemo`, `CEvent::SetDemo` | Present and called. | Yes |

Unlike the demo, Valueware keeps `ReadConfig` (with the `Benchmark=` and `TrueColor=` keys),
`DrawInfo`, the full game's closing texts and the CD-test behaviour of Speedy Eggbert 1. `GetBlupiInfo`
and `MockeryDetect`, which are order-sensitive in the demo ([EGGBERT_DEMO.md](EGGBERT_DEMO.md)), have
Speedy Eggbert 1's code here.

## Matching

| Tier | Functions | Bytes | Share of game `.text` (141,536) |
|---|---:|---:|---:|
| STRICT + FOLDED (accepted, `tools/target_coverage.py`) | 406 + 6 | 123,232 | 87.1 % |
| EQUIVALENT (certificates) | 3 | 15,456 | 10.9 % |
| Neither | | 2,848 | 2.0 % |

The verifier emits the same 123,232 STRICT + FOLDED bytes.

**EQUIVALENT** ([EVIDENCE], `tools/equivalence_target.py`, unchanged checker; certificates in
`evidence/equivalence/sb1-eggbert-value/`):

- **`ChangePhase`** (7,040 bytes): 32 bytes differ, at the same offsets as in the primary target's
  guard-order residue.
- **`TreatEventBase`** (7,056 bytes): 3 bytes differ, all alignment padding before a jump table
  (`+0x1aa5`).
- **`WindowProc`** (1,360 bytes): the same 3 padding bytes as in Speedy Eggbert 1 (`+0x42d`).

**Neither tier** (2,848 bytes): `DoInit` (2,816 bytes; code bytes equal, 235 relocations verified,
RELOC-UNVERIFIED for the raw comparator until `WindowProc` is STRICT, as in Speedy Eggbert 1) and 32
bytes of DirectX import thunks and padding.

## Whole image, resources and build

- [EVIDENCE] **Ordinary build:** `tools/build.py --target sb1/eggbert/value` builds the executable,
  534,528 bytes (the original's size), digest without its 72 timestamp fields `391361178ad4…`. No
  degradation; the historical sound path was established.
- [EVIDENCE] **Relink of the verified objects** (the regression gate's `tools/relink.py --target`
  run): no unresolved or duplicate symbols; the file has the original's size, and 26,367 bytes differ
  at the same offset (Speedy Eggbert 1: 26,625).

  | Section | Result against the original |
  |---|---|
  | `.text` | virtual size equal; 392 of 411 mapped accepted functions at their original address |
  | `.rdata` | 1,532 of 1,536 bytes equal at the same offset |
  | `.data` | 329,204 of 329,216 bytes equal; virtual size equal |
  | `.idata` | same DLL order, 128 imports on both sides; 3,044 of 3,446 bytes equal |
  | `.rsrc` | equal except the 71 resource-directory timestamps |

  The functions not at their original address are the `jauge`/`network` order effects known from
  Speedy Eggbert 1 (docs/EGGBERT.md).
- [EVIDENCE] **Resources:** `games/sb1-eggbert-value/res/sb1-eggbert-value.rc` is Valueware's own
  script, generated by `tools/make_rc.py` from the original's resources. The relinked `.rsrc` differs
  from the original in 284 bytes, all of them inside the timestamp fields of the 71 resource
  directories, which RTM CVTRES fills with the current time. This is the state Speedy Eggbert 1 has.
- **No byte-identical whole image is claimed**, as for every target.

## Inventory notes

[EVIDENCE] `oracle/sb1-eggbert-value/` was derived with
`tools/derive_target.py --target sb1/eggbert/value --via sb1/eggbert/en` and then audited; the
corrections are listed in its README.
