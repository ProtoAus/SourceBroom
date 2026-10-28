# Try Windows SourceBroom A5.38 — clip and mirror cached brushes

This is an experimental native-VMF editor candidate, not a public release or a
claim of complete A4/A5 acceptance. CSS and Momentum are the first tested games;
generic Source 1 content, FGD and external tool paths remain configurable.
FTESurf is an optional runtime, not an editor dependency.

## Start safely

1. Extract the **whole** package to a writable folder and run `SourceBroom.exe`.
   Keep the matching Qt DLLs, plugins, `games/` and other resources beside it;
   copying only the EXE is not an installed-package test.
2. Choose **Counter-Strike: Source**, **Momentum Mod**, or generic **Source 1**.
   Set its content path to the installed game directory containing `gameinfo.txt`
   (for example `cstrike` or `momentum`), not just its parent Steam directory.
3. Open a **copy** of an existing VMF, or the authored
   `games/Source-common/starter.vmf`. Use **Save As** to an owned new filename.
   No original user/corpus map is a test write target.
4. Move an existing plain ramp one small grid step, undo/redo, and Save As.
   Unsupported operations must refuse preserving-save with a diagnostic. Never
   treat an error as a successful save or overwrite the original to test it.

## A5.38: clip and mirror Strata and Hammer++ brushes

Maps saved by Strata Hammer or Hammer++ store a vertex cache on every face, and
until now that made **Clip** (front/back or Both) and **Flip** refuse on every
brush in them. Those tools now work: each changed face's `point_data` or
`vertices_plus` is rewritten from the face's new shape, wound and started the
way those editors write it, new cut faces get a cache of the same kind, and
untouched faces keep their exact bytes. Undo/Redo and repeated clips before a
save work as for plain brushes.

Still refused: brushes whose faces are only partly cached or mix both kinds,
displacements, hidden solids, and brushes an unknown entity may reference.
About 17% of the brushes in those maps (6,993 of 41,245) have cached vertices
0.0001 to over 1 unit from the vertices their planes imply. Those refuse
clipping, and in a real-map sample 63 of 85 also refused a plain move (22
accepted one, not yet explained). That is the next fix (BACKLOG).

The VMF tests (117 cases, 24132 assertions) and full
Model332/App50/GL21/Render28 pass; UI 63/64 fails only the known
untouched QPreferenceStore rename. Seven targets build with zero compiler
diagnostics. On 31 of the owner's 38 maps, 634 real brushes were
clipped, saved, reopened and edited again; every original is unchanged.

**Small owner check (pending):** on a COPY of one of your maps, clip an ordinary
brush (front/back, then Both on another), Undo/Redo, Save As a new file, then open
that copy in Strata Hammer or Hammer++ and check the clipped brushes look right.
Keep whole A5.37/A5.36/A5.25.

## Retained A5.37: sew terrain faces of different powers

**Sew selected displacement position seams** now accepts connected selections
that mix displacement powers 2–6. Each matching edge must still be a full shared
base edge with one selected partner. Samples both faces have meet at one average
of their original positions; samples only the finer face has along that edge are
placed on the straight segment between the coarser face's solved samples. Finer
detail along the seam is flattened on purpose so no gap remains. Everything else
from A5.36 holds: one Undo/Redo command, no history for an already-closed seam,
and refusal of ambiguous, disconnected, non-manifold, partial/aliased and
read-only selections. A face that touches another selected face without sharing a full edge with
it (a T-junction) is refused, because sewing would open a gap there. Powers, triangles, distances, alphas and
normals are not changed; this is not normal welding, neighbor generation or
Hammer's algorithm.

Focused core (21 cases, 35537 assertions) and the native inspector (410
assertions) pass; full Model330/UI64/App50/GL21/Render28 pass. Seven targets
build with zero compiler diagnostics.

**Small owner check (pending):** only if a copied map already has neighbouring
terrain faces of different power: select them, Sew, look along the seam for a
gap, then Undo. No hunt or original-file save. Keep whole A5.36/A5.35/A5.25.

## Retained A5.36: sew a connected terrain patch in one operation

Select a connected set of **2 to 64 same-power native displacement faces**, then
use **Sew selected displacement position seams** in the face inspector. Each
matching full base edge may have one selected partner. Three/four-way junctions
use one average of all original world positions, independent of selection order;
two-face behavior is retained. Disconnected, vertex-only, ambiguous/overlapping,
non-manifold, mixed-power, partial/aliased and read-only selections stay refused.

Only needed XYZ offsets change; explicit Sew may initialize absent offset rows.
A member already at the common position need not acquire a table. All changed
meshes rebuild and verify before any commit. It is one non-collating Undo/Redo
command; already-close seams do not add history. Source alpha/distance, base UVs,
triangles, native IDs and unknown records stay independent. This is **position**
sewing, not normal welding, neighbor generation, sculpting or mixed-power sewing.

Actual native inspector281 passes after the desired three-face eligibility red;
focused model2888/13 and full Model322/App50/GL21/Render28 pass. Current full
UI63/64 retains only the untouched known Unicode preference-rename failure,
not skipped/fixed/retried. Seven builds0 compiler diagnostics/six fingerprints.
Read/derived-build/serializer boundaries remain A5.31; corpus proof is retained,
not fresh. Installed-package checks and human/Hammer acceptance are separate.

**Small owner check (pending):** only if your copied map already has three/four
compatible terrain faces meeting at a corner, Sew then Undo. They should meet
and return. No hunt or original-file save needed. Optional Save As a new copy
and matching-Hammer comparison remain separate. Keep whole A5.35/A5.33/A5.25.

## Retained A5.35: preview static Source material RGB

Known basic preview shaders now retain literal `$color` / `$color2` factors rather
than omitting them. Three finite normalized components, optionally `[ ... ]`, or
byte-scale `{ ... }` components multiply together. It works across the five current
fragment configurations and after masked terrain mixing, before editor overlays.
Alpha/cutout/opacity/fade stay independent; raw pixels/layout/native writes unchanged.

This is not full shader/branch/gamma parity: normalized range only, no HDR,
expressions/proxies or per-pixel tint coupling. Nonwhite coupled tint stays refused.
Legacy scalar exactly `1` remains the existing neutral base-image omission; arbitrary
scalar colors/native scalar semantics are not certified. Default/strict and unknown
shader guards stay unchanged.

Actual same-test desktop GPU3847 passes after59 desired runtime RGB failures;
full Model316/UI64/App50/GL21/Render28 pass, seven builds0 compiler diagnostics on
12 held fingerprints. An old base-image scalar1 regression was caught and fixed in
production without rewriting its old test. Native/corpus source proof is retained,
not a fresh corpus claim. GPU vertices are synthetic shared-cache type through
real production holder/VBO/Face stages, not an actual native cache instance.

**Small owner check (pending):** if your usual copied map already has a known tinted
material, check that color only—no hunting/editing/saving needed. Matching-game/
Hammer/full shader comparison remains separate. Keep whole A5.34/A5.33/A5.25.

## Retained A5.34: preview standard terrain blend-modulation masks

