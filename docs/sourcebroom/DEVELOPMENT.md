# SourceBroom development

Native A5.2 authoring is exercised through the production model/command path,
not a MAP export. `VmfMap`/`VmfScene` cover fresh and native-template identities,
side-set/ownership admission, duplication/deletion, command history and preserving
save/reopen. The existing authored-room fixture may additionally opt into
`SOURCEBROOM_A52_AUTHORING=1` alongside its explicit `SOURCEBROOM_A4_INPUT` and
create-new `SOURCEBROOM_A4_OUTPUT`; it emits a production-saved room with a fresh
brush/point after duplicate/delete controls for external compiler checks.

A5.3 adds explicit immutable compatibility loading; strict admission remains the
default. Read-only commands/undo default-deny except selection-only operations,
native admissions reject before allocating identities, and serialization rejects
all native deltas. Normal save/export/build are not unchanged-copy operations.
The separately labelled copy uses create-new atomic native saving, does not
rebind history/path, and refuses an existing target. Scene omissions stay outside
the editable node hierarchy; diagnostics are samples, never proxy hulls.

Read [the ordered alpha roadmap](ROADMAP.md) and [alpha limits](ALPHA.md) first.
The existing CMake application target remains `TrenchBroom`; the Windows alpha
output, configuration namespace, and package identity are `SourceBroom`.
A normal upstream build is not evidence of VMF GUI support.

## Basic WorldVertexTransition preview — A5.24

TerrainBlendPreview is a separate opt-in material mode. Only brush loadTexture
uses it: default Strict and BaseTexturePreview still reject WorldVertexTransition.
The final Patch-resolved material requires one safe base path per layer, preserving
existing block/feature/alpha validation. Missing/repeated/unsafe second images,
blend modulation, independent transforms and proxies remain unsupported. Optional
bump/detail/specular parameters are omitted by preview, not rendered.

Both VTFs share the caller's retained-byte allowance, capped at256MiB; the second
receives only the bytes left after the first. Texture privately owns at most one
loaded, unnested child through upload/drop, exposes only const observation, and
keeps Source render hints. Ready requires both images; neither image is a new
independent catalog resource. Ordinary single-image behavior remains covered.

The real brush VertexSpec adds a fixed-function UV1 scalar: ordinary vertices0,
native displacement alphas/255. Existing node invalidation refreshes a scalar edit,
undo and redo even with identical geometry. Face-only SourceBlendMaterialRenderFunc
checks linked uniforms before GL state, preserves the old non-null lazy-demand
activation, binds the second sampler, and pairs material activation/restores the
caller active unit plus unit1 binding. UV1 cleanup restores client unit0. Face and
Patch configs share the real Face vertex/fragment stages; only Face's callback
sets the new uniforms, leaving the other shader callers unchanged.

The Face shader interpolates both layers with shared UV0 before the existing
cutoff/opacity/fade/shading/tint/fog pipeline; grid shade choice uses the same blend.
No model, lightmap, bump/detail/specular, blend-mask or full Source shader parity.
Native frame/cache/provenance/world/ownership and preserving-save guards do not
change. The inspector now labels basic WorldVertexTransition/no-paint scope.

Actual CPU parser22/demand25, child-lifecycle76 and native cache/history117 controls
pass. Actual compiled PARTIAL GPU wiring has loader/child/VBO/helper/tests present
but old Face host/stages:2545 controls pass before the missing new uniform produces
two nested contract failures (observed process42). This is not a stub/compile RED;
an earlier missing test include and an overly narrow expected-exit1 wrapper check
are preserved separately, not credited as product failures. After real host/stage
wiring, the same native GPU case passes2580 assertions, including real vertex-stage
pixels,0/.5/1 blend, raw child alpha17, separate fade, following plain material and
server/unit1/client cleanup. No inactive-uniform guard was weakened.

All20 code files use explicit-UTF8 changed-range formatting and direct full parent
review, not independent review. Actual seven-target build has no warning/error
lines. Current full Model296/GL21/UI59/App50/Render28 pass. The prior intermittent
Windows preference rename failure remains unfixed despite this UI pass. Existing
modernize hints are not a clean clang-tidy claim. Portable/corpus receipts are
separate execution-boundary evidence; human/branch/Hammer checks in ../../lextest.md
remain pending. This is progress toward Source support, not overall completion.

## Retained stored displacement alpha samples — A5.23

The existing bounded row reader now owns alpha entry/text/numeric-span bindings.
Complete original rows are required; absent/partial tables are not authored.
The scalar getter/setter enforce current grid coordinates and finite values in
0..255. Alpha changes update only the weight vector: no geometry/normal/bounds
rebuild. The selected-face Map route uses existing applyAndSwap/native preflight,
exact one-face selection and normal history; same value adds no command.

editsFrom replaces ONLY blanket alpha-value equality with immutable original
alpha-row provenance, size/range/complete-binding checks and the existing surgical
numeric-span emitter. Base/frame/corner/elevation/vector/cache/opaque/world and
operation authorities are unchanged. Foreign same-value row text still refuses.
Height and alpha eligibility are independent. Both UI sample getters use the
current selected-mesh index guard; the explicit Alpha Apply/status does not claim
material blending, paint/smoothing or missing-row/topology creation.

Actual compiled PARTIAL-wiring RED retains API/row binding/local setter/Map route,
with the existing emitter refusal and missing UI: Model180 assertions/176 controls
pass/4 desired failures, UI24/23/1. It is not an all-code-withheld or stub RED.
After wiring, focused2Model/1UI pass. Full Model294/294; full UI58/59 with ONLY the
known unchanged Windows QPreferenceStore Unicode-fixture rename failure. No
preference fix or all-UI-green claim. Four actual client/checker/test targets
build0new diagnostics; all9 source changed ranges use explicit UTF8 formatting.
Complete669-line parent review and source/executable fingerprints identify this
cut. Refreshed ignored clang DB uses889 actual TUs/30 Release CL logs; modernize
hints remain, not a clang-tidy clean claim. App50/Render28 are retained A5.21.

Installed corpus/portable receipts are a separate real-boundary check. Owner
weight/control/Hammer observation is pending in ../../lextest.md. Stored alpha
editing is not full material blending or complete A6/Source support.

## Existing plain native brush affine scope — test/docs clarification

Do not generalize the displacement/cache guards to plain brushes. Inspection of
BrushFace::transform741–813 found its explicit policy applies only with native
displacement data; ordinary faces already use affine plane/UV handling and mirror
winding correction. On unchanged A5.22 production code, an added VmfMap section
acts scaleSelection(2,1.5,.5), transformSelection shear and flipSelection(X), each
with UV lock off/on. Independent transformed-plane and locked-texel checks,
Save As/reopen/stable save, native bindings, exact undo/redo and unchanged input
all pass through existing helpers. Actual Model build0newwarnings/one VmfMap case
pass. No implementation, new alpha/product patch, geometry guard or independent
Hammer claim; prior global wording was overbroad. Keep all cached/displacement/
opaque/prop restrictions. Human check remains pending in ../../lextest.md.

