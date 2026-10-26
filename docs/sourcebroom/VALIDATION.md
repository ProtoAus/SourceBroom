# SourceBroom validation — A5.17 native point-data brush editing

This is a native Source 1 VMF editor fork with configurable game/content/tool
paths. CSS and Momentum/Strata are the first tested profiles; FTESurf is an
optional runtime. Implemented, automated, packaged and manually accepted are
separate states. Full A4/A5 exit and a general release are **not** claimed.

## A5.17 — strict indexed face-polygon content edits

An owner reported a loaded VMF refusing brush-content edits. Actual production
VmfDocument/VmfScene entry lookup reproduced it after a real transform: nine
assertions, eight positive controls passed and the intended opaque refusal. Schema
was side `point_data` with `numpts` and repeated indexed xyz rows. The previous
corpus probe changed metadata, NOT brush geometry; its pass was insufficient for
this acceptance question. That distinction is now explicit.

- Synthetic predispatch RED:62 assertions,57 controls passed/five opaque failures.
  Final focused model **77/77**, one full model **246/246** at the final cut and
  actual original/Hammer++ combined Map opt-in **42 assertions** pass.
- Original requested brush: acted Map move, modification count, Save As, reopen,
  undo/redo/exact source restore. Variant resolved by production native ID, not
  equal entry number: different `vertices_plus` schema has an ACTED negative
  transform, unchanged brush/selection/history/count/native bytes/input controls.
  Both originals retain their SHA-256 hashes; only test-owned outputs are written.
- Independent cube/ramp controls cover cache/no-cache movement, proper rotation,
  positive nonuniform scale, material/UV and boundary changes with unchanged plane
  but changed polygon. A second ACTED reopened transform validates saved cache
  consistency; identical-brush validation would bypass the helper and is not proof.
- Strict count/index/finite/cyclic-bijection validation; malformed, duplicate,
  missing, out-of-range, nonfinite, noncyclic, unknown/nested/misplaced and
  mismatched inputs refuse. Mirror/topology/clone/remove/clip and combined
  displacement/cache base motion remain guarded. Later-invalid Map batch is
  mutation-free; exact unedited source and lexemes survive accepted edits/undo.
- Whole-brush rings/incidence/orientation mappings are computed once lazily and
  reused across cached faces; no numerical performance improvement is claimed.
  Full model was rerun only when parent-required production/test corrections
  invalidated the prior cut. Build/filter/test-oracle failures remain preserved.
- Parent independently reviewed the final three-file cut and verified raw hashes
  against the frozen worker report; no focused blockers remain. Actual app/checker/
  App targets rebuild0/zero new compiler warnings and full App **50/50** passes.
  No UI-source change or fresh full-UI claim. Standalone clangd fallback had no
  compilation database; actual MSVC gates, not that fallback, are authoritative.
- Rebuilt production checker remains document38/strict35/preview38, exact same
  three strict refusals/all38 original hashes unchanged. These are load/metadata
  checks only; brush evidence above is separately scoped to actual actions.

Known boundary: existing 1e-6 arithmetic cache/ring tolerance, unchanged whole-brush
face/vertex incidence and relative winding. No global opaque relaxation, stale
cache, loader-authority switch, branch/profile hardcoding or native ID allocation.
Hammer++ `vertices_plus` needs its own contract, not the point-data allowance.
Human editor/Hammer/visual acceptance and full Source support remain pending.
Installed package provenance belongs in the final candidate receipt.

## Retained A5.16 — live native cordon state, history and UI

VmfScene owns bounded cordon state with strict local admission, owning snapshots,
strong scene-scoped record/box handles, pure complete staging and semantic-state
replay. Original document/bindings stay immutable; surgical emission shares the
single generated-root append with new native entities/groups. Map requests never
use saved snapshots or row/name lookup as authoring authority.

- MODEL runtime RED: compilable live staging acted; 16 controls passed and three
  intended serialized deltas failed before dispatch. Final model focus14/371
  assertions and preservation/group gate59 pass.
- MAP/HISTORY runtime RED: nine controls passed/seven intended live/history/save
  failures. Final history10/229 assertions and one full model **243/243** pass.
  Changed/Unchanged/Refused results, semantic no-op, Save As/reopen/further edits,
  same handles on replay, stale/foreign tokens, native namespace and real open-group
  selection/context/index/bounds/visibility invariants are covered.
- UI runtime RED: 18 controls passed/six intended live/history/save failures.
  Final scoped UI **13/13**, 492 assertions, includes six cordon cases/174 assertions
  and seven retained inspector/window/action/group controls. Actual clicks/typing/
  press-release stale row/delete/undo/document replacement act, not only API calls.
- Optional fields and untouched invalid name bytes/noncanonical finite numbers are
  preserved; changed fields round-trip double precision. Multi-record/multi-box,
  legacy in-place restrictions, empty/final removal, later-invalid atomic refusal,
  new-controller plus new-native-entity root composition and local budgets pass.
- Private serializer helper was renamed `emitEdits` at three sites for Qt macro
  compatibility, not a macro/include-order workaround. Actual model rebuilt and
  focused **24/24**, 600 assertions, pass after rename; unchanged full243 was not
  rerun. Two retained window tests now reuse existing owned INI settings scope.
  Earlier unscoped pass results are not proof of settings isolation.
- Parent verified all20 code hashes against the final reviewed snapshot. Actual
  app/checker/App/UI targets rebuild with zero new compiler warnings; full App
  **50/50** passes. Final scoped UI13 is reused by exact source identity. Fresh
  independent review found no issues in the focused implementation.
- Rebuilt production checker: document38/strict35/preview38, same three strict
  refusal inputs and all38 original SHA-256 hashes unchanged. Probes act in memory.

