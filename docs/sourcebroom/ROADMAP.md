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
- **A5.36 candidate:** connected2..64 same-power native displacement POSITION
  sewing, full unique shared base edges and simultaneous original-position means
  at multi-face junctions. Staged all-mesh commit, one Undo/Redo/Save As/reopen;
  no unneeded offset table for an unchanged member. Inspector281 after desired
  connected3 red (198 controls), focused core2888/13 and full Model322/App50/
  GL21/Render28 pass. Current UI63/64 only known untouched Unicode preference
  rename failure, no skip/fix/retry. Seven builds0 compiler diagnostics/six held
  fingerprints/direct parent review. Read/derived-build/serializer boundaries and
  private object fields remain A531; corpus proof retained, not re-dated. No
  normal weld/neighbors/mixed-power/SDK/Hammer/new human/full Source acceptance.
  Keep whole A5.35/A5.33/A5.25 recovery; full Source development continues.
- **Retained A5.35 candidate:** static literal `$color`/`$color2` RGB factors in known basic
  previews, five fragment configurations +masked terrain, independent of raw alpha/
  opacity/fade. Same GPU3847 passes after59 desired RGB runtime failures; full
  Model316/UI64/App50/GL21/Render28 pass, seven builds0 compiler diagnostics,12 held
  fingerprints. Legacy scalar1 omission preserved after regression, not arbitrary
  scalar parity. HDR/proxies/per-pixel tint coupling/full gamma/branch semantics
  remain guarded/pending. No geometry/layout/native writes; corpus proof retained.
  Keep whole A5.34/A5.33/A5.25; human/Hammer and full Source support still pending.
- **Retained A5.34 candidate:** standard WorldVertexTransition modulation-mask preview on
  demanded brush materials: mounted scalar path, flat third data image, shared
  byte budget, red/green weight remap and sampler2 restoration. No layout/geometry/
  native-write widening. Same GPU3587 passes after128 desired runtime pixel RED;
  full Model315/UI64/App50/GL21/Render28 pass, seven builds0 compiler diagnostics.
  GPU values are synthetic shared-cache Vertex type through real holder/VBO/Face
  stages, not actual native cache construction. VMF/corpus boundaries retained,
  not rerun. Transforms/proxies/masked-blending/selfillum-enabled masks stay gated;
  no full shader/SDK/Hammer/new human acceptance. Keep A5.33/A5.32/A5.25 recovery.
- **Retained A5.33 candidate:** same-power single-shared-edge native displacement POSITION
  seams via staged XYZ offsets, one undo and preserving Save As/reopen. Partial,
  ambiguous/disconnected/power/readonly guards stay; no normals/neighbor/topology
  or general sculpt/sew claim. Model177/UI170 and full Model314/App50 pass.
  Earlier UI64 passed; latest63/64 only known untouched preference rename, no fix/
  skip/suppression. Zero compiler diagnostics on8fingerprints/direct parent review. Exact reader/
  serializer bodies unchanged: A531corpus retained, not re-dated. Human/Hammer/
  arbitrary compiler/full Source pending; keep A5.32/A5.31/A5.25 recovery.
- **Retained A5.32 candidate:** current3D-view eye/look capture into a LIVE camera draft,
  not an automatic native write or viewport/FOV/roll change. Owned provider and
  existing scoped Add/Apply/history; active2D/invalid/guarded modes refuse.
  UI175/3, full UI64/App50 pass on9compiled UI fingerprints; intermittent preference
  rename passed this run, not fixed. Retain unchanged A531 Model312/corpus and A527
  GPU boundaries, not re-dated. Human/Hammer/all-layout/full Source pending.
- **Retained A5.31 candidate:** native displacement per-vertex XYZ offsets and explicit
  absent-table initialization, production history/Save As and independent distance/
  alpha. Partial/empty/case-aliased data stays protected; alpha aliases also now deny
  typed authoring without flattening. Offset Model449/3 and actual UI116/1 pass;
  full Model312/App50 pass, UI63/64 only known untouched Windows preference rename.
  Five-target build0 without compiler diagnostics, nine fingerprints/direct parent
  review, fresh corpus38/35+same3 reasons/38 and original hashes. No full Source,
  sculpt/sew/new/power/normal authoring, arbitrary compiler or human/Hammer/GPU
  acceptance. Keep whole A5.30/A5.29/A5.25 recovery; pending small owner check.
