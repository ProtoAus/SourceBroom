# Native Source authoring tools — bounded alpha candidates

SourceBroom edits native Source 1 VMF, not a converted Quake MAP. CSS and
Momentum/Strata are the first tested profiles; generic Source paths and external
tools remain configurable. This is an experimental editor candidate, not full
Source branch, Hammer++, shader or performance compatibility.

## Connected displacement position-seam sewing — A5.36

Select2..64 connected same-power native displacement faces, then **Sew selected
displacement position seams**. Corresponding full base edges match in either
orientation; each edge can have one selected partner. All boundary equivalence
groups solve together from ORIGINAL world positions. Three/four-way corners get
one common mean, not sequential pair bias. Two-face behavior stays intact.

Only needed XYZ offsets change. Explicit Sew may create absent zero rows, but a
member already at the target needs no new table. All changed copies rebuild and
verify before the whole set commits. It is one non-collating history command;
Undo/Redo and preserving Save As/reopen apply to the whole batch, while already-
close seams add no history. Existing stale-press/read-only/backend gates remain.
Distance/alpha/directions, base UV, triangles, native IDs and unknown data stay.

Disconnected/vertex-only/ambiguous/overlapping/non-manifold sets, mixed powers,
partial/aliased data and more than64 meshes refuse. This is POSITION sewing, not
normal welding, neighbor generation, mixed-power resampling or arbitrary compiler
validity. The64 cap bounds this editor planner, not the native file format.

Inspector281 passes after one desired connected3 runtime failure with198 acting
controls. Focused model2888/13 and full Model322/App50/GL21/Render28 pass; current
UI63/64 only known untouched Unicode preference rename AccessDenied, not fixed/
skipped/retried. Seven builds0 compiler diagnostics/six fingerprints. Direct parent
review is not independent/clean-LSP/tidy/human/Hammer. A531 read/derived-build/
serializer boundary proof and corpus38/35same3/38 are retained, not rerun/re-dated.
See [pending small owner check](../../lextest.md); use whole A5.36 on copies and
keep whole A5.35/A5.33/A5.25. Complete Source support remains in progress.

## Retained static Source material RGB preview — A5.35

Known basic preview shaders now apply `$color` and `$color2` componentwise, rather
than dropping the authored RGB. Three finite normalized values, optionally `[ ... ]`,
or quoted byte-scale `{ ... }` triples are supported. Opacity/cutoff/alpha/editor
fade are separate. Common host resets ordinary/Q2/raw/null/disabled hints to white;
five fragment configurations, masked terrain and grid decision colors use the tint
before editor overlays. Raw image data, byte budget, geometry/layout/native writes
and content authority do not change.

Strict/default and unknown-shader guards stay intact. HDR, malformed/duplicate/
block colors, expressions/proxies and nonwhite per-pixel tint coupling are refused.
Scalar exactly`1` preserves the old neutral base-image omission, not general scalar
or native parity. Full regression caught and fixed that compatibility in production,
old test unchanged. This is not full gamma/branch/material-shader reproduction.

Same-test real desktop GPU3847 passes after59 desired partial-wiring RGB failures;
A5.35 full Model316/UI64/App50/GL21/Render28 pass, seven builds0 compiler
diagnostics/12 held fingerprints. GPU uses synthetic shared Vertex values through
actual production holder/VBO/Face, not an actual native cache instance. Native
source/corpus proof retained, not re-dated; no new owner/Hammer acceptance.
Keep whole A5.34/A5.33/A5.25 and see [pending check](../../lextest.md). Full Source
support remains in progress.

## Retained standard terrain blend-modulation mask preview — A5.34

Demanded brush WorldVertexTransition materials may additionally name one mounted
`$blendmodulatetexture` VTF. Its red channel is the transition half-width, green its
center; the clamped interval smoothly remaps stored vertex weight before mixing
both color layers. Mask B/alpha are not opacity. All three images use default UV0
and share the existing <=256MiB retained-image allowance. No catalog-wide decode,
vertex layout, geometry, weight/save mutation or new editing permission is added.

Missing/unsafe/repeated/block paths refuse. Only explicit TerrainBlendPreview
admits masks; default/Strict/BaseTexturePreview stay strict. Independent transforms,
proxies, masked blending and enabled self-illumination with a mask remain guarded.
Zero-width intervals use a stable hard edge, lower exactly at threshold; no native
DX/branch bit parity or full material shader reproduction is implied.

Same-test desktop GPU3587 passes after128 desired partial-wiring mask pixel failures.
Literal demand/budget/lifecycle controls and full Model315/UI64/App50/GL21/Render28
pass; seven actual builds have no compiler diagnostics on eleven fingerprints.
GPU values are synthetic shared-cache Vertex type through the REAL production
holder/VBO/Face stages, not an actual native mesh/cache instance. VMF source/corpus
proof stays retained, not rerun. Parent review is not independent/clean-LSP/tidy or
new human/Hammer acceptance. Whole A5.33/A5.32/A5.25 remains recovery; see the small
[pending owner check](../../lextest.md). Full Source support remains in progress.

## Retained displacement position-seam sewing — A5.33

Select exactly two compatible native displacement faces; **Sew selected displacement
position seam** averages their corresponding edge positions using XYZ offsets.
The pure planner requires same power, one unambiguous full shared base edge and
all base samples within existing1e-6 TransformEpsilon; reverse orientation works.
Self/overlapping ambiguity, vertex-only/disconnected/partial overlap, different
power or protected offset data refuse. Already-close position seams are no-ops.

Sew stages both meshes, uses overflow-safe scalar midpoints, explicitly initializes
absent zero rows only for a changed seam, rebuilds once per copy and checks native
representability before two nothrow assignments. Later bounds failure cannot edit
only one mesh. Distance/alpha, other positions, base UV, triangles, IDs and unknown
metadata stay independent. Derived normals/bounds/picking follow actual positions.

