# Status

The single current status page for every historical executable and the SDL3 port, as of
**2026-10-25** on branch `sb2-matching`. The rows for the Speedy Eggbert Demo and Valueware were added on
**2026-11-11** (branch `editions`), fresh on that day. The function-matching, certificate and relink figures of the 14 earlier targets were
re-run on that day with identical results. On **2026-11-12** the
Russian edition was documented ([EGGBERT_RU.md](EGGBERT_RU.md)), the port's provenance map was
updated, this page moved to `main`, and the port section (5) was re-run for port 1.5.0, and again
for port 1.5.2 (`wss://` on the desktop and Android) the same day.
 Each figure is marked **fresh** (re-run for this page) or
**doc** (taken from the cited document). Progress is reported per executable; no figure here is a
combined metric. The previous SB1-only page is preserved as
[research/SB1_STATUS_2026-10-19.md](research/SB1_STATUS_2026-10-19.md).

The four kinds of milestone below are independent claims:

- STRICT coverage;
- EQUIVALENT certificates;
- whole-image identity;
- port fidelity.

A built executable is none of them ([BUILDING.md](BUILDING.md)).

## 1. Function matching: STRICT

Fresh: `tools/target_coverage.py --target <t>` over the accepted manifests. The bytes are unique game
`.text` bytes; FOLDED bodies count once, under their owner.

| Executable | Target(s) | STRICT + FOLDED records | STRICT bytes / game .text |
|---|---|---:|---:|
| Speedy Blupi 1 (1998), US | `sb1/blupi/us` (primary) | 407 + 6 | 127,456 / 141,488 (90.1%) |
| Speedy Blupi 1, German | `sb1/blupi/de` | 407 + 6 | 127,456 / 141,488 (90.1%) |
| Speedy Blupi 1, French | `sb1/blupi/fr` | 407 + 6 | 127,456 / 141,488 (90.1%) |
| Speedy Eggbert 1 (1999) | `sb1/eggbert/en`, `de`, `es`, `fr`, `it`, `pt` (each) | 403 + 6 | 123,088 / 141,328 (87.1%) |
| Speedy Eggbert Demo (1999) | `sb1/eggbert/demo` | 404 + 6 | 122,448 / 141,280 (86.7%) |
| Speedy Eggbert SE Valueware (1999) | `sb1/eggbert/value` | 406 + 6 | 123,232 / 141,536 (87.1%) |
| Speedy Blupi 1 June 1998 demo | `sb1/blupi/demo` (own tree since 2026-11-11) | 406 + 5 | 132,672 / 140,784 (94.2%) |
| Speedy Blupi 2 (2001) | `sb2/blupi/en`, `de`, `fr` (each) | 452 + 5 | 122,240 / 173,072 (70.6%) |
| Speedy Eggbert 2 (2004) | `sb2/eggbert/en` | 450 + 12 | 136,448 / 172,992 (78.9%) |

Notes:

- **The June demo is a completion target since 2026-11-11** (owner decision of 2026-11-09). It has its
  own source tree, and the verifier emits the same 132,672 bytes it accepts (docs/SB1_DEMO.md).
- **Reaktivniy Egbert v1.1 (Russian) is not a target.** Its `.text` and `.rdata` equal the original of
  `sb1/eggbert/en`; everything else is a third-party change made after linking, including a CD check
  ([EGGBERT_RU.md](EGGBERT_RU.md)).
- **Earlier pages give the primary as "411 STRICT".** That is the verifier's emitted-record sum,
  which counts repeated bodies; there are 407 unique accepted records.

## 2. EQUIVALENT certificates

Fresh. EQUIVALENT means static local behavioural equivalence under pinned certificate assumptions. It
never counts as STRICT, and the byte status stays MISMATCH ([VERIFYING.md](VERIFYING.md)).