Basic WorldVertexTransition previews now admit one mounted `$blendmodulatetexture`
VTF alongside the two base images. Red/green data shapes the transition using the
stored displacement weight and shared default UVs; mask blue/alpha is not opacity.
No vertex layout, geometry, stored weight or save permissions change. Three images
share the existing retained-image budget, with ordinary/no-mask paths retained.

This is bounded preview, not full Source shader parity: independent transforms,
proxies, masked-blending and enabled self-illumination with a mask remain refused.
A zero-width modulation interval uses a deterministic hard edge (lower at exact
threshold); no branch/DX bit equivalence at that boundary is claimed.

Same-test desktop GPU3587 passes after 128 desired partial-wiring pixel failures.
Full Model315/UI64/App50/GL21/Render28 pass; seven targets build with no compiler
diagnostics on eleven held fingerprints. The GPU fixture uses synthetic shared
Vertex values through the actual production holder/VBO/Face shaders, not an actual
native mesh/cache instance. Reader/serializer source is unchanged; old corpus
controls are retained, not new results. Packaging and owner acceptance are separate.

**Small owner check (pending):** if your usual copied map already has masked
terrain, check its transition pattern instead of editing/saving or hunting for a
special map. A matching-game/Hammer comparison remains separate. See
[lextest](../../lextest.md). Keep whole A5.33/A5.32/A5.25 recovery.

## Retained A5.33: join one compatible pair of displacement edges

On a COPY, select two native displacement faces with the SAME power and one full
shared BASE edge, then use **Sew selected displacement position seam** in the face
inspector. It averages corresponding edge positions using stored XYZ offsets.
If needed, Sew explicitly initializes genuinely absent zero-offset rows; existing
partial/aliased data is never repaired or overwritten. Already-close seams are
no-ops, including no new table/history. One Undo restores both meshes.

Only one full shared edge within the existing1e-6 base tolerance is admitted, with
reversed orientation supported. Disconnected, vertex-only, ambiguous overlapping,
different-power and read-only selections refuse. No topology/power/ID change,
normal welding, neighbor metadata generation, general sculpt or compiler guarantee.

Model177/2 and actual inspector170/1, full Model314/App50 pass. Earlier UI64
passed; latest full UI63/64 has only the known untouched preference rename failure,
no skip/suppression or fix. Zero compiler diagnostics on eight fingerprints. Read/serializer bodies are unchanged A5.31;
retain its corpus38/35+same3/38, not fresh results. Human/Hammer/full Source pending.

**Small owner check (pending):** on a copy, Sew two matching edges with differing
heights, then Undo. Positions should meet and return; do not expect smooth-normal
welding. See [lextest](../../lextest.md). Keep whole A5.32/A5.31/A5.25 recovery.

## Retained A5.32: read the current 3D view before explicitly writing a camera

On a COPY, activate a 3D viewport, then use the Map inspector's LIVE native cameras
**Read current 3D view into draft**. Eye/look fields fill from that window's current
perspective pose; look is an absolute target64 units along its heading. No record
or active choice changes until you explicitly Add/Apply. The view itself does not
move. A currently active2D pane is refused rather than guessing a different pane.
No projection/FOV/roll capture. Unsupported/opaque/read-only/non-native guards stay.

Native-camera UI175/3, full UI64/64 and App50 pass; the known intermittent preference
rename passed this run, not fixed. Model312 and corpus38/35+same3/38 are retained
unchanged A5.31 core boundaries, not rerun/re-dated. Nine UI-only fingerprints and
direct parent review are not human/Hammer/all-layout/full Source acceptance.

**Small owner check (pending):** aim a3D view, read its camera draft, then Apply
and Undo on a copied VMF. Fields should fill without moving the view or changing
the map until Apply. See [lextest](../../lextest.md). Keep whole A5.31/A5.30/A5.25.

## Retained A5.31: edit a native displacement vertex's XYZ offset

On a COPY with a native displacement, select its single base face and use the
face inspector's displacement row/column controls. **Offset X/Y/Z** are stored
vectors in native VMF world axes, not absolute positions or scalar distance.
**Apply XYZ offset** changes only that vector: drafted distance/alpha are not
applied. If no offset table exists, **Initialize zero offset rows** is an explicit
separate undo step and does not move the mesh. Existing empty/partial tables or
case-aliased names are preserved, not silently repaired or overwritten.

Undo/redo and Save As/reopen use the normal production model. Stale selection,
row or backend refresh cancels a pending offset action before queued UI refresh;
pressed field values and button identity are frozen. Alpha and offset initialization
compose into one preserving append per native parent. Case-aliased alpha names
also now deny typed authoring while independent supported distance edits work.

Offset model449 and actual inspector116 assertions pass, as do full Model312 and
App50. UI63/64 retains the known untouched Windows preference Unicode-rename
failure; no skip/suppression or64/64 claim. Five actual targets have zero compiler
diagnostics. Fresh corpus38/35+same3 reasons/38, edit probes38/35/38 and all original
hashes pass. These are not matching-Hammer, human, full-branch or new GPU results.

This is bounded single-vertex offset editing, not sculpt/sew/create/power changes,
native normal/offset-normal authoring, or a guarantee that arbitrary deformation
compiles or remains non-self-intersecting. Keep the whole A5.30/A5.29 and A5.25
recovery kits. Full Source support continues.

**Small owner check (pending):** on a copy, add 1 to one vertex's Offset Z, Apply,
then Undo. The vertex should move and return without changing distance/alpha.
Save As/reopen in matching Hammer is a separate optional check. See [lextest](../../lextest.md).

## Retained A5.30: edit live camera records, not just the saved snapshot

On a COPY of an editable native VMF, open the Map inspector and scroll beneath
map properties to **LIVE native cameras**. Select a record, edit eye/look XYZ
(look is an ABSOLUTE target), and Apply edited eye/look. Add uses the displayed
draft. Delete removes the selected camera; deleting the active one explicitly
clears the stored choice. Deleting an earlier record keeps the active record's
identity and remaps its native ordinal. The separate active dropdown supports
Not stored, No active camera (-1), or a record; its Apply button is explicit.
Remove camera controller removes its supported records/field, with undo.

These are production-model undo/redo and Save As edits to UNSAVED live metadata,
not raw entry-index writes. No-op/refusal/undo preserve original native bytes;
unknown/repeated/malformed camera controllers remain protected, not flattened.
Foreign/stale requests, bad poses, reordering, excessive records, read-only preview
and unsupported versions refuse before mutation. Existing geometry/native-ID
allocation, compiler/launch gates and saved Source Tools authority stay unchanged.

This does NOT capture the current view, move the viewport, import projection/FOV,
author arbitrary unknown fields or prove full Hammer state/branch parity. The
**Run -> Source Tools (Saved VMF)...** dialog still reads only the SAVED file;
after Save As, reopen/reload that snapshot to navigate newly saved cameras.

Model262/live UI136 assertions, full Model309/App50 and fresh corpus38/35+same3/38
pass. Full UI63/64 retains the untouched known Windows preference Unicode-rename
failure (also isolated); no skip/suppression or64/64 claim. All38 original hashes
stay unchanged; five actual build targets have zero compiler warning/error lines.
Seventeen fingerprints/direct parent review are not independent/Hammer or human
acceptance. Keep whole A5.29 and owner-confirmed A5.25 recovery.

