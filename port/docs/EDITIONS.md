# Edition programs: the demos and Valueware (since 1.3.0)

Three editions of Speedy Blupi 1 / Speedy Eggbert 1 have historical source of their own in the matching
reconstruction. The port builds each as a program of its own, next to `blupi-sdl3`, in the same way
`blupi2-sdl3` is a program of its own:

| Program | Edition | Historical source | Data folder |
|---|---|---|---|
| `eggbert-demo-sdl3` | Speedy Eggbert Demo (eGames, 1999) | `src/` + overlay `games/sb1-eggbert-demo/` | the demo's installed folder (`WCSUP.DLL`, `DATA`, `IMAGE08`, `SOUND`) |
| `eggbert-value-sdl3` | Speedy Eggbert SE Valueware v1.0 (eGames, 1999) | `src/` + overlay `games/sb1-eggbert-value/` | the Valueware folder (`Data`, `Image08`, `Sound`) |
| `blupi-demo-sdl3` | Speedy Blupi Demo v0.8 (Epsitec, June 1998) | its own tree `games/sb1-blupi-demo/src/` | the demo's folder (`Data`, `Image`, `Sound`) |

**How each is built** (`games/sb1/editions.cmake`):

- **The edition's own historical code, unchanged.**
  - The shared historical TUs come from the edition's tree. A file in the overlay replaces the tree's file
    of the same name, the same rule as `tools/source_tree.py`.
  - The edition's own code is therefore what runs, including its restrictions:
    - the demos stop after their last mission;
    - the menus are in their demo state;
    - the official levels need the demo marker.

  Nothing removes or bypasses a restriction.
- **The port's SB1 code (`games/sb1/game`), compiled again** in that context. It is given the edition's
  identity (`BLUPI_EDITION_KEY`: 1 eggbert-demo, 2 eggbert-value, 3 blupi-demo):
  - its program name;
  - its settings file (`eggbert-demo-sdl3.json`, …) and user folder, so a demo's saves never mix with
    the full game's;
  - its data environment variable (`BLUPI_EGGBERT_DEMO_DATA`, `BLUPI_EGGBERT_VALUE_DATA`,
    `BLUPI_DEMO_DATA`);
  - the edition for the language packs ("eggbert" or "blupi").
- **Port-side differences, where the edition's code or data differ from the full game's:**
  - **Speedy Eggbert Demo:** its `ReadConfig` reads no `Benchmark=` / `TrueColor=` keys, as the demo's
    did.
  - **June 1998 demo:** its pixmap class has no true-colour field (always 8 bpp with a palette), its
    images are in one `image\` folder, and its data is recognised by `Image\init.blp`.
- **The title screens** show the editions' own labels, from their historical code: "Version 1.0 / D E
  M O", "Version 1.0 / VALUEWARE", "Version 0.8 / DEMO".

**Tests:** `edition_eggbert-demo_attract`, `edition_eggbert-value_attract`, `edition_blupi-demo_attract`
(`tests/edition_smoke.cmake`). Each starts its program with its edition's data
(`BLUPI_TEST_DATA_EGGBERT_DEMO`, `BLUPI_TEST_DATA_EGGBERT_VALUE`, `BLUPI_TEST_DATA_BLUPI_DEMO`), shows
the title screen and plays the attract-mode demo.

**Fidelity (after 1.5.2, 2026-11-13):**

- [EVIDENCE] **Cross-build oracle.** Each edition's ordinary historical build (`tools/build.py`, from
  this repository's source in the win10 VM; never the original) ran under Wine against the port
  (`tools/oracle_wine.py`, [ORACLE.md](ORACLE.md) "Under Wine"): the title screen, then the attract-mode
  demo, 2,200 displays.

  | Edition | Depth | Identical displays |
  |---|---|---:|
  | Speedy Eggbert Demo | 8 bpp (its only depth) | 2,200 / 2,200 |
  | Speedy Eggbert SE Valueware | 16 bpp | 2,200 / 2,200 |
  | Speedy Eggbert SE Valueware | 8 bpp | 2,200 / 2,200 |
  | Speedy Blupi Demo, June 1998 | 8 bpp (its only depth) | 2,200 / 2,200 |

  The same harness first reproduced the Windows result for Speedy Blupi 1 US (2,200 / 2,200 at 16 and
  8 bpp), and gave 2,200 / 2,200 for Speedy Eggbert 1 at 16 bpp. Wine's DirectDraw and GDI stand in for
  Windows'; these runs are bounded receipts, not whole-game certification.
- **Golden traces:** `golden_eggbert-demo_attract8`, `golden_eggbert-value_attract16`,
  `golden_eggbert-value_attract8`, `golden_blupi-demo_attract8` (`tests/golden/`, 7,500 frames,
  a checkpoint every 10), recorded on unplayed data from the code that matched the oracle. They are the
  regression gate for these programs, as the other goldens are for theirs.
- **Android (after 1.5.2):** the app has the three editions too: `libeggbert_demo.so`,
  `libeggbert_value.so` and `libblupi_demo.so`, each run by its own activity in a process of its own,
  with its import in `files/game-<edition>` and its saves in a folder of its own. The importer tells
  the editions apart by their installed files (`GameFolder.java`, `ctest -R android_game_folder`): the
  Speedy Eggbert Demo's `WCSUP.DLL`, Valueware's `SpeedyEggbertSEValueware*.exe`, and the June demo's one
  `IMAGE` folder. Built, not run on a device.

**Not claimed:**

- These editions stay outside the port's supported catalog (`provenance.json`: excluded targets; edges
  F454, F473, F474): the oracle covers the title and the attract mode only (no menus, editor or saves),
  and there are no multiplayer tests.
