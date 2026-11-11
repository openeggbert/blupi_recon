# Speedy Eggbert Demo (eGames, 1999)

Target `sb1/eggbert/demo`, added 2026-11-11. The original is `WCSUP.DLL` of the eGames demo package,
registered at `assets/downloads/eggbert_demo/WCSUP.DLL`: despite its name it is the game executable
(534,528 bytes, SHA-256 `fc071424356a9a270e8abf9852bfa08e7b592374a269bf28083071bcc9dfe52c`, PE link
time 1999-06-24 15:11:50 UTC). It is read statically and never executed. The eGames trial launcher
that starts it (`egames.exe`, `RES1.DLL`, `VB40032.DLL`) is out of scope and was not analysed. The
demo's limits are reproduced as they are; nothing removes or bypasses them.

Source: the whole-file overlay [games/sb1-eggbert-demo/](../games/sb1-eggbert-demo/README.md) on
`src/`, with `_EGAMES=1`. Inventory: [oracle/sb1-eggbert-demo/](../oracle/sb1-eggbert-demo/README.md).
All figures below were measured on 2026-11-11 with VC5 SP3 (compiler) and VC5 RTM LINK 5.00.7022.

## What the demo is

- [EVIDENCE] **The same compile session as Speedy Eggbert 1.** `sb1/eggbert/en` was linked at
  15:13:35 UTC on the same day, 105 seconds later, with the same linker (5.00, no Rich header, a separate
  `.idata`). Both files have the same size, the same `.data` virtual size (335,064) and the same
  imports. The demo's `.text` is 48 bytes shorter (175,203 against 175,251).
- [EVIDENCE] **The same source with the original demo switch on.** Every function that differs from
  Speedy Eggbert 1 in its code differs where the source lineage has a `_DEMO` arm. With those arms
  taken, eight of the ten are byte-identical (STRICT), and `ChangePhase` and `TreatEventBase` differ
  only as the unchanged retail residues do (below). Two more functions differ in instruction order
  alone (below).