| Executable | Target-tier certificates | Bytes | Not covered by STRICT, FOLDED or EQUIVALENT |
|---|---:|---:|---:|
| SB1 US | 0 (the 2 standalone certificates are below) | 0 | 14,032 B |
| SB1 German, French | 2 each: ChangePhase, TreatEventBase | 14,000 | 32 B |
| Eggbert 1 (each language) | 3: ChangePhase, TreatEventBase, WindowProc | 15,360 | 2,880 B |
| Eggbert Demo | 4: ChangePhase, TreatEventBase, WindowProc, GetBlupiInfo | 15,456 | 3,376 B |
| Valueware | 3: ChangePhase, TreatEventBase, WindowProc | 15,456 | 2,848 B |
| SB1 demo | 4 (variant checker): ChangePhase, QuickIcon, GetBlupiInfo, MockeryDetect | 8,080 | 32 B |
| Blupi 2 (each language) | 9 | 50,800 | 32 B |
| Eggbert 2 | 4 | 36,304 | 240 B |

- **SB1 US has two standalone certificates.** ChangePhase (6,928 B) and TreatEventBase (7,072 B)
  are checked by `tools/equivalence.py` on every primary verify, and both passed fresh on
  2026-10-25 ([STANDALONE.md](STANDALONE.md)). `equivalence_target.py` intentionally declares no
  target tier for the primary. Of the 14,032 uncovered bytes, 14,000 are these two functions; the
  rest are 32 bytes of DirectX import thunks and padding.
- **The 32-byte remainders** of SB1 German/French and Blupi 2 are that same linker output.
- **Eggbert 1's remaining bytes** include DoInit (relocations verified, raw comparator
  RELOC-UNVERIFIED).
- **The Eggbert Demo's and Valueware's remaining bytes** are DoInit in the same state, and in the
  demo MockeryDetect, whose instruction order depends on the declaration count
  ([EGGBERT_DEMO.md](EGGBERT_DEMO.md), [EGGBERT_VALUE.md](EGGBERT_VALUE.md)).
- **The SB1 demo's four certificates** (2026-11-12) are checked by `tools/equivalence_variant.py`
  ([SB1_DEMO.md](SB1_DEMO.md)); its remaining 32 bytes are thunks.
- **Eggbert 2's 240 bytes** are CreateSoundBuffer (208 B, an `[OPEN]` sound.cpp line difference;
  docs/SB2.md) plus 32 bytes of thunks.

## 3. Whole image

**Fresh relinks (2026-10-25).** These are the stage 1 regression-gate relinks
(`tools/regression_gate.py`): the verified objects in the oracle-derived TU order, compared statically
with each original.

- **The primary relink runs under the historical host clock** (`--historical-host`). It is
  `557ca92e…3533`, unchanged.
- **The other relinks use the current clock.** Their PE and resource timestamp bytes therefore
  differ too.

**No executable is byte-identical to its original.**

| Executable | File size (orig / relink) | Same-offset differing bytes | Byte-exact sections | `.data` equal bytes, virtual-size delta | Accepted functions at original RVA |
|---|---|---:|---|---|---:|
| SB1 US | 535,552 / 535,552 | 121,401 | `.rsrc` | 329,727 / 329,728, 0 | 131 / 412 |
| SB1 German, French | 538,112 / 538,112 | 26,261 each | `.rsrc` | 329,715 / 329,728, 0 | 394 / 412 |
| Eggbert 1 (en, de, es, it, pt; fr) | equal for each language | 26,625; fr 26,553 | none (`.rsrc` differs in timestamps only, doc) | 329,202 / 329,216, 0 | 390 / 408 |
| Eggbert Demo | 534,528 / 534,528 | 96,755 | none (`.rsrc` differs in timestamps only, fresh) | 329,202 / 329,216, 0 | 158 / 409 |
| Valueware | 534,528 / 534,528 | 26,367 | none (`.rsrc` differs in timestamps only, fresh) | 329,204 / 329,216, 0 | 392 / 411 |
| SB1 demo (2026-11-11, fresh) | 533,504 / 533,504 | 135,793 | none (`.rsrc` differs in timestamps only) | 328,685 / 328,704, +32 | 90 / 410 |
| Blupi 2 (each language) | equal for each language | 47,503 | `.rdata` | 347,915 / 349,184, 0 | 339 / 456 |
| Eggbert 2 | 602,624 / 602,624 | 164,336 | `.rsrc` | 347,487 / 363,520, 0 | 87 / 461 |

**Causes** (doc):