**Small optional owner check (pending):** on a copy with a valid camera, change
one eye coordinate here, Apply, then Undo. The viewport should not move and the
record should return. Save As/reopen or matching-Hammer validation is a separate
optional check; never overwrite the original to test it. See [lextest](../../lextest.md).

## Retained A5.29: inspect saved native tools without opening Compile

Open a saved native VMF and choose **Run -> Source Tools (Saved VMF)...**. This
works in normal editing and an explicitly chosen read-only compatibility preview.
It reads the SAVED snapshot, not current unsaved drafts, and does not autosave,
create a compiler or enable Compile/Launch. Reopening reuses that saved path's
dialog; use its explicit reload when appropriate. New unsaved and non-native maps
have no saved-tools authority. Metadata and camera records remain read-only.

The custom-package tab exposes the optional BSP's **output contract (not VMF
version)**: Generic/unknown, CSS20 or Strata25. It initially uses saved build
preferences but a local change does not save them. Changing the contract clears
plan approval; an invalid selection disables discovery. Explicit custom-only
rights, immutable-plan review, bounded readers and new-ZIP-only writing remain.
Generic is not a game/branch certification or automatic installed-asset packing.

Actual builds have zero compiler warning/error lines. Tools132 and native Qt
menu -> dialog -> camera75 assertions pass, including readonly compile denial,
legacy eye/view behavior, dialog reuse and unchanged live/native/file state.
Regular offscreen UI61/61 passes. App50 and Model299/Render28/GL21/GPU2693 are
retained prior boundaries, not new A5.29 runs. Parent review is not independent,
new human/Hammer or all-layout acceptance. Keep whole A5.28/A5.25 recovery.

**Small optional owner check (pending):** on a copied saved VMF, open this direct
menu and confirm it is reachable without a save/compile step. If the map offers
an explicit read-only preview, the tools should still open while Compile/Launch
remain disabled. No edit/save or hunt for a map is needed. See [lextest](../../lextest.md).

## Retained A5.28: go to the saved camera view, not only its eye

On a saved native VMF, use the direct Source Tools menu above (the older Compile
route remains for editable VMFs), choose a valid camera and press **Go to camera
view**. It uses the saved eye and look direction; the old **Go to camera eye** still preserves your
current direction. These are saved-file snapshot tools: reload after saving.
No camera records, active-camera metadata, map history, geometry or file bytes
are written. Current projection/FOV/zoom remain; this is not camera authoring,
full Hammer state import or permission to compile a read-only preview.

Invalid, missing, degenerate/nonfinite or outside-preview-range eyes remain
non-targets under the existing policy. Double-before-float direction math handles
close targets near1,000,000 and tiny valid look vectors without collapse. Existing
world-up fallback handles vertical views; applying heading before the position
tween avoids interpolating opposite directions through zero.

Actual product/UI builds have no compiler warning/error lines. Tools118 and
actual MapWindow26 assertions pass; full UI60/App50 pass. Retained A5.27
Model299/Render28/GL21/GPU2693 are not newly run A5.28 claims. Complete direct
parent review is not independent/Hammer or new human acceptance. Keep whole
A5.27/A5.26 and owner-confirmed A5.25 recovery; avoid A5.24.

**Small optional owner check (pending):** if the copied map has a saved camera,
compare **Go to camera eye** and **Go to camera view** in3D. The latter should
also face its saved look target. No edit/save is needed; no need to hunt for a
case. Metadata/native-authoring gates remain; see [lextest](../../lextest.md).

## Retained A5.27: basic translucent VMT scalar opacity

Supported basic `LightmappedGeneric`, `UnlitGeneric` and `VertexLitGeneric`
previews now accept finite scalar `$alpha`0..1 when `$translucent 1` is explicit
and cutout is absent/disabled. Patch inheritance reaches demanded brush and prop
pixels. The scalar multiplies texture opacity, not terrain blend weight or an
editor fade;0 is invisible and1 preserves the old appearance. No image bytes,
retained-byte budgets, catalog loading policy or native geometry are changed.

This deliberately does NOT admit strict mode, missing/disabled translucency,
alpha-tested or WorldVertexTransition combinations, malformed/repeated/block
values, proxies or arbitrary shader effects. Those still report refusal. Full
Source shader/branch parity is not claimed. Alpha-test-reference0 is still a
separate unsupported sentinel, not authorized by accepting scalar opacity0.

Current full Model299/UI59/App50/Render28/GL21 and seven-target build pass with
no compiler warning/error lines. Native desktop OpenGL packed production VBO
pixels2693 pass, alongside material136 and brush/prop metadata10224 controls.
Parent review is not independent/Hammer or new human acceptance. Keep the whole
A5.26 kit and owner-confirmed A5.25 as recovery; avoid A5.24.

**Small optional owner view check (pending):** if a known copied map has a basic
translucent `$alpha` material, open it in the whole A5.27 kit and report whether
the surface/prop looks sensible rather than falling back. No edit/save is needed;
do not strip unsupported parameters just to force a preview. See [lextest](../../lextest.md).

## Retained A5.26: initialize an absent displacement alpha table

On a COPY, select one supported displacement face. If the native `alphas` table
is entirely absent, **Displacement vertex** offers **Initialize zero alpha rows**.
Press it deliberately to create a complete zero-weight table; normal **Apply
alpha** then edits individual samples. Height drafts are not applied by either
alpha action. Initialization is a separate undo step even with normal command
collation, and undo removes the newly created table exactly.

Present empty, partial, unknown or malformed tables are not repaired or replaced.
Existing read-only, frame, geometry/cache, opaque/reference and topology guards
remain. Geometry, picking and supported translation are unchanged; displacement
cloning is still refused. This is not painting, sculpting or Source shader
parity. Current full Model298/UI59/App50/Render28/GL21 and seven-target build pass
with no new compiler warnings. Parent review is not independent/Hammer or human
acceptance. Keep the whole owner-confirmed A5.25 kit as recovery.

**Small optional owner check (pending):** on a copied face offering the button,
initialize, set one sample to128, undo alpha then initialization, and confirm the
controls make those separate steps clear. Save As a NEW file only. No owner
observation of this new control has been received; see [lextest](../../lextest.md).

## Retained A5.25: restore correct brush positions

**Do not use A5.24:** the owner observed scattered vertices and an unusable large
VMF view. Its cached vertices were36 bytes but the renderer's holder/copy/stride
still used32. A5.25 shares the complete cache type and replaces the untyped copy
with typed copying. Native geometry, preservation and save guards do not change.
The actual copied input loads strictly and both original/copy hashes stay unchanged.

Production upload checks141 and the real packed-VBO GPU case2597 pass; earlier
shader/draw-count tests did not cover this boundary. The owner confirmed the
requested A5.25 same-map geometry/edges view works ("yep, works"). Saving, terrain
alpha, performance, other maps and Hammer behavior were not separately confirmed.
This is A5.25 view acceptance, not human acceptance of new A5.26 controls.

## Retained A5.24 feature: preview basic two-texture terrain blending

Basic **WorldVertexTransition** VMTs now load both mounted VTF layers. On a
supported displacement, stored alpha0 shows the first layer, alpha255 the second,
and intermediate weights interpolate between them. Use the existing selected-face
**Displacement vertex → Alpha (0..255) → Apply alpha** controls. One sample affects
its neighboring triangles, not every vertex on the face. Undo/redo restores the
preview without changing height or native geometry. Image alpha/opacity and editor
fade remain separate from terrain weight.