- [EVIDENCE] **Not the June 1998 demo's code.** The June demo (`sb1/blupi/demo`) is an earlier
  source revision: no true-colour field in the pixmap class, a single `image\` folder, level revision
  1.2. The 1999 demo has Speedy Eggbert 1's pixmap, `misc.cpp` and image loading unchanged: every
  function of `pixmap.cpp` and `misc.cpp` has the status it has for `sb1/eggbert/en`. That is why this
  target does not set `_DEMO=1`, which in `src/` also selects the June revision's code, and uses an
  overlay instead.
- [EVIDENCE] **The build path is `F:\Profil\sound.cpp`,** as for every SB1 build: all 28 functions
  of `sound.cpp` are STRICT with that path.

## What the demo changes against Speedy Eggbert 1

All [EVIDENCE], each read from the demo's bytes and reproduced by the overlay:

| Function | Change |
|---|---|
| `ReadConfig` (blupi.cpp) | Ends after `MouseType=`. The `Benchmark=` and `TrueColor=` keys are not read; neither string is in the executable. 512 bytes instead of 656. |
| `CDecor::DrawInfo` (decblock.cpp) | The treasure count is hidden only for mission 1, not for every tenth mission. |
| `CDecor::BlupiStep` (decblupi.cpp) | Winning mission 40 ends the game (`m_term = -2`); the full game tests mission 99. |
| `CDecor::Write`, `CurrentWrite` (decor.cpp) | The level and save files carry the marker 1 at offset 4 and offset 12. The level revision stays 1.3. |
| `CDecor::Read` (decor.cpp) | An official level (mission below 200) without that marker is rejected. |
| `CEvent::DrawButtons` (event.cpp) | Title screen: `Version 1.0` at y 437 and `D E M O` in gold at y 450. The closing screen loads `TX_DEMO_END1..4` (199-202) instead of `TX_FULL_END1..4`. |
| `CEvent::NetAdjustLobbyButtons`, `ChangePhase` (event.cpp) | Buttons are put into their demo state with `SetDemo` on the lobby, mission-info, player and pause screens. A failed CD test closes the program instead of asking for the disc. |
| `CEvent::TreatEventBase` (event.cpp) | Cheat 0 (build official missions) does nothing, and Escape in a world hub does not return to the first hub. |
| `CButton::SetDemo`, `CEvent::SetDemo` | Present and called. The retail builds contain neither: no code calls them there, and the linker drops them. |

## Matching

| Tier | Functions | Bytes | Share of game `.text` (141,280) |
|---|---:|---:|---:|
| STRICT + FOLDED (accepted, `tools/target_coverage.py`) | 404 + 6 | 122,448 | 86.7 % |
| EQUIVALENT (certificates) | 4 | 15,456 | 10.9 % |
| Neither | | 3,376 | 2.4 % |

The verifier emits the same 122,448 STRICT + FOLDED bytes.

**EQUIVALENT** ([EVIDENCE], `tools/equivalence_target.py`, unchanged checker; certificates in
`evidence/equivalence/sb1-eggbert-demo/`):

- **`ChangePhase`** (7,040 bytes): 32 bytes differ, at the same offsets as in the primary target's
  guard-order residue. The 247 further bytes that differ in Speedy Eggbert 1 do not differ here.
- **`TreatEventBase`** (6,976 bytes): 3 bytes differ, all alignment padding before a jump table
  (`+0x1a5d`). The 8 other bytes that differ in Speedy Eggbert 1 lie in the cheat-0 code, which the
  demo does not have.
- **`WindowProc`** (1,360 bytes): the same 3 padding bytes as in Speedy Eggbert 1 (`+0x42d`).
- **`GetBlupiInfo`** (80 bytes): two loads in the other order (general region proof).

**Neither tier** (3,376 bytes):

- **`DoInit`** (2,816 bytes): its code bytes are equal and its 235 relocations are verified. As in
  Speedy Eggbert 1 it stays RELOC-UNVERIFIED for the raw comparator, because its reference to
  `WindowProc` is proven only once `WindowProc` is STRICT.
- **`MockeryDetect`** (528 bytes in the original, 532 from the source): see the next section. The
  certificate checker requires equal extents, so it cannot certify this function.
- **32 bytes** of DirectX import thunks and padding, as in every target.

## The two order-sensitive functions

[EVIDENCE] `CDecor::GetBlupiInfo` (decblock.cpp) and `CDecor::MockeryDetect` (decmove.cpp) are STRICT
for Speedy Eggbert 1 and differ in the demo, although their source cannot differ: `decmove.cpp` has
no function that changes in the demo. In the demo `GetBlupiInfo` loads `m_blupiTank` before
`m_blupiJeep`, and `MockeryDetect` computes the cell index in another instruction order.

[EVIDENCE] Both are the declaration-count effect of [SYMBOL_ORDER.md](SYMBOL_ORDER.md): VC5 orders
equal-cost operands by a hash of the compiler's running symbol number. Diagnostic sweeps
(2026-11-11; probe declarations in a scratch copy, never in source):

| Function | Unmasked differing bytes with N probe declarations before it |
|---|---|
| `GetBlupiInfo` | 0 for N = 2-5 and 10-13; 2 for N = 0-1, 6-9, 14-19. Period 8. |
| `MockeryDetect` | 0 for N = 239-494; 452 for N = 0-238 and from 495. Period 512. The result is the same with the probes at the top of the file, between any two includes, or directly before the function. |

So the demo's compiler had counted a different number of symbols before these two functions than
Speedy Eggbert 1's did, while the three executables of that day (Valueware 15:08, demo 15:11, Speedy
Eggbert 1 15:13) can differ only in the edition switches. Valueware has Speedy Eggbert 1's order in
both functions.

[EVIDENCE, negative] One large shift in the shared header does not explain it: 243, 244, 251 or 252
probes in `def.h` make both functions STRICT and change the status of 14 to 15 others (`BltFast`,
`QuickIcon`, `CreateSoundBuffer`, `ModifDecor`, `DrawButtons`, `IsWorld` and more become MISMATCH).

[OPEN] Which real declarations or statements the demo configuration added or removed. The reconstructed
headers are a candidate declaration set (docs/HEADERS.md), so where the retail builds sit inside their
windows is itself unknown, and a difference of a few symbols would be enough. No evidence identifies
it. The repo's rules forbid a declaration whose only purpose is to move the count, so both functions
stay as they are: `GetBlupiInfo` EQUIVALENT, `MockeryDetect` uncovered. The June 1998 demo was built
with the same switch; its matching (docs/SB1_DEMO.md) may supply the missing evidence.

## Whole image, resources and build

- [EVIDENCE] **Ordinary build:** `tools/build.py --target sb1/eggbert/demo` builds `WCSUP.DLL`,
  534,528 bytes (the original's size), digest without its 72 timestamp fields `2a26c4245e7e…`. No
  degradation; the historical sound path was established.
- [EVIDENCE] **Relink of the verified objects** (the regression gate's `tools/relink.py --target`
  run): no unresolved or duplicate symbols; the file has the original's size, and 96,755 bytes differ
  at the same offset.

  | Section | Result against the original |
  |---|---|
  | `.text` | virtual size +16; 158 of 409 mapped accepted functions at their original address |
  | `.rdata` | 1,503 of 1,536 bytes equal at the same offset |
  | `.data` | 329,202 of 329,216 bytes equal; virtual size equal |
  | `.idata` | same DLL order, 128 imports on both sides; 3,044 of 3,446 bytes equal |
  | `.rsrc` | equal except the 71 resource-directory timestamps |

  Every function up to `MockeryDetect` is at its original address. `MockeryDetect` is 16 bytes longer
  in the relink than in the original, which moves the functions after it; that one function accounts
  for most of the 96,755 bytes (Speedy Eggbert 1's relink differs in 26,625). The remaining differences
  are the `jauge`/`network` order effects known from Speedy Eggbert 1 (docs/EGGBERT.md).
- [EVIDENCE] **Resources:** `games/sb1-eggbert-demo/res/sb1-eggbert-demo.rc` is the demo's own
  script, generated by `tools/make_rc.py` from the original's resources. The relinked `.rsrc` differs
  from the original in 284 bytes, all of them inside the timestamp fields of the 71 resource
  directories, which RTM CVTRES fills with the current time. This is the state Speedy Eggbert 1 has.
- **No byte-identical whole image is claimed**, as for every target.

## Inventory notes

[EVIDENCE] `oracle/sb1-eggbert-demo/` was derived with
`tools/derive_target.py --target sb1/eggbert/demo --via sb1/eggbert/en` and then audited; the three
corrections are listed in its README. The `--via` mode is new: it locates the functions through
Speedy Eggbert 1's inventory, which is closer to the demo than the primary target is, and still writes
an address map from the primary target's addresses. Without it the tool placed `ChangePhase` 112 bytes
too late.
