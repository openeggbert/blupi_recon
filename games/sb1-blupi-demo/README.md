# Speedy Blupi Demo, June 1998: source tree

Target `sb1/blupi/demo`. The original is `Blupi.exe` of the Epsitec demo, version 0.8 (533,504 bytes,
SHA-256 `06faf60adf7585ade95dbeeeedcf0fc5e58decb46ef08d8e0adf111f34053fd7`, linked 1998-06-18
21:29:17 UTC). It is read statically only and never executed. Status and figures:
[docs/SB1_DEMO.md](../../docs/SB1_DEMO.md).

Unlike the other SB1 targets this one has a full private tree, because the June demo is an earlier
revision of the source, four months before the retail game, and differs in many translation units:

- `src/` is the tree. It was seeded on 2026-11-11 from the repository's `src/` with every `_DEMO` arm
  resolved into plain demo code; `def.h` defines `_DEMO` as `TRUE` only for the record. From then on it
  changes only from the June executable's bytes, and `src/` never changes for it. `sound.cpp` is
  byte-identical to `src/sound.cpp`, so its line numbers (which `TRY_DS` embeds) are kept.
- `recon.json` is its manifest. All addresses in it are the demo's own. It compiles with VC5 RTM and
  links with VC5 RTM LINK 5.00.7022. `tools/verify.py --target sb1/blupi/demo --accept` promotes new
  matches into it.
- `oracle/sb1-blupi-demo-tree/` holds its function inventory.
