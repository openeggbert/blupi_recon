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

**Not claimed:**

- **No fidelity review beyond that.** These editions are outside the port's supported catalog
  (`provenance.json`: excluded targets; edges F454, F473, F474 keep their follow-ups):
  - no golden traces;
  - no oracle comparison;
  - no multiplayer tests.
- **Desktop only.** The Android app does not include them.
