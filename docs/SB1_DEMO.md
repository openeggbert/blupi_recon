# June 1998 Speedy Blupi demo

[POLICY 2026-10-24] The demo is evidence for the retail SB1 reconstruction, not a completion target. Demo work
must name the unresolved retail problem it addresses, the evidence expected, how it would change the retail
reconstruction, what would falsify it and when to stop; matching unrelated demo code is out of scope.

[POLICY 2026-11-09, owner decision] The June demo becomes a completion target: it is matched for its own sake,
to the level the retail editions have. The policy of 2026-10-24 above is kept as a record and no longer limits
demo work. Since 2026-11-11 the target has its own source tree, `games/sb1-blupi-demo/`
([its README](../games/sb1-blupi-demo/README.md)); the `_DEMO` arms in `src/` stay as they are but no target
compiles them any more. The sections below this line were written before that date and describe the earlier,
evidence-only configuration, except where a 2026-11-11 heading says otherwise.

[EVIDENCE] `sb1/blupi/demo` is the tenth SB1 target, an earlier source revision rather than a retail
language. The static oracle is `assets/downloads/openeggbert/Speedy_Blupi_Demo/exe/Blupi.exe`
(owner-approved openeggbert download, see docs/ORACLE.md), 533,504 bytes, SHA-256
`06faf60adf7585ade95dbeeeedcf0fc5e58decb46ef08d8e0adf111f34053fd7`, registered at that path in
`targets.json`. It is never executed.

## Completion work (2026-11-11)

[EVIDENCE] All figures in this section were measured on 2026-11-11 (VC5 RTM compiler and LINK 5.00.7022,
DirectX 5 SDK), with `tools/verify.py --target sb1/blupi/demo` and `tools/target_coverage.py`.

| | Before (2026-10-25) | After |
|---|---:|---:|
| Accepted STRICT + FOLDED | 372 + 3, 73,248 bytes (52.0 %) | **406 + 5, 132,672 bytes (94.2 %)** |
| Emitted by the verifier | 89,296 | 132,672 |
| Resources | the retail script, PARTIAL | the demo's own script |
| Ordinary build | 534,016 bytes | 533,504 bytes, the original's size |

**Own tree.** The target compiles `games/sb1-blupi-demo/src/` with its own manifest
`games/sb1-blupi-demo/recon.json` and inventory `oracle/sb1-blupi-demo-tree/`. The tree was seeded
from `src/` with every `_DEMO` arm resolved into demo code. Before any change it reproduced the earlier
result exactly: 89,296 bytes emitted, and every function had the same status as in the run on the
unchanged base.