## Retained explicit own-record visgroup RGB — A5.22

NativeVisgroupRecord now owns optional parsed RGB plus a COLOR-ONLY refusal
reason. One explicit scoped loaded/draft target + three int channels extends the
existing request; the complete desired tree does not silently author color.
Exactly3 ASCII decimal components0..255 are parsed without token-sized allocation;
leading zeros/whitespace remain source text unless an actual value change is
requested. Digit accumulation is bounded at255 before the next multiply.

Unfamiliar/repeated scalar color preserves prior independent rename/tree/manual
behavior. Only RGB authoring refuses it; legacy malformed blocks/general record
protection stay unchanged. Existing complete-record equality covers expected
snapshots and VmfScene referenced-definition replay. No epoch/lifetime/retry
policy, visibility, member editor.color or geometry admission change.

Private original color entry provenance drives BOTH retained/global scalar edits
and moved-subtree-local emission. Absent fields append only on explicit intent;
new records without intent remain colorless. Same value is no-op, full undo exact,
and removal is intentionally not authored. UI RGB column/three bounded controls/
separate Set capture existing press-time intent; white for an absent field is a
labelled draft, not a stored/default claim. Name/reparent never applies it.

Actual compiled pre-wiring ModelRGB169 assertions/123controls/46desiredfails and
UI6/5/1 precede the implementation. New3Model/1UI cases then pass; A5.22 full
Model292/UI59 and actual4target client/checker/tests build pass0newMSVCdiagnostics.
Six-file full delta parent-reviewed; no independent RGB review claim. All6 files
match clang-format using explicit UTF8. A mixed Python cp1252 preview was corrected
in the preview only, not treated as source corruption. Existing tidy/self-test
limitations remain. A521 App50/Render28 are retained, not new RGB results.

Installed corpus/portable receipts are a separate dependency-boundary gate; owner
RGB/Hammer observation remains pending in ../../lextest.md. No material tint,
member-color propagation, automatic visibility or full Source claim.

## Retained live native manual visgroups — A5.21

NativeVisgroups owns separate scene-scoped record/member handles, request-local
creation keys, immutable contextual identity/admission and complete desired tree
state. Stage/emit uses a document-scoped session and the existing combined root
append. Retained siblings keep order; moved/new records append at the destination.
Ordered membership remove/re-add likewise persists the live append order while
untouched reference lexemes/unknown data survive. No-op/refusal/full undo retain
exact source. Do not substitute a name, native number, Node pointer or row index
for scoped authority; decimal source0002 and canonical2 are not a safe identity join.

Scene registration, parked activation, terminal removal/disposal and relevant
semantic replay use exact admitted bindings and pointer-free tombstones. Metadata
alone does not prove geometry eligibility. Map commands publish authored state,
native masks and selection together through existing history. Both clip captures
before/after relevant metadata expectations and validates Front operation admission
BEFORE any selection, Back removal or geometry mutation on replay. Failed undo/
redo policies are unchanged; no compound retry certificate was introduced.

Node's native wrapper/manual/automatic causes veto ordinary session visibility.
All typed selection/picking paths, aggregate closure and renderer showHidden paths
honor that veto. Session visibility is not authored VMF: serialization ignores its
state while operation=true clip/reparent geometry guards remain intact. Manual
Show/Hide acts on current supported descendant/owning closure; later assignment
never replays historical Hide. Missing flags use the declared no-veto preview
fallback, malformed/unknown data is protected rather than guessed.

The LIVE UNSAVED inspector uses one owning immutable binding frame and complete
press-time intent. Row/document/button mismatch refuses rather than retargeting.
Existing nonmodifying visibility notifications refresh ordinary session reasons.
Map observations/selection use the exact registered scoped member handle, with
active/projected checks; unproven rows retain unknown optional observations.
QScrollArea and bounded combo sizing keep the actual inspector reachable. The UI
test runner redirects BOTH Ini User/System QSettings paths before any QApplication
or settings construction; no owner/default registry is a fixture. Startup cannot
use fs::TestEnvironment before Catch result capture exists.

Before fixes, actual dispatch RED17/13controls/4fail, session-persistence ModelRED31/
22controls/9fail and scoped-ID join RED71/68controls/3fail were retained. Independent
whole-family review then found early-child reparent, stale Both replay and ordered
reference emission defects: compiled actual REDs9/7/2,64/46/18,10/8/2 respectively.
The surgical fixes passed focused re-review. Current full Model289/UI58/App50/
Render28 and BOTH real cache variants128 pass, actual product/checker0newwarnings,
corpus document38/strict35/partial38 and original hashes unchanged. Source-derived
review, runtime controls, corpus, package receipts and manual acceptance are separate.
Prior preference rename failures remain untouched despite a passing current UI;
clangd AST parsing is not a clean exit (feature self-tests still fail).

No automatic group predicates, RGB editing, wrapper/automatic flag authoring,
saved-Hammer checkbox truth, general Node lifetime/epoch framework, arbitrary
observer/OOM atomicity or widened cached/opaque geometry is promised. Human/Hammer
checks remain pending in ../../lextest.md; generic Source1 is not Source2 support.

## Retained validated ordinary polygon-cache ownership — A5.20

`polygonCacheEdits` has an explicit forced-unchanged validation mode; the default
content no-op fastpath remains, before any canonical Brush copy. Ownership uses
one pure complete-closure helper, exact direct-side block IDs and canonical
original/current source-side correspondence. Generated prototype normalization
must preserve a unique owned side bijection, not launder foreign/duplicate/stolen
bindings. Archived original children are checked as immutable source, never
against a later live registered-clip side state. Exact point_data/vertices_plus
keys outside actually validated direct-side blocks refuse even under editor data.
Default opaque/editor/parser policy and raw displacement ownership stay unchanged.

Allowances compose only existing duplication/removal/addition/emission and
NON-clipping generated-brush content/save checks, plus the existing entity-owner
publication scan. Complete source parents retain unknown siblings/ancestors and
references. Cached-member group current/target ancestry stays CLOSED; uncached
native groups and registered clipping retain prior behavior. Preflight runs before
identity allocation; invalid batches retain selection/history/count/bytes/next IDs.

