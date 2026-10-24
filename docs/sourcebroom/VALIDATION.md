# A1 validation — native VMF document foundation

Verified on 2026-10-24 using Windows x64, MSVC 19.50 / Visual Studio 18 BuildTools,
CMake 4.3, and the existing Qt 6.10.3 kit. Upstream base:
`6e9185edd35e614f96864cd20a7abe4fed23e408`.

This records the production document parser and diagnostic tool, **not a working
VMF GUI or a released editor**. No external map contents or local per-map logs
are included here.

## Implemented

- Ordered, parented block/property entries with repeated keys retained.
- Source-token spans over original bytes; no numeric conversion/reformatting.
- Quoted and bare input tokens, line/block comments, UTF-8 BOM, CRLF, high-bit
  bytes, escaped quotes/backslashes, and Source output separator bytes.
- Opaque unknown/Hammer++/displacement blocks retained as syntax, not interpreted.
- Existing property value replacement with one explicitly quoted raw token.
- Rejection of malformed syntax, NUL/UTF-16, nesting beyond 256 blocks, invalid
  edit targets, duplicate edits, and token/comment injection.
- `SourceBroomVmfCheck`, a read-only executable using the same production code.
  It additionally requires exactly one top-level world block. That is only a
  minimal map-shape check, not whole-VMF semantic validation.

## Checks and outcomes

| Check | Outcome |
|---|---|
| Build `TbMdlLibTest` and `SourceBroomVmfCheck`, Release x64 | Passed |
| First-party compiler diagnostics | No warnings/errors in new sources; final incremental build has no warnings/errors |
| Focused VMF tests | 9 passed |
| Entire model-library CTest suite, including VMF tests | 141 passed, 0 failed |
| CLI controls | Help, no arguments, unknown option, valid edit probe, malformed VMF, non-map document, missing input, empty directory, and continuing after a bad file all produced expected exits/results |
| Original external corpus unchanged serialization | 38/38 byte-identical |
| External in-memory acted control | 38/38 selected-token mutations changed exactly expected bytes and reparsed with unchanged hierarchy |
| Original corpus hashes | All SHA-256 hashes unchanged before/after; same inputs as initial run |
| LSP with context derived from actual MSBuild commands | No compiler errors/warnings in new files; optional modernization hints remain |

The first dependency build emitted 35 MSVC command-line warning overrides in
third-party targets (`/W1` overridden by upstream's `/w`). These were not new
first-party source warnings, and the dependencies were not edited to hide them.
See [development notes](DEVELOPMENT.md) for the initial generator/dependency
population recovery; those failures are not counted as feature progress.

## External corpus structure encountered

| Item | Count |
|---|---:|
| Files | 38 |
| Original bytes | 131,550,600 |
| Parsed entries | 4,201,358 |
| Blocks | 619,092 |
| `solid` blocks | 41,350 |
| `side` blocks | 261,351 |
| `dispinfo` blocks | 104 |
| Properties under `connections` | 1 |

Counts establish that the syntax parser traversed the documents; they do not
assert that those objects have valid geometry or that displacements are editable.
Duplicate outputs and raw comma/ESC separator bytes are covered by authored
synthetic syntax cases, not semantically decoded. The real corpus contains only
one connection property: request
an I/O-heavy map before the entity-I/O authoring stage rather than claiming broad
real-world I/O coverage now.

## Explicitly not verified or implemented

- VMF scene/brush construction, viewport opening, selection, or GUI saving.
- Semantic escape decoding, numerical/geometry validity, or float precision after
  a geometric edit. Original numeric text is preserved because it is not rewritten.
- Safe filesystem save/overwrite, backups, crash recovery, or undo integration.
- Independent Hammer++ reopening of newly saved output.
- Any Source compiler invocation, BSP compatibility, packing, or FTESurf play test.
- Source materials/models, game discovery, or compiler profile UI.
- Linux/macOS builds, performance comparisons, installers, or release packaging.

Next dependency is **A2: read-only VMF-to-scene bridge**, keeping the native
document/ID mapping authoritative. See [the roadmap](ROADMAP.md).