- **Retained A5.30 candidate:** scoped LIVE native camera eye/look/record CRUD and stored
  active choice, production undo/redo/Save As, not generic raw-entry writes.
  Strong IDs/revisions/staged replay and protected unknown shapes; saved Tools
  remains saved/read-only. No viewport capture/motion or projection/FOV import.
  New Model262/live UI136, full Model309/App50 and fresh corpus38/35+same3/38 pass;
  original38 SHA unchanged. UI63/64 keeps the untouched known Windows preference
  Unicode-rename failure, also isolated; no skip/suppression. Actual five-target
  builds0 new warnings,17 fingerprints and direct parent review, not independent/
  Hammer/human/all-layout or full Source acceptance. Keep whole A5.29/A5.25 recovery.
- **Retained A5.29 candidate:** direct saved native Source Tools entry, including explicit
  read-only previews, without autosaving current drafts or creating/enabling a
  compiler/launcher. New/non-native documents remain refused; repeated entry
  preserves the same-path dialog/review. Local optional-BSP target changes clear
  approval without saving build settings; existing rights/header/budget gates
  remain. Tools132, native Qt menu-to-root75 and regular offscreen UI61/61 pass;
  product/UI builds have zero compiler warning/error lines on eight fingerprints.
  App50 and Model299/Render28/GL21/GPU2693 are retained prior boundaries, not new
  runs. Direct parent review only; owner/Hammer acceptance pending. No camera
  authoring/persistence or full Source claim. Keep whole A5.28/A5.25 recovery.
- **Retained A5.28 candidate:** separate saved camera eye+look navigation, retaining
  eye-only behavior and existing unsafe-target guards. Double-before-float
  direction, opposite/vertical heading, pending position animation, Qt forwarding
  and original map/file preservation act. Tools118/actual MapWindow26 and full
  UI60/App50 pass; actual product/UI builds have no compiler warning/error lines.
  Model299/Render28/GL21/GPU2693 remain A5.27 unchanged-boundary evidence. Parent
  review only; new human/Hammer pending. No camera/native-metadata authoring,
  active-camera persistence, build authority or full Hammer camera state import.
  Full Source support continues; keep whole A5.27/A5.26/A5.25 recovery.
- **Retained A5.27 candidate:** finite scalar VMT opacity0..1 only for explicit
  translucent, noncutout basic previews. Both brush/prop demanded metadata and
  Patch inheritance act; strict/nontranslucent/cutout/terrain/invalid/proxy paths
  remain refused. Native packed production GPU2693, material136 and brush/prop10224
  assertions pass. Seven-target build has no compiler warning/error lines; full
  Model299/UI59/App50/Render28/GL21 pass. Parent review only, new human/Hammer
  acceptance pending. No catalog decode, image-budget/native-geometry authority,
  full shader compatibility or performance claim. Keep whole A5.26/A5.25 recovery.
- **Retained A5.26 candidate:** explicit initialization of an entirely absent native
  displacement alpha table, then existing per-sample editing. No automatic
  repair of present partial/empty/unknown data or wider geometry authority.
  Initialization is a separate undo boundary; save/reopen, unchanged geometry,
  normal alpha collation and original bytes have acted controls. Actual full
  Model298/UI59/App50/Render28/GL21 and seven-target build pass without new
  compiler warnings. Parent review only; new control/Hammer checks pending.
  Keep whole A5.25 as recovery; full A6/Source support is not complete.
- **Retained A5.25 correction:** owner-confirmed A5.24 scrambled vertices traced to the
  real cache36-byte/upload-holder32-byte mismatch. Shared complete vertex type
  and typed production copy fix that boundary without changing native geometry,
  source bytes or edit/save authority. Production upload141 and packed real-GPU
  checks2597 pass. Previous shader/draw-count controls missed the upload seam.
  The owner confirmed the same-map geometry/edges view works. Other editing,
  performance/maps and Hammer acceptance were not confirmed. Avoid A5.24.
- **A5.24 known-broken candidate, superseded by A5.25:** basic WorldVertexTransition brush/terrain preview loads
  two mounted VTF layers under one retained-byte budget. Native alpha0..255 is a
  separate vertex blend weight; opacity and editor fade remain independent.
  Existing scalar Apply/undo/redo invalidates the real node cache, with no native
  geometry/save-authority change. Default strict/base-only material modes remain
  strict; masks, independent transforms and proxies still refuse. Bump/detail/
  specular rendering, paint/smoothing and full shader parity are not implemented.
  Current Model296/GL21/UI59/App50/Render28 and real GPU2580 assertions pass; seven
  targets build with zero warnings/errors. The old intermittent preference issue
  is not proven fixed. Parent review only; owner appearance/Hammer check pending.
- **Retained A5.23 candidate:** explicit per-sample stored displacement alpha0..255 through
  the face inspector and normal native Map preflight/history. Complete original
  alpha rows only; no-op/surgical save/reopen/exact undo/redo preserve source and
  geometry. Missing alpha does not disable valid height editing. This is not
  material blending, paint/smoothing, topology or missing-row creation. Full
  Model294/294 and the inspector pass; full UI58/59 retains the known unchanged
  Windows preference rename failure. Actual client/checker/test builds have0new
  diagnostics; parent review only, owner/Hammer weight acceptance pending.