Desired synthetic RED210/202 controls/eight ownership failures and actual RED49/
47 controls/two Ctrl-D/delete failures were compiled before guards. Parent review
required destination-group ANCESTORS, lazy unchanged normalization and separation
of raw-original/live side-state validation; all have bounded regression controls.
Final focus91/model260/BOTH actual128 and parent product/App50 pass with zero new
compiler warnings. Two actual variants resolve native identity rather than equal
entry offsets, then move/clone/move/reclone/delete/repeated save/reopen/further
ownership/history/exact restore. Corpus38/35/38 is load/metadata only. Human/Hammer
checks remain pending; no group/clip/reparent/topology/displacement ownership claim.

## Compose polygon cache with parsed displacement — A5.19

The entire production change moves existing before-face parsed displacementSides
collection ahead of polygonCacheEdits/preguard and passes that SAME set to opaque
instead of null. Later exact shared frame/proper similarity/base-mesh consistency/
every-plane/structure/editsFrom checks remain byte-for-byte unchanged. Unknown/raw
mesh still has no allowance. Topology path and other opaque/ref/root operation
admission unchanged; no SourceDisplacement API/header/policy/schema/tolerance change.

Desired positive BEFORE wiring: syntheticRED619/613controls/6preguard failures;
actual direct+MapRED13/11controls/2failures. Original/H++ terrain subjects must be
PRODUCTION-discovered parsed+cached brushes then SAME native-ID matched, not the
previous ordinary subject or same entry offset. Final BOTHactual61 assertions,
source restore/inputSHA unchanged; source retained by hash after test-only additions.

Final affected83/fullmodel252 after final source; actual compiler0/newwarnings0.
Synthetic separate/no-cache/both-cache translation/rotation/uniformscale verify
renderer positions/normals, exact frame, emitted normal/offset/offset_normal vectors,
raw translation preservation and ACTED reopened continuation. Existing exact shared
multi-mesh guard now runs with both caches. Negative tinyone-sidedpose/stalecache/
opaque/ref/malformedmesh/nonuniform/shear/reflection/unclassified operation and
invalid-batch Map state controls remain. Test-only compile/stalebinary/oracle errors
retained separately, not credited as runtime RED. Parent independent final3hash/
source review/product0/App50/corpus38/35/38 pass; human retry pending, not inferred.
Keep whole A5.18 recovery. Larger topology/visgroup/hidden gaps remain.

## Hammer++ polygons and strict inert roots — A5.18

polygonCacheEdits shares per-brush correspondence with thin row parsing: explicit
point_data indices versus implicit physical vertices_plus order. Raw xyz spans are
patched backwards, preserving unchanged lexemes/spacing. Opaque allowance now
identifies EXACT validated block IDs, not side IDs; duplicates/mixed same-side
caches refuse, so one schema cannot lend its side approval to another. No default
clone/remove/group/reparent/clip/ref/displacement guard change.

geometryProblem separately recognizes exact observed root schemas before isBlock,
catching malformed scalar known names and scalar+block duplicates. Exact named
fields reject color00/name00 aliases; singleton roots/fields and no unknown/nested
data. Numeric/RGB/count/toggle shapes are strict; LUT slots paired and strings
EMPTY ONLY, texlight EMPTY, bgimages EMPTY. bounced/supersample/cubemap admit
nonnegativeint32 count/level-shaped scalars without certifying boolean semantics.
No wildcard suffix, resource resolving, default insertion or metadata rewriting.

Desired H++ ACTED positive was compiled BEFORE wiring: RED32/31controls/1 side
opaque, then side-only ROOT-RED32/31controls/1 root failure, then both-positive.
Final original+H++ actual Map54 includes production native-ID lookup, save/reopen,
second ACTED reopened edit/undo/redo/exact restore and unchanged original hashes.
Public tests synthesize both cache schemas and root shapes; no private assets/paths/
IDs embedded. Earlier failed test compile followed by a stale-binary pass is not
RED. Final focus80/fullmodel249, actual parent app/checker/App build0/newwarnings0/
App50 and separate metadata/loadcorpus38/35/38 pass. Parent final read/hash review
found no focused blocker; owner/Hammer/compiler retry remains pending.

Known limitations remain same1e-6/incidence/winding, mixed caches, mirrors/topology/
other operations and nonempty/future root data. Coupled parsed displacement/base
motion is separately admitted in A5.19 above.
Keep whole A5.17 as recovery. Larger visgroup/hidden protocol is not implemented.

## Native side point_data — A5.17

The point_data branch of polygonCacheEdits in VmfScene.cpp is pure per-content staging. Default opaque remains strict;
only nodeEdits gets a validated-side allowance after count/index/finite/ring checks.
Its before brush is immutable original provenance at all current callers. Numeric
indices define polygon order, not repeated-row physical position. Count is bounded
by actual rows/ring before count-sized allocation. Existing displacement arithmetic
epsilon1e-6 matches authored xyz to original projected vertices; mismatches refuse.

Lazy per-brush ring/incidence maps are built ONCE, with finite double position keys,
unique whole-brush vertex identities, unchanged side/cardinality/incidence sets and
forward old/current ring correspondence. This preserves relative cache winding;
mirrors refuse rather than silently reindex. Every cached boundary is considered,
not just changed plane triples. Surgical backwards coordinate-span replacement
preserves unchanged index/coordinate lexemes and spacing inside changed rows.
Local edits are unpublished until complete validation; later refusal stays atomic.

Keep default clone/remove/group/reparent/clip/reference and parsed-displacement
guards. A5.17/18 conservatively refused coupled geometry at the cache preguard;
A5.19 passes the already-parsed side set there while retaining all later checks.
Vertices_plus is NOT point_data. Variant checks resolve native ID through production
bindings; entry offsets differ. Its ACTED negative control is not a positive pass.

Regression: synthetic predispatch RED62/57controls/5failures; final affected77,
fullmodel246, actual original+H++ Map42assertions, parent App50. Actual requested
brush moves/saves/reopens/undo/redoes; public fixture has a second reopened edit.
The scene oracle also performs a second ACTED transform: identical-brush requests
return before==after and cannot validate cached geometry. Parent review required
both this oracle and once-per-brush correspondence before final freeze.

Read-only real-map hidden opt-ins use SOURCEBROOM_NATIVE_EDIT_INPUT/ENTRY and
optional VARIANT. No map assets/private paths in public tests; all writes are
owned TestEnvironment outputs, original hashes unchanged. Earlier corpus probe-edit
is metadata-only, NOT evidence arbitrary brush edits work. Actual production app/
checker rebuild0/newwarnings0 and parent load/metadata corpus38/35/38 pass.
Standalone clangd --check lacked a VS compilation database; its fallback errors
are recorded, not represented as a clean LSP result. MSVC is the build gate.
Owner/Hammer/visual retry pending; keep whole A5.16 as recovery. No full Source claim.

## Live native cordons — A5.16