This is a shared-UV linear preview, not a Source shader replacement. Blend-mask
textures, independent texture transforms, proxies and ambiguous/missing second
layers still refuse; optional bump/detail/specular effects are not rendered.
No painting/smoothing, alpha-row creation, topology/power/sew or geometry/save
permission is added. Other Source games remain configurable; CSS/Momentum are
first tested profiles, not hard-coded exclusive targets.

Current full **Model296 / GL21 / UI59 / App50 / Render28** pass, and the real GPU
case passes **2580 assertions**, including previous alpha/culling controls, both
layers, fade separation and GL-state cleanup. The seven-target build has zero
warnings/errors. A current UI pass does not prove the prior intermittent Windows
preference failure fixed. Automated/parent review is not human/Hammer acceptance.

**Quick owner check (pending):** in the whole A5.24 kit, open a COPY with a basic
WorldVertexTransition displacement. Pick one native sample, try0 then255, undo/redo
and confirm the local texture blend changes without moving terrain. Save As a
NEW VMF and reopen. Compare its weights in matching Hammer++ if convenient. Keep
the whole A5.23 kit as unmerged recovery; never overwrite an original to test it.

## Retained A5.23: edit stored displacement alpha weights

Select exactly one supported native terrain **face**. In the face inspector's
**Displacement vertex** section, choose its native grid row/column, enter an
**Alpha (0..255)** value and press **Apply alpha**. Fractional values are allowed.
This changes the stored blend weight, not the terrain's height, normals, bounds
or picking. Distance has its own separate Apply action.

Only complete existing alpha rows are editable. Missing/partial rows remain
preserved and disable alpha authoring without disabling otherwise valid distance
editing. Same value is a no-op; invalid indices/range refuse. Normal undo/redo,
Save As and reopen preserve native identity and untouched numeric spelling.
This is **not** paint/smoothing, missing-row creation, topology/power/sew or full
Source material blending preview. Existing terrain/cache/frame guards remain.

Current Model **294/294** and the new inspector case pass. Full UI is **58/59**:
the known unchanged Windows QPreferenceStore Unicode-fixture rename failure is
preserved, not repaired or counted as a pass. Actual client/checker/test builds
have zero new compiler diagnostics. App50/Render28 remain retained A5.21 evidence,
not new alpha results; parent review is not independent/Hammer acceptance.

**Quick owner check (pending):** use the whole A5.23 kit and a copied VMF with a
supported displacement. Change one stored weight, undo/redo, Save As a new VMF
and reopen; compare that terrain alpha in matching Hammer++ if available. Check
the controls are usable without expecting shader/blend-paint parity. Keep the
whole A5.22 kit as unmerged recovery and never overwrite an original to test it.

## Retained A5.22: explicitly edit a native visgroup's RGB

In **Map → LIVE native manual visgroups**, select a supported record, edit its
three bounded RGB channels, then press **Set RGB on this visgroup only**. The RGB
column and controls show current UNSAVED state. Name/reparent controls do not
silently apply drafted RGB. An absent field is labelled **not stored**; the white
controls are only a draft until explicit Set. This edits the record's native
`color`, not member `editor.color`, material tint or brush rendering, and does not
propagate to descendants or members.

Supported scalar colors are three decimal integers in0..255. Same parsed RGB is
a true no-op, preserving original spelling. Unsupported or repeated scalar color
is preserved and refuses ONLY RGB authoring; independent ordinary rename/tree
operations keep their previous behavior. General protected-record guards still
apply. Save/reopen, new/moved subtrees, undo/redo and press-time scoped action
identity use the existing native preservation/history path; color removal is not
an authored operation.

At A5.22, full **Model292 / UI59** and actual product/checker builds passed with
zero new compiler diagnostics. A5.21 App50/Render28 remain retained evidence, not new
RGB suite results. Parent reviewed the complete six-file scalar delta; no new
independent-review or human/Hammer acceptance claim.

**Quick owner check (pending):** use the whole A5.22 kit and a copied VMF. Pick a
manual visgroup, set17/128/255, undo/redo, Save As a new VMF and reopen in matching
Hammer++. Confirm the record's RGB, not a promised object tint or checkbox. Keep
the whole A5.21 kit as recovery and never overwrite an original to test refusal.

## Retained A5.21: live manual visgroups without converting the VMF

In the **Map** inspector, scroll to **LIVE native manual visgroups**. This shows
current UNSAVED state, separately from the saved-file metadata dialog. Create,
rename, reparent or delete supported records; assign/unassign the current native
selection or explicitly highlighted member rows. Moving a record appends it at its
new parent while retained siblings keep their order. Save As/reopen and undo/redo
preserve native identities, ordered direct references and untouched source bytes.

**Show members / Hide members** are explicit manual actions on the supported
current descendant/owning closure. Assigning later does not replay an old Hide.
The panel reports own flags, inherited/effective native causes, ordinary session
visibility and protected/unprojected rows; it does not invent a Hammer checkbox.
Absent manual/automatic flags use the declared no-veto preview fallback, not a
certification of every Hammer branch. Manual Show does not clear wrapper or
automatic causes, ordinary session hiding, or protected/unknown data. Ordinary
Show All cannot bypass native-hidden selection/rendering. Session hiding itself
is transient and saves without changing native flags.

Hidden unassigned supported members can be assigned through their explicit member
rows, then targeted through a visgroup. This is metadata authoring, not permission
to pick/edit hidden geometry. Ambiguous/opaque/unsupported rows refuse atomically;
independent supported branches remain usable. RGB/color editing, wrapper/automatic
flag authoring, automatic group classification, layers, linked instances and a
reconstructed saved-Hammer checkbox are not implemented by this family.

**Quick owner check (pending):** in the whole A5.21 kit, use a copied editable map.
Create a group/child, assign one ordinary object, Hide then Show, and undo/redo.
Move an early child to Root while another root exists; Save As a new VMF and
reopen. Confirm the scrollable panel is usable and compare the copy in matching
Hammer++. Keep the whole A5.20 kit as recovery; do not merge DLLs or overwrite an
original to test refusal. These observations are not supplied by automated tests.

Current production passes Model289, UI58, App50 and Render28 cases with zero new
compiler warnings. Both real retained polygon-cache variants pass128 assertions;
corpus document38/strict35/partial-preview38 and all original hashes are unchanged.
The independent whole-family review exposed three real defects (reparent order,
Both-clip stale replay and ordered-reference save); each was reproduced before
repair. Focused independent post-fix re-review found no remaining blocker. A prior Windows
preference-file rename failure remains untouched even though the current UI run
passes. This is a bounded candidate, not full Source or human/Hammer acceptance.

## Retained A5.20: duplicate and delete ordinary cached brushes

Ordinary non-displacement brushes with validated Strata point_data or Hammer++
vertices_plus can now be duplicated/deleted through the existing tools. Supported
whole/partial plain brush entities and fresh plain additions to a cached entity
use the same closed-owner validation. Copies receive stable independent native
object/side IDs and remain editable after repeated saves and reopening.

