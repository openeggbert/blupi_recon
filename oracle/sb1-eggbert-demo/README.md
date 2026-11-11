# sb1/eggbert/demo inventory

[EVIDENCE] Static oracle: `assets/downloads/eggbert_demo/WCSUP.DLL`, 534,528 bytes, SHA-256
`fc071424356a9a270e8abf9852bfa08e7b592374a269bf28083071bcc9dfe52c`. Status and evidence are in
[EGGBERT_DEMO.md](../../docs/EGGBERT_DEMO.md). Never execute the oracle.

[EVIDENCE] Generated on 2026-11-11 by
`tools/derive_target.py --target sb1/eggbert/demo --via sb1/eggbert/en`: `functions.csv` and
`tu_map.csv` are Speedy Eggbert 1's rows at the demo's addresses, and `address_map.json` translates
the primary target's addresses. Then audited; three corrections:

- **`CEvent::SetHide` moved from 0x16560 to 0x16590.** Its body is identical to the other one-call
  button helpers once the call target is masked, and the derivation took the first free slot. The
  demo has `CEvent::SetDemo` in that slot: the body at 0x16560 calls `CButton::SetDemo` (0x2c90), the
  one at 0x16590 calls `CButton::SetHide` (0x2cc0).
- **Two rows added** for the demo-only methods, `CButton::SetDemo` (0x2c90) and `CEvent::SetDemo`
  (0x16560). The retail executables do not contain them.
- **One address-map pair added** for `CJauge::CJauge` (primary 0x41edd0, demo 0x1ed10), which lies
  between two map segments.

[EVIDENCE] `functions.csv` and `tu_map.csv` are placement inputs, not acceptance proofs. The effective
target manifest and a fresh comparator run determine acceptance. The `strict` annotations in
`tu_map.csv` are inherited from Speedy Eggbert 1's inventory and claim nothing about this target.

The three files are marked `-text` in `.gitattributes`, because the equivalence certificates pin
their bytes.
