# SourceBroom validation

## Current A2–A4 Windows alpha candidate

Verified on 2026-10-24 with Windows x64, MSVC 19.50, Qt 6.10.3 and CMake 4.3.
External maps, installed assets and local raw logs are not public ship inputs.

| Control / subject | Prediction and observed outcome |
|---|---|
| Full production-library tests | Fs 20, Gl 21, Mdl 150, Ui 40: all 231 CTest entries passed. Final first-party build logs contain no warnings/errors. |
| Native no-op and acted edits | Authored tests cover byte-preserving round trips, actual existing brush/entity changes, UV preservation, undo/redo, unsupported edit refusal, stale bindings and save transactions; silent no-op is not the acted control. |
| External syntax corpus | All 38 inputs still pass no-edit byte identity and original hash checks. Original files were never write targets. |
| External scene corpus | 32 scenes accepted; six explicitly refused with geometry/UV diagnostics. No promise those six work in the GUI, and no objects were silently dropped to make them pass. |
| Content readers | Authored tests exercise loose/VPK precedence, bounds/error handling, VMT includes/fallbacks, VTF byte layouts and preview budgets. Unsupported shader behavior is not called complete Source rendering. |
| Pixel demand | Unrequested Source resources remain unloaded; metadata lookup does not decode pixels; viewport activation and scene material references request them. Legacy resources retain eager behavior. |
| Real GUI | CSS and Momentum authored-room previews opened, showed brick textures, and closed with exit 0. Fixture hashes remained unchanged. This is a basic preview/close check, not a latency, FPS or exhaustive library-browsing benchmark. |
| Native authored edit | Production native scene edit moved a ramp, wrote a new VMF, reopened it, and verified the changed bounds/bytes rather than accepting an unchanged output. |
| External classic Source compiler | CS:S VBSP/VVIS/VRAD compiled both authored control and native-edited VMFs to version-20 BSPs; tool exits and output existence were checked. |
| External Strata compiler | Momentum VBSP/VVIS/VRAD compiled the native-edited VMF to a version-25 BSP. VMF format and BSP target version remained separate. |
| Local runtime | All three compiled BSPs loaded in separately launched, owned FTESurf clients, produced map-specific timer/status logs and screenshots, and exited 0. The classic edited run produced changed server body coordinates and forward speed while approaching the ramp. |
| Windows install | Existing CMake install entrypoint succeeded; binary, Qt runtime and game resources staged without copying Valve compilers/assets or corpus maps. |

Runtime rigs contained only copied engine/plugins/progs and tracked public game
assets/configs. They had no owner config/player data, used no lobby connection,
and never wrote either canonical install. Unknown `ui_close` messages in that
runtime fixture did not prevent the actual HUD/world screenshots; they are not
counted as a passed menu-command check. Runtime spawn timing differs between
arms, so body logs are **not** a controlled numerical collision/prediction
comparison. Exact ramp joins/feel still need the small owner check below.

### Remaining acceptance and limits

- Independent Hammer++ reopening and room/ramp/texture inspection: **pending
  actual owner confirmation**, tracked in [lextest.md](../../lextest.md).
- Owner inspection of the edited ramp joins/collision and first interactive
  editor feel: pending. Automated compile/load is not that visual acceptance.
- Six scene refusals remain compatibility findings; see [BACKLOG.md](../../BACKLOG.md).
- MDL props, full VMT shader fidelity, displacement/instance/overlay authoring,
  new/deleted geometry, full FGD/I/O tools, packing and broad branch coverage are
  not implemented. Unsupported changes must not be called safe saves.
- Long-session cache eviction, broad performance claims, non-Windows packages,
  an installer and public binary release are not verified.

The automated A4 chain is implemented and exercised. The full A4 exit is not
claimed until its independent/manual gates have actual evidence.

## Historical A1 validation — native VMF document foundation

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

## Historical A1 exclusions (see current status above)

- VMF scene/brush construction, viewport opening, selection, or GUI saving.
- Semantic escape decoding, numerical/geometry validity, or float precision after
  a geometric edit. Original numeric text is preserved because it is not rewritten.
- Safe filesystem save/overwrite, backups, crash recovery, or undo integration.
- Independent Hammer++ reopening of newly saved output.
- Any Source compiler invocation, BSP compatibility, packing, or FTESurf play test.
- Source materials/models, game discovery, or compiler profile UI.
- Linux/macOS builds, performance comparisons, installers, or release packaging.

At A1 the next dependency was **A2: read-only VMF-to-scene bridge**, keeping the
native document/ID mapping authoritative. Current results are above; see
[the roadmap](ROADMAP.md) for the remaining acceptance and later stages.
