# sb1/blupi/demo inventory for the private tree

[EVIDENCE] `functions.csv` and `tu_map.csv` are copies, made on 2026-11-11, of
`oracle/sb1-blupi-demo/functions.csv` and `tu_map.csv` (see that directory's README for how they were
derived and audited). They are placement inputs, not acceptance proofs.

This directory has no `address_map.json` on purpose. The target's manifest
`games/sb1-blupi-demo/recon.json` uses the demo's own addresses, and `tools/targets.py` translates a
manifest's addresses whenever its inventory directory has an address map. `oracle/sb1-blupi-demo/` is
the earlier inventory with the map from the primary target's addresses; it is kept unchanged as a
record, and no target reads it any more.