Production multi-face preflight/content swap provides one non-collating command,
Undo/Redo, Save As/reopen/repeat save. UI starts disabled, invalidates synchronously
on selection/backend changes before deferred refresh and rejects stale releases.
Read-only preview cannot obtain either UI or backend sewing authority.

Model177/2 and actual inspector170/1 pass, including acting movement/height controls,
rotated correspondence,65x65 maximum, rejection/atomic rollback, no-op/Undo/Redo,
Save As/reopen and preview. Full Model314/App50 pass. Earlier UI64 passed; latest
UI63/64 has only the untouched known preference rename failure, no skip/suppression
or fix. Zero compiler diagnostics on eight compiled fingerprints. Direct parent review is not independent/clean-LSP/
tidy, all-layout/human/Hammer. Exact reader/serializer bodies remain A5.31; retain
its corpus38/35+same3/38, no rerun/re-date. Current installed-loader proof is separate.

This is a POSITION seam, not smooth-normal welding, SDK neighbor data, general
sculpt/sew or power/topology changes, nor guaranteed compiler validity. Pending
[owner check](../../lextest.md); keep whole A5.32/A5.31/A5.25 recovery.

## Retained active3D camera draft capture — A5.32

The LIVE native camera panel can read this MapWindow's current perspective eye
and an absolute look target64 units along its heading into the editable draft.
Values convert to double before target addition. Explicit Add/Apply retains all
scoped/history/save write authority; capture does not create records, change active
choice, modify the map/redo or move the viewport. It cancels a previously pressed
write intent. Active2D/invalid/no-provider or guarded modes do not replace fields
or gain authoring. Projection/FOV/roll are not captured.

An owned provider passes through existing Inspector/MapInspector construction;
there is no global active-window lookup or native ID in draft values. Existing
constructor call forms remain valid. Saved Source Tools stays a saved snapshot.

Native-camera UI175/3, full UI64 and App50 pass; preference rename passed this run,
not fixed. Nine UI-only fingerprints/direct parent review, not human/Hammer or
all-layout acceptance. Model312 and corpus38/35+same3/38 are retained A5.31
unchanged-core boundaries, not fresh runs. See pending [owner check](../../lextest.md).

## Retained native displacement XYZ offset authoring — A5.31

Select one native displacement face, then choose its grid row/column in the face
inspector. **Offset X/Y/Z** are native world-axis vectors, independent of stored
normal/distance and alpha. Apply changes only the displayed vector. It rebuilds
positions/derived normals/bounds/picking but not base UV coordinates, grid power
or triangle layout. No-op/revert preserve existing row spellings and unknown data.

Complete bound rows permit surgery. A genuinely absent table permits explicit
**Initialize zero offset rows** only, then normal editing. Initialization is its
own history boundary and adds one bounded table (up to the existing65x65 maximum).
Present empty/partial/malformed or case-aliased tables never become missing-table
authority. Typed alpha authoring likewise now refuses case aliases rather than
flattening or guessing them; independent supported distance edits still act.

Production undo/redo, Save As/reopen/repeat save and proper admitted base movement
compose. Alpha+offset initialization emits one combined append per parent: the
existing structural emitter rejects overlapping inserts, so do not submit two
appends at the same native parent. Original bytes/IDs/unknown metadata remain
owned by the preserving document. No geometry-ID allocation or raw metadata API.

UI selection/backend notifications synchronously disable offset actions before
deferred widget refresh. Pressed generation/row/column/XYZ and button identity
freeze; stale or cross-button releases cannot redirect an old draft. Nonfinite,
out-of-world, wrong selection/plain face and read-only preview refuse atomically.

Offset Model449/3 and actual UI116/1 pass, including acting legacy controls. Full
Model312/App50 pass; UI63/64 retains the untouched known Windows preference rename
failure, with prior isolated evidence retained. Five actual targets build without
compiler diagnostics on nine fingerprints. Fresh corpus38/35+same3 reasons/38,
probes38/35/38 and original hashes pass. Direct parent review is not independent,
clean-LSP/tidy, human/Hammer, full branch or new GPU acceptance.

This is not sculpt/sew/create/re-power or normal/offset-normal authoring, and does
not guarantee arbitrary deformations compile or avoid self-intersection. Keep
whole A5.30/A5.29/A5.25 recovery; the small [owner check](../../lextest.md) is pending.

## Retained LIVE camera records and active choice — A5.30

The Map inspector's **LIVE native cameras** panel authors supported editable
VMF metadata, including UNSAVED changes. Eye/look are manual XYZ, look an absolute
target. Apply changes only edited pose fields; Add appends the displayed draft.
Delete preserves the identity of any retained active record and remaps its native
ordinal, explicitly clearing the active choice when deleting that record. The
separate active dropdown/Apply supports absent field, -1, or a retained record.
Removing the controller is explicit and undoable. Undo/redo, Save As, reopen and
repeat save use the production model, not a generic raw metadata editor.

Opaque scoped IDs/revisions, immutable expected snapshots, full preflight,
staged non-consuming allocation and controller-incarnation/replay checks protect
the operation. No loaded-record reordering, unknown-field guessing, stale/foreign
IDs, partial/nonfinite/degenerate poses, or excess1024 records. Unsupported,
repeated or malformed camera controllers stay protected while independent entity
edits remain possible. No-op/refusal/undo preserve BOM/comments/CRLF and tokens;
geometry IDs are independent and camera/cordon/generated-root edits compose.
Pressed intent freezes scope/row AND fields; refresh and cross-button releases
cannot redirect it. Preview/version/non-native guards still deny live authoring.