**What the June demo differs in** ([EVIDENCE], each read from the executable's bytes):

| Where | June demo |
|---|---|
| `Phase` (event.h) and the screen table | Image names are plain `image\x.blp` in a 20-character field, there is no `bLocked` field, the setup screens have two more buttons (movies on/off), and the insert-CD screen's button leads to quitting. All 38 table entries were checked against the executable's data. |
| `CEvent::ChangePhase` | The background name is copied with `strcpy`, not formatted. A failed background load, a failed mission read and a failed CD test all close the program (`PostMessage(WM_CLOSE)`). The two movie buttons are in demo state on the setup screens. The music guard tests `m_bDemoPlay` first. |
| `CEvent::DrawButtons` | `Version 0.8`; the win screens show the demo's closing texts (199, 290-296); the setup screens show the movie setting (yes/no); the save-slot numbers are drawn through `pos`. |
| `CEvent::TreatEventBase` | No cheat 0, no Escape shortcut from a world hub, no CD check before playing. |
| `CEvent::DemoPlayStart` | The demo files are read from the CD path. |
| `CDecor::BlupiStep` | Bubbles every 100 frames (at 0 and 38) instead of every 70; winning mission 40 ends the game; two explosions do not shake the screen. |
| `CDecor::MoveObjectStep` | Two explosions do not shake the screen. |
| `table_blupi`, `table_mirror` (decblock.cpp) | One animation of 30 instead of 32 frames; five icons are their own mirror image. |

**What remains** (8,112 bytes, 5.8 %):

- [EVIDENCE] **`ChangePhase`** (6,688 bytes): 1,922 of its 1,956 instructions are equal. The rest is
  register allocation: the compiler keeps the constants 0 and `WM_PHASE_PLAY` in `edi` and `ebx` the
  other way round than the original. It is the same kind of residue as the retail guard residue that
  has only equivalence certificates. The compiler-tracing tools (`tools/c2trace`) need a VC6 tree,
  which this environment does not have.
- [EVIDENCE] **`QuickIcon`** (784 bytes, 8 bytes differ) and **`GetBlupiInfo`** (80 bytes, 2 bytes
  differ): the order of two operands. Five equivalent spellings of `QuickIcon`'s lines compile
  identically, so this is the declaration-count effect of [SYMBOL_ORDER.md](SYMBOL_ORDER.md).
  Diagnostic probes (never source) make `GetBlupiInfo` match with 4 to 7 more declarations before
  it; `QuickIcon` did not flip within 70.
- [EVIDENCE] **`MockeryDetect`** (528 bytes): the same declaration-count effect as in the Speedy
  Eggbert Demo ([EGGBERT_DEMO.md](EGGBERT_DEMO.md)).
- **32 bytes** of DirectX import thunks and padding, as in every target.
- **All four functions have EQUIVALENT certificates since 2026-11-12** (next section).

**Whole image** ([EVIDENCE], relink of the verified objects): same file size, same import order
(128 imports), `.rsrc` equal except the 71 resource-directory timestamps, and `.data`
initialized content equal except 19 pointers into code that the relink places differently. [OPEN]
`.data` is 32 bytes larger in virtual size, and `.text` 32 bytes larger: the folded destructor copy
that LINK keeps and `ChangePhase` (16 bytes longer) move the code after them.


## Equivalence certificates (2026-11-12)

[EVIDENCE] The four residues above are EQUIVALENT: static local behavioural equivalence, with each
byte status still MISMATCH ([VERIFYING.md](VERIFYING.md)). They were checked by
`tools/verify.py --target sb1/blupi/demo` and counted with `tools/target_coverage.py`. The
certificates are in `evidence/equivalence/sb1-blupi-demo/`.

| Function | Original / candidate extent | Proof | Regions | Verified relocations |
|---|---:|---|---:|---:|
| `GetBlupiInfo` | 80 / 80 | region proof | 1 | 0 |
| `QuickIcon` | 784 / 784 | region proof | 2 | 29 |
| `MockeryDetect` | 528 / 544 | region proof | 2 | 1 |
| `ChangePhase` | 6,688 / 6,704 | region proof with whole-CFG constant invariants (as SB2's `ChangePhase`) | 26 | 227 |

| Tier | Bytes of game `.text` (140,784) |
|---|---:|
| STRICT + FOLDED | 132,672 (94.2 %) |
| EQUIVALENT | 8,080 (5.7 %) |
| Neither | 32 (DirectX import thunks and padding) |

**Why a separate checker.** The certificates are checked by `tools/equivalence_variant.py`, which this
target declares (`"equivalence_checker": "equivalence_variant"` in `targets.json`).
`tools/equivalence_target.py` is pinned by hash in every existing certificate, so it stays unchanged.
The variant uses its proof functions unchanged and differs in exactly two points:

1. **The SB1 getter identity is skipped when it does not apply.** `equivalence_target` binds
   `CPixmap::GetTrueColorBack` to the STRICT body of `CSound::GetEnable` for every SB1 target. The June
   pixmap class has no true-colour field and no such method. The variant skips the binding only when no
   object defines the getter and no relocation names it, so no proof can depend on it.
2. **Unequal extents are allowed when declared.** `ChangePhase` and `MockeryDetect` are 16 bytes longer
   than the originals. The region proof already maps the end of one body onto the end of the other. A
   declaration gives `candidate_size` next to the original's `size`, and only when the two differ.
   Coverage counts the original's size.

Each certificate pins `equivalence_target.py` (`checker_sha256`) and the variant
(`variant_checker_sha256`), and records which of the two points applied. All other targets still use
`equivalence_target.py`, and the all-target regression comparison shows their reports unchanged.

## Target and toolchain

[EVIDENCE, 2026-10-25] Two fresh figures with different denominators:

| Figure | Value | Source |
|---|---|---|
| Accepted-manifest coverage | **73,248 / 140,784** (372 STRICT + 3 FOLDED) | `tools/target_coverage.py` |
| STRICT emitted by the verifier | 89,296 | `tools/verify.py --target sb1/blupi/demo`, all TUs |

The demo-only STRICT bodies are verified but not promoted into an accepted record, because the
demo is an evidence-only target. The figures below are earlier snapshots.

[EVIDENCE] The ordinary verifier works with `--target sb1/blupi/demo`. The effective manifest has
375 expectations, including the demo's differently mangled `CPixmap::Create`. Fresh verification
has no accepted regression and covers **76,768 / 140,784** bytes in this sandbox; the historical
sound-path gate remains **INCOMPLETE**. The final 32 denominator bytes are DirectX thunks/padding,
as in the primary metric. Observed emitted records: 382 STRICT, 3 FOLDED, 4 RELOC-UNVERIFIED,
69 MISMATCH (458 total, including unused definitions). These are different denominators from the
older 343/408 relocation-masked comparison in [ORACLE.md](ORACLE.md). Unresolved demo bodies have
no inherited retail acceptance or retail equivalence certificate.

[EVIDENCE] VC5 **RTM 11.00.7022** is distinguished from tested SP2/SP3 by ten complete masked body
matches before the new source switches: DDCopyBitmap, DDColorMatch, CMenu::TreatEvent, four network
methods, two sound methods, and WindowProc. Seven RTM LIBC contributions match where both SP2 and
SP3 fail: `__isctype`, `tolower`, `__InternalCxxFrameHandler`, `__setmbcp`, and the A forms of CRT
CompareString, LCMapString, GetStringType. This is evidence for RTM libraries, independently of
the game compiler. The pinned LINK **5.00.7022** and DX5 libraries are the supported reproducing
configuration. Among the installed LINK candidates, RTM is the one with the 5.00 banner matching
the demo's PE version; the other candidates report 5.02, 5.10 or 5.11. An unknown unpublished
5.00 build cannot be distinguished by that header alone. `scratch/w-demo10/toolchain.json` pins
the tested binaries/libraries and retains complete section fingerprints.

[EVIDENCE] `_DEMO=1` is build-wide; `def.h` defaults it to FALSE. The June pixmap has no true-color
field, its following fields are four bytes earlier, and Create has no true-color parameter. The
switches reproduce Create's entire 832-byte contribution and most affected pixmap bodies. The
release arms retain their original tokens. All nine existing targets have identical complete
function reports before/after under like sandbox conditions, excluding only output-path envelopes.
Both primary relinks are exactly `557ca92e6ec8c71cfaf5e9f2d55969d3ebcd614c771c9010e074c6ad82ba3533`.
Those link controls explicitly reuse the retained, unchanged historical-path sound object; fresh
verifiers retain the expected sound limitation.

## Independent linker constraints

[EVIDENCE] Physical June owners differ from the retail links: button destructor at `0x2970`,
network GetNbSessions at `0x20a00`, sound GetEnable at `0x22b00`, **FrameUnwindFilter** at
`0x270a0`, and **aw_cmp.obj's _strncnt** at `0x2b3e0`. The last two follow from complete RTM
member-body hits and their adjacent contributions, not from importing the retail owner choices.
Entry is `0x24a60`; callbacks precede their callers. Same-object CallMember aliases and the empty
event class do not identify source names independently.

[EVIDENCE] Binary relocation recovery includes image-pointer instruction operands, embedded
switch-table pointers, rel32 calls/jumps to other contributions, and `FS:[0]` exception-chain
references. It recovers **every fixup site in 419/419 US and 418/418 Eggbert** checked bodies.
Independent next-start extents agree in 419/419 US and 417/418 Eggbert cases; the exception is an
inventory hole after Eggbert FlushGame (its omitted changed WindowProc), not a usable extent.
The June vector has 786 executable COMDAT entries: compiled contributions plus 23 binary-derived
live keys. Its unobserved canonical definitions and CRT closure remain assumptions. Complete
vectors, decoded sites, explicit unknowns and validation are in `scratch/w-demo10/`.

[EVIDENCE] The recovered RTM qsort exactly replays all 786 baseline entries and all 792 entries of
the six-method diagnostic. A forecast frozen before a repeated real link matches every input and
sorted position; all 12 folds in seven classes also follow the class-first rule. No model vector
is supplied to LINK.

| Bounded family | US | US + Eggbert | + June demo |
|---|---:|---:|---:|
| Fixed eight-helper roster, all 256 subsets, full physical constraints | 1 | 0 | 0 |
| Same roster, game owners only | 27 (20 inclusion-minimal) | 0 | 0 |
| 42-method pool, all dependency-closed subsets through six (1,483,685) | 4,053 minimum-six | 3 | **0** |
| Established +6 dead-import encounter pattern | 1 witness | 1 | **1** |

[NEGATIVE] All three six-method solutions lose the demo's getter, hide and empty owners, even
without its CRT constraints. Their eight relevant candidate keys were freshly remeasured with
RTM and are unchanged. This rejects a **common population under the stated base assumptions**;
it does not prove those methods were absent in a later retail source revision. The source
realizations also change MouseMove by 7 bytes and IsVentillo by 371 bytes in every image. No
tested family satisfies all three links while preserving every existing payload.

[EVIDENCE] One hidden KERNEL32 contribution after CreateDirectoryA, two USER32 contributions
between InvalidateRect and GetClientRect, and three after wsprintfA yield **128/128 import slots
on all three images**, versus 15/128 for the baselines. A real RTM demo link confirms 128/128.
RTM's import comparator is at `0x4062f0` (SP3: `0x40f8d0`); both independent import sorts replay
exactly. This retains the encounter-pattern witness, not a unique inverse or API identification.
The real probes use OutputDebugStringA, SetTimer/KillTimer and diagnostic GetDC/GetWindowRect/
ReleaseDC calls. The latter three have no demonstrated ancestry cause. The demo has **no Rich
header**, so it adds no loaded-module count capable of deciding the US +6-import / -4-module
arithmetic or selecting an original TU partition.

[OPEN] Original dead population, complete original CRT/member closure, larger subsets (seven or
more), unconstrained dead-import caller keys, earlier source-specific dead keys, original TU partition, and remaining demo code/data/
resources are unresolved. The conditional key model is not an authenticated original input census.
See [SB1_CLOSURE.md](SB1_CLOSURE.md) and `scratch/w-demo10/REPORT.md` for raw real-link numbers,
failed gates, reproduction commands and the remaining boundaries. No matching acceptance status
for any existing target changed.

## Demo matching and demo-only code (2026-10-24, `scratch/w-demolive/REPORT.md`)

[EVIDENCE] `_DEMO` arms for June differences read from the demo's bytes: the single `image\` folder (DoInit cache
paths, LoadImages), the CD-test first phase in DoInit, ReadConfig ending after `MouseType` (no benchmark or
true-colour keys), the treasure count hidden only for official mission 1, and DecorNextAction without the retail edge
clamp. 18 more functions are STRICT: 76,976 -> 89,296 / 140,784 bytes. All nine retail reports and payloads are
unchanged, certificates pass, the primary relink stays `557ca92e…`, and the port's SB1 golden tests pass.

[EVIDENCE] Census: 410 game bodies and four DirectX thunks. The only two demo-only methods were already reconstructed
and already emitted, REF-dead, in every retail build (they disappear from the retail maps). The demo has 128 imports,
none absent from all retail editions (`GetCurrentDirectoryA`, from June ReadConfig, is imported by Eggbert 1).
[NEGATIVE] So demo code supplies neither the retail dead population nor the six dead imports. Fourteen changed live
June keys remain binary-derived.