No native object/side/group ID, scene node, selection, visibility, index or soft
bounds is changed by cordon editing. Unknown inside/duplicate/mixed/malformed
controllers refuse; unrelated outside data and original bytes remain. Preview
immutability and legacy geometry guards remain. No viewport clipping, compiler
region execution, automatic legacy migration, arbitrary observer/OOM atomicity,
full UI pass or universal Source compatibility claim. Human/Hammer/visual checks
remain pending. Portable installed provenance is recorded by the final receipt.

## Retained A5.15 — ordinary native group membership and safe replay

Supported loaded/new/duplicated nested groups, world brushes and whole ordinary
entities now act through the existing native Group/Ungroup model and UI actions.
Surgical membership/group-entry edits preserve VMF lexical ownership, repeated and
unknown fields, numeric spellings and unrelated bytes. Exact live undo/redo ordering
is distinct from deterministic lexical reopen ordering; no new member-order field
or TrenchBroom link ID becomes native authority.

Executed controls and review corrections:

- Runtime projection RED retained 18 acted property/edit/undo/raw controls before
  the missing loaded-group projection failed. Native group action gates then
  passed with stable IDs, nested membership, undo/replay and preserving saves.
- Ambiguous repeated membership retains every normalized candidate reference for
  component isolation. Block-form/mixed malformed group/visgroup/visibility fields
  refuse before publication; rejected operations do not consume identities/history.
- Added notifications cover nonoverlapping live subtree roots exactly once after
  placement, independent of parent addresses; property-index removal/lifetime and
  actual notification controls cover the previous duplicate-index risk.
- Registered parked replay validates native binding/link/support/visibility plus
  command-local lock state; Locked refuses while ordinary Inherited and explicitly
  Unlocked controls act. Current opened-group/ancestor Ungroup refuses pre-mutation.
- A later RED reproduced six unrelated-brush failures beside an ambiguous group
  with 266 other assertions passing. The private raw-record recognition fix leaves
  unsupported components withheld but permits independent GROUP, transform and
  Front/Back/Both; true opaque geometry-reference negatives still refuse. Raw bytes,
  allocator state, selection/count/history, undo/replay and reopen are checked.
- Final full model **219/219**, full App **50/50**, affected UI **5/5** pass. Actual
  app/checker/App/UI callers rebuilt with zero new compiler warnings. Fresh focused
  follow-up review found no remaining blockers. Earlier unrelated Fs/GL evidence is
  retained, not rerun or relabelled as a new A5.15 measurement.
- Rebuilt production checker: document38/strict35/preview38, exactly the same three
  strict refusals and all38 original SHA-256 hashes unchanged. Probe edits act only
  in memory; originals are never write targets.

Original runtime failures and correction handoffs remain in owned evidence. A valid
known empty group deliberately changed from an obsolete negative expectation to an
acted positive with its input unchanged; genuine foreign-input controls were not
removed. Budget ceilings remain checked, not dynamically exhausted. General
failed redo/undo/transaction policy and arbitrary observer/OOM atomicity are not
expanded. Owner/Hammer/appearance and complete Source compatibility remain pending;
portable boundary provenance belongs to the final candidate receipt.

## Retained A5.14 — native BOTH, one admission and actual preview parity

One compound command and staged scene admission split proper ordinary brushes:
Front keeps original identity, Back is a same-parent sibling with remapped stable
IDs and preserved native prototypes. Loaded/fresh/template/duplicate world and
plain-entity origins act. No-op/tangent subjects retain identity; mixed selections
retain unchanged subjects. Undo parks exact siblings, restores original selection
and excludes them from save. Redo validates live originals AND parked payloads and
restores the same nodes/IDs. Deterministic preserving save/reparse, continued
edits, multi-parent and refusal controls pass. Invalid mixed/hidden/opaque/ref/
displacement/owner/budget input refuses before predictable mutation. Actual object/
side ID-length and aggregate ID-byte refusal have allocator controls; million-entry
ceilings are checked, not dynamically exhausted. Arbitrary OOM/notification strong
atomicity is not claimed; local ownership reconciles attachment/removal unwind.

Parent preproduction Both RED12 and actual native preview RED24 (12 material/UV
mismatches + inheritedBoth12) are retained. Actual preview-vs-commit assertions
cover both orientations and first-nearest-normal ties, not just saved cut faces.
These repair an earlier evidence gap: post-commit checks alone did not establish
preview parity. Admitted default surface data is checked; arbitrary nondefault
SurfaceAttributes are not newly supported.

Both **9/9**, retained native/core **8/8**, ClipTool **4/4** (670 assertions), parent
full model **201/201**, full App **50/50**, and real app/checker/UI caller builds
pass with zero new warnings. Independent focused review found no remaining static
blockers. The unchanged private corpus remains38/35/38 with38 original hashes;
final installed portable/checker/archive evidence belongs to the candidate receipt.
Manual/Hammer and the untouched full-UI preference failure remain pending/unfixed.
Global failed redo/undo/transaction policies are unchanged; Both is not certified
retryable by inheriting single-output clip permission. Keep A5.13 as recovery.

## Retained A5.13 — fresh/duplicated/entity one-output clips and bounded retry lifetime

Visible fresh/template and duplicated plain brushes, world-owned or within a
supported plain brush entity, now use the same native Front/Back in-place command
before first save. Loaded-world controls remain. Acted native/tool tests check
geometry, live owner/visibility, solid/side identities, surviving source subtrees,
registered cut states, undo/redo, deterministic preserving saves and reopen.
Hidden/unsupported owners, opaque attachments/global references, displacement,
invalid mixed batches, arbitrary topology and BOTH halves remain refused.

A hidden-owner redo regression first reproduced two lost-history checks. A narrow
default-false command hook certifies only an already-registered native clip's
pre-swap validation refusal, reset before each attempt. The SAME parked command
then succeeds after owner repair with exact prior cut bytes/IDs and modification
count. Other failed redo/undo and transaction disposal remain unchanged.