No viewport movement/capture or projection/FOV import is added. Saved Source Tools
remains the saved read-only snapshot, not the live camera source. Save As/reload
before using its existing eye/view navigation on newly saved camera data.

Model262/live UI136 and full Model309/App50 pass; regular UI63/64 has the untouched
known Windows QPreferenceStore Unicode rename failure, also isolated. No skip or
suppression. Five build targets have zero compiler warning/error lines. Fresh
corpus document38/strict35+same3 reasons/preview38 and edit-probe38/35/38 pass;
all38 original hashes unchanged. Seventeen fingerprints/direct parent review do
not establish independent/human/Hammer/full-branch or all-layout acceptance.
See pending small [owner check](../../lextest.md); keep whole A5.29/A5.25 recovery.

## Retained direct saved-source entry — A5.29

**Run -> Source Tools (Saved VMF)...** opens the saved native snapshot directly,
including on explicit read-only compatibility previews. It does not autosave
current drafts, edit metadata, create a builder or enable Compile/Launch. New
unsaved and non-native documents are refused by both menu and controller. Repeat
entry reuses the same saved path's dialog/review state; reload is explicit. The
older Compile-dialog tools entry remains for editable VMFs.

The package tab's local optional-BSP contract uses the existing Generic/unknown,
CSS20 and Strata25 validation policies. It initially reflects saved build settings
but changes are not persisted there. A target change invalidates reviewed-plan
approval; no selection disables discovery. Rights acknowledgement, bounded saved
reader, exact immutable inputs, incomplete-assessment warning and new-ZIP-only
writing remain. No branch certification or installed stock-asset resolver added.

Native Qt menu -> dialog -> root eye/view75 and Tools132 assertions pass. Regular
offscreen UI61/61 and actual product/UI builds pass with zero compiler warning/
error lines; eight code fingerprints/direct parent review are not independent,
human/Hammer or all-layout proof. App50 and Model299/Render28/GL21/GPU2693 are
retained boundaries, not new runs. The small direct-menu owner check remains
pending in [lextest](../../lextest.md). Keep whole A5.28/A5.25 recovery kits.

## Retained saved camera eye-and-look navigation — A5.28

In the saved native metadata tab, **Go to camera view** uses the selected camera's
safe eye and look direction. **Go to camera eye** remains separate and preserves
the current heading. Directions are computed in double before float conversion;
projection/FOV/zoom remain current. Existing unsafe/degenerate/missing/invalid
snapshot guards stay. No metadata, active-camera, history, geometry or file writes;
no new permission to compile a read-only preview or full Hammer state import.

Tools118, actual MapWindow26 and full UI60/App50 pass with no compiler warning/
error lines. A5.27 Model299/Render28/GL21/GPU2693 are retained unchanged boundaries,
not new runs. Parent review is not independent/Hammer or new human acceptance.
If a copied map has a saved camera, the small owner 3D-eye-versus-view check needs
no edit/save; see [lextest](../../lextest.md). Keep whole A5.27/A5.26/A5.25 recovery.

## Retained explicit translucent material opacity — A5.27

Basic LightmappedGeneric/UnlitGeneric/VertexLitGeneric previews admit finite VMT
`$alpha`0..1 only with explicit `$translucent 1` and no alpha test/terrain layers.
Patch inheritance and demanded brush/prop pixels carry it; raw image bytes,
budgets, catalog loading, geometry and native save permissions are unchanged.
The scalar multiplies texture opacity, not displacement blend weight or editor
fade. Zero is invisible, not the unsupported zero alpha-test-reference sentinel.

Strict mode and nontranslucent/cutout/WorldVertexTransition/invalid/repeated/block/
proxy combinations still refuse rather than guessing. This is not full shader
parity. Actual material136/brush-prop10224/shared-Vertex GPU2693 and full
Model299/UI59/App50/Render28/GL21 pass with no compiler warning/error lines.
Parent review is not independent/Hammer or new human acceptance. If a known
copied map uses such a translucent material, a small owner view check needs no
edit/save; see [lextest](../../lextest.md). Keep whole A5.26/A5.25 recovery kits.

## Retained explicit absent terrain alpha table — A5.26

On one supported displacement face whose native `alphas` table is entirely
absent, the face inspector offers **Initialize zero alpha rows**. This deliberate
command creates a complete zero table before ordinary **Apply alpha** edits a
sample. Height drafts are not applied. Initialization is its own undo step,
separate from neighboring height/alpha changes; undo removes the table exactly.
Save As a new copy and reopen retain the weights without moving terrain.

Present empty/partial/unknown tables are not repaired. Read-only, frame/cache/
opaque/reference, displacement clone and topology guards remain. This is not
painting, sculpting or full shader compatibility. Full Model298/UI59/App50/
Render28/GL21 and the actual seven-target build pass with no new compiler warnings.
New control/Hammer acceptance is pending in [lextest](../../lextest.md). Keep the
whole owner-confirmed A5.25 kit as recovery; use copies and avoid broken A5.24.

## Retained brush upload correction — A5.25

A5.24 is known broken: an owner load produced scattered geometry. The brush cache
had a36-byte vertex, while the renderer holder still copied/drew32-byte vertices.
A5.25 shares the cache type through upload/attribute setup and uses typed copying;
production byte/attribute checks141 and packed real-GPU checks2597 pass. This does
not alter native geometry/UVs, IDs, edit permissions or preserving saves. Use the
whole A5.25 kit with copied VMFs. The owner confirmed the requested same-map
geometry/edges view works, not other editing/maps/performance or Hammer behavior.
The earlier direct shader checks did not test this upload path.

## Retained basic terrain material blending — A5.24