- **Retained A5.22 candidate:** explicit own-record native visgroup RGB authoring through
  the existing scoped request/history. Three bounded channels and separate Set
  in the LIVE inspector; absent RGB is not an invented default, same-value is
  lexical no-op, and unsupported/repeated scalar color is color-only protection.
  Retained and moved-subtree emission, new drafts, save/reopen, exact undo/redo
  and stale-row refusal are covered. Actual preimplementation RGB REDs retained;
  A5.22 fullModel292/UI59 and product/checker builds0 new diagnostics. No
  member editor.color/tint/propagation/color removal or new geometry authority.
  Parent reviewed the six-file delta; owner/Hammer RGB acceptance pending.
- **Retained A5.21 candidate:** loaded/new manual-visgroup tree CRUD, ordered direct
  assignment/unassignment and explicit manual Show/Hide through owning scoped
  snapshots and the LIVE UNSAVED Map inspector. Native wrapper/manual/automatic
  causes remain independent of ordinary session visibility; native-hidden bodies
  are not picked, selected through aggregates or submitted through showHidden.
  Protected/unprojected rows remain explicit, hidden-member recovery uses metadata
  handles, and ordinary session hiding saves without changing native flags.
  Full Model289/UI58/App50/Render28 and actual cached-ownership128 assertions pass,
  zero new compiler warnings; corpus38/35/38 and every original hash unchanged.
  Whole-family review findings were reproduced and fixed; focused independent
  re-review reports no remaining blocker. Human/Hammer acceptance is pending.
  No automatic classification, RGB/wrapper/automatic authoring or reconstructed
  Hammer checkbox; unsupported geometry/cache ownership remains guarded.
- **Retained A5.20 candidate:** ordinary validated NON-displacement point_data/vertices_plus
  ownership: duplicate/delete, whole/partial plain brush entities and safe entity
  parent additions. Generated copies continue move/save/reopen/further ownership;
  no cache/ID stripping or global opaque allowance. Exact source closure, original
  side ownership and canonical unique generated prototypes are validated even for
  unchanged geometry. Both actual inputs128 assertions/focus91/model260/App50 and
  zero-new-warning product builds pass. Same corpus38/35/38/original hashes.
  Displacement ownership, cached-member groups/target ancestors, reparenting,
  clipping/topology and malformed/unknown payloads remain guarded. Owner/Hammer
  retry pending; full visgroup/hidden/advanced Source authoring remains outstanding.
- **Retained A5.19 candidate:** parsed displacement meshes and validated point_data OR
  vertices_plus now compose for typed whole-brush translation/proper rotation/
  positive uniform scale. No frame/schema/tolerance inference change: only the
  existing parsed-side allowance reaches cache staging before unchanged later
  shared-frame/base-mesh checks. Actual discovered same-ID terrain brushes in BOTH
  files pass61 assertions; focus83/model252/App50/productbuild0/newwarnings0 pass.
  Same load/metadata38/35/38/input hashes, separately scoped brush evidence. Unknown
  mesh/cache, stale/one-sided or inconsistent frames, nonuniform/shear/reflection/
  topology/other opaque operations remain guarded. Owner retry pending; larger
  native operation/visgroup/hidden authoring still outstanding.
- **Retained A5.18 candidate:** Hammer++ `vertices_plus` implicit-order polygons share the
  strict indexed `point_data` ring/incidence/winding correspondence and surgical
  xyz patcher. Exact block IDs prevent cross-schema admission; mixed caches refuse.
  Observed inert preview-root shapes are separately validated, not `*_plus`
  whitelisted. Both actual original and Hammer++ Map/save/reopen/further-edit/undo
  paths pass54 assertions; fullmodel249/focus80/App50 and real productbuild0/new
  warnings0 pass. Corpus load/metadata38/35/38 and original hashes unchanged, not
  universal brush acceptance. Nonempty LUT/texlight/backgrounds, future root fields,
  mirrors/topology and combined displacement/cache motion remain guarded. Owner
  retry pending. Larger native visgroup/hidden authoring still needs its protocol.