- **SB1 US:** almost all of its differences are a +16 shift. LINK keeps a different copy of a folded
  empty function (an ICF survivor), plus the CRT filter survivor and the import order. Its Rich count
  is 268 against 270 ([SB1_CLOSURE.md](SB1_CLOSURE.md), [BLOCKER_CHECKS.md](BLOCKER_CHECKS.md)).
- **The original linker inputs are underdetermined,** and that limit applies to every target.
- **SB1 demo:** its own resource script since 2026-11-11; `.rsrc` differs in the directory timestamps
  only. LINK keeps a different copy of a folded destructor and `ChangePhase` is 16 bytes longer, which
  moves the code after them (docs/SB1_DEMO.md).
- **Eggbert Demo:** MockeryDetect is 16 bytes longer than the original's, which moves every function
  after it ([EGGBERT_DEMO.md](EGGBERT_DEMO.md)).

**SB2 `.data` virtual size.** [EVIDENCE] The fresh relinks show the same `.data` virtual size as the
originals for all four SB2 targets. docs/SB2.md round 20 had recorded +32. [OPEN] The cause of the
change was not investigated in this consolidation; the hotspot-table correction (`96afd2e`) followed
that round.

**Experimental witness.** Branch `sb1-histlink` holds a 412/412-at-RVA linker-input candidate. It is
evidence only, not recovered source (SB1_CLOSURE.md).

## 4. Ordinary historical build

Fresh: consolidation stage 1, on commit `a4a2352`. Details are in [BUILDING.md](BUILDING.md).

- **The Speedy Eggbert Demo and Valueware build too** (2026-11-11), each with its own resource script;
  `--all` then builds 16 executables.
- **All 14 executables build with `tools/build.py`,** using their historical compiler and linker.
  No original executable, oracle, certificate or research tool is read; `tests/test_build_boundary.py`
  enforces this.
- **The clean-export test passed:** `tools/check_clean_build.py`, a pruned `git archive` with no
  assets or oracle.
  - Every image equals an independent build apart from its PE and resource-directory timestamps.
  - Every toolchain component matched `toolchains.json`.
  - Every historical `__FILE__` path was established.
- **The SB1 demo builds with its own resource script** since 2026-11-11; before, it used the retail
  script and was labelled PARTIAL.

## 5. SDL3 port

The port is five native programs: `blupi-sdl3` (SB1, Eggbert 1), `blupi2-sdl3` (Blupi 2, Eggbert 2),
and since 1.3.0 `eggbert-demo-sdl3`, `eggbert-value-sdl3` and `blupi-demo-sdl3` (the editions with
source of their own). They compile the historical game TUs unchanged: 12 from `src/` and 11 from
`games/sb2/src/` ([port/README.md](../port/README.md)). Release 1.5.2 has Windows, Linux, Android and
web packages.

**Fresh results (2026-11-12, port 1.5.2, tag `port-v1.5.2`):**

- **Linux test suite: 285 / 285 passed** in the release build (`port/tools/linux_release.sh`, `ctest -j 1`,
  3,050 s), with the new `net_tls` (eight certificate and transport cases against free-direct-server behind
  a TLS front) and `net_sb1_relay_wss` (the relay game over `wss://`).
- **Windows:** 34 / 34 of the network, TLS, launcher and golden subset under Wine; `test_net_tls` reached
  `wss://fds.openeggbert.com` through the Windows certificate check and refused badssl.com's bad
  certificates.
- **Web:** unchanged code; the five programs pass in Chrome and Firefox, and the relay game in both.
- **Android:** built and signed, not run.

**Fresh results (2026-11-12, port 1.5.0, branch `port-1.5.0`):**

- **Linux test suite: 279 / 279 passed** (`ctest -j 4`, 1,436 s), with unplayed copies of every
  edition's data, Blupi 2 included, and the Free Direct server tests (`net_sb1_relay`, `net_sb2_relay`).
- **Windows:** the suite of the MinGW-w64 cross build under Wine (`port/tools/windows_release.sh`):
  see port/docs/RELEASING.md, 1.5.0. On Windows 10 (VM), 1.4.0's package replayed the tour, editor and
  German tour goldens and Speedy Eggbert 2's attract mode exactly.