For a basic **WorldVertexTransition** VMT, both mounted `$basetexture` and
`$basetexture2` VTFs are loaded on demand within one retained-image budget.
Displacement weights use a separate vertex attribute:0 selects the first image,
255 the second. Existing UVs are shared. The selected-face alpha Apply/undo/redo
updates the cached preview while positions, normals, bounds and picking stay
unchanged. Alpha for image opacity and editor fade is not a terrain weight.

Only the brush demand path opts into this mode. Strict/default material readers
and base-only readers do not silently accept the new shader. Missing/duplicate/
unsafe second paths, blend-modulation textures, independent transforms and proxies
were guarded at A5.24. A5.34 separately adds bounded standard masks above; independent
transforms/proxies remain guarded. Optional bump/detail/specular effects are omitted, not implemented.
Plain brushes have weight0; this adds no paint tools, alpha rows or geometry/save
permissions. Full Model296/GL21/UI59/App50/Render28 and real GPU2580 pass; seven
build targets have zero warnings/errors. Human appearance/Hammer comparison is
pending in [lextest](../../lextest.md). Keep the previous whole kit and use copies.
The old intermittent preference-file failure remains open despite this UI pass.

## Retained stored native displacement alpha — A5.23

Select exactly one supported terrain face. **Displacement vertex** in the face
inspector shows the native grid row/column, independent distance and **Alpha
(0..255)** values, and separate Apply buttons. Press **Apply alpha** to change
one stored weight, including fractional values; it does not move/rebuild terrain,
change picking/bounds or apply a drafted distance. Same parsed value is a no-op.

At A5.23 only complete original alpha tables were editable; missing/partial rows
stayed preserved and alpha-disabled without disabling valid distance editing.
A5.26 separately adds the explicit absent-table path above; present partial data
still cannot initialize.
Read-only preview remains non-authoring. Current-mesh index checks cover both
sample getters during queued selection changes. Native save/reopen and exact
undo/redo retain original bindings and all untouched number spelling/source.

This is stored blend-weight authoring, **not** full Source material blending
preview, paint/smoothing, alpha-row creation or topology/power/sew. Existing
geometry/cache/frame/opaque authorities are unchanged. Full Model294/294 and the
new inspector case pass; full UI58/59 preserves the known unchanged Windows
QPreferenceStore rename failure. Actual product/checker/test builds have0new
diagnostics. Owner control/weight/Hammer observation remains pending; use a copy
and keep the previous whole kit. See [lextest](../../lextest.md).

## Retained explicit native visgroup RGB — A5.22

Select a supported record in **Map → LIVE native manual visgroups**. The RGB
column, three channels and state explanation are current UNSAVED metadata. Press
**Set RGB on this visgroup only** to author its own native `color`. Name/reparent
never applies drafted channels; there is no propagation or member `editor.color`,
brush/material tint or saved-Hammer-checkbox change.

A supported scalar is exactly3 decimal integer channels0..255. Same parsed RGB is
a true no-op retaining original spelling. An absent field is **not stored**: white
is a draft, written only by explicit Set. Unknown/repeated scalar colors remain
preserved and disable/refuse only RGB authoring; ordinary rename/tree behavior
and general protected-record guards remain. Removal is not authored; undo can
restore an originally absent field exactly.

Known/absent/new/moved records use the existing scoped request/history and both
native emission paths, including local edits inside moved source subtrees. Use
Save As on a copy; compare actual record RGB in matching Hammer++. A5.22 Model292/
UI59 and its product/checker builds pass0new diagnostics; human acceptance is
pending in [lextest](../../lextest.md).

## LIVE native manual visgroups — retained A5.21 controls

In the **Map** inspector, scroll to **LIVE native manual visgroups**. Unlike the
saved-file dialog, this is current UNSAVED native state. Supported loaded/new
records can be created, renamed, reparented and deleted; moved records append at
the new parent, retaining other sibling order. Assign/unassign either the current
native selection or explicitly highlighted member rows. The latter can recover
hidden unassigned supported objects without allowing hidden geometry picking.

**Show members / Hide members** set manual intent over the supported current
record descendant/owning closure. Assignment never replays an earlier Hide. Own
manual/automatic flags, inherited/effective native causes, ordinary session state
and protected/unprojected reasons are displayed separately. Manual Show does not
clear wrapper, automatic, inherited-owner or session causes that it did not author.
Ordinary Show All likewise cannot override native-hidden selection/rendering.
Missing flags use the declared no-veto preview fallback; this is not a claim to
reconstruct a Hammer checkbox or automatic classification.

Native record/member handles and captured action scope are authoritative; names,
native numbers and row positions are not. Protected/ambiguous/opaque rows refuse
atomically while independent supported branches remain usable. Native no-op,
refusal and full undo preserve original bytes. Ordered Unassign/reassign persists
live reference order across save/reopen. Session-only hiding saves without
changing native flags. Actual Both clip undo/redo checks the expected relevant
metadata and Front operation authority before changing selection or the tree.

A5.22 adds only explicit own-record RGB above. Wrapper/automatic flag authoring,
automatic group predicates and arbitrary opaque/cached geometry remain outside
this boundary. Use Save As on a
copy and keep the previous whole kit. Model/UI/render/runtime checks are not
independent Hammer or human appearance acceptance; see [lextest](../../lextest.md).

## Read-only compatibility preview

Normal editable loading remains strict. A native load refusal can offer an
explicit **Open read-only preview** choice, with Cancel as the default. This
mode preserves duplicate IDs and the original document, but failed solids are
reported and **not projected**. It does not invent UVs, repair a hull, replace an
all-failed brush entity with a point, or claim that the displayed scene is complete.
Syntax, structural and resource bounds still apply.