**Quick owner check (pending):** in the whole A5.20 kit, open a COPY of either map.
Select an ordinary non-terrain brush, duplicate it, move the copy one grid step,
undo/redo, Save As a new file and reopen. Duplicate/delete the reopened copy and
undo. Report the exact tool/diagnostic if it refuses. Matching Hammer++ reopening
is helpful but not implied by the automated native reopen.

Both actual inputs passed this acted Map path, including another ownership edit
after reopen and exact undo/source restoration. Full model260/affected91/App50
pass; automated tests are not human visual/compiler acceptance. Unknown/mixed/
misplaced/malformed caches, displacement ownership, cached members in current or
destination groups, reparenting, clipping and topology remain guarded. Existing
plain uncached groups/clipping still work. Do not strip blocks or overwrite an
original to bypass a refusal. Keep the whole A5.19 kit as unmerged recovery.

## Retained A5.19: move native terrain and cached base polygons together

A known parsed displacement can now move with its validated Strata point_data or
Hammer++ vertices_plus base polygon. Use whole-brush **Move**, **Rotate**, or a
**positive uniform Scale**. The base planes, cached polygon, submitted mesh and
normals follow one proper similarity frame. No clipping, mirroring, shear or
nonuniform terrain scaling is unlocked; unknown/malformed payloads stay opaque.

**Quick owner check (pending):** on a COPY of either map, select a terrain brush,
move one grid step, undo/redo, Save As a new file and reopen. If desired, try a
proper rotation or uniform scale. Actual terrain subjects were discovered through
the production parser and matched across variants by native ID, not guessed entry
numbers; both passed the automated move/save/reopen/further-edit/history check.

Stale cache, mesh-only/base-only motion, inconsistent multiple displacement frames,
references, topology and unsupported root settings still refuse before partial
publication. Existing non-terrain supported edits remain available. Keep the whole
A5.18 kit as recovery; never strip blocks or overwrite originals for a refusal.

## Retained A5.18: ordinary Hammer++ polygon-cache edits

You can now retry the **Hammer++ counterpart as well as the original VMF** on
copies. Repeated `vertices_plus` xyz rows have physical polygon order rather than
Strata's explicit indices; they are separately parsed and strictly matched before
sharing the same geometry correspondence. Known observed scalar preview roots are
preserved raw and admitted only with their exact validated shapes.

**Quick owner check (pending):** open a copy of either file, move the brush that
refused, undo/redo, Save As a new file and reopen. Automated checks did this on BOTH
actual inputs and performed another real edit after reopening. Keep the whole
A5.17 folder as recovery; never merge DLLs or strip native cache/settings blocks.

This does not unlock arbitrary topology: mirrors, changed vertex/face incidence,
clone/remove/group/reparent/clip and combined displacement/cache base motion stay
restricted. Same-side mixed cache formats refuse. Root settings must have exact
known fields, finite numeric shapes, unique roots/fields; LUT names and texlight
must be EMPTY and bgimages_plus must be EMPTY. Nonempty resources/backgrounds,
unknown/nested/duplicate/malformed/future settings still guard geometry edits.
Those are explicit limits, not a full Hammer++/compiler or all-map certification.

## Retained A5.17: edit ordinary native point-data brushes

A reported Strata map loaded correctly but refused an ordinary brush edit because
its side `point_data` polygons were opaque. This candidate understands the strict
indexed polygon shape and updates affected vertex rows during admitted content
edits. The exact reported brush passed move, Save As, reopen, undo and redo using
the actual Map command path; your original map was never a write target.

**Quick owner retry (pending):** open a COPY of the original VMF, select the brush
that refused, move one grid step, undo/redo, then Save As to a new file and reopen.
Please mention the exact tool if it still refuses. Positive scaling, rotation,
UV/material edits and boundary changes work only while face/vertex incidence and
relative polygon winding stay unchanged. Even stationary-plane cache boundaries
must follow changed neighbouring planes; unedited numeric text remains preserved.

This is not a whitelist bypass. Malformed/unknown/mismatched caches still refuse.
Mirrors, topology/cardinality changes and point-data duplication/removal/grouping/
reparenting/clipping remain guarded. Combined displacement + point-data base
motion is still conservatively refused. The `_h++` counterpart uses a DIFFERENT
`vertices_plus` schema: its controlled refusal was expected in A5.17, not a
point-data success. A5.18 separately supports the bounded counterpart described above. Keep the **whole A5.16**
kit as recovery and do not merge DLLs, strip cache blocks or overwrite originals.

Corpus load/metadata-edit passes do not prove every brush or operation is editable.
Human editor/Hammer checks and full Source support remain pending.

## Retained A5.16: live native cordons beside map properties

Open an editable **copy** and find **Live native cordons** in the map inspector.
This is current UNSAVED map state, not the saved-file metadata dialog. With no
controller, add a modern cordon: its named record, -64..64 initial box and inactive
flags are deliberate new defaults. Select a record/box, edit name, ordered min/max
and optional active flags, then **Apply**. **Not set** preserves absent loaded flags.
Add boxes/records or remove them; the last box must be removed with its record,
and removing the final record removes its controller. Empty modern controllers
also have explicit removal. Semantically unchanged Apply is an exact no-op.

**Fit** copies valid selected bounds; it does not change selection or soft-map
bounds. Reversed, nonfinite or zero-sized bounds refuse, as do stale row actions,
unsupported/ambiguous controller data and read-only preview. Loaded legacy cordons
allow in-place bounds/active/removal only: no rename, second record or silent
conversion. Unknown data outside the controller remains untouched; unknown payload
inside the controller requires refusal rather than a guessed rewrite.

Undo/redo and Save As/reopen retain native data and identities. This tool changes
native metadata only: **it does not clip this viewport, create a gizmo, or guarantee
an external compiler's region behavior**. Matching Hammer++/SDK interpretation is
still a manual check. A5.16 did not unlock visgroup/hidden authoring; the separate
A5.21 manual-visgroup boundary is described above.

**Quick owner check (pending):** on a copy, add/edit a cordon and a second box,
Apply, undo/redo, Save As and inspect those bounds/flags in matching Hammer++.
If available, repeat a bounds edit on an existing legacy cordon without converting
it. Check the live panel is readable/useful. Keep the **whole A5.15** kit as recovery;
never merge DLLs or overwrite an original just to test a refusal.

## Retained A5.15: group ordinary native objects without converting the VMF

In a new editable map or **copy**, select two plain brushes and use the existing
**Group** action. The native label is derived as `Group <id>` with no name prompt.
Open the group to edit its members normally; Group can also nest supported groups
and include whole ordinary entities. **Ungroup** dissolves only the selected
immediate containers, retaining nested groups and entity solid ownership. Do not
ungroup the current opened group or its ancestor: close it first; refusal keeps the
editing context live. Native rename is unavailable, not a hidden VMF name field.

Undo/redo retains the exact live nodes, IDs, sibling positions and selection.
Save As and reopen preserve native IDs, memberships and untouched source text;
member order on reopen follows deterministic VMF lexical order because `groupid`
does not encode an arbitrary selection-created sequence. Supported grouped ordinary
creation, transformation, duplication, deletion and Front/Back/Both clipping stay
available. Undo-parked containers are not saved or assigned fresh replay IDs.