Independent ownership review found no remaining blockers in that scope. The new
shared-core review then found a concrete notification-lifetime issue: a history-
clearing observer could destroy the restored command before later observers.
Safe shared destructor markers reproduced **15 failures** before production repair;
no freed command was read. A scoped local unique owner now retains only that
notified command until dispatch ends/unwinds, while all three clear routes remove
visible history immediately without resurrection. Recursive redo during dispatch
refuses. Normal/throwing clear/execute/store, later observer access, exactly-one
destruction and throwing-without-clear followed by real successful retry pass.
Focused independent follow-up review found no remaining scoped static blockers,
including actual ownership/guard analysis, not merely inference from test results.

Actual model/App rebuilds, core/native **8/8**, App ClipTool **3/3**, full model
**194/194**, full App **49/49**, and actual app/checker/UI caller builds pass with
zero new compiler warnings. The source-level private corpus remains38/35/38 with
all38 original hashes unchanged; final installed checks/hashes/portable windows/
archive provenance belong separately to the candidate receipt. Unchanged graphics
and earlier toolchain evidence remain retained, not rerun or re-dated. The known
untouched full-UI preference failure remains unfixed. Owner/Hammer acceptance is
pending; no full Source support or general release is claimed.

## Retained A5.12 — native ordinary one-output clip

Loaded visible ordinary world brushes use the real Map/ClipTool Front/Back path
in place, not delete/add replacements. Surviving native side bytes/IDs, original
solid ID, exact cut-side allocation, surgical removal/append, repeated clipping,
undo/redo, saving and reopen have acted geometry/serialization controls. Whole-
batch later-invalid/no-op/identity guards, opaque/global references, displacement,
wrong ownership and generic-swap/direct-save bypass controls remain enforced.
BOTH, generated/duplicated solids and brush entities remain deferred, not supported.

Independent static review found preview setup omitted a load argument and the new
App test lacked its filesystem test-utility link. Both seams were corrected before
parent compilation. Real model/App targets then built0 with zero new warnings;
first focused run exposed two test-helper child-count failures in reference/instance
arms. Native-ID target resolution corrected only that oracle, not production guards.
Corrected native model focus **4/4**, retained/native App ClipTool **3/3**, full model
**191/191**, full App **49/49** pass. Actual app/checker/UI callers rebuilt0 without
new warnings. The private38-file corpus remains document38/strict35/preview38,
same3 strict refusals and all38 original SHA256 unchanged. Existing graphics/body
shader evidence is retained, not a new clip visual or performance measurement.

Before the next candidate, a parent regression also reproduced **two** intended
preview/commit material and UV mismatches after an actual changed-bounds clip.
Copying the same nearest-normal attributes before clipping makes that control and
real ClipTool Front/Back attribute checks pass. Full model191/App49 and actual
app/checker rebuild remain green with zero new warnings; identity/guard logic did
not change. Compiler const-result/incomplete-face-header failures were preserved
and corrected; this is not a claim that LSP diagnostics alone proved compilation.

This is bounded candidate/source evidence, not an all-UI-green claim or owner/
Hammer acceptance. Final installed hashes/portable windows/corpus/ZIP provenance
belong separately to the candidate receipt. The known untouched full-UI preference failure
remains unfixed. The small copied-plain-brush human check remains pending in lextest.

## A5.11 uniform scale / bodygroups / transaction-path controls

Typed command intent admits only positive uniform native terrain scale through
both real Map APIs. Changed distance/elevation/offsets, retained direction magnitude,
sparse/default row spelling, shared similarity frames, numeric sample covariance,
UV cache/normals/indices, picking/bounds, undo, continued rotation/move/height edits
and native save/reopen have acted controls. Tiny nonuniform/general/shear/flip,
opaque references and unsafe numeric motion still refuse. A test stale face index
after oblique rotation was corrected by resolving the actual native side and
checking its original binding; this was an oracle fix, not a production pass.

Full-context native saves exposed Windows transaction names exceeding the path
budget. A valid248-character binary-input control passed its input checks then
refused temporary creation before the fix. Short PID/clock/sequence sibling leaves
now pass existing/fresh saves, exact backup preservation, stale-original refusal
and locked-promotion recovery. No safety checks were relaxed; this is not general
unlimited Windows path support. Native focus: **5,257 assertions/39 cases**.

Static body selection retains decoded bases/counts and all model memberships,
including blanks, without Cartesian geometry copies. Selected renderers, bounds,
picking, entity refresh and canonical caches agree. The original independent
review found duplicate GPU holders and default-body browser bounds. Parent
reproduced VBO8/16 versus expected4 in **14 assertions/1 case**, then weak shared
holders preserved four buffers across bodies/skins, released all with model data
still alive and rebuilt body0 to two then zero. Browser bounds now query the same
body as the renderer. Fresh focused fix review found no remaining blockers in
that scope. Body focus **2,748/6**, weak-array **63/1**; these are not an SDK-wide
memory/performance measurement or visual/browser acceptance.

Current normal suites: model **187/187**, graphics **21/21**. Actual affected app,
checker and UI ABI/callers rebuilt with zero new warnings; affected UI **129/3**.
Corpus remains document38/38, strict35/38 and explicit read-only38/38, all38
original SHA256 unchanged. Clean pinned portable staging independently opened
owned CSS and Momentum document windows with System-only PATH and normal exits0;
final installed hashes/runtime/ZIP provenance are recorded separately in the
candidate receipt. No desktop capture, foreground changes or owner/peer-window
manipulation were used; only the Popen-owned document HWND received normal WM_CLOSE.
Manual/native/Hammer/Source runtime observations remain pending in lextest.
The untouched full-UI preference rename failure remains unfixed, not all UI green.

## Retained A5.10 native rigid rotation / finite preservation controls