The title/status/viewport persistently identify the mode and omission count.
Labelled orange points are only source-plane samples; unlocated omissions are
counted and at most 128 markers draw. Camera movement and selection/undo/redo
are allowed; native authoring, entity drops, normal save/overwrite, export and
compile/play are not. A5.29's direct saved Source Tools entry permits bounded
snapshot inspection/navigation and the independently guarded explicit package
workflow; it does not make the live preview editable or enable build/launch.
The Compile route remains for normally editable documents.

**Save unchanged native copy…** is a separate new-file operation. It copies the
loaded original bytes, refuses existing destinations or any native delta, and
does not rebind the preview to the copy. Repair in a matching source editor and
reopen through the normal loader before attempting edits.

## Basic native object authoring

New Source maps start from the profile's native VMF starter, not a Quake MAP.
On a new map or owned copy, normal tools can create closed plain brushes and
simple point entities, transform them, duplicate supported plain objects and
remove them. Brush-to-entity creation and whole/partial duplication of plain
brush entities preserve native solid ownership. Undo/redo and Save As remain
available. Opaque/geometry-dependent attachments or native references can make
an operation unsafe; refusal is intentional, not permission to strip that data.

New object and side IDs are allocated at admission, independently, above all
existing IDs in their namespaces, including unprojected groups. Saved IDs do not
change on each write and undo does not recycle an issued ID. Untouched native
text, repeated keys, unknown blocks and metadata remain authoritative. Duplicates
use new IDs, preserve supported template metadata and refuse unsafe opaque data.
Removing a last brush also checks its implicitly removed native owner.

This is not arbitrary topology support. Generic content edits keep the current
complete, unique side-identity set; unregistered/aliased topology and unsupported
clipboard/CSG operations refuse before mutation, with a preserving-save backstop.

A5.14 adds **Both-halves clipping** alongside retained Front/Back for visible
loaded/fresh/template/duplicated plain world and supported brush-entity solids.
Front keeps its original identity; Back is a new same-parent sibling with remapped
stable IDs. One staged batch admission/command preserves native side prototypes,
undo/redo, selection and native Save As/reopen. No-op/tangent subjects stay intact;
mixed selections retain them without extra identities. Undo-parked siblings are
not serialized. Actual preview cut material/UV parity now has direct controls for
both orientations and nearest-normal ties, repairing a reproduced mismatch.
Single-output narrow retry remains; Both/default failed redo/undo/transaction
policies do not gain certification. Retain whole A5.13 as unmerged recovery.
Other CSG/arbitrary topology, displacement and hidden/opaque/reference ambiguity
remain refused; no stripping or MAP conversion. Use a copy, Save As and reopen in
matching Hammer++; no owner/visual acceptance is implied by automated/static checks.

## Ordinary polygon-cache ownership — A5.20

Existing Duplicate/Delete actions now admit ordinary NON-displacement brushes with
validated point_data or vertices_plus, including supported whole/partial plain
brush entities and safe plain-brush additions to cached entities. Stable new
object/side IDs are allocated only after pure preflight; repeated save/reopen and
further movement/duplication/removal preserve those identities and native caches.
Original/current side ownership, unique canonical generated prototypes and the
complete owner/source closure are checked even when geometry is unchanged.

Malformed/mixed/misplaced caches, unknown siblings/ancestors and removal references
still refuse. Cached-member current/destination GROUP ancestry remains closed,
as do displacement ownership, reparenting, clipping and topology. This overrides
only the historical clone/delete limitation below; it does not make a known cache
universally safe. Both actual input variants passed 128 acted assertions and full
model260/App50 pass; owner/Hammer/compiler acceptance is separate and pending.
Use copies, Save As and keep the whole previous kit as recovery.

## Coupled terrain and native polygon motion — A5.19

Already-parsed displacement meshes can compose with separately validated indexed
point_data or implicit vertices_plus on the same brush. Use typed whole-brush
translation/proper rotation/positive UNIFORM scaling. Both caches and rendered
mesh/base/normals follow the existing exact shared similarity frame; later base/
mesh/plane/structure checks are unchanged. This is composing known allowances,
not admitting raw/unknown displacement data or extending transform inference.

A displacement's nonuniform/tiny-anisotropic scale, shear/reflection, stale cache,
one-sided mesh/base motion or inconsistent multiple mesh frames still refuses.
Cache incidence/winding, opacity/reference/root and other-operation guards remain.
Actual terrain subjects in both input variants passed move/save/history/reopened
continuation; owner visual/Hammer retry pending. Always test copies.

## Hammer++ polygons and observed preview settings — A5.18

`vertices_plus` contains repeated finite xyz `v` rows in physical polygon order.
`point_data` instead has explicit indices. Both use the same strict original-ring,
vertex-incidence and relative-winding correspondence; only their EXACT validated
blocks are admitted for supported content edits. Same-side mixed schemas refuse.
Changed coordinate spans update surgically; untouched row spacing and root text
remain raw. No other opaque operation becomes safe merely because a cache is known.

The separate global geometry guard admits only exact observed scalar shapes for
palette_plus, colorcorrection_plus, light_plus, postprocess_plus and EMPTY
bgimages_plus. Duplicate roots/fields, known-root scalars, unknown/nested fields or
slot aliases refuse. RGB is integer0..255, count-shaped values nonnegativeint32,
explicit toggles0/1, other numeric values finite; paired LUT names and texlight
must be EMPTY. Nonempty resources/backgrounds remain unsupported, not silently
resolved or dropped. These are bounded preserved editor settings, not arbitrary
future `*_plus` metadata or a Hammer/SDK certification.

Both real-map variants pass supported Map/save/history/further-edit paths. Test
copies; mirrors/topology/clone/remove/group/reparent/clip stay guarded. A5.19 composes
already-parsed displacement/base-cache motion only under the strict rules above. No universal map/operation editability claim.

## Native side point-data brushes — A5.17

