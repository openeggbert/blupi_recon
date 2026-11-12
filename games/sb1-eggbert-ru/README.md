# Reaktivniy Egbert v1.1 (Russian): documentation only

**This edition is not a matching target and has no source here.**

The original is `ReaktivniyEgbert_v1.1_RU.exe` (565,248 bytes, SHA-256
`c13447acf5d4b42e8b5387f4a098920be39aef18baa0458917f760496d1a9002`, linked 1999-06-24 15:13:35 UTC).
It is read statically only and never executed. Full inventory: [docs/EGGBERT_RU.md](../../docs/EGGBERT_RU.md).

- [EVIDENCE] **Its game code is Speedy Eggbert 1 v1.1.** `.text` and `.rdata` are byte-identical to
  the original of `sb1/eggbert/en`, with the same link timestamp. The edition is therefore covered by
  the `sb1/eggbert/en` reconstruction (`src/`, `_EGAMES=1`).
- [EVIDENCE] **Everything else was changed after linking, by a third party.** The changes are:
  - the Russian strings, typed on the Latin key positions;
  - the version label and the glyph width tables in `.data`;
  - an import table rebuilt into a new `.mackt` section;
  - added sections `.DDeMCod` and `.DDeMImp` holding a CD check ("Gefest Protection System").

  No compile session produced these changes, and the repository does not patch binaries. So this
  edition cannot be a matching target, and nothing here reproduces, emulates or circumvents the CD
  check.
- [HYPOTHESIS] **The file is a memory image of the running release**, not the release as it was sold.
  The evidence is in docs/EGGBERT_RU.md.
