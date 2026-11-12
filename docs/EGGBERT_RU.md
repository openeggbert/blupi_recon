# Reaktivniy Egbert v1.1 (Russian)

**Not a matching target; documentation only** (2026-11-12). This edition is covered by the
`sb1/eggbert/en` reconstruction plus the third-party changes listed below.

The original is `ReaktivniyEgbert_v1.1_RU.exe`: 565,248 bytes, SHA-256
`c13447acf5d4b42e8b5387f4a098920be39aef18baa0458917f760496d1a9002`, PE link time 1999-06-24 15:13:35 UTC.

- **Where it is.** The only copy on the host is inside the owner's Ghidra project (`Speedy Blupi 2.rep`,
  see [Measurement](#measurement)). It is not under `assets/`.
- **How it was read.** It was read statically only and never executed.
- **Third-party code.** It contains code added by a third party: a CD check (`.DDeMCod` below). That
  code was identified by its strings and section headers only. It was not disassembled, and nothing in
  this repository reconstructs, emulates, patches or circumvents it.

Directory: [games/sb1-eggbert-ru/](../games/sb1-eggbert-ru/README.md).

## Result

- [EVIDENCE] **The game code is Speedy Eggbert 1 v1.1.** The `.text` section (virtual size 175,251
  bytes) and the `.rdata` section (1,536 bytes) are byte-identical to the original of `sb1/eggbert/en`
  (`egames.exe`, `c759447e…`). The link timestamp is the same, the linker is the same (5.00) and the
  entry point is the same (RVA `0x24ca0`). The section addresses and virtual sizes of the five original
  sections are unchanged, except that `.rsrc` was enlarged.
- **No compiler produced this file from source.** It is the `sb1/eggbert/en` executable with changes
  made after linking by a third party. Every function in it is the corresponding function of
  `sb1/eggbert/en`, whose reconstruction status applies unchanged ([EGGBERT.md](EGGBERT.md),
  [STATUS.md](STATUS.md)).
- **No target `sb1/eggbert/ru` is registered** (reasons below).

## Changes after linking, by section

| Section | Change against `sb1/eggbert/en` | Purpose |
|---|---|---|
| headers | File alignment `0x1000` (the original: `0x200`). Every section's file offset equals its address (RVA), the headers are `0x1000` bytes, `SizeOfInitializedData` is `0x59200` (the original: `0x58e00`). `.text` is marked writable (`0xe0000020`; the original: `0x60000020`). | The layout of a loaded image, not of a linked file (see [the last section](#is-the-cd-check-part-of-this-files-start-up)). |
| `.text`, `.rdata` | none | |
| `.data` | 150 bytes in two places, plus the uninitialised part stored as 5,848 zero bytes. | Russian text with a remapped font. |
| `.idata` | 385 bytes: the initial values of the import address table slots. | Rebuilt import table. |
| `.rsrc` | All 31 string tables replaced. | Russian text. |
| `.DDeMCod` (new, 8,192 bytes) | Code and data added by a third party. | CD check. |
| `.DDeMImp` (new, 4,096 bytes) | An import descriptor table of its own. | The CD check's imports. |
| `.mackt` (new, 4,096 bytes) | The active import descriptor table and its names. | Rebuilt import table. |

### Text and font (`.data`, `.rsrc`)

All [EVIDENCE] unless marked:

- **Version label** (`.data`, VA `0x476d50`, 7 bytes). `Version` becomes `DTHCBZ;`, so the title
  screen label reads `DTHCBZ; 1.1`. The letters `DTHCBZ` are the keys that type «Версия» on the
  Russian keyboard layout.
- **Character widths** (`.data`, 143 bytes). The two 128-entry glyph width tables of `GetCharWidth`
  (`src/text.cpp`) are changed:
  - `table_width` at VA `0x47ab00`: 76 entries;
  - `table_width_little` at VA `0x47ab80`: 67 entries.

  In both tables the changed entries lie between index 32 and 127. The original's bytes at those
  addresses are the source tables.
- **Strings** (`.rsrc`). The original has 329 non-empty strings:
  - 325 are replaced;
  - three single spaces (621–623) are emptied;
  - only string 610 (`C`) is unchanged.

  Every new string is plain ASCII: Russian typed on the Latin key positions of the Russian keyboard
  layout. For example, string 150 `Training #` becomes `J<EXTYBT #`, which is «Обучение #».
- **The rest of `.rsrc` is unchanged:** the cursors, the icon, the dialog and the version resource,
  with the same language IDs (`0x0`, `0x100c`). The section is 1,024 bytes longer than the original;
  within the original's 23,552 bytes, 11,227 differ.
- [HYPOTHESIS] **The Cyrillic glyphs are in the game's font images.** The game draws text with its own
  bitmap font, and the text is stored as Latin character codes. So the Cyrillic glyphs must be drawn at
  the Latin positions of the font images, and the width tables were changed to fit them. The font
  images are data files outside the executable and were not examined.

### Imports (`.idata`, `.mackt`)

All [EVIDENCE] unless marked:

- **The import descriptor table moved to `.mackt`.** The import directory points to RVA `0x89000`,
  `0x8c` bytes. The descriptors have no `OriginalFirstThunk`, and the IAT data directory is cleared.
- **The import address table is unchanged in place.** It stays at the original addresses in
  `.idata`, so the unchanged game code calls through the same slots. Only the slots' initial values in
  the file changed: they now point to names in `.mackt`. These values are the 385 changed bytes.
- **The same functions are imported:** the same 7 DLLs, 128 functions, each in its original slot. The
  DLLs are listed in alphabetical order.
- **Four slot names differ:**
  - `DPLAYX` ordinals 1 and 2 are imported by name, `DirectPlayCreate` and `DirectPlayEnumerateA`.
  - In `KERNEL32`, `_lread` becomes `_hread` and `SetHandleCount` becomes `LockResource`.
- [HYPOTHESIS] **The table was rebuilt by a tool that reads addresses back into names.** Such a tool
  resolved each slot from the address it held back to an export name. Where two exports share one
  address, it picked the other name. `.mackt` is the section name the ImportREC tool creates.

### The CD check (`.DDeMCod`, `.DDeMImp`)

[EVIDENCE] Identified by its strings (Windows-1251) and by its import table only:

- **File names:** `F:\Autorun.exe` and `F:\6C00004A.fil`.
- **Messages:**
  - «Для корректной работы этой игры требуется оригинальный компакт-диск. Вставьте, пожалуйста,
    диск и нажмите ОК» ("This game needs the original CD to run correctly. Please insert the disc and
    press OK");
  - «Ошибка запуска игры» ("Error starting the game");
  - «Запуск игры. Подождите, пожалуйста!» ("Starting the game. Please wait!").
- **Registry key:** `Software\Gefest Protection System\Eggbert`.
- **Import table in `.DDeMImp`:**
  - descriptors for the game's seven DLLs, pointing at the game's import address table in `.idata`;
  - four more descriptors (`KERNEL32`, `GDI32`, `USER32`, `ADVAPI32`) for the added code, whose import
    address table is at the start of `.DDeMCod`.
  - The import directory does not point to this table.

## Is the CD check part of this file's start-up?

- [EVIDENCE] **Nothing in this file passes control to `.DDeMCod`.**
  - The entry point is the game's own start-up code in `.text`, at the same address as in
    `sb1/eggbert/en`.
  - There is no TLS directory.
  - `.text` and `.rdata` are unchanged, and the `.data` changes are text data.
  - No 32-bit value in any section except `.DDeMImp` points into `.DDeMCod` or `.DDeMImp`. This was
    checked by a byte scan, without disassembling anything.
  - The active import table names only system DLLs.
- [EVIDENCE] **The added code's import address table holds addresses that the Windows loader filled
  in.** A file on disk holds name references in those slots. These hold absolute addresses in the
  system-DLL range: `0x7c80…` (`KERNEL32`), `0x77d3…` (`USER32`), `0x77f1…` (`GDI32`) and `0x77dc…`
  (`ADVAPI32`).
- [HYPOTHESIS] **This file is not the release as it was sold.** It looks like a memory image of the
  running protected release:
  - saved after the CD check had run;
  - with its import table rebuilt in `.mackt`;
  - with the entry point set back to the game's own.

  The loader addresses fit the system DLLs of Windows XP. In this file the CD check is present but
  never reached.
- [OPEN] **The release as sold is not on this host.** Its entry point and layout are unknown.

## Why it is not a target

1. **Its game code is the `sb1/eggbert/en` executable.** A target would verify the same `.text`
   against the same objects and report the same figures.
2. **Its other differences cannot come from a build.** They are changes after linking by a third
   party: data, resources, the import table, the file layout and the added code. No compile session
   produced them, and this repository reproduces executables only by compiling and linking source. It
   does not patch binaries. An ordinary build or relink could never reproduce this image, and a
   whole-image comparison would measure only those changes.
3. **The file looks like a memory image**, not the release as it was sold (above).
4. **The original is not registered:** it is only in the owner's Ghidra project, not under `assets/`.

## Related findings (not part of this edition)

- [EVIDENCE] **`SpeedyEggbert_v1.0_EN.exe` is a one-byte relabel.** This file from the same Ghidra
  project (534,528 bytes, `5ce66f29c717d18da8398c5522a181b11ffaec0577f123b66b1bd566668d754a`) differs
  from the original of `sb1/eggbert/en` in one byte. It is at file offset `0x7555a`, where `Version 1.1`
  becomes `Version 1.0`, and the link timestamp is the same. Measured on 2026-11-12.
- **Not re-measured in this campaign:** the `EGAMES.EXE` copies in the Wise language installers
  (`set_ger/fre/ita/por/spa.EXE`). They were recorded on 2026-11-09 with the same link timestamps as
  the `sb1/eggbert/*` targets, each differing in that byte only.
- [OPEN] **Which label is the original?** These files cannot tell which label the compiler produced and
  which was edited afterwards.

## Measurement

Measured on 2026-11-12, statically, with no original executable run:

1. The Ghidra project
   `/rv/data/library/openeggbert/drive.openeggbert.com/Projects/Free Eggbert/speedy-blupi-2_robertvokac/speedy-blupi-2/Speedy Blupi 2.{gpr,rep}`
   was copied into an ignored `build/` directory. The original file bytes of
   `ReaktivniyEgbert_v1.1_RU.exe` and `SpeedyEggbert_v1.0_EN.exe` were exported with Ghidra 12.0.4
   `analyzeHeadless … -process <name> -noanalysis -readOnly`, using a post-script that writes
   `Memory.getAllFileBytes()`. The SHA-256 hashes above match the 2026-11-09 records.
2. Both files were compared section by section with the original of `sb1/eggbert/en`
   (`c759447e…`) using Python `pefile` 2024.8.26:
   - the raw bytes within each section's virtual size;
   - the import descriptors and slots;
   - the resource tree, and the decoded string tables.
3. The new sections were read only for printable strings and import descriptors, and scanned for
   32-bit values pointing into them.

The project copy and the exported files were deleted afterwards.