Known Strata-style side `point_data` is indexed polygon geometry, not inert editor
metadata. Ordinary content edits now validate its unique direct block, integer
count, exactly-once indices, finite xyz and cyclic bijection against the original
face ring. Unique vertex incident-side identities map current geometry without
inventing a global transform. Whole-brush side/cardinality/incidence and relative
winding must remain unchanged, including uncached sides.

Accepted movement, proper rotation, positive scaling, material/UV and bounded
boundary edits patch changed coordinate tokens only; stationary face planes can
still need updated boundary rows. Physical repeated-row order, unchanged index/
coordinate spelling, comments and outside bytes survive. No-op and undo restore
exact source; reopened scenes must accept another actual supported edit.

Out-of-tolerance (existing1e-6), malformed, unknown or inconsistent caches refuse.
Mirror/topology/cardinality changes and point-data clone/remove/group/reparent/clip
are NOT enabled. A5.19 separately admits properly typed parsed displacement plus
point-data base motion under unchanged shared-frame and safety rules.
Hammer++ `vertices_plus` is a different schema, separately admitted in A5.18 with
the strict root limits above; equal native solid IDs alone never justify admission. No map or cache block is silently rewritten
or removed to make an edit pass. The actual reported original brush passes the
Map/save/history path; every other operation/map still requires its own evidence.

## Live native cordons — A5.16

The map inspector contains **Live native cordons**, reading current UNSAVED state
through the Map model rather than the saved metadata snapshot. Add a modern cordon
with an initial valid box, select records/boxes, edit modern name/bounds/optional
activation and Apply. New records are deliberately inactive with -64..64 bounds.
Not set is distinct from false on loaded optional flags. Add/remove boxes/records;
removing the last box requires removing its record, and the last record removes
its ordinary controller. Empty modern controllers can also be removed.

Loaded legacy singleton cordons support bounds, active and removal in place, not
rename/adds/implicit modern conversion. Fit copies current valid selected bounds,
leaving selection and soft bounds unchanged. Semantic no-op Apply changes neither
bytes nor history; accepted atomic changes support undo/redo and preserving Save As.
Unsaved UI refreshes through existing document notifications, including undo.
Scoped handles/revisions prevent stale row/document/delete/undo actions retargeting
another box. Invalid later fields refuse before partial name/flag publication.

Unknown outside controller data remains independent; malformed/duplicate/competing
controllers or opaque payload inside them refuse. Preview stays immutable. This
edits native metadata only: no viewport clipping/gizmo, forced external compiler
region, camera persistence or visgroup visibility authoring. Test copies in matching
Hammer++/SDK; those human interpretations and appearance checks remain pending.

## Ordinary native editor groups — A5.15

Use the existing **Group/Ungroup** actions on supported ordinary objects in an
editable VMF. Loaded, fresh and duplicated nested groups can contain world brushes
and whole ordinary entities; solids remain inside their native entity, not moved
lexically to a group. Open a group to edit members normally. Ungroup dissolves only
the immediate selected containers, retaining nested identities. Close the current
group before ungrouping it or its ancestor; predictable refusal preserves context.

Names are derived as `Group <id>`; native grouping has no name prompt and rename is
unavailable. Native group IDs are staged in the shared object namespace, separately
from internal scene bindings/link identities. Undo/redo restores exact live nodes,
sibling order, selection and IDs. Save/reopen retains membership/attributes with
deterministic lexical child order: native `groupid` does not store arbitrary
selection order. Grouping does not grant visgroup/layer/hidden/linked authoring.

Malformed/ambiguous or foreign membership components are preserved and refused,
not flattened or silently reassigned. Independent ordinary brush operations remain
possible beside an ambiguous component; actual opaque geometry references retain
the conservative global guard. Supported grouped create/transform/duplicate/delete,
plain entity ownership and Front/Back/Both routes remain available. Use copies,
Save As and matching Hammer++ for manual acceptance, still pending.

## Entity properties and ordered outputs

Select an **existing** native entity and use the Entity inspector. FGD metadata
provides typed property hints, choices and flag labels where definitions exist.
Unknown keys and unknown flag bits are retained. A definition default is a hint,
not permission to insert every missing property into the VMF. FGD input/output
declarations are not fabricated as ordinary keyvalue rows; a genuine stored
keyvalue with the same name is still retained.

For a single selected existing entity, its Source outputs table keeps the native
order and repeated event names. Edit target/input/parameter/delay/fire-count
fields, add or remove ordinary rows, then use normal undo/redo. Special targets
such as `!self` remain literal. Unrecognized or malformed original rows are
preserved rather than silently repaired; opaque connection children can make
editing unavailable. FGD hints do not prove that an entity/input/event exists in
all Source games. Native save guards remain authoritative.

## Material progress and base-image limits

For native Source maps, Material Collections settings group the configured root
and gameinfo-mounted content under the active **game/profile name**. This is a
browsing entry, not one entry per nested folder or a new physical mount manager.
All registered material names/paths remain available. Existing saved partial
folder choices are unchanged by opening settings; a partial mount can be listed
on both sides. Explicitly enabling the entry selects the full mount, disabling
selects none, and undo restores the previous subset. Read-only previews cannot
change these saved choices. Non-Source folder selection remains unchanged.

VMT opacity is separate from a VTF's alpha-channel flag, which may describe a
shader mask. The basic preview uses explicit `$translucent`/`$alphatest` hints;
opaque mask alpha is not blended as transparency. Raw cutout rejection happens
before surviving pixels become opaque, and editor whole-brush fading remains
independent. Normal graduated opacity and non-Source material behavior remain.
This is not arbitrary `$alphatestreference`, dual-mode blending, scalar `$alpha`
or full shader equivalence; basic cutout uses the existing 0.5 threshold.