NativeCordons is VmfScene-owned authoritative state, not a mutable saved inspector.
Owning snapshots and typed full desired-state requests use strong scene-scoped
record/box handles, never VmfEntryId/row/name/native numeric identity. Pure prepare
validates all shapes, values, budgets and expected scope/revision before publish;
first-dispatch freshness is separate from semantic before/after undo/redo checks.
New accepted handles replay unchanged and are not recycled into unrelated records.
No-op/refused requests consume no native namespace or Map history/dirty count.

Immutable document/provenance drives surgical optional scalar/record/box patches;
unchanged spellings remain untouched, removed-parent children are suppressed and
new roots join existing native entity/group output in ONE AppendRoot. Repeated
save and undo after save don't rebase bindings or duplicate controllers. Legacy
singleton stays legacy; no implicit migration. Malformed/opaque inside refuses,
unknown outside remains independent. Preview/version and geometry guards remain.

Map_NativeCordons exposes owning live snapshots and Changed/Unchanged/Refused
requests; NativeCordonCommand uses existing modifying history/transaction behavior.
Existing document notifications refresh value-only changes while already dirty.
No node/selection/context/index/bounds/visibility mutation or retry exception.
NativeCordonEditor in MapInspector captures scope/revision/row identity at action
press; stale release refuses rather than reading another row/document. Only
explicitly changed text is parsed/encoded, with blocked refresh signals and
17-digit numeric display; absent flags/invalid native name bytes remain untouched.

The internal serializer is named emitEdits, NOT emit: Qt's macro collides when the
model header follows QWidget. This private three-site rename was verified against
frozen hashes and actual Qt callers. Retained MapWindow tests now use the existing
owned INI settings scope before all widgets; never run default owner settings as a
regression fixture or claim earlier unscoped passes establish profile isolation.

Runtime REDs for model/Map/UI retained positive controls and specific dispatch
failures. Full model243, post-rename focused24, scoped UI13 and parent App50 pass;
actual public callers rebuild0/zero new warnings, fresh review no findings. Local
state/name/text/batch boundaries checked; uint64 exhaustion not dynamically tested.
No viewport/region compiler guarantee, arbitrary observer/OOM atomicity or human
Hammer/visual acceptance. Further visgroup support needs a separate loaded
membership/hidden-cause protocol, not a fresh-only toggle.

## Ordinary native groups — A5.15

`Map_Groups` exposes pure `canGroupSelectedNodes`/`canUngroupSelectedNodes`; native
execution uses the dedicated compound `NativeGroupCommand`, not public add-then-
reparent transactions. Valid loaded groups project before the original snapshot.
Physical GroupNode ancestry is distinct from native lexical ownership: effective
parents skip only individually admitted group ancestors. Whole entities may be
members; GroupNodes are never placed inside an EntityNode.

VmfScene stages group IDs, bindings, membership deltas and limits as one closure.
Surgical editor/groupid/group-entry edits preserve repeated/unknown raw data.
Loaded/generated duplicate closures remap native identities atomically; internal
link/persistent identities are separate. Exact live sibling/order/selection restore
uses narrow indexed Node insertion with existing hooks/counts, not unrelated suffix
detachment. Reopen ordering is deterministic lexical, not a new VMF order field.

Command placement finishes before added notifications for nonoverlapping live roots,
so descendants are not indexed twice in detached and attached containers. Registered
parked containers use `validateRegisteredDetachedGroup` plus local `locked()` guard;
Unlocked is not confused with Locked. Pure and execution Ungroup reject the current
opened group/selected ancestor rather than detach the editing context. General
history failure policies and retry certificates remain unchanged.

SourceEditorMetadata retains normalized candidate group IDs for ambiguous members,
without physically resolving them. Candidate components and malformed property
shapes are withheld before mutation; unrelated objects are not globally locked.
The private `membershipOnlyGroupRecord` recognizes inert known raw metadata for
geometry-reference reasoning independently of projected authoring authority; opaque
attachments and actual geometry-reference guards remain intact. The saved-file
inspector remains a read-only consumer, never the authoring owner.

Runtime REDs cover projection, ambiguous/malformed membership, notification/index
lifetime, parked replay/context and independent brush isolation; original evidence
is retained. Final full model219/App50/affectedUI5 pass with actual public callers
rebuilt and zero new compiler warnings. Fresh focused correction reviews found no
remaining blockers. No arbitrary observer/OOM strong atomicity, universal budget
exhaustion, native visibility/layer/linked semantics or manual Hammer acceptance.

## Retained native ordinary clipping — A5.12–A5.14

Single-output `mdl::clipBrushes` uses actual BrushFace/Brush clipping and one
in-place SwapNodeContents command. A5.14 `mdl::clipBrushesBoth` uses one native-only
ClipBrushesBothCommand and one staged VmfScene whole-batch admission, never public
duplication + two clip transactions or the MAP delete/add path. Front keeps the
original node/solid; Back is a same-parent sibling with fully remapped native IDs.
Loaded/fresh/template/duplicated plain world/entity brushes are supported. No-op
subjects validate and remain selected in mixed batches, all-no-op returns false
without history/count/identity consumption. Undo parks exact siblings; replay
validates live originals AND detached siblings without temporary attachment or
new IDs. Only detached siblings are uniquely owned; tree edits reconcile each
sibling's actual parentage on unwind. Predictable refusals are mutation-free;
arbitrary OOM/notification strong atomicity is not claimed.

Validate live parent/owner and visibility, not only parsed source ownership.
Displacements, hidden and opaque/reference ambiguity remain refused.
Generated admission must retain exact accepted states, not use guessed plane
matches or make generic swaps an arbitrary-topology route.

Explicit clip admission stages an entire batch's identities, counters/budgets and
exact allowed side sets before publication. Reproduce Brush::clip to validate
survivor provenance/data, never match floating planes or allocate during saving.
Unchanged surviving source side subtrees remain authoritative, removed sides are
surgical removals and each new cut side is explicitly authored. Retained admitted
sets support repeated clips and undo/redo; generic content swaps must keep their
current complete side set. Generated cut sides must belong to that solid; other
solids' or alias/zero bindings cannot become an arbitrary topology bypass.

The native model commit must copy the same nearest-normal face material/UV
attributes as ClipTool's preview, with first-match tie behavior. A parent acted
control clipped to changed x bounds but committed __TB_empty/default UVs instead
of the preview's authored material/.25 scale; both checks failed before the fix.
Extract an owned face from the moved result before copying attributes: lvalue
Result::value exposes a const reference. App tests inspecting actual face values
need BrushFace.h, not only its forward declaration. Compiler diagnostics caught
both seams despite clean recorded clangd diagnostics; do not call LSP build proof.