- **Retained A5.17 candidate:** native Strata `point_data` polygons no longer blanket-block
  supported brush-content edits. Strict indexed cache/ring matching, whole-brush
  vertex incidence and preserved relative winding admit same-combinatorics motion,
  positive scaling, bounded boundary edits and UV/material changes with surgical
  row updates. The exact owner brush passes actual Map move/save/reopen/undo/redo;
  the native-ID-matched Hammer++ `vertices_plus` variant correctly remains guarded.
  Model246/246, affected77, App50 and actual app/checker rebuild0/newwarnings0 pass;
  corpus load/metadata38/35/38 and original hashes unchanged, NOT universal brush
  acceptance. Mirrors/topology/point-data clone/delete/group/clip and combined
  displacement/base-cache motion remain restricted. Owner retry pending. Next
  separate schema increment: `vertices_plus`, before larger visgroup authoring.
- **Retained A5.16 candidate:** live native cordon editing now handles loaded modern and
  legacy ordinary controllers plus new modern records, multiple boxes, names and
  optional activation. Atomic Map history, scoped handles, exact no-op/raw save,
  stale-action refusal and live UNSAVED UI are separate from the read-only saved
  inspector. Fit copies valid selection bounds without changing selection or soft
  bounds. No viewport clipping, automatic legacy migration or compiler-region
  guarantee. Full model243, post-private-rename model24, App50 and affected UI13
  pass; real callers rebuilt0/no new warnings, independent focused review ready.
  Corpus38/35/38 and all38 originals unchanged. Human/Hammer acceptance pending;
  keep whole A5.15 as recovery. Visgroup/hidden authoring needs a larger loaded
  membership/visibility protocol, not a misleading fresh-only toggle.
- **Retained A5.15 candidate:** ordinary native Group/Ungroup actions now handle supported
  loaded/new/duplicated nested groups, plain world brushes and whole ordinary
  entities. Native group IDs/membership are staged together; physical editor groups
  do not change VMF lexical solid/entity ownership. Undo/redo restores exact live
  siblings/selection; native reopen uses deterministic lexical order. Derived
  `Group <id>` labels need no name prompt; rename and visgroup/layer/hidden/linked
  authoring stay guarded. Ambiguous references isolate their components, not
  independent plain brushes; opaque geometry-reference protection remains active.
  Review-found malformed membership, duplicate added-index notifications, parked
  replay and opened-context failures have acted regression controls. Full Model219,
  App50 and affected UI5 pass; actual app/checker/UI callers rebuilt with no new
  compiler warnings. Fresh focused review found no remaining blockers. Manual
  owner/Hammer acceptance is pending; keep whole A5.14 as recovery.
- **Retained A5.14 candidate:** ordinary native BOTH clipping now retains Front under its
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
  arbitrary metadata authoring, complete dependency discovery or all-branch support.
  A5.15 separately admits ordinary native groups; the saved-file inspector itself
  remains read-only.
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
  branch defaults. That candidate did not admit scalar$alpha. A5.27's
  narrow explicit-translucent path above is separate; zero/sentinel cutoff
  semantics are still not guessed.
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
  is not claimed: displacement nonuniform scale/shear/flip, sculpt/create/power/sew, blend
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
Plain uncached non-displacement brushes already admit valid nonuniform scale,
shear and flip through normal commands; six actual Map/UV-lock controls on A5.22
verify existing behavior, not a newly implemented transform feature. Cache,
displacement, opaque-reference and prop restrictions are separate and unchanged.
Existing native entities have typed FGD hints and an ordered output table;
supported props have a static root/default-pose preview.
Ordinary native groups now have bounded scene Group/Ungroup authoring (A5.15).
The saved-file metadata inspector stays read-only. A5.16 separately admits live
ordinary cordon authoring in map properties. A5.21 separately adds manual-visgroup
hierarchy/direct membership and explicit manual visibility intent, without
wrapper/automatic flag authoring, automatic classification or a reconstructed
Hammer checkbox. A5.22 separately adds explicit own-record visgroup RGB only,
not member editor.color, object tint or color propagation. A5.30 adds scoped
LIVE camera record/active-choice authoring separately. The saved-file inspector
is still read-only; its distinct eye and A5.28 view-navigation actions preserve
or apply look direction respectively, without projection/FOV import. A5.29 makes
that saved inspection reachable directly on read-only previews, not editable.
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

The current foundation admits bounded native meshes, distance samples, selected
proper similarity transforms and separately stored alpha samples (A5.23).
A5.26 adds explicit absent-alpha-table initialization; A5.31 adds independent
per-sample XYZ offsets and explicit absent-offset-table initialization. A5.33
adds bounded same-power position-seam sewing; A5.36 extends this to connected
2..64-mesh patches with simultaneous junction means. Neither claims smooth-normal
welding, SDK neighbor generation, mixed-power or general sew support. A5.27's
bounded scalar VMT opacity is distinct from displacement weights or shader parity. This foundation
does not complete A6: stored blend-weight editing is not full shader blending,
paint/smoothing or topology/power/sew, and independent native acceptance remains
pending. The remaining feature/exit requirements below still apply.

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