Actual Map controls rotate a native raised quad by quarter/oblique angles around
a pivot with both UV locks, then pick, undo/redo, translate, distance edit, Save As,
reopen and save again. Cache controls inspect submitted positions/normals/base-UVs
and retained indices. Two-displacement controls admit one whole frame but refuse
later-only or different nonzero frames, including tiny motions; mesh-only/base-only,
opaque attachments/native references and generic/scale/shear/flip remain guarded.

Rotated start/corners are points, authored vectors are vectors, scalar fields and
sparse/unchanged numeric spelling remain retained. Malformed previously ignored
offset-normal rows still load/translate unchanged but refuse rotation. Parent
reproduced finite cancellation that deformed samples and added pre-publication
matrix covariance. Independent review then found unused offset-normal overflow:
actual Core/Map red **357 assertions/2 cases**, then finite vector/provenance/output
checks yield **364/2 green**, unchanged world/history/count/serialization. Large
finite vectors remain supported, not clamped. This is a resolved finding, not a
second independent re-review or universal numerical/SDK certification.

Final native focus **4,009 assertions/41 cases**, normal model **184/184**, actual
app/checker/UI rebuilds and zero new compiler warnings pass. Earlier affected UI
**129/3** and Windows GPU **2,539** remain valid (only new rigid-precision/overflow
failure branches changed later, not inspector/material ABI). Prior full UI42/43
preference rename failure remains untouched/unfixed; no all-UI-green claim.
Built corpus retains document38/38, strict35/38 and read-only38/38, all38 original
hashes unchanged. Installed boundary is recorded separately in candidate receipt.
Native appearance/rotation feel, matching Hammer and compiler/game acceptance are
pending. A5.9 is recovery; full Source support is not complete.

## Retained A5.9 native translation / fixed-studio skin controls

Pure displacement translation now uses existing brush/Map movement, including
start-coordinate token preservation, raised mesh bounds/picking, undo/redo,
locked/unlocked UVs, Save As/reopen and subsequent distance edits/repeated saves.
All base planes and every displacement must share movement intent; arithmetic
epsilon is not permission for an unchanged/moved mismatch. Independent review
found that a later tiny mesh-only shift passed the original first-delta-only
check. The new two-mesh control reproduced two missing refusals, then passed
**16 assertions** after the minimum exact per-side zero-status correction.
Unknown attachments/references, topology and nontranslation transforms stay gated.

SourceVMF definition loading now opts into default-off fixed-literal studio skin
normalization, including definitions from includes. Other-game, model()/sprite,
explicit skin/frame, legacy and dynamic declaration controls retain behavior.
Entity `0 -> 1 -> 0` resolves actual red/green/red canonical material identities and
one-pixel bytes through the existing two-family model fixture. This does not
establish external universal FGD/SDK, animation or bodygroup parity.

Actual Release model/app/checker/UI builds pass without new compiler warnings.
The final normal model suite passes **181/181**; affected UI passes **129/3**,
native scene/UV case **233/1** and existing Windows GPU controls **2,539**.
The earlier full UI 42/43 preference rename failure is untouched, preserved and
unfixed; it was not rerun into green. Earlier Fs20/Gl21 evidence remains valid.
The new code retains strict35/38 and explicit preview38/38 corpus results with
all 38 original hashes unchanged; package receipts record the final installed
boundary separately. Native appearance, movement feel and Hammer/game acceptance
remain pending. A5.8 is recovery, not replaced in place.

## Retained A5.8 native mesh / integer VTF controls

The A5.8 candidate adds bounded native displacement geometry and existing
vertex-distance editing; A5.7 remains recovery. Mesh triangles,
base-position UVs, raised ray/rtree hits and bounds, surgical token save/reopen,
undo/redo, malformed meshes, misplaced attachments and ordinary-face shared-edge
ownership have acted synthetic controls. The real inspector test switches
power6 row64 to power2 without draining its queued refresh, then applies a centre
distance change and undoes it: 21 assertions pass. Native/VTF focus passes 3,182
assertions in 37 cases; the exact integer VTF loader case passes 276 assertions.

The normal baseline is Fs20/20, Gl21/21, Mdl178/178 and Ui42/43. The untouched
QPreferenceStore Windows `Access denied` replacement failure occurred again and
remains preserved/unfixed, not an all-green claim. Added guard/inspector cases
pass separately. The unchanged five-configuration Source alpha GPU control still
passes 2,539 assertions; this is not a new displacement visual/Hammer acceptance.
All 38 corpus originals keep their SHA256: document38/38, strict35/38 and explicit
read-only38/38, with the same three strict refusals and no new loader regression.

Displacement creation, sculpt/power/sew, native scale/shear/flip,
alpha-material blending, overlay/instance authoring and full shader/model parity
remain missing; A5.10 adds proper rotation, not arbitrary affine transforms.
The integer-format work does not add HDR/palette/normal/animated/cubemap/volume
support or claim a startup/whole-map speed gain. Independent review's two guard
findings and the adjacent-edge selection defect were fixed with acted controls;
manual native mesh appearance and independent Hammer checks remain pending.

## Current implementation boundary