- **Web:** all five programs import a game folder, start and start again from the browser's storage in
  Chrome and in Firefox 140 ESR (`port/tools/web_check.mjs`); a browser and a native Linux program
  play one game through a Free Direct server in both browsers (`--relay`).
- **Android:** the APK is built and signed, not run (no device or emulator on this host).
- **Goldens:** recorded on unplayed data since 2026-11-12. Five of them had been recorded on a copy
  whose `DATA` held the game's own player and user-mission files (`INFO001.BLP`, `U001-*.BLP`), which
  the game reads too; they were re-recorded after showing that 1.2.0 and the current code give the
  same traces on unplayed data (port commit messages of `port-tests`).
- **The provenance map passes:** `port/tools/check_fidelity.py --check`, with 141 edges and 0 issues.
  The categories describe source relationships, not percentages of fidelity:

| Target | Direct shared | Verified derivative | Verified platform replacement | Intentional difference | Unverified | Edges |
|---|---:|---:|---:|---:|---:|---:|
| SB1 US, German, French (each) | 12 | 24 | 16 | 13 | 6 | 71 |
| Eggbert 1 (each language) | 12 | 24 | 16 | 14 | 6 | 72 |
| Blupi 2 (each language) | 11 | 30 | 17 | 15 | 7 | 80 |
| Eggbert 2 | 11 | 27 | 17 | 15 | 7 | 77 |
| SB1 demo | not supported by the port (F454) | | | | | |

**Runtime evidence** (doc: [port/docs/ORACLE.md](../port/docs/ORACLE.md) and
[port/oracle/SB1_SAVE.md](../port/oracle/SB1_SAVE.md)):

- **Cross-build display oracle against the rebuilt historical executables.** Attract mode is 2,200 of
  2,200 frames identical for SB1 and for each SB2 edition, at 8 and 16 bpp.
- **SB1 screen tour:** identical except for the multiplayer provider list.
- **SB1 US save interchange:** identical in one recorded scenario, at both depths.

These are bounded receipts, not whole-game certification.

**The port is not functionally complete.** These services are unavailable or differ:

- **Multiplayer:** both games have native multiplayer since port 1.2.0
  ([port/docs/MULTIPLAYER.md](../port/docs/MULTIPLAYER.md)): two to four players across Windows, Linux
  and Android, with LAN discovery, a content check and `--net-join`; an in-game address entry since
  1.3.0; and since 1.5.0 play through a Free Direct server (rooms with secret IDs, no open ports), which
  is also the browser's way. Tested automatically on one machine and, for each game, by the user between
  real devices (1.2.0); little other real-world play.
- **AVI movies:** not played; the phase is skipped.
- **SB2 CD audio:** plays ripped tracks the player supplies (`music/trackNN.wav`, port 1.3.0; also `.ogg` and `.flac` after 1.5.2); without
  them it fails and logs once. No archive holds the tracks.
- **MIDI:** played with a modern SoundFont, not the 1998 device's timbre.
- **Coverage gaps:** hardware gamepads and Android keyboard/background are open. SB2 on Android
  (since 1.2.0-rc2) is checked on the emulator (start, menus, touch controls, play) and by the user's
  multiplayer test.

## 6. Where work stands

- **Matching.** The SB1 and SB2 function residues are at the limit of the current evidence: the
  open functions need authentic source or context evidence (docs/SB1_CLOSURE.md, docs/SB2.md
  section 12). Matching research resumes only on new evidence.
- **Whole image.** The original linker inputs (dead contributions, TU population, order) are
  underdetermined by the binaries. Experimental witnesses, such as branch `sb1-histlink`, are
  evidence, not recovered source.
- **Consolidation.** Stages 1 (ordinary builds), 2 (this documentation) and 3 (port cleanup: Linux
  release and test portability, Android import, port docs; `083944a`) are done. Stages 4 and 5 are
  optional (docs/CONSOLIDATION_PLAN.md).
- **Port.** 1.5.2 ships the five programs on Windows, Linux and the web (the two games on Android),
  with multiplayer on a LAN, by address, and through a Free Direct server (`ws://` or `wss://`). Open: running the Android
  build on a device, real-network fault tests, and the optional consolidation stage 4 (shared
  language, pixmap and audio services).