Malformed/ambiguous, hidden, opaque or foreign membership components remain
preserved and uneditable. They do not automatically forbid an independent plain
brush operation; genuine opaque geometry references can still require refusal.
Visgroup/layer/hidden/linked-instance authoring and arbitrary topology are NOT
unlocked by ordinary grouping. The saved-file metadata inspector remains read-only.

**Quick owner check (pending):** on a copy, group two plain brushes, open/edit one,
undo/redo, close and ungroup, then Save As a new VMF and reopen in matching Hammer++.
Confirm the same objects/membership and no lost entity ownership or unknown data.
Optionally repeat with one point entity and a nested group. Keep the **whole A5.14**
kit as recovery; never merge its DLLs into A5.15. Automated checks are not manual
Hammer/appearance or universal Source-branch acceptance.

## Retained A5.14: keep BOTH halves of an ordinary native brush

In a new editable map or **copy** of a VMF, select a visible ordinary closed brush
and place a cutting plane through it. Choose **Both** in the normal clipping tool.
Front keeps the original solid identity; Back is a new sibling in the same world
or supported plain brush entity, with its own remapped side IDs. Both halves are
selected. Loaded, fresh/template and duplicated plain brushes work before first
save. Undo restores the original and selection; redo restores the SAME halves/IDs.
A plane missing/tangent to a brush is a no-op, not a deletion or extra history item;
mixed selections retain unchanged subjects.

One whole-batch native admission checks parents/visibility, contents and identity
budgets before predictable mutation. It is not duplicate + two unrelated clips or
MAP replacement. Parked siblings do not appear in saved output. Front/Back remains
supported, and its narrow retry certificate remains; compound failures do not
inherit retry permission. Other failed redo/undo/transaction policies stay unchanged.

Actual preview cut material/UV data is now checked against committed cuts for both
orientations and first-nearest ties. Those controls reproduced a native preview
mismatch before repair; earlier post-commit checks alone did not prove preview
parity. Human visual acceptance is still pending.

Displacement topology, hidden/opaque/global-reference ambiguity and other CSG/
arbitrary topology remain gated. Do not bypass refusals through MAP export or
use an original as an experiment.

**Quick owner check:** Both-cut one copied or fresh plain brush, undo/redo, then
Save As a new VMF. Confirm two closed halves and matching cut texture placement in
Hammer++. Optionally repeat within `func_detail`. Keep the whole A5.13 kit as
unmerged recovery; automated tests are not universal SDK/visual acceptance.

## Retained A5.11: uniformly scale terrain and preview static bodygroups

Basic editable native VMFs already exist; **read-only compatibility preview is not
an editing mode**. On a copied editable VMF, select a whole raised displacement
brush and use the normal scale controls with equal positive factors (try2 or0.5).
Base positions, offsets, elevation and distances scale together; normal-like
vectors retain magnitude. UV locks, picking/bounds, undo/redo, native Save As and
continued rotation/movement/distance edits have acted controls. Bounds-based
commands conservatively require exactly uniform ratios; near-uniform arithmetic
is not permission for nonuniform changes. Zero/negative/shear/flip/topology and
unsafe numeric motion still refuse rather than flattening or dropping metadata.

Supported static Source MDL/VVD/VTX models now select multiple bodyparts/models
with the entity `body` value independently of `skin`. Blank choices hide those
parts. Rendered surfaces, bounds, ray picking, entity refresh and browser layout
use the same selection. Four authored meshes in the acted fixture share four
VBOs across body/skin combinations and release buffers when renderers go away;
this is not a whole-map VRAM/performance claim. Root-pose only, not animation,
controllers, game collision or universal Source model/shader/FGD parity.

Native save transactions also use short unique sibling names so a valid long
Windows destination does not fail merely because its temporary filename repeats
the full leaf. Exclusive creation, stale-original/overwrite/link guards, backups
and failed-promotion recovery remain; this is not unlimited long-path support.

**Quick owner check:** on a **copy**, uniformly scale one whole raised terrain
brush by2, undo/redo and Save As; compare height, position and UVs in matching
Hammer++. If convenient, separately change body and skin on one known bodygroup
prop and compare the visible parts/bounds/browser thumbnail. All those human
observations remain pending. Keep the whole A5.10 kit as recovery; never merge DLLs.

## Retained A5.10: rotate native terrain without flattening it

Select a whole displacement brush in object mode and use the normal rotation
command. Its base, raised samples and authored vectors now rotate together,
including arbitrary angles/pivots, UV-lock behavior, picking/bounds and undo/redo.
Save As a new file, reopen, then continue moving or distance editing normally.
Nonuniform scale/shear/flip, face/vertex topology and sculpt/create/power/sew remain refused.
Opaque attachments and native references remain guarded; no conversion through MAP.

Finite in-world geometry alone is not enough: precision-losing cancellation and
rotated vector overflow refuse before world mutation. Legacy untyped offset-normal
rows can still load/translate unchanged but cannot rotate safely. No value clamping
or metadata removal makes those operations pass.

**Quick owner check:** on a **copy**, rotate one whole raised terrain brush a small
angle, undo/redo and Save As. Compare its position, shading and UVs in matching
Hammer++. Manual/native appearance remains pending; keep A5.9 as recovery.

## Retained A5.9: move native terrain and select fixed-model skins

Select a whole displacement brush in object mode and move it normally. Its base
and raised mesh now translate together, with updated picking/bounds, undo/redo,
UV-lock behavior and preserving saves. You can then edit a distance sample and
save again. Rotation/scale/shear/flip and face/vertex topology remain refused;
this is not a sculpt/create/power/sew or blend-material tool. Unknown attachments
and geometry references still block unsafe changes rather than being discarded.

SourceVMF also enables entity-driven skins for simple nonempty fixed `studio` /
`studioprop` FGD paths. If the model has multiple skin families and matching
assets, edit the entity `skin` property0->1->0 to select the preview family.
Explicit/legacy/dynamic model declarations and other games keep prior semantics.
This is a bounded Source preview policy, not animation/bodygroup or universal
FGD/SDK support.

**Quick owner check:** move one whole raised displacement brush a small grid step
on a **copy**, undo/redo, then Save As a new file and compare in matching Hammer++.
Keep A5.8 as recovery. Existing native visual/Hammer acceptance remains pending.

## Retained A5.8: native displacement meshes and more texture layouts

Admitted native power2..6 displacements now draw their actual raised triangles,
with base-position UVs, surface picking and expanded selection bounds. Select
exactly one displacement **face** to use **Displacement vertex** in the face
inspector: choose row/column, enter a small distance and click **Apply distance**.
Normal undo/redo and preserving Save As retain the other native bytes. Incomplete
or zero-direction authoring data does not enable the tool. Unknown attachments
and references still refuse unsafe edits rather than discarding source data.

The bounded VTF preview also decodes 13 more common integer channel layouts,
including intensity, alpha-only and bluescreen formats, with an expansion-aware
mip budget. This is not HDR/normal/animated texture support or a measured speedup.