| Area | Implemented and checked | Not implied |
| --- | --- | --- |
| Native document | Ordered/repeated keys, comments, unknown blocks, numeric text, byte-exact no-edit serialization; surgical token edits and entry/nesting bounds | All VMF branch semantics or independent Hammer acceptance |
| Scene/edit/save | Stable bindings; native starter/new plain brushes and point entities; supported duplication/deletion and brush-entity ownership; ordinary Front/Back/Both clips; strict geometry/UV/side-identity admission; undo/redo and transactional preserving save/Save As | Other CSG/arbitrary topology, native editor-group/layer/displacement/overlay/instance authoring |
| Compatibility preview | Explicit opt-in immutable partial reader; duplicate IDs/bytes unchanged; omitted-solid counts and labelled plane samples; selection-only commands; create-new unchanged copy | Editable repair, complete geometry, proxy hulls, normal save/export/build or relaxed syntax/resource limits |
| Entity tools | FGD property hints, choices/flags and I/O metadata; ordered/repeated outputs; unknown data retained, definition defaults not blindly inserted | FGD completeness or correctness for every game; malformed data silently repaired |
| Props/materials | Bounded static MDL44/48/49 with matching VVD/VTX, root/default pose, installed loose/VPK materials; Source fixed-studio entity skins and static body selection; explicit fallbacks | Animation, physics/full shader parity or every model variant |
| Native metadata | Read-only groups/visgroups/membership/hidden/cameras/cordons, known-reference diagnostics; camera-eye navigation | Scene parenting, visibility/cordon authoring or arbitrary compiler clipping behavior |
| Build tools | Actual staged-byte preflight; bounded reader; CSS20/Strata25 per-stage BSP-header contracts; leak/coordinate diagnostics; fresh staging and stop-on-failure | Whole-BSP validation, every Source branch, successful runtime load from process creation alone |
| Custom packages | Explicit custom-only root/rights acknowledgement, bounded discovery, reviewed immutable snapshots, separate approval, create-new distribution ZIP | Automatic stock/VPK redistribution, complete discovery, licence certification or BSP embedding |

See [SOURCE_TOOLS.md](SOURCE_TOOLS.md) and [SOURCE_BUILD.md](SOURCE_BUILD.md)
for the precise user-facing contracts.

## Current automated evidence

Windows x64 MSVC/Qt Release build of the actual application and test targets:
zero compiler warnings/errors in the final feature builds. Static analysis is
not substituted for compilation or runtime checks.

| Check | Actual result |
| --- | --- |
| Current filesystem / graphics / model / normal UI CTest suites | **20/20 + 21/21 + 175/175 + 43/43 passed** (259 entries) |
| Preference history | A5.5 full42/43 and isolated QPreferenceStore Windows rename failures remain preserved/unfixed. The current pass has no preference-code fix and is not an all-runs-green claim |
| Focused cutout/two-sided/Patch/rendering controls | Graphics **990 assertions /2 cases**, model **7654 /7 cases** and expanded shared-alpha GPU **2539 /1 case** passed |
| Focused accepted-fix UI controls | **4/4 passed**: property/I/O namespace controls, production build backend and metadata/package tools |
| Lossless supplied corpus | **38/38 passed**, no-edit plus acted in-memory mutation; original SHA-256 unchanged |
| Scene supplied corpus | Strict **35/38 constructed**, same three ID/geometry/UV refusals; explicit read-only preview **38/38**, counting/reporting unprojected solids with original hashes unchanged. Shared +/-131072 policy; retained smaller-envelope strict control was 32/38 |
| Native edited ramp/entity/output save | Production native map save/reopen control changes actual ramp bounds, property data and one repeated-output delay; original/copy separation maintained |
| Retained A5.2 installed CSS/Momentum pipeline | Production-saved room includes a fresh transformed brush and point after duplicate/delete controls; actual VBSP/VVIS/VRAD completes for CSS20 and Strata25, inputs unchanged; no game launched by this check |
| Retained A5.3 runtime-stage GUI smoke | Authored invalid-UV copy first refuses strict loading; explicit preview choice observed, READ ONLY title with one omitted solid, open/close exit zero and input unchanged. Title readiness is not visual/owner acceptance; final package receipt pins any installed rerun |
| Build-tree A5.1 GUI smoke | Actual Momentum prop fixture and two external copied maps opened and closed with exit zero; inputs unchanged. Title readiness is not visual or owner acceptance |
| Historical A5 installed GUI smoke | CSS/Momentum starter fixtures opened; the installed-prop description was overbroad because an unsupported input environment variable was ignored. Build-tree prop evidence is separate; installed A5.1 coverage belongs to its receipt |
| Packaging implementation | Authored custom files and optional short-name synthetic BSP written to new ZIP; independent reader extracts exact bytes; no-overwrite/approval/snapshot/path/budget refusals exercised |

The 38-file syntax, strict and explicit-preview checks were rerun for A5.3 with
all original hashes unchanged. Older envelope/compiler observations below are
retained A5.1/A5.2 evidence; the current package receipt distinguishes its actual
installed-candidate reruns from retained checks. This sample is useful evidence,
not all-map/all-game coverage. External map names, content and raw private logs
are not public fixtures.

### A5.7 authored cutout-reference controls

- Existing UnlitGeneric/LightmappedGeneric/VertexLitGeneric preview now validates
  an explicit finite `$alphatestreference` in (0,1] after direct/Patch resolution.
  Duplicate/case-variant/block/empty/vector/junk/nonfinite/out-of-range/zero/-zero
  inputs refuse, even without active alpha test. Strict optional-feature, scalar
  `$alpha` and WorldVertexTransition refusals remain. Missing threshold retains
  the existing0.5 preview fallback, not certified Source branch defaults.
- Direct controls cover all3 admitted shaders and decimal/exponent/whitespace
  syntax; Patch controls prove inherited .25, override .75 and invalid/zero final
  refusal. Demanded brush/prop transport matrix expands84 to252 arms by absent,
  .25 and .75, preserving acted2x2base/1x1override/32fallback and same-image VMT
  isolation. Raw/Q2/default metadata and deferred-loading controls remain.
- Binary textures derive the Source threshold; Opaque/Graduated do not invent
  cutoff. Explicit material alpha-function overrides win. No shader, winding,
  geometry, catalog-loader or GL-scope rewrite was needed.
- The real5-config GPU case retains1994 assertions and adds545 across50 authored
  cutoff scenarios: low threshold survives, high rejects, equality at1 survives,
  disabled alpha test ignores reference, explicit override reverses rejection,
  and raw cutoff precedes editor fade. Forcing a high-cutoff pixel's uniform to
  .5 makes it visibly leak; restoring .75 removes it. All5 acted negative arms
  fire. Real blend/depth and `$nocull` polygon restoration are checked.
