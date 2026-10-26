# SourceBroom alpha roadmap

SourceBroom is a GPL-3.0-or-later fork of [TrenchBroom](https://github.com/TrenchBroom/TrenchBroom)
for native Source 1 VMF editing across Source games. **Counter-Strike: Source and
Momentum Mod are the first supported game profiles.** FTESurf/WorldofSurf is an
additional verification target, never an editor dependency. Content mounts,
FGDs, compilers, launch settings, and branch limits are profile-configurable.
Source 2 / VMAP is out of scope; untested Source 1 branches are not presumed
compatible merely because they also use VMF.

## Current status

- Fork established from upstream `6e9185edd35e614f96864cd20a7abe4fed23e408`.
- **A0 complete:** repository and ordered plan.
- **A1 complete:** native, lossless VMF document foundation and read-only corpus
  checker. All 38 supplied maps pass byte-preservation and
  in-memory mutation checks with original SHA-256 hashes unchanged.
- **A2 implemented and verified:** native-backed VMF scene construction, stable
  bindings, Source UVs, explicit opaque-feature restrictions, and GUI opening.
  The 38-file supplied corpus remains byte-identical; the A5.1 larger finite
  preview envelope constructs 35 strict editable scenes. Three remain refused
  for editable loading. A5.3's separate, explicitly chosen read-only preview opens
  all 38 with original bytes/IDs intact and clearly counted unprojected solids;
  this is not editable repair or a complete-scene claim.
- **A3 implemented for the first alpha:** game-configurable loose/VPK content,
  bounded VTF/VMT base-texture preview, CSS/Momentum/generic Source 1 profiles,
  and demand-driven pixel loading. Actual CSS and Momentum GUI previews render
  and close cleanly. This is not full Source shader or MDL rendering.
- **A4 automated loop verified; manual acceptance pending:** bounded native-backed
  brush/entity edits, undo/redo, preserving Save As/backup transactions, and an
  external Compile / Compile & Play dialog. An authored ramp edit saves/reopens
  natively; classic Source and Strata compilers build BSPs that load in isolated
  FTESurf clients. Independent Hammer++ reopening/visual inspection is pending
  in [lextest.md](../../lextest.md), so the full A4 exit is not yet claimed.
- Windows packaging/install smoke passes. No public binary release is claimed.
- **A5.14 candidate:** ordinary native BOTH clipping now retains Front under its
  original identity and adds Back as a same-parent sibling with remapped stable
  IDs. Loaded/new/template/duplicated world and supported plain brush-entity
  routes use one staged whole-batch admission and command; no-op subjects remain
  untouched, mixed selections retain them, and undo parks exact siblings without
  leaking them into saves. Redo restores the same nodes/IDs and native source
  attributes. Actual preview cut material/UV mismatch was reproduced and repaired;
  first-nearest ties and both orientations are tested. Full Model201/App50, Both9,
  retained native8 and ClipTool4/670assert pass, actual app/checker/UI build0/zero
  new warnings. Focused independent review found no remaining blockers. Corpus
  remains38/35/38 with38 original hashes unchanged; final receipts record portable
  boundary evidence. Other CSG/topology and displacement/hidden/opaque/ref bypass
  remain guarded. Generic failure policies stay unchanged. Owner/Hammer checks
  remain pending; keep the whole A5.13 kit as recovery.
- **A5 bounded candidate implemented; full exit pending:** FGD-backed property
  hints and ordered/repeated Source outputs, bounded static MDL/VVD/VTX preview,
  saved-file metadata inspection/camera-eye navigation, native capability
  preflight, target BSP-header checks, leak/position diagnostics, and explicit
  custom-only distribution ZIPs. These are [bounded tools](SOURCE_TOOLS.md), not
  native metadata authoring, complete dependency discovery or all-branch support.
  The A5.1 usability follow-up removes duplicate initial Source indexing and
  quadratic enabled-folder selection. A5.2 adds bounded native new-map/brush/point
  creation, plain-object duplication/deletion and brush-entity ownership, with
  side-identity admission, stable ID allocation and preserving undo/save.
  A5.3 adds an immutable partial compatibility preview with default-deny commands,
  no-delta/exclusive-copy safeguards, and persistent omission diagnostics. Texture
  work resumes fairly, dead catalog resources compact once, and the browser
  avoids unnecessary full relayouts; large initial mounts still take seconds.
  A5.4 corrects Source prop winding and VMT mask/opacity handling, preserving
  editor fades with actual GPU controls. Material settings show game-level roots
  without pruning the catalog or rewriting saved subsets on presentation.
  The owner accepted the observed A5.4 visual retry; detailed Hammer/authoring
  gates remain pending. A5.5 adds bounded, on-demand scalar Patch VMT inheritance
  for brush/prop textures through existing mounts, not full Patch/shader support.
  A5.6 adds demanded `$nocull` two-sided brush/prop previews and scoped restoration
  of prior polygon state, preserving explicit overrides and existing alpha/fade.
  This is not backface lighting or another winding/shader rewrite. The owner
  gave overall A5.6 visual approval; specific surface/Hammer gates stay pending.
  A5.7 honors explicit supported nonzero `$alphatestreference` through demanded
  brush/prop/Patch previews; absent0.5 remains an editor fallback, not certified
  branch defaults. Scalar$alpha and zero/sentinel semantics are not guessed.
  [Validation](VALIDATION.md) records current Fs20/Gl21/Mdl175/Ui43 passes,
  focused Gl990/Mdl7654 and GPU2539 assertions. The earlier intermittent Windows
  preference rename failure remains unfixed despite the current passing run.
  Final receipts retain the source/binary/run distinctions and appearance gates.
- A5.8 starts A6 with native power2..6 displacement meshes, picking/bounds and
  selected-face distance editing; A5.9 adds **whole-base translation** and A5.10
  **proper rotation** through normal commands, UV policy, undo and preserving saves.
  Every displacement shares an exact motion frame; numeric overflow/cancellation
  refuses before world mutation, not merely after a finite-bounds check.
  A5.9 also adds bounded Source-only entity skins for simple fixed FGD studio
  paths, preserving other-game/explicit/legacy/dynamic declaration semantics.
  Common integer VTF formats retain bounded RGBA8 preview. Full A6/Source support
  is not claimed: native nonuniform scale/shear/flip, sculpt/create/power/sew, blend
  materials, overlays/instances and broader shader/model tools remain pending.
  All 184 model entries and native focus4,009 assertions/41 cases pass. Review
  overflow red->green364/2 acts through Core and Map; affected UI129/3 and existing
  GPU2,539 remain valid. Earlier
  Fs20/Gl21 remain valid; the known untouched Windows preference failure is
  preserved/unfixed, not all UI green. Native appearance/Hammer checks remain
  pending; A5.9 is recovery.
  The A5.11 candidate adds explicit **positive uniform scaling** of
  whole displacement brushes through both normal scale APIs, including numeric
  distance/elevation/offset handling, UV locks, continued rotation/movement/height
  edits, undo and native saves. Conservative exact input intent still denies
  nonuniform transforms. A short transaction-leaf fix also preserves saving valid
  Windows filenames whose old temporary names exceeded the path budget.
  Current source checks: model187/187, native5,257 assertions/39 cases and affected
  UI129/3; actual app/checker/UI callers rebuilt without new warnings. Fresh
  read-only source review found no blockers.
  A5.11 also adds static Source model **bodygroup selection** across
  multiple bodyparts/models and blank choices, with selected render geometry,
  bounds, picking, entity refresh and canonical caches. Source studio defaults
  honor `body`; explicit and non-Source declarations retain their semantics.
  Review-discovered duplicate GPU uploads and body-zero browser bounds were fixed
  and acted: four unique meshes retain four VBOs across body/skin renderers and
  release all buffers when renderers go away. Model187/187, GL21/21, body-focused
  2,748 assertions/6 cases, weak-holder63/1 and targeted UI129/3 pass; actual public
  callers rebuilt without new warnings. Fresh fix review found no remaining
  blockers in its inspected scope. Corpus remains document38/strict35/preview38
  with all38 originals unchanged. Root-pose/preview only, not animation or full
  shader/FGD parity. Clean pinned portable staging opened owned CSS and Momentum
  document windows with System-only PATH and normal exits0; final installed
  artifact/runtime provenance belongs in the candidate receipt. Independent
  native/browser/Hammer acceptance remains pending; retain A5.10 as recovery.
  No Source compiler, Valve content, or third-party/user map is bundled. See
  [alpha instructions](ALPHA.md) for the owner test path.

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

**Candidate boundary:** basic native brushes/points can be created, duplicated,
deleted and placed in supported plain brush-entity ownership, without a Quake
MAP round trip. Arbitrary topology and opaque-reference mutations remain gated.
Existing native entities have typed FGD hints and an ordered output table;
supported props have a static root/default-pose preview.
Groups, visgroups, hidden state and cordons are interpreted/validated read-only,
not scene-parent or visibility authoring. A camera action moves the eye only.
Custom dependency discovery is incomplete and writes an explicitly reviewed
new distribution ZIP, not a BSP pak lump. Generic Source remains configurable
without certifying arbitrary branches. [Tool limits](SOURCE_TOOLS.md) and
[human gates](../../lextest.md) remain part of the full A5 exit.

- Basic native object creation, stable object/side IDs, duplication/deletion,
  limited brush-entity ownership and preserving command history/save.
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