**Quick owner check:** open a **copy** with a raised displacement, confirm the
surface and face-selection position, apply one small distance edit, undo/redo,
and Save As a new VMF. Compare that vertex in matching Hammer++ when convenient.
Sculpt/create/power/sew, arbitrary base transforms and blend-material parity are
not implemented. Keep A5.7 as recovery; native visual/Hammer acceptance is pending.

## Retained A5.7: authored Source cutout thresholds

Fences, grates and similar alpha-tested brush/prop materials now use a supported
explicit `$alphatestreference`, including values inherited or replaced through
Patch VMTs. A lower threshold keeps more pixels; a higher threshold removes more.
Previously these previews all used 0.5. Explicit editor alpha-function overrides
still win, and cutoff happens before editor fading. Catalog metadata still does
not decode textures or rewrite original assets.

This bounded preview accepts one finite threshold **above 0 and at most 1**.
Absent values keep the existing **0.5 preview fallback**, not a newly certified
Source-game default. Explicit zero, malformed/duplicate/out-of-range values
refuse instead of guessing branch-specific semantics. A reference alone does
not enable alpha testing. Scalar `$alpha`, full shader opacity and backface
lighting remain unsupported; combined flags retain existing cutout precedence.

**Optional quick check:** open a copied map with a familiar fence or grate using
an authored threshold and compare its holes/edges to the matching game. Do not
edit installed VMTs for this test. Keep A5.6 as recovery; detailed A5.7 appearance
confirmation remains pending.

## Retained A5.6: two-sided Source materials

Basic Source `$nocull 1` now makes demanded brush and static-prop base-image
previews two-sided, including flags inherited or replaced through a Patch VMT.
Absent/false flags keep normal culling, and explicit editor material overrides
still win. Drawing restores the actual previous culling, blend and depth state;
this is not another winding flip or a global disable of backface culling.
Original map/material files are not rewritten and catalog metadata does not
trigger texture loading. Malformed/duplicate flags refuse; strict material
inspection remains strict. This does not reproduce Source backface lighting or
add geometry, displacements or complete shader support.

**Optional quick check:** open a copied map with a familiar two-sided fence,
foliage or thin prop, and look from both sides. Compare the matching game's
material without editing its installed VMT. Keep the prior A5.5 folder/ZIP as
recovery. The owner gave overall A5.6 visual approval; detailed surface/game
comparison remains pending in lextest.md.

## Retained A5.5: inherited Source Patch materials

Brush and static-prop textures can now resolve a Source **Patch VMT** through the
configured mounted filesystem, instead of rejecting that wrapper outright.
`include` names a relative `materials/... .vmt` file; slash/case normalization and
existing mount priority apply. Scalar `insert` adds or overwrites a parameter;
`replace` updates only an existing parameter. Final supported-shader, base-texture
and opacity checks still run after resolution. This is VMT inheritance, not a
new geometry/displacement tool or complete shader support.

Resolution happens only when the texture is demanded, with no global include
scan or persistent cache. Limits are 1 MiB per file, 4 MiB combined text and
16 included files. Explicit text reads are guarded; work internal to filesystem
open/decompression is not a new whole-I/O guarantee. Cycles, unsafe paths,
duplicate/overlapping commands/parameters, nested/proxy blocks, empty-value
removal and unsupported final features produce diagnostics rather than guesses.
Original material and map files are not rewritten.

**Optional quick check:** if a familiar brush or prop uses a Patch-based material,
open a copied map with matching content and compare that material with the game.
Report the exact remaining fallback; a visible base image is not full shader
fidelity. Keep the known-good A5.4 folder/ZIP separate as recovery.

## Retained A5.4: rendering and game-level material settings

- Source VTX triangles no longer receive an extra reversal. A real prop's outward
  normals and native brush-cache controls distinguish this from a blanket global
  culling flip; Source coordinates, UVs and source bytes were not reflected.
- Source VTF alpha can be a shader mask. Basic VMT `$translucent`/`$alphatest`
  flags now control preview opacity/cutout instead of blindly blending that mask.
  Raw alpha is tested before masking; surviving opaque/cutout pixels still accept
  the editor's whole-brush fade. Actual GPU readbacks cover these cases, but this
  is not full Source shader support, arbitrary cutout thresholds or lighting.
- **Material Collections** settings show the active game/profile name for its
  material root and mounted search paths, not thousands of nested folders.
  The full catalog remains. Opening settings never rewrites a saved partial
  selection. A partial legacy mount can appear in both available/enabled lists;
  enable it to browse all folders, or disable it to hide the mount. Undo works.
  This controls browsing of already mounted content, not a new cross-game mount
  manager. Read-only previews cannot change saved collection choices.

The owner confirmed the observed A5.4 retry **looks good** on 2026-10-25.
That is qualitative visual acceptance, not an individual attestation of every
optional selection/undo action, all-map rendering or measured performance.
Detailed/unrelated manual gates remain explicit in [lextest.md](../../lextest.md).

## If a VMF cannot open for editing

The editor first attempts its normal strict loader. On a native refusal, it
now offers **Open read-only preview**; **Cancel** remains the default. This is
an explicit, possibly partial inspection mode, not automatic repair.

- Duplicate native IDs and all source bytes remain unchanged. A failed solid is
  not given invented UVs, a substitute hull or a fake point-entity representation.
- The title, status bar and viewport say **READ ONLY** and report unprojected
  solids. Orange markers are labelled **source-plane samples**, not solid bounds.
  At most 128 markers draw; omissions without a reliable sample are counted as
  **unlocated**. Missing geometry is not claimed to be visible.
- Camera movement and selection/undo/redo remain available. Creation, dragging
  entities into the view, editing, normal Save, export, compile and launch are
  disabled. Inspection is not a way to compile a repaired map.
- **File -> Save unchanged native copy…** writes the loaded native bytes to a
  **new** filename only. It cannot overwrite any existing file and does not
  change which source file the preview is inspecting. Repair the source in an
  appropriate editor and reopen normally if editable loading is needed.

## Basic native authoring to try

Create a **new map** from the profile's native VMF starter, or use an owned copy.
Draw a small closed brush, move it, duplicate it, delete the duplicate and try
undo/redo. Add a simple point entity from your game's definitions. Save As a new
`.vmf` and reopen it in SourceBroom and matching Hammer++. Plain brush-to-entity
creation and whole/partial brush-entity duplication are also enabled.

Native IDs remain stable across save and undo/redo; no MAP conversion is used.
Unknown metadata is not permission to guess: opaque attachments or references
can make an otherwise ordinary operation unsafe and require refusal.

## Retained usability follow-up

Source startup no longer repeats its just-completed content scan, and enabled
material-folder selection uses a hashed lookup instead of repeatedly walking the
whole list. This does not skip materials or introduce a stale cross-map cache;
explicit material reload still discovers added and removed loose files. Initial
loose-directory indexing errors are reported without rescanning healthy mounts.

The shared GUI/checker preview envelope is now **+/-131072 units**. It is an
editor preview policy, not a promise that every Source branch compiles or plays
at that size. Open/incomplete brushes, duplicate IDs and invalid UVs still refuse
**editable** loading rather than being silently repaired. The optional read-only
mode above reports omissions instead. Syntax/structure and resource limits remain
in effect. Retry a failing map on a copy and report its exact diagnostic if both
modes refuse.

