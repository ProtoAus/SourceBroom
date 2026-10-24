# SourceBroom alpha roadmap

SourceBroom is a GPL-3.0-or-later fork of [TrenchBroom](https://github.com/TrenchBroom/TrenchBroom)
for native Source 1 VMF editing. FTESurf is the first verification target, not the
only intended destination. Source 2 / VMAP is out of scope.

## Current status

- Fork established from upstream `6e9185edd35e614f96864cd20a7abe4fed23e408`.
- **A0 complete:** repository and ordered plan.
- **A1 in progress:** native, lossless VMF document foundation and read-only corpus checker.
- **A2 and later not implemented.** The GUI is still upstream TrenchBroom. It does
  not yet open, render, edit, or save VMFs. There is no SourceBroom editor release.
- No Source compiler, Valve content, or third-party/user map is bundled.

A stage is complete only when its exit criteria have evidence. Parsing is not
semantic validation; byte preservation is not Hammer compatibility; a build is
not a playable-map test. Keep those claims separate.

## Non-negotiable contract

1. VMF is the authoritative map source, not an export from Quake MAP.
2. Preserve ordered/repeated keys, IDs, references, unknown blocks, comments,
   line endings, original numeric text, and unedited bytes.
3. Native VMF is distinct from a compiler's output BSP version. Keep game/target
   profiles separate from compiler profiles.
4. Never silently discard an unsupported feature. Preserve it safely, restrict
   the affected operation, or refuse an unsafe save with an actionable reason.
5. Unedited surf planes and UV axes must not be rounded or reconstructed.
6. Use installed external compilers first; do not copy restricted SDK code into
   the GPL editor or redistribute compilers/assets without permission.
7. Keep source documents independently reopenable in Hammer. Do not make a
   SourceBroom sidecar authoritative for data VMF can represent.
8. No original corpus file is a write target. Use owned copies for future
   Hammer/compiler tests. Public fixtures must be newly authored or licensed.

## Implementation order

### A0 — Fork, ownership, and a reproducible starting point

- Preserve upstream history/license and record the pinned base.
- Keep Source-specific changes reviewable and avoid an early wholesale rename.
- Establish the existing CMake build on the development machine.

**Exit:** accessible fork, recorded upstream SHA, documented build entrypoints,
and an ordered plan. Full application rebranding is not required here.

### A1 — Lossless native document core

- Parse VMF's ordered KeyValues-like block/property structure.
- Retain source-token spans rather than rewriting every field through floats.
- Represent repeated outputs and opaque extension blocks without flattening.
- Support surgical replacement of existing value tokens; reject invalid edits.
- Add malformed-input diagnostics and explicit resource limits.
- Add a read-only diagnostic tool and run it on the supplied Momentum/Hammer
  corpus without committing those files.

**Exit:** focused tests prove syntax is actually parsed, no-edit serialization is
byte-identical, edits affect only the selected token, duplicate keys remain
ordered, malformed inputs fail, and the external corpus is unchanged by hash.

**Not claimed:** geometry semantics, viewport rendering, Hammer acceptance,
compiler acceptance, or safe filesystem save/overwrite transactions.

### A2 — VMF-to-scene bridge, initially read-only

- Map world/entities/solids/sides onto the existing scene and brush model.
- Parse planes and UV axes with checked double-precision conversions.
- Keep explicit native-document-to-scene and stable-ID mappings.
- Preserve hidden objects, visgroups, editor blocks, and unrepresented data.
- Recognize document/feature versions and report capabilities explicitly.
- Enable GUI opening only once unsupported features cannot masquerade as a
  complete editable map. Start with a clearly labelled read-only preview.

**Exit:** synthetic brushes and representative real maps appear with correct
bounds, winding, materials references, IDs, and UVs. Invalid geometry produces
specific diagnostics, not dropped faces. Unsupported objects remain intact.

### A3 — Source content and an honest viewport

- Discover Steam libraries and installed game paths; allow manual overrides.
- Resolve gameinfo search paths, loose files, VPKs, and mount precedence.
- Evaluate MIT-licensed sourcepp components before implementing formats anew.
- Load VTF and the common VMT shaders/parameters; display explicit fallbacks for
  unsupported materials. Evaluate Strata VTF 7.6 separately.
- Cache content indexes and thumbnails without blocking the editor thread.

**Exit:** known installed HL2/CSS/Momentum materials resolve with verified
precedence. Missing assets and unsupported shaders are distinguishable. No
Valve assets are copied into the repository or distributed with the editor.

### A4 — First complete brush-edit → compile → play loop

- Add reversible, native-backed brush/entity edits and undo/redo.
- Preserve IDs, face references, plane precision, UVs, and unrelated bytes.
- Restrict transforms that would invalidate opaque attached data.
- Implement Save As and transactional save/backups before enabling overwrite.
- Configure one known-working external toolchain/profile end-to-end.
- Provide a simple Build & Play action and useful compiler diagnostics.

**Exit:** edit a surf ramp, save a new VMF, reopen it in Hammer++, compile it,
and play the BSP in FTESurf. Check ramp joins/collision and inspect the compiler
logs. An owner visual/in-game check is valid evidence for what was observed;
it is not a substitute for preservation or numerical claims.

This is the first interactive alpha, **not** a claim of complete Source support.

### A5 — Everyday Source authoring

- Entity FGD integration and Source input/output editing, including repeated
  outputs and both documented output separators.
- Source MDL/VVD/VTX props and branch-specific model variants.
- Visgroups, hidden objects, groups, cameras, cordons, and meaningful validation.
- Separate classic Source and Strata target/compiler capability profiles.
- Dependency packing with exclusions for stock/copyrighted content.
- Leak display, clickable errors, build presets, and repeatable launch settings.

**Exit:** representative brush/entity/prop maps survive edits, undo, save,
Hammer reopening, and the selected compiler. Unsupported branch features are
reported before a misleading successful build.

### A6 — Advanced native Source features

- Displacements: correct topology, normals, offsets, alpha blending, sculpting,
  sewing, transforms, undo, and serialization.
- Overlays and face-reference maintenance.
- Instances and their fixups, with explicit flattening/export semantics if used.
- Branch extensions, prop scaling, and other supported target-specific data.

**Exit:** each feature has a preservation control, an actual edit, independent
Hammer/compiler verification, and a playable representative output. Preserving
a feature's bytes alone does not make it editable or supported.

### A7 — Easy setup, surf tools, performance, and packaging

- Project wizard with automatic content/toolchain detection and clear repairs.
- Surf-specific ramp/seam/alignment tools, based on real mapper feedback.
- Profile large maps on the same machine/content against existing editors.
- Improve load time, frame-time spikes, selection, undo cost, and memory based
  on measurements; do not advertise "faster than Hammer++" without evidence.
- Incremental rebranding, installers, crash reporting, and cross-platform builds.
- Publish alpha artifacts only with an explicit capability/limitation checklist.

UX/performance work is continuous, but must not displace the correctness gates.
Using the same compilers does not itself improve compile speed.

## Test corpus policy

Initial owner-provided corpus: 38 VMFs, 131,550,600 bytes, from the local Momentum
editor maps directory. This is a useful syntax/preservation sample, not evidence
of coverage of every Source branch or feature.

- Read originals in binary mode; compare SHA-256 before/after local validation.
- Report document and structure counts so an opaque pass-through cannot count as
  parser coverage. Exercise an in-memory token edit as the acted control.
- Keep original names/content and machine-specific result logs out of commits.
- Request additional maps when a specific upcoming feature needs coverage, not
  an indiscriminate collection of every available VMF.
- Before A4, ask for a quick owner check of a newly compiled test ramp rather
  than building a large visual automation framework.

## References and candidate reuse

- [TrenchBroom Source support](https://github.com/TrenchBroom/TrenchBroom/issues/1159)
- [Strata BSP support](https://wiki.stratasource.org/modding/overview/bspsupport)
- [Strata BSP v25](https://wiki.stratasource.org/modding/formats/bsp-v25)
- [Strata gameinfo](https://wiki.stratasource.org/modding/formats/gameinfo)
- [Strata VTF 7.6](https://wiki.stratasource.org/modding/formats/vtf-v76)
- [sourcepp](https://github.com/craftablescience/sourcepp) — evaluate component
  maturity and dependency licenses; filesystem/model support is not assumed complete.
- [WifeRadiant](https://github.com/erysdren/WifeRadiant) — implementation reference;
  do not copy code without checking its specific license/provenance.
- [Tools++](https://ficool2.github.io/HammerPlusPlus-Website/tools.html) — use local
  executables; redistribution requires a separate permission check.