- All6 product/checker/test targets built with zero compiler warnings/errors;
  normal CTest259 entries and focused counts above pass. Final receipt identifies
  clean committed binaries, installed evidence and source-identical test reuse.
- Research used a pinned public MIT renderer as semantic evidence only; no code
  copied. Its default0.7/lightmapped-alpha TODOs differ from an engine-wide proof.
  VDC pages returned access challenges, not evidence. Nonzero interval is an
  explicit bounded editor admission policy; scalar/zero semantics remain refused.
- After A5.6 delivery the owner said "looks dope! please continue!": overall
  observed visual approval only. Detailed A5.7 material/game/Hammer appearance
  remains pending in lextest.md; no performance/all-shader claim is made.

### A5.6 two-sided material controls (retained)

- Preview-only `$nocull` flags survive direct and Patch resolution into both
  demanded brush textures and static-prop materials via tagged Source metadata.
  Absent/false/true, malformed/duplicate/block, spelling and inherited
  enable/override-disable controls act; strict material inspection still refuses.
- The production brush/prop matrix now covers 84 raw-VTF/alpha/Patch/no-cull
  combinations, with 2x2 inherited base versus 1x1 override versus 32x32 fallback.
  A second VMT sharing the same base image retains ordinary culling; raw VTF and
  Q2/default metadata remain controls. Metadata peeks do not request loading.
- Configured culling remains separate from the effective Source hint; explicit
  overrides win. None/Front/Both save polygon attributes before blend/depth and
  pop in reverse order. Default/Back preserve their existing no-op GL behavior.
- The existing actual GPU case retains 394 alpha/cutout/fade/non-Source assertions
  and adds 160 culling draws /1600 assertions across all five configurations.
  A deliberately back-facing quad is culled blue with flag0 and visible red with
  flag1. Disabled/FRONT/BOTH entry state, explicit Front override, opaque and
  graduated opacity check pixels and actual cull enable/mode/front-face, blend
  and depth restoration. This is not Source backface-lighting/whole-viewport proof.
- A fresh independent read-only source review found no issues. Supplied tests
  were inspected, not independently rerun; async readiness was reviewed against
  production resource publication and existing deferred controls. The parent
  rebuilt all six affected app/checker/test targets and passed the 258 CTest
  entries with zero compiler warnings/errors after the public variant/API change.
- Two early fixture failures were preserved: new MockGl paths required explicit
  callbacks, and the basic synthetic prop has only one skin. The latter now
  reuses the passing aggregate-budget multi-skin fixture. No product guard or
  pixel/state assertion was weakened. Original assets/maps remain read-only.
- Final receipts identify the clean committed binary and installed checks.
  Appearance confirmation remains pending; no geometry/winding/shader rewrite,
  eager catalog scan, startup/FPS or full Source shader claim is made.

### A5.5 on-demand scalar Patch materials (retained)

- Both brush and static-prop texture paths resolve bounded Patch includes using
  existing mounted lookup priority. Scalar insert adds/overwrites, replace only
  changes present keys. Final supported-shader/base-texture/alpha validation is
  retained; pure-text inspection without a filesystem still refuses Patch.
- Authored inherited/overridden/domain controls use actual production loaders:
  a 2x2 base, 1x1 replacement and 32x32 fallback distinguish acted resolution from
  placeholder success for 42 raw-VTF/domain combinations. Mount override, chained
  inheritance, scalar rules and exact original-before/after bytes are checked.
- Cycles, normalized path/parent/absolute/drive refusals, 16-vs-17 includes,
  1 MiB-file and 4 MiB-total bounds have successful controls as well as refusals.
  Guarding explicit text reads does not constrain filesystem open internals.
  Nested/proxy/empty/deletion/duplicate/overlapping operations remain unsupported.
- Initial fresh review found NUL truncation through `Reader::readString` and
  quadratic repeated inherited-key searches. Both were accepted and fixed:
  length-sized owned exact reads retain NUL rejection, and a resolver-local
  ordered key/index/count map replaces full-vector searches. Root/include NUL
  negatives have valid counterparts; 40,000 inherited plus 40,000 inserted keys
  still apply the actual override. No timing/speedup claim or global cache.
  Fresh localized closure review accepts both fixes; not independent execution.
- Early test setup failures (macro precedence, missing parent directory and
  Windows LF/CRLF literal comparison) were preserved and corrected in fixtures,
  not product contracts. A Python cp1252 capture failure during the known UI
  failure was repaired by preserving native CTest logs and binary capture; no
  passing suites were repeated or evidence dates changed to hide it.
- Final package receipt, not this source-level evidence, identifies the clean
  committed executable, installed checks and archive hashes. Original content
  remains read-only. No initial-index/FPS/full-shader improvement is claimed.

### A5.4 orientation / material-root controls (retained)

- The owner previously reported one newly opening map looked inside out,
  including props. After A5.4 delivery, the owner confirmed on 2026-10-25 that the
  retry **looks good**. This is observed visual acceptance, not every detailed
  authoring/Hammer/model/shader or performance gate.
- A referenced real prop's triangle-cross/outward-normal control acts: the old
  importer produces 0 clockwise / 10,876 counterclockwise triangles; keeping VTX
  winding produces 10,876 / 0. Authored asymmetric list/strip and ownership/fixup
  controls pass. Native box/ramp renderer-cache vertices remain outward clockwise,
  with 12 actual triangles per brush and unchanged serialized native bytes.
  No global culling, coordinate or UV reflection was used.
- Authored VTF flag/VMT alpha matrices exercise actual brush collection and prop
  loaders. Basic opaque, cutout and translucent hints override shader-mask flags;
  direct VTF behavior remains. Unknown/repeated alpha flags refuse explicitly.