Texture processing now resumes beyond a busy queue prefix, retires unused
catalog resources in one compaction pass, and avoids re-sorting the whole browser
on loading/failure/upload notifications whose layout dimensions did not change.
Additional enumerated PBR/tint/tree-sway metadata is ignored only for the stated
base-image preview; it does not add those Source shader effects. Catalog entries
are not pruned, and pixel loading stays demand-driven. Initial content mounting
still takes seconds in a large installation; no whole-startup speedup or
Hammer++ performance advantage is claimed.

## New tools to try

- Select an existing entity. Inspect its FGD-backed property hints and Source
  outputs table. Repeated event rows remain ordered; edit a delay, undo/redo,
  Save As, and reopen the copy in Hammer++. Defaults are hints, not inserted
  keyvalues. Unknown native data is preserved, not automatically repaired.
- Open a prop-containing copy using matching installed content. Supported
  MDL/VVD/VTX data has a bounded static/default-pose preview with material lookup;
  missing/unsupported data has a diagnostic fallback. Full animation, physics,
  controllers and shader parity are not claimed.
- Save, then use **Run -> Compile -> Native metadata / Custom package…**. The
  metadata tab reads the **saved** snapshot: groups, visgroups, hidden state,
  cameras and cordons are read-only in THIS saved inspector. Live ordinary cordon
  editing and A5.21 manual-visgroup editing are separately available in the map
  inspector. **Go to camera eye** moves
  only the eye, not full Hammer orientation. Reload the saved inspector after saving.
- To test packaging, use a dedicated **custom-only** game-relative content root
  you have rights to distribute. Discover/review the exact plan, then approve it
  separately and write a **new** distribution ZIP. Do not select a Steam install.
  This is incomplete discovery, not automatic BSP embedding or stock-content
  redistribution. Read every unresolved/excluded item.

[Native tool details](SOURCE_TOOLS.md) explain the exact limits and safety checks.

## Compile and play

Use **Run -> Compile** on a saved VMF. Select the intended CSS, Momentum, FTESurf
or editable tool configuration. Check all tool, content, staging, runtime and
maps paths before deliberately clicking **Build** or **Build & Play**. Extra
arguments are one argument per line, not a shell command.

The backend preflights the actual staged native document before starting a
compiler. CSS20 and Strata25 target-header contracts are checked after every
successful stage. Generic acceptance does not certify arbitrary Source games.
A fresh owned staging directory prevents an old BSP from passing as a new build.
A failed stage never promotes a BSP or launches a runtime. Compiler warnings and
preview/unassessed feature diagnostics still need inspection.

The dialog supports bounded leak display and navigation to diagnostic positions.
Compiler indices are not native VMF IDs; navigation is not a repaired leak.
[Source build configuration](SOURCE_BUILD.md) covers the process and file contract.

Build & Play copies only a fully successful BSP under a new unique name and
requests a detached runtime launch. It does not kill an existing game. Launch
success is not proof the map loaded, materials match, or ramp collision is right.
Owner Hammer++/in-game observations remain pending in [lextest.md](../../lextest.md).

## Important limits

- Basic plain-brush/point-entity creation, duplication, deletion and limited
  brush-entity ownership are enabled. Arbitrary face/vertex topology changes,
  other CSG/arbitrary topology, native layer/linked-instance authoring, displacement
  creation/sculpt/power/sew and displacement nonuniform scale/shear/flip, overlays
  and instances remain unsupported. Plain uncached non-displacement brushes
  already support valid nonuniform scale, shear and flip through normal commands;
  six actual Map/UV-lock arms verify planes, texel mapping and exact save/history
  on the existing A5.22 code. This is a scope clarification, not a new feature or
  permission for cached/opaque/terrain/prop transforms. Ordinary native groups, live cordons and manual visgroups
  have the separate bounded paths above; cached-member group/clip/reparent guards remain. Common unsafe paths refuse before mutation;
  some upstream controls can remain visible and save guards stay authoritative.
- The saved-file metadata dialog is read-only; it is not the live group/cordon/
  manual-visgroup authoring interface. SourceBroom does not apply cordon viewport
  clipping; external compiler region behavior must be checked separately.
- Base texture/static model preview is not full Source shader/game parity.
- All 38 supplied maps pass lossless syntax/no-edit checks; **35/38** construct
  strict editable scenes. **38/38** open with explicit read-only preview, where
  omitted solids remain in the original document but are not scene geometry.
  This is not all-map compatibility or editable repair.
- Current A5.28 full **60 UI / 50 App** cases pass; A5.27 **299 Model / 28
  Render / 21 GL** are retained unchanged-boundary evidence, not new runs.
  Earlier preference rename failures remain unfixed, not erased by a passing
  current run. Packed production GPU2693 is retained A5.27; standalone upload141
  and A5.26 VMF corpus38/35+same3/38 are retained unchanged-boundary evidence,
  not newly measured A5.28 counts.
  Installed-package evidence belongs to the matching receipt; source/build results
  alone are not installation, manual appearance or full Source acceptance.
- Historical A5.14/A5.11 evidence (not current suite totals): **201 model** and
  **50 App/tool** entries passed; existing **21 graphics**
  evidence remains valid for unchanged graphics code. Both focus **9/9**, retained
  native/core **8/8**, and ClipTool **4/4** (670 assertions) pass. Independent focused
  review found no remaining blockers. Actual preview red24 (12 attribute mismatches
  plus inherited Both red12) became green. Retained certified redo/lifetime controls
  still act. A real changed-bounds cut
  reproduced two preview material/UV failures, then both and Front/Back attributes
  passed after the fix. Actual app/checker/UI callers rebuilt with zero new warnings.
  Uniform-scale/save-path
  native focus passes **5,257 assertions/39 cases**; body-focused tests pass
  **2,748/6** and weak vertex-holder tests **63/1**. Actual app/checker/UI public
  callers rebuilt with zero new compiler warnings; affected UI **129/3** passes.
  The reviewed body GPU fault was reproduced before the fix, then shared-buffer
  and release/rebuild controls passed. Existing five-config shader GPU **2,539**
  evidence is not bodygroup/native visual acceptance. The prior full UI **42/43**
  Windows preference-file replacement failure remains preserved/unfixed; these
  targeted passes are not an all-UI-green or manual/Hammer acceptance claim. [VALIDATION.md](VALIDATION.md) and package
  receipts distinguish source checks, installed reruns and historical evidence.
- The package includes no Valve assets, Source compilers or external user maps.
  Keep the whole A5.21/A5.20 kits and prior A5.19/A5.18/A5.17/A5.16/A5.15/A5.14/
  A5.13/A5.12/A5.11/A5.10/A5.9/A5.8/A5.7/A5.6/A5.5 and accepted A5.4 recovery
  packages; do not merge DLL sets.

For additional compatibility feedback, retry one previously refused copied VMF, accept the
read-only preview and confirm the warning/counts and camera/selection behavior.
Then open a familiar map and briefly scroll the material browser: report whether
textures progress and the UI feels less stuck. These observations are not measured
speedup, complete-map rendering or shader parity. The native authoring/Hammer++
checks above also remain pending. Report the game/profile and exact observation,
not a blanket compatibility verdict. No performance advantage
over Hammer++ or all-branch compatibility has been measured.