Source catalogs remain complete and demand-loaded. Texture processing resumes
across frames rather than repeatedly servicing a busy prefix; unused catalog
resources compact in one pass. Browser resource notifications repaint without a
full relayout unless a tracked texture's dimensions changed. This avoids several
real stalls, but large first-time content indexing still takes seconds.

The base-image preview accepts additional enumerated optional roughness/AO,
tint/color and tree-sway parameters without resolving those extra textures or
reproducing their effects. Strict material inspection still rejects those
unsupported features. Unknown shaders/parameters, dynamic base selection and
unsupported proxy blocks are not blanket-accepted. A visible base image is not
Source lighting, blending, animation or full shader support.

A5.5 brush/prop image loading resolves a bounded Source **Patch VMT** through
configured mounted paths. Scalar insert adds/overwrites and replace changes only
existing keys; final supported-shader/base-image/alpha policy still applies.
Includes must stay in relative `materials/... .vmt` paths, with 1 MiB/file,
4 MiB combined text and 16-include limits. Resolution is on demand, without a global include scan/cache
or material-file writes. Nested/proxy, empty-value deletion, duplicate/overlapping
operations and unsafe/missing/cyclic includes refuse explicitly. This does not
add geometry patches, displacements or full shader/conditional semantics.

A5.6 also honors basic `$nocull` for demanded brush/prop base-image previews,
including resolved Patch flags. Only configured Default derives two-sided
culling from a tagged Source hint; explicit editor material overrides win.
Previous polygon enable/mode/front-face state is restored after drawing, nested
with existing blend/depth state. No catalog-time decoding, geometry/winding
change or original material rewrite is introduced. Absent/false flags retain
normal culling; invalid/duplicate flags refuse and strict inspection still
rejects unsupported features. Two-sided preview is not Source backface lighting.

A5.7 adds explicit authored `$alphatestreference` for existing alpha-tested
brush/prop previews, including Patch inheritance. One finite value in (0,1] is
supported; zero/-zero/invalid/duplicate/block values refuse. Without the parameter,
the prior0.5 preview fallback remains; game-wide default/sentinel parity is not
claimed. Merely specifying a reference does not enable alpha testing. Configured
alpha-function overrides win; raw cutoff precedes editor fade and `$nocull`
state still restores. Scalar$alpha/full opacity effects remain unsupported and
combined flags retain existing cutout precedence. Original assets are unchanged.

## Static prop preview

The bounded reader accepts supported MDL 44/48/49 layouts with matching
VVD/VTX sidecars, root/default pose and a limited static preview path. Installed
loose/VPK materials and VTF data are read through the configured Source content
paths; nothing from an installed game is bundled with this editor. Unsupported
versions/layouts or missing/mismatched/oversized data use a diagnostic fallback
instead of pretending the model rendered. Check the console.

A5.4 preserves the Source VTX clockwise order instead of reversing its faces.
Actual outward-normal checks on one real model and authored list/strip controls
cover this correction; native brush renderer winding and coordinates were left
unchanged. This is orientation evidence, not all-model visual acceptance.

This does not add skeletal animation, physics/collision authoring, full bodygroup
or shader equivalence, or instance/displacement/overlay authoring. A static pose or
visible mesh is not proof of the game's collision hull or runtime behavior.
Invalid/out-of-range skin/frame numbers fall back safely without rewriting the
original native property; this is not validation of every model's skin table.

## Read-only editor metadata and camera navigation

Open **Run -> Source Tools (Saved VMF)...** directly (including read-only preview),
or use the older editable-only Compile dialog's **Native metadata / Custom
package…** entry. These tools read a snapshot of the
**saved file**, not unsaved editor changes. Reload explicitly after saving again.

The metadata tab interprets groups, nested visgroups, object membership, hidden
containers/flags, camera eye/look data and cordon bounds/active state. Invalid,
duplicate, dangling or cyclic known references are reported; unknown data stays
opaque. The display is capped, with an explicit notice if not all rows fit.

These inspector fields remain **read-only**. A5.15 separately enables ordinary
native grouping through the normal map actions and guarded native bindings, not
by making this snapshot authoritative or treating group IDs as TrenchBroom parent
bindings. A5.16 separately enables ordinary live cordons and A5.21 enables the
bounded LIVE manual-visgroup interface in the map inspector. A5.30 separately
adds scoped LIVE camera record/active-choice authoring there; this saved inspector
remains read-only. It does not grant wrapper/automatic visibility authoring.
SourceBroom does not apply cordon clipping itself; native metadata
is passed to external tools whose actual region behavior must be tested.

**Go to camera eye** moves only the eye and preserves the current view direction.
**Go to camera view** additionally uses the saved look direction in the perspective
view. It normalizes the delta in double before converting to float, so nearby
large coordinates do not collapse. Existing world-up fallback handles vertical
views; orientation applies before the normal position tween. Neither imports
Hammer's whole camera state or edits camera/active-camera records; current
projection/FOV/zoom remain. Invalid/degenerate/missing/nonfinite cameras or eyes
outside the safe preview-navigation range are not targets. The range is a UI
safety policy, not a Source branch/world-bound assertion.

## Capability preflight before external compilers

Builds assess the **actual owned staged VMF bytes** before starting a compiler.
Native syntax/structure and read budgets can refuse a build. Known preview-only
or unassessed data is disclosed without stripping it from the staged document.
Warnings are not a claim that an external compiler or game supports the feature.
Generic targets never certify arbitrary Source branches.

The shared Source-tools reader bounds a saved document to 128 MiB and one million
entries while parsing, with the existing native nesting guard. Larger documents
are explicitly refused by this tool path, not partially compiled or silently
truncated; use an appropriate external toolchain. Concrete CSS20/Strata25 BSP
header contracts are checked after every successful compiler stage. Neither a
preflight pass nor a header match is whole-map/game acceptance.

