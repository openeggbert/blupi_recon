# sb1/eggbert/value inventory

[EVIDENCE] Static oracle: `assets/downloads/eggbert_value/SpeedyEggbertSEValueware_v1.0_EN.exe`,
534,528 bytes, SHA-256 `2cf7fa8f1ebb5aba1ea31ae805b788f67f224a0b1e0aef314f8e889568800ee3`. Status and
evidence are in [EGGBERT_VALUE.md](../../docs/EGGBERT_VALUE.md). Never execute the oracle.

[EVIDENCE] Generated on 2026-11-11 by
`tools/derive_target.py --target sb1/eggbert/value --via sb1/eggbert/en`: `functions.csv` and
`tu_map.csv` are Speedy Eggbert 1's rows at Valueware's addresses, and `address_map.json` translates
the primary target's addresses. Then audited; three corrections:

- **`CEvent::SetHide` moved from 0x16610 to 0x16640.** Its body is identical to the other one-call
  button helpers once the call target is masked, and the derivation took the first free slot.
  Valueware has `CEvent::SetDemo` in that slot: the body at 0x16610 calls `CButton::SetDemo`
  (0x2d20), the one at 0x16640 calls `CButton::SetHide` (0x2d50).
- **Two rows added** for the demo-state methods, `CButton::SetDemo` (0x2d20) and `CEvent::SetDemo`
  (0x16610). The retail executables do not contain them.
- **One address-map pair added** for `CJauge::CJauge` (primary 0x41edd0, Valueware 0x1ee10). Without
  it the map's neighbouring segments put the constructor 16 bytes too late.

[EVIDENCE] `functions.csv` and `tu_map.csv` are placement inputs, not acceptance proofs. The effective
target manifest and a fresh comparator run determine acceptance. The `strict` annotations in
`tu_map.csv` are inherited from Speedy Eggbert 1's inventory and claim nothing about this target.

The three files are marked `-text` in `.gitattributes`, because the equivalence certificates pin
their bytes.