Tests exercise real Map/ClipTool actions and unchanged-refusal controls. The App
fixture must forward readOnlyVmfPreview and tests require actual preview state
before claiming a preview refusal. TestEnvironment needs the PRIVATE
TbFsTestUtilsLib dependency for both its header and out-of-line implementation.
Resolve native integration targets by their authoritative IDs: reference/instance
controls append siblings and invalidate a helper's two-display-child assumption.
Do not weaken the helper/production guard to hide that oracle error. Native clip
model4/App3 focuses, fullModel191/App49 and actual app/checker/UI rebuild pass;
known untouched preference failure and manual/Hammer checks remain separate.

A5.13 ownership/current-state controls extend that evidence to fullModel194/App49
and core/native8+AppClipTool3. Actual app/checker/UI callers rebuild0 without new
warnings. A real hidden-owner redo control first reproduced two lost-history
failures. Only registered native clip pre-swap validation refusal certifies retry;
reset certification before every performDo, including read-only early exits.
Initial/post-swap/linked-group failures and transactions never inherit permission.
Repairing the owner then retries the SAME parked command/cut bytes/IDs, not a new
clip. Generic failed redo/undo disposal remains unchanged.

Before DoFailed observers, restoring sole ownership to the redo vector is unsafe:
a valid observer can clear history or execute a new edit while other observers
still borrow Command&. Independent review identified this; safe alive/destructor
markers reproduced15 failures without reading freed memory. A scoped local
unique_ptr now defers destruction when any of the three redo-clearing paths removes
that protected command. Existing kdl::set_temp guards reset borrowed pointers
before local destruction, including exception unwind. Visible history clears
immediately and never resurrects; recursive redo during this scope returns false.
Normal/throwing clear, execute/store, later observers, exactly-one destruction and
throw-without-clear followed by successful redo all pass. Independent follow-up
static review found no remaining blockers in this lifetime scope. This is not
whole-Source, compiler, packaging or owner/Hammer acceptance.

## A5.14 Both and actual preview controls

Parent Both runtime RED12 preceded production edits. Component Both9/retained8,
ClipTool4/670assert, parent fullModel201/App50 and real app/checker/UI builds0/
zero new warnings pass. Independent focused static review found no blockers.
ID-length/aggregate ID-byte refusals act after earlier candidate staging with a
valid allocator control. Production million-identity/membership caps are checked
but not dynamically exhausted; no stronger numeric claim is made.

Actual native preview skipped nearest-normal attribute copying while committed
cuts used it. Direct cache-face data assertions reproduced12 material/scalar-UV
mismatches (plus inheritedBoth12); repair and first-nearest-tie controls pass.
Earlier post-commit checks/prose alone were not preview evidence. A tiny private
read-only friend exposes existing preview data to tests, not a runtime channel.
Actual installed-boundary provenance belongs to the candidate receipt; manual/
Hammer acceptance remains separate. No global history/failure policy changed.

## Build

Use the existing [BUILD.md](../../BUILD.md), CMake dependency declarations, and
Catch2 test infrastructure. Do not create a separate parser implementation for
tests or bypass the production library.

Example for Windows with a suitable Visual Studio toolchain, Qt 6.10 kit,
CMake, Git, and Pandoc already installed (substitute local paths):

```powershell
cmake -S . -B ../build -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.10.3/msvc2022_64 `
  -DCMAKE_CONFIGURATION_TYPES=Release