## Explicit custom-content distribution ZIP

The package tab writes an ordinary **distribution ZIP**, not automatic BSP
embedding and not an automatic pack of installed assets.

1. Prepare a **dedicated custom-only root** using game-relative directories such
   as `materials/`, `models/` and `sound/`. Do not choose a Steam library or installed
   game. Only include content for which you have redistribution rights.
2. Optionally choose an existing compiled BSP read-only. It is placed at
   `maps/<safe-name>.bsp`; the exact included snapshot must meet its native header
   and selected CSS20/Strata25 contract. No original BSP is modified.
3. Acknowledge the custom-only content and rights requirement. This defaults to
   **off** and is not a licence verification service.
4. Choose **Discover and review exact plan**. Read every included file and every
   excluded/missing/unresolved/error finding. VMF material/model/sound references
   and a bounded set of VMT/model dependencies are discovered. Scripts, runtime
   lookups, some skin/branch-specific data and unknown dependencies need manual
   review. An apparently small plan does not mean nothing else is needed.
5. Separately approve that exact plan, then choose **Write NEW distribution ZIP…**
   with a new filename. Existing files are never overwritten. Changes to the
   root, BSP, rights acknowledgement or saved snapshot invalidate approval.

The reviewed plan holds immutable binary snapshots and CRCs. Writing packages
those reviewed bytes, not a later reread of potentially changed source files.
Traversal, linked/reparse paths, case collisions, unsafe archive names and budget
failures are refused. Rights, review and output checks are not bypass switches.
Installed game/VPK stock content is not automatically resolved or redistributed.

**Discovery is deliberately incomplete.** A successfully written ZIP is not proof
of licence rights, a self-contained map, valid gameplay or compatibility with
another branch. Review exclusions and runtime dependencies, then test the package
in the intended Source game with only its declared prerequisites installed.

## A5.10 native displacement movement, rotation and distance editing

The A5.10 candidate renders and picks admitted native power2..6 displacement
meshes rather than their flat base faces. In object mode, select the whole
brush and move or rotate it normally: the base and raised grid transform together.
UV-lock policy, undo/redo and preserving Save As retain the mesh; later distance
edits still work. Scaling, shear/flip and face/vertex topology remain refused.
Legacy untyped offset-normal rows can load/translate but refuse rotation. Numeric
overflow or precision-losing motion also refuses without changing the world.

Select exactly one displacement **face**
to show a **Displacement vertex** row/column/distance section in the face inspector.
Choose a grid row and column, enter a distance and click **Apply distance**. This
uses normal undo/redo and preserving Save As. The row/column order is the native
Source grid, not a screen-space brush. The distance tool is disabled when the
original normal/distance data is incomplete or cannot safely express a distance
change. A5.23 adds the separate stored-alpha controls described above; alpha-row
eligibility is independent, and neither tool silently authors missing rows.
A5.31 separately adds explicit single-vertex XYZ offset authoring and absent-table
initialization, not native normal or offset-normal authoring.

Use a copy until manual/Hammer acceptance is recorded. Unknown attachments and
references remain guarded; missing or malformed meshes are not silently replaced
by a flat editable proxy. This is not a sculpt brush, creation/power/sew tool,
blend-material preview or permission to transform arbitrary displacement bases.
A5.9 remains recovery with native movement/distance tools but without rotation.

A5.11 also permits **positive uniform scaling** of whole
native displacement brushes through the normal numeric and bounds-based scale
commands. Positions, offsets, elevation and distances follow the scale while
normal-like metadata retains magnitude. UV locks, undo/redo, native save/reopen
and continued distance edits have automated controls. Bounds-based authorization
is conservative: ratios must be exactly uniform; near-uniform arithmetic is not
permission for a nonuniform transform. Shear, reflection, topology changes and
unsafe numeric motion remain refused. Legacy untyped offset-normal rows also
refuse this new operation. A5.10 remains unmerged recovery; source and portable
checks do not fill native appearance/Hammer acceptance.

A5.11 also previews supported static Source model **bodygroups**.
An entity's bounded `body` value selects each decoded bodypart's model using its
actual base/count, independently of skin and animation-frame index. Source studio
FGDs default to the entity's body0; explicit definitions and other games are not
globally reinterpreted. Blank choices hide the relevant geometry. Bounds, picking,
renderer caches and browser centering follow the same selection without copying
all geometry for every combination. This remains static/root-pose preview, not
animation, body controllers, game collision or complete model/shader parity.
Source checks pass and candidate receipts identify exact portable artifacts;
no owner bodygroup/browser appearance acceptance is implied.

## Source model skins on fixed FGD paths

In a SourceVMF map, a simple fixed nonempty `studio` or `studioprop` model path
can now use the entity's `skin` property. With matching model/material assets and
multiple skin families, changing `0 -> 1 -> 0` selects the corresponding preview skin.
No MDL/VMT asset is edited. Other games, explicit skin/frame declarations and
legacy/dynamic/model()/sprite forms keep their prior semantics; this is not
animation, bodygroup or universal FGD/SDK parity.

## Still outside this candidate

Basic plain-object authoring and A5.21 manual-visgroup controls do not imply
arbitrary topology, automatic/wrapper visibility, complete per-object editor
metadata, layers, linked instances, complete displacement/overlay authoring,
full Source material rendering
or all-branch performance. Common unsupported operations are gated;
some upstream actions can remain visible, but unsupported content changes must
refuse before mutation and preserving-save remains the final backstop. Never ignore an error or assume an
unsuccessful save changed the original. Use **Save As** and inspect a copy in
Hammer++ before relying on it. Actual owner Hammer++/game observations belong in
`lextest.md`; automated checks do not fill those human gates.