- Fresh review found a fade regression in an initial `.vmt` blend-disable approach.
  That approach was removed. A Source-only shader uniform now suppresses reserved
  alpha **after** raw cutout rejection, preserving editor fades and ambient draw
  state. Separate actual offscreen GPU controls compile all five shared alpha-test
  configurations (Face, Patch, Entity Model, material browser and UV view),
  activate real materials, query blend/depth state and read back pixels
  for lower/upper-case VMT and non-Source paths. The reserved-alpha negative arm
  acts; opaque, below/above-cutout, true opacity and 0.4 editor fade pass (394
  assertions). Bounded independent closure accepts the fix. This is not an entire
  viewport screenshot, every shader or owner acceptance.
- The first actual A5.4 app smoke caught an introduced active-uniform assertion
  in material-browser painting: including the shared alpha file alone did not
  keep the new unused uniform active. Material-browser and UV fragments now apply
  the same post-cutout policy. The existing GPU case was extended to every caller;
  the same corrected staging app opens/closes normally. The failed executable and
  crash evidence were preserved; the shader assertion was not weakened, a fresh
  candidate name was not used to bypass it, and no ZIP was promoted on failure.
- Actual Qt Source/non-Source collection-list controls prove one game-labelled
  browsing entry versus ordinary folders, a nonempty complete catalog, unchanged
  saved partial state on presentation, explicit all/none selection and undo/redo.
  Read-only previews disable saved-choice actions. No catalog pruning or new
  game-specific mount implementation was used.
- Tooling controls preserved: a binary probe initially used text `readString`
  (truncates at NUL), then matched production exact reads; the first GPU locator
  missed ShaderManager's `shader/name` prefix. Neither failure was product proof.
  Existing entrypoints were corrected, not bypassed with a wrapper chain.
- The unchanged preference rename test failed in the first full UI run, passed
  alone, then passed in the reviewed full 43/43 run. No preference product/test
  code was changed or flake claimed fixed. Final package receipt identifies the
  clean source/binaries and actual installed smoke separately from these checks.

### A5.3 compatibility and texture controls (retained)

- Synthetic valid/duplicate-ID/invalid-UV/incomplete/unlocated/all-invalid-brush-entity
  cases preserve native bytes in preview; strict controls act/refuse appropriately.
  No all-failed brush entity becomes a point. Native property/geometry/topology
  deltas, non-modifying mutation commands and overwrite/export attempts refuse;
  exclusive unchanged-copy and selection undo/redo controls act.
- Actual Qt drag-enter/leave and raw QMenu regressions have editable positive
  controls. Review found that a rejected entity drop stranded a long transaction
  and raw context creation stayed enabled. Both were reproduced and fixed:
  preview gestures stop before tools, menus/leaf actions disable, undo/history
  and exact native serialization survive. All 56 assertions pass. Independent
  bounded closure review accepts both fixes with no remaining issue.
- An introduced GUI error-message lifetime error was caught by actual strict
  refusal/preview prompting: `result.error()` returns a variant by value, so a
  retained member reference dangled. The owning message copy fixes the prompt;
  the runtime-stage positive result is separate from backend-only checks.
- A busy queue-prefix red control proves the previous tail resource never acted;
  resumable processing now reaches it. For 40,000 unused on-demand resources,
  two runs of per-entry erase took 600.9/592.1ms; one-pass compaction took
  7.86/8.41ms, with zero decodes and an empty retired catalog. This targeted
  teardown microbenchmark is not whole-editor latency or a frame-time guarantee.
- Two fresh-process catalog runs per arm preserve complete name/order signatures,
  CSS 17,696 and Momentum 66,741 registrations, no metadata-triggered pixel decode,
  and one explicitly loaded texture. Mean catalog time was about 587->452ms CSS
  and 3889->3144ms Momentum. Mount time was slower in the subject runs, about
  451->503ms and 5230->6515ms respectively. OS caches were not flushed; no
  whole-startup speedup is established. Broad browsing/long-session memory and
  owner-perceived texture responsiveness remain unaccepted.
- Browser weak-resource dimension tracking avoids layout invalidation on
  loading/failure/upload/unrelated notifications while retaining repaint and
  true dimension-change relayout. VFS controls preserve original path case and
  reload add/remove freshness. New optional VMT metadata is preview-only, not a
  shader-support claim. One explicitly selected real material previously rejected
  for optional roughness metadata now decodes through the production catalog path
  (one request/one decode); that sample is not broad material acceptance.
  Independent performance review found no issue.
- A public-layout change exposed one stale MSVC test object whose direct header
  dependencies were missing from its tracking log. Only that verified disposable
  object was rebuilt; tracking was restored and all model tests passed. No
  connection product/test code was changed to hide ABI crashes. The unchanged
  preference replacement test also failed once, then passed in isolation and
  later full 42/42; its host cause remains unresolved.

### Native authoring controls (retained and regression-covered)

- Production model controls act on fresh brushes/points, native-template and
  fresh duplicates, transforms, deletion, ownership and save/reopen. Original
  inputs and untouched native fragments remain unchanged; no MAP re-export is used.
- Native IDs reserve large decimal and leading-zero object/group/side IDs without
  narrowing. Undo/redo and repeated saves preserve bindings and issued IDs are
  not reused. Fresh duplicates receive independently owned complete side sets.
- Native starter creation clears saved-file association before the first save.
  Whole/partial plain brush-entity duplication preserves ownership. Deleting an
  implicit owner after a descendant edit removes only the highest original
  source span; undo restores exact original bytes.
- Unsupported clipboard/CSG/clip/topology/opaque-reference paths are refused
  before mutation, with transaction serialization as a backstop. Acted controls
  check selection, modification count, bindings, command history and save bytes.
- Independent review found a fresh-brush repeated/missing side-identity hole
  and a repeated-classname load regression. Both were reproduced, fixed and
  regression-tested. A bounded fresh closure review found no remaining issue
  in those fixes or native same-parent normalization. The later opt-in
  authored-room export is a test-only extension, not an independently reviewed
  production change. The review itself is not desktop, Hammer++, compiler or
  all-branch certification.