cmake --build ../build --config Release --target TrenchBroom SourceBroomVmfCheck TbFsLibTest TbGlLibTest TbMdlLibTest TbUiLibTest --parallel 1 -- /p:CL_MPCount=2
ctest --test-dir ../build/lib/TbMdlLib/test -C Release --output-on-failure
cmake --install ../build --config Release --prefix ../alpha
```

The pinned TinyXML2 dependency fails generation with multiple Visual Studio
configurations under CMake 4.3; selecting one configuration avoids that problem.
Upstream's Ninja/MSVC route is another supported option. The initial local
reconfigure also encountered an already-applied Assimp dependency patch. After
all pinned dependencies were fetched, `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`
allowed reuse without rerunning population/patching. **Do not enable that option
on a fresh tree with missing dependencies.** No dependency source was hand-edited.

For this Visual Studio example, request low project/compiler concurrency with
`--parallel 1 -- /p:CL_MPCount=2`. A shared-header rebuild on the 16GB host hit a
420s tool timeout with the old invocation; the same existing build completed
with these requested settings, reusing valid objects. Generated projects enable
multiprocess compilation, but this setup's CL-command tlogs retain no `/MP`
switches: no measured worker cap, memory-root-cause or speedup is claimed.
Preserve valid objects/logs; do not clean/copy the whole build or keep repeating
the timed-out invocation. Other generators have different native arguments;
this MSBuild option is not a Ninja option.

Use the build's actual executable location (multi-config builds add `Release`):

```powershell
../build/app/SourceBroomVmfCheck/Release/SourceBroomVmfCheck.exe --probe-edit C:/path/to/maps
```

The checker reads inputs only. It parses every document, verifies unchanged
serialization, reports structure counts, and optionally proves an in-memory
value-token edit took effect without touching any file. `--scene` additionally
checks the production scene bridge and reports explicitly refused geometry.
The default +/-131072 preview envelope matches the GUI. Use, for example,
`--scene --world-bounds 65536` for a smaller-envelope control; the finite positive
extent is bounded by 1048576 and requires a scene mode. It is not an engine limit.
`--read-only-preview` explicitly checks immutable partial projection and reports
`unprojected_solids` plus omission locations/diagnostics. It never promotes the
scene to editable, repairs IDs/UVs, or declares absent geometry present. Strict
`--scene` remains the compatibility control; a partial-preview pass is not a
strict-scene pass. The checker does not invoke compilers, write a VMF, or
decompile BSPs.

After public C++ class/layout changes, verify that affected translation units
actually recompile. One measured MSVC incremental-build failure retained an old
Source connection test object whose tracking log omitted its direct MapFixture,
Map and VmfScene headers, causing three ABI-related crashes. Retiring only that
verified disposable object rebuilt it, restored all three tracked dependencies,
and passed the full model suite. Preserve the failure and inspect tracking or use
a supported clean target rebuild; do not change tests to hide a stale object.

## A5.1 content/startup checks

Normal model tests cover initial loose-material discovery followed by explicit
reload after file addition and removal. Initial Source loading skips only the
redundant rescan of its newly constructed filesystem; explicit reload and other
map formats keep their prior behavior. No cross-document cache is introduced.
Game filesystem mounting supplies a synchronous constructor-only error reporter,
so an initial disk-index traversal failure still has an actionable diagnostic
without a second successful-path scan.

For a read-only installed-content measurement, set `SOURCEBROOM_PROFILE_GAME`
to a game directory containing `gameinfo.txt`, then invoke the hidden Catch2 case
`Source content catalog profiling opt-in` in `TbMdlLibTest.exe`. It reports mount,
reload, catalog and equivalent linear/hashed membership timings, checks identical
ordered selection/name signatures, proves catalog registration did not decode
pixels, then explicitly requests a texture to prove the loader acts. Keep game
files/logs private and report the workload; this is not an end-to-end benchmark
or a substitute for visual acceptance.

## A5.7 authored cutout-reference contracts

Preview `$alphatestreference` is now validated after direct/Patch resolution:
one finite float in (0,1], full-token locale-independent parse, surrounding
whitespace/optional plus and decimal/exponent syntax. Explicit zero/-zero,
nonfinite/out-of-range/malformed/duplicate/block values refuse even if alpha test
is disabled. Zero-as-default/literal behavior is not certified across branches.
Strict material inspection still rejects this optional feature.

Carry the authored float in demanded SourceEmbeddedDefaults for both brush and
prop paths; keep VMT cache identity, no metadata-time I/O and Q2/default tagging.
Only Binary alpha derives this threshold; explicit setAlphaFunc wins. A reference
alone must not invent cutout/blending. Existing shader cutoff precedes editor
fade, so no shader rewrite is needed. Preserve the absent0.5 preview fallback
and existing combined-flag cutout precedence; do not call this universal Source
default parity or add scalar$alpha based on an ambiguous renderer reference.

Semantic reference: public MIT noclip.website commit
`e47a8cc8a9fe16e83adc0bf19153ff04c666ceca`, SourceEngine/Materials/Material_Generic.ts
cutoff branch. Its default is0.7 and lightmapped scalar-alpha behavior has TODOs;
VDC requests returned access-challenge HTML, not documentation. No SDK/reference
implementation was copied. The bounded nonzero editor policy is explicit.

The public metadata changed: all6 app/checker/test targets must rebuild and
actual affected compilation must be checked, regardless of possible padding.
The production transport matrix is252 arms (prior84 x absent/.25/.75). Shared
five-config GPU controls retain1994 assertions and add545/50 scenarios, including
an acted negative: forcing .5 makes a pixel leak that authored .75 rejects.

## A5.6 two-sided material / polygon-state contracts

Preview-only `$nocull` uses the existing bounded boolean policy after direct or
Patch resolution. Both demanded brush and prop loads carry the Source-only hint
in tagged `SourceEmbeddedDefaults`; Q2/default metadata and VMT cache identity
remain separate. `Material::culling()` is configured policy; `effectiveCulling()`
peeks without requesting loading and derives None only with configured Default.
Explicit overrides win. Do not eagerly inspect the catalog or cache by base-image
identity: two VMTs using the same image may have different draw hints.

A real culling override saves `GL_POLYGON_BIT` before blend/depth attributes and
pops in reverse order. Never guess that prior culling was enabled or GL_BACK.
None/Front/Both are scoped; Default/Back keep existing no-op behavior. Readiness
still gates activation/deactivation. The EmbeddedDefaults variant/API changed;
build all six affected product/test targets and inspect actual dependencies,
not an assumed equal size. The existing opt-in five-config GPU case now includes
160 acted pixel/state culling scenarios alongside the retained alpha/fade arms.

New tests need explicit MockGl disable/blend/depth callbacks, and the base
SyntheticProp has only one skin. Reuse the existing aggregate-budget multi-skin
fixture for same-image metadata isolation rather than assuming skin(1) exists.
Neither fixture correction weakens a product or pixel assertion.

## A5.5 Patch material contracts / measured pitfalls

`loadSourceMaterial` resolves only demanded scalar Patch chains through the
existing mounted filesystem. Keep canonical relative materials VMT paths, cycle
tracking, 1 MiB/file, 4 MiB combined text and 16-include guards, plus final shader/
alpha validation. Byte limits guard explicit reads, not arbitrary `openFile` work.
No global scan/cache, host-relative lookup or guessed missing-file fallback.
Insert adds/overwrites; replace touches existing scalars only. Nested/proxy,
empty/deletion and duplicate/overlapping operations intentionally refuse.

Read exact owned bytes with `Reader::read`: `readString(size)` silently truncates
at NUL and bypasses the parser's explicit rejection. Root and include controls
must keep the valid counterpart. Do not replace the generic Reader contract.
Use a resolver-local ordered index of inherited key positions/occurrence counts;
count/find per operation is quadratic even under text limits. Stable numeric
indices survive vector inserts; the 40k+40k control checks acted override, not a
performance promise. A5.5 did not require a public Material/Texture layout change.

Tests must create fixture parent directories and compare exact before/after
bytes, not LF literals with Windows text-mode files. Capture subprocess output
as bytes/to-file: a default cp1252 reader lost UTF8 preference-failure output.
Preserve native CTest LastTest/LastTestsFailed evidence if capture fails; never
repeat passed suites or keep retrying the unrelated preference flake for green.

## A5.4 rendering / collection contracts

Source VTX indices are already clockwise for the renderer. The authored prop
fixture has explicit outward normals; do not silently change winding based on a
synthetic fixture with unspecified normals. The opt-in `Source installed model
winding opt-in` case accepts `SOURCEBROOM_WINDING_GAME` and
`SOURCEBROOM_WINDING_MODEL` for one bounded read-only installed model. It compares
actual submitted triangle crosses with VVD outward normals, including fixups.
Use exact binary `Reader::read`, not `readString` (which stops at NUL).

`sourceMaterial` separates basic VMT opacity from raw VTF alpha; `sourceBaseTexture`
retains its existing path-only/strict interface. Deferred brush loading and prop
cache hits retain VMT identity. `setAlphaFuncUniforms` sets Source-only opacity
suppression for non-graduated VMTs, case-insensitively, AFTER raw alpha rejection.
Do not disable Source blending globally: that breaks editor transparent-tag fade.

The opt-in `Source alpha preview GPU controls opt-in` UI case uses an owned
compatibility context, all five shared alpha-test program configurations and real material
activation. It checks pixels plus draw blend/depth state for opaque/cutout/real
opacity, editor fade and non-Source controls; forcing mask suppression off is an
acted negative arm. It does not need game assets or owner-window interaction.
ShaderManager passes `shader/name` into its locator, not a bare filename. New
uniform/shader changes must ship together; a compiler-only pass is not GPU proof.
A linked uniform is inactive when its shared function is never used: the first
A5.4 app paint exposed this in material-browser/UV fragments despite Face/Entity
pixel controls passing. Cover every `setAlphaFuncUniforms` caller (Face, Patch,
Entity Model/browser, material browser and UV view), consume the policy in each
fragment, and keep the active-uniform assertion. Do not make setters silently
ignore missing uniforms to conceal an integration error.

Source collection grouping is presentation-only over existing material paths.
Never migrate/expand a saved partial subset on dialog creation. Only explicit
root enable/disable changes the saved choices; normal undo must restore them.
Non-Source folder behavior and read-only-preview command safety remain controls.

## Common integer VTF conversions

The bounded loader also accepts ABGR8888, ARGB8888, BGRX8888, RGB565, BGR565,
BGRX5551, BGRA4444, BGRA5551, I8, IA88, A8 and RGB/BGR888 bluescreen layouts.
These convert to retained RGBA8 mip buffers; the original four direct byte
layouts and BC1/BC2/BC3 retain their existing upload representations. X bits are
not opacity; intensity and alpha-only semantics and exact bluescreen keying are
explicit. This is not HDR, palette, normal-map, animation, cubemap or volume
support, and it does not establish a whole-map texture-load speedup.

Precompute both encoded-chain and retained-chain byte counts before allocating.
The retained-mip budget must include conversion expansion (for example I8 to
RGBA8), not just source bytes. Strata AXC inflates into the encoded representation
first, then uses the same conversion. Never pass a larger converted layout as
zlib's expected size. `loadVtfTexture` tests include independently authored literal
pixel/mip and compressed-stream oracles, direct-layout controls, exact and one-byte
under-budget checks, truncation, and explicit unsupported formats.

## Native displacement mesh and distance controls

`SourceDisplacement` is the bounded scene representation of an admitted side's
`dispinfo`, not a conversion to a Quake patch. Power2..6 grids use the Source
start-corner/alternating triangle layout, authored direction/distance/offset and
elevation, and undeformed base positions for UVs. Derived normals, triangles and
bounds feed the normal brush cache and picking. A document-wide 1,048,576-vertex
budget precedes allocation; numeric rows reject excess values while reading.
Immutable shared mesh data keeps plain face size and ordinary copies bounded.

The selected-face distance command is a normal preflight/content-swap operation.
It changes only existing distance-number spans, supports undo/redo and preserving
Save As/reopen, and refuses invalid indices/nonfinite/out-of-world edits before
mutation. Sparse/missing/zero-direction authoring data does not enable this tool.
The inspector's queued selection refresh may lag a row-change signal: always
check indices against the **current selected mesh**, not the old widget range.
A5.23 adds the separate stored-alpha route above; both getters share this guard,
and missing alpha rows do not disable independently valid height editing.

A5.9 adds pure native base translation through the existing brush/Map command.
Retain startposition scalar spans and translate the start, corners, derived mesh
and bounds before setting new face data. Directions/distances/offsets/elevation/
alpha/tags remain unchanged. All base defining points must share the mesh delta
(absolute1e-6 for arithmetic); **every mesh** must also agree on exact zero/nonzero
movement. Comparing only the first delta lets a later1e-8 mesh-only edit pass an
epsilon check. The two-mesh validateContents/save control reproduced that failure
before the per-mesh intent guard, then passed with genuine whole-base movement.
UV locked/unlocked behavior, undo/redo and continued saves after movement act.

Only parser-admitted displacement sides relax the opaque-attachment check;
misplaced/nested unparsed blocks remain opaque. Unknown fields, native geometry
references, scale/shear/flip, duplication, deletion and topology remain guarded.
This is not bit-exact floating geometry across arbitrary edit histories.

A5.10 adds proper rigid rotation with an explicit command policy: only the existing
`rotateSelection` path opts in; generic/scale/shear/flip still default-deny native
rotation. Proper finite affine/isometry checks (1e-12 arithmetic) are not operation
permission. Repeat retains policy. Prepare native data before publishing faces.
Track original corner/normal provenance and cumulative frame; all displacement
sides must share EXACT relative frames, including nonzero motions. Check every
base defining point within1e-6 and distinguish exact identity from changed bases.
Avoid noisy M*inverse(M) when stored frames are equal.

Bind authored normals/offsets/offset_normals numeric spans. Rotate start/corners as
points; face normal/directions/offsets/offset normals as vectors. Scalar distances/
elevation/alpha/tags/topology remain unchanged. Missing rows keep default semantics;
malformed previously ignored offset_normals retains no-op/translation coverage but
refuses rotation. Surgical scalar output retains unchanged spelling/rows/bytes.

Rebuild sample/base positions, then require actual input-matrix covariance before
returning transformed data. Large cancelling elevation/distance operands can leave
finite in-world points but lose requested movement; refuse, don't bake offsets.
Independently check every rotated vector finite, including metadata unused by
sample rebuild. Matching infinities may compare equal in vm::is_equal: check
finiteness explicitly in provenance and numeric output too. Reviewer P1 overflow
reproduced through actual Map and Core before finite guard; red357/2 then green
364/2, including unchanged history/count/serialization and large finite control.
Final focus4009/41 and model184/184 pass; separate native visual/Hammer gates remain.
The native grid owns its wire edges; adjacent ordinary faces retain their own
base boundary for selected-edge highlighting. Tests cover actual cache vertices,
UVs and triangle indices, raised ray/rtree hits, bounds, one-token save/undo and
real inspector Apply/undo plus the large-grid-to-small-grid queued-refresh case.
This foundation is not sculpt/create/power/sew, alpha-material blending, overlays,
instances or complete Source rendering. Build/test is not visual owner acceptance.

## Native VMF transaction path budget

Transaction siblings use a short directory-local PID/steady-clock/atomic-sequence
leaf, rather than appending a nonce to the complete destination filename. A valid
248-character Windows target previously could not create its longer temporary
path; a binary-input control reproduced the refusal before the naming change.
The same regression now checks create-new saving, replacement, exact original
backup bytes and stale-original refusal. Exclusive creation, no-replace promotion,
link/overwrite/expected-byte checks and failed-promotion recovery remain unchanged.
This is not unlimited Windows long-path support: the parent directory and actual
destination must still be usable. No Windows path-prefix bypass is introduced.

## Source fixed-literal model skins

`FgdParser` and `loadEntityDefinitions` have a default-off Source studio-skin
option, enabled only by production `MapFormat::SourceVMF` loading and inherited
by includes. Simple nonempty `studio("models/x.mdl")` / `studioprop(...)` string
literals gain the entity `skin` lookup already used by empty studio declarations.
Inspect unoptimized syntax; restore the tokenizer snapshot for other forms.
Other games, `model()`, sprites, explicit map/skin/frame operands and legacy or
dynamic expressions retain their existing behavior. Do not globally reinterpret
all string-valued `ModelDefinition`s or override explicit authored choices.
This is a bounded Source preview capability, not universal FGD/SDK certification.

Controls attach parsed definitions to entities and edit0->1->0, including default-
off/numeric/include/scale cases. The authored two-family MDL fixture proves actual
red/green/red canonical `materials/...vmt` identities and pixel bytes through
FGD/entity/model loading; a non-null renderer alone is insufficient. `kdl::overload`
is a factory function, not a braced aggregate. The integration compile and initial
noncanonical-name oracle failures were retained and corrected before the pass.

## Static Source model body selection

Supported static MDL/VVD/VTX readers retain every bodypart/model, blank choice,
base and surface membership. Check every model's VVD byte base and VTX tables;
aggregate allocation budgets and sidecar/version/checksum/index guards remain.
`ModelSpecification::body` is an independent bounded numeric selection, never an
animation-frame overload. Source studio declarations read entity body0 by default;
explicit operands and other-game/legacy forms keep their authored semantics.
Selected surface geometry must agree across renderers, bounds, ray picking,
entity refresh and caches. Normalize selection by actual bases/model counts,
including noncanonical bases, without enumerating Cartesian combinations.

Lazy composite frames reference immutable meshes. Their renderers share a weakly
cached `VertexArray` holder: locks create fresh draw-state wrappers, and only live
renderers own GPU buffers. The original VBO manager/deferred destruction lifetime
is unchanged. Four body selections over four authored meshes previously allocated
eight VBOs (sixteen after another skin); an acted MockGl regression now requires
four, release with model data alive requires zero, and rebuilding body0 gives two
then zero. This fixture count is not a whole-map VRAM/performance claim.
Entity-browser layout must query `frame(spec.frameIndex, spec.body)`, matching its
renderer. Fully blank Source choices are valid empty renderers/point bounds;
ordinary empty-renderer error behavior is not globally changed. Root-pose only,
not animation/controllers, game collision, full FGD or shader/branch parity.

## A5.3 texture progress controls

Resource-manager tests prove progress past a busy prefix, cursor survival across
removal/appending, and retirement of an unused on-demand catalog without decoding.
The browser tracks weak texture resources and only invalidates layout when a
tracked size changes; loading/failure/upload/unrelated events still repaint.
Root-mounted VFS lookup keeps original suffix spelling/case and explicit reload
freshness. New optional VMT parameters are enumerated and preview-only.

The existing opt-in catalog profiling case also accepts
`SOURCEBROOM_PROFILE_MATERIAL` for one explicit registered material name. It still
requires one actual decode and unchanged full-catalog signatures, never an empty
or skipped catalog. `SOURCEBROOM_RESOURCE_GC_COUNT` selects the bounded count for
the existing unused-catalog graphics test; its default is small. Timing is
reported, not a portable pass/fail deadline. See [validation](VALIDATION.md) for
both gains and the lack of a whole-startup improvement claim.

## A5 tool checks

Normal `SourceBuild` controls always use an acted synthetic helper; they do not
skip when installed compilers are absent. The separate hidden Catch2 case
`SourceBuild installed toolchain opt-in` requires `SOURCEBROOM_REAL_VMF` (an
absolute owned saved fixture), `SOURCEBROOM_REAL_STAGING` (an absolute owned
staging root), and `SOURCEBROOM_REAL_TARGET` (`css` or `momentum`). Invoke that
exact case in `TbUiLibTest.exe`. It uses the production backend with **no play**,
checks all stages and verifies the original input remains unchanged. Installed
compiler outputs/assets are local evidence, never public fixtures.

The shared saved-tools/compiler reader enforces 128 MiB, one million entries
before insertion, and the native nesting guard. It explicitly refuses excess;
it does not partially parse/compile a document. Metadata remains read-only and
custom dependency discovery remains incomplete. See [SOURCE_TOOLS.md](SOURCE_TOOLS.md)
and [VALIDATION.md](VALIDATION.md) for current controls and the known untouched
Windows preference replacement test failure.

## A1 document architecture

The core lives in `lib/TbMdlLib`, not a second map converter. The diagnostic app
uses that same production code.

- Retain original bytes and token offsets, including comments, BOM, line endings,
  high-bit bytes, and opaque extension data. Do not infer an encoding or reformat.
- Store entries in source order with explicit parent/child IDs. Repeated property
  names remain separate entries.
- Treat values as raw lexical tokens at this stage. Semantic escape decoding,
  geometry parsing, and typed entity values belong to later layers.
- Surgical edits replace an existing property value with exactly one explicitly
  supplied, quoted VMF token. Quotes prevent lexical merging with adjacent tokens
  or comments. Unedited bare tokens remain unchanged. Validate the replacement
  before producing output. A raw token API is deliberately not a user-facing
  text-field API.
- Never retain string_views into a movable source string. Accessors derive views
  from numeric spans when requested; callers must not outlive the document.
- Reject malformed quoting/braces/comments, NUL/binary input, UTF-16, and excessive
  nesting with a source location. Unknown well-formed blocks remain opaque.
- A1 has no file-saving API. Safe Save As/overwrite/backups are an A4 requirement.

The scene bridge keeps the native document and ID mapping alive;
regenerating a VMF solely from the existing Quake scene would lose information.
An edit that changes topology must update known references and invalidate or
block incompatible opaque attachments. Blindly carrying stale metadata through
an arbitrary transform is not a preservation guarantee.

## Pre-registered A1 checks

| Control / subject | Prediction | Falsifier |
|---|---|---|
| Synthetic nested document | Enumerated world/solid/side/connection entries and duplicate order match authored expectations | Missing/mis-parented entries or collapsed duplicate outputs |
| Unedited bytes | Serialization equals input byte-for-byte, including BOM/CRLF/comments/unknown blocks | Any byte difference |
| One selected value-token edit | Exact expected bytes differ only at that token and reparsing exposes the new token | No-op edit, wrong occurrence, dropped bytes, or changed structure |
| Invalid edit | Bad ID, block target, duplicate edit, or extra syntax is rejected | Any output accepted for invalid edit |
| Malformed input | Bad strings/braces/comments/NUL/UTF-16/depth are rejected with location | Accepted malformed input, crash, or hang |
| External corpus | Every original parses; counts show parser activity; no-edit and mutation checks pass; input hashes unchanged | Parse failure, mismatch, invisible mutation, or changed original hash |

A failed corpus input is a compatibility finding, not permission to skip the file
or silently relax grammar. Determine the actual syntax before changing the parser.

## Contribution boundaries

Keep user/third-party VMFs, installed assets, proprietary SDK files, machine paths,
and local logs out of public commits. Add synthetic tests for newly learned
syntax. No promise of whole-VMF semantic validation or target compatibility is
made by the document parser. Record verified outcomes and remaining limits in the
roadmap; do not mark GUI/compiler stages complete based on parser tests.