### Retained A5.1 usability controls and measured limits

- A closed authored box translated far beyond the old preview envelope has the
  expected six faces/eight vertices and exact bounds in the new envelope; its
  preserving save/reopen is byte-identical. The old envelope refuses it. An open
  authored brush still refuses even at +/-1048576; no missing face is repaired.
- The three remaining strict editable refusals are duplicate native IDs, invalid
  UV scale and an incomplete brush. Larger bounds alone do not conceal them;
  A5.3's separate read-only partial mode is not a strict-admission repair.
- A smaller +/-32768 diagnostic arm reproduces the reported `entry 938` error on
  one supplied map; its owned copy opens in the A5.1 GUI. This does not establish
  which executable/configuration produced the owner's original observation.
- On one installed Momentum workload, **10,488 collections / 66,741 material
  registrations** retain the same complete catalog/name signature. Equivalent
  ordered membership selection measures about **2545 ms linear / 12.6 ms hashed**,
  including hash-set construction. This is a targeted microbenchmark, not a
  claimed whole-editor speedup or a Hammer++ comparison.
- That workload still needs about **5593 ms** for its first mount/index and
  **3841 ms** for catalog construction. Skipping a redundant initial rescan saves
  repeated work; no stale cross-map cache or pruning is used. Metadata registration
  requests zero pixel decodes; an explicitly requested texture subsequently loads.
- Initial disk-index traversal errors are surfaced during mounting. An authored
  self-referential symlink control proves the constructor error callback acts,
  no partial cache is published, and removing the fault allows explicit reload
  recovery. A healthy constructor is silent; the callback is not retained.
- A normal map-level regression proves an initial loose material is registered,
  a newly added file is absent before explicit reload and present afterward, and
  removal is reflected after the next reload. Non-Source callers still rescan.

Initial content latency, broad browsing and long-session residency remain limits
in [BACKLOG.md](../../BACKLOG.md); no universal startup-time promise is made.

### Acted controls and review closure

- Numeric skin/frame indices are finite, nonnegative and below the exact
  exclusive `2^long-digits` boundary **before** conversion. Tests include valid
  fractional/boundary values and invalid/nonfinite values; the FGD `studio()`
  path preserves the raw original skin property. No sanitizer run is claimed.
- I/O-only FGD declarations do not fabricate ordinary property rows/completions;
  real native keys and an ordinary keyvalue sharing an I/O name remain editable.
- Native legacy/modern parenthesized cordons and bare compatibility tuples are
  interpreted; camera square syntax stays distinct. Delimiter/component/extent
  negatives refuse interpretation without repairing or changing the document.
- Capability locations agree across LF, CRLF and lone CR. Invalid preflight
  refuses before compiler execution: the real `calls.txt` helper marker and
  production command output are absent, paired with a positive helper that acts.
- Complete `maps/<filename>` paths are validated in package planning. Pure policy
  tests accept 240 bytes and reject 241 before filesystem access. A physical
  235-character-filename ZIP round trip is **not** claimed: an initial authored
  fixture attempt encountered the host Windows MAX_PATH limit. The separate
  short-name BSP ZIP round trip is the actual writer/extraction control.
- Fresh independent review covered native core/I-O/models, packaging policy and
  metadata/preflight/build integration. All accepted findings were fixed and
  revalidated; a fresh bounded closure review found no remaining blocker. Parent
  pre/post SHA-256 attestation proved the exact reviewed code stayed unchanged.

The preference replacement failure predates this follow-up; implementation and
fixture code remain untouched. It recurred in current full 42/43 and isolated runs;
earlier passes did not fix it. A watcher/sharing or host cause is not conclusively
diagnosed. It remains recorded, not hidden by weakening a test or rerun campaign.
See [BACKLOG.md](../../BACKLOG.md).

## Retained A4 evidence and manual gates

The earlier A4 committed candidate passed all **231** Fs20/Gl21/Mdl150/Ui40 CTest
entries. It demonstrated an actual native ramp edit, undo/redo, save/reopen,
classic Source and Strata compiler acceptance, and isolated FTESurf load of
control/edited BSP20 and edited BSP25, with owned processes exited and original
inputs unchanged. It did not measure numerical collision/prediction equivalence.
These are historical A4 results, not a claim that those exact totals were rerun
for the current A5 candidate.

Independent Hammer++ reopen and owner ramp join/collision/editor feel remain
**pending**, as do the new A5 output/property/prop/tool observations in
[lextest.md](../../lextest.md). Automated screenshots cannot fill those boxes.
Compiler acceptance and a visible mesh do not prove runtime I/O or collision.

Package-local `SOURCE-RECEIPT.json` records the exact source commit, file hashes
and actual installed-executable smoke for that package. Do not infer an installed
pass from a build-tree executable or combine receipts/DLLs across versions. No
Valve content, Source compilers, external maps, player data or private raw logs
belong in the editor package.

## Evidence handling and limits

- Original maps/assets/configs are read-only inputs; mutation/compile/GUI tests
  use task-owned authored fixtures/copies. Failed pipelines never promote output.
- The optional real-compiler test requires explicit owned VMF/staging environment
  and does not launch a runtime. Normal synthetic build tests run independently.
- Initial wrapper/path/link failures and a host commit-exhaustion VRAD failure
  were preserved separately, not counted as feature progress or successful BSPs.
- Distribution ZIP discovery remains deliberately incomplete; scripts/runtime
  lookups/branch-specific dependencies require manual review. Stock exclusion is
  not proof of redistribution rights.
- No whole-branch, large-map, broad performance, all-shader or all-model claim is
  made. Remaining measured findings are in [BACKLOG.md](../../BACKLOG.md);
  unbuilt advanced features remain on the [roadmap](ROADMAP.md).
