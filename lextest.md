# Human acceptance — SourceBroom alpha candidates

Only the owner observations explicitly recorded below are confirmed. Do not turn
an automated build/screenshot or a general visual approval into unrelated passes.
Use newly authored maps/copies; never overwrite an original user/corpus VMF.

## A5.29 direct saved Source tools — pending small owner check

Use whole A5.29 on a COPY; keep whole A5.28 and owner-confirmed A5.25 recovery.
No A5.29 owner observation received. Tools132, native Qt menu-to-root75 and
regular offscreen UI61/61 are not human or matching-Hammer acceptance.

- [ ] On a saved copied VMF, choose **Run -> Source Tools (Saved VMF)...** and
  confirm the tools open without a save/compile step. If that map offers explicit
  read-only preview, this entry should still open while Compile/Launch stay
  disabled. No edit/save or hunt for a special map is needed.
- [ ] If it already has a valid saved camera, the A5.28 eye-versus-view comparison
  below can use this direct entry. This does not author cameras or import FOV.

All pending; no all-map/layout/branch or measured performance claim.

## A5.28 saved camera view navigation — pending small 3D owner check

Use whole A5.28 on a COPY; keep whole A5.27/A5.26 and owner-confirmed A5.25
recovery. No A5.28 owner observation received. Tools118, actual MapWindow26 and
full UI60/App50 are not human/full-Hammer-camera-state acceptance.

- [ ] If the copied map has a saved camera, open **Run -> Compile -> Native
  metadata / Custom package…**. In3D compare **Go to camera eye** (heading stays)
  with **Go to camera view** (also faces its saved look target). No edit/save or
  hunting for a case is needed. Reload the saved snapshot only when appropriate.
- [ ] If convenient, compare that saved eye/look in matching Hammer. Current
  projection/FOV/zoom are deliberately not imported; camera records/active-camera
  and map history remain read-only. Unsafe/degenerate targets stay disabled.

All pending. Does not enable camera authoring, compile a read-only preview, certify
all layouts/maps/branches or establish performance/new render acceptance.

## A5.27 translucent scalar material opacity — pending small owner view check

Use whole A5.27 on a COPY; keep whole A5.26 and owner-confirmed A5.25 as recovery.
No A5.27 owner observation has been received. Automated material136, brush/prop
metadata10224, native packed GPU2693 and full Model299/UI59/App50/Render28/GL21
are not human/Hammer/full-shader acceptance.

- [ ] If a known copied map uses a basic VMT with explicit `$translucent 1` and
  scalar `$alpha`, open it and report whether the brush/prop surface looks sensible
  rather than falling back. No edit/save is needed; no need to hunt for a case.
- [ ] If convenient, compare that same material in matching Hammer/the game.
  Do not strip unsupported parameters, alter original VMTs or overwrite originals
  just to force eligibility. Cutout/terrain/proxy combinations remain unsupported.

All pending. Zero means invisible in this bounded scalar path, not the separately
unsupported alpha-test-reference sentinel0 or terrain weight. No measured speed,
all-map/branch or full native prop GPU-pipeline claim.

## A5.26 explicit missing terrain alpha — pending control/Hammer check

Use the whole A5.26 kit on a COPY; keep whole owner-confirmed A5.25 as recovery.
Automated Core80/Map124/UI64 and full Model298/UI59/App50/Render28/GL21 do not
confirm layout/feel or native Hammer interpretation. No A5.26 owner observation
has been received.

- [ ] On one copied terrain face that offers **Initialize zero alpha rows**, press
  it, set one sample Alpha128, then undo alpha and initialization separately.
  Confirm the controls distinguish table creation from height/alpha changes.
- [ ] If convenient, Save As a NEW file, reopen and inspect that weight in the
  matching Hammer++ branch. Never replace an original or strip present partial
  native data to force eligibility. Existing duplication/topology guards remain.

All pending. This is not painting, measured performance or all-map/shader/branch
acceptance. A5.25's confirmed view is not a manual pass for these new controls.

## A5.25 brush vertex upload — owner-confirmed same-map view

A5.24 owner observation is FAILED: loading the supplied large VMF scattered
vertices and made the view unusable. A5.25 fixes the actual36/32-byte cache/upload
mismatch; automated production-upload141 and packed-GPU2597 checks do not replace
this human check. Use the whole A5.25 kit and a COPY; A5.23 is fallback, not A5.24.

- [x] Reopen the same copied VMF and confirm brush geometry and edges stay in place
  and the viewport is usable. The owner replied “yep, works” to the A5.25 same-map
  geometry/edges check. This confirms the observed view, not unrelated editing.
- [ ] If convenient, check one supported terrain alpha change/undo separately.

The owner confirmed the requested A5.25 view check and asked to continue
development. Terrain alpha/undo, saving, performance, other maps and Hammer/compiler
acceptance were not separately confirmed. Original VMFs must not be changed for
these checks; no broader pass is implied.

## A5.24 basic terrain blend preview — FAILED owner load, superseded

Use the whole A5.24 kit and a COPY with a basic WorldVertexTransition displacement.
Keep the whole A5.23 kit as unmerged recovery. Automated real GPU2580 and full
Model296/GL21/UI59/App50/Render28 do not confirm these human observations. The prior
intermittent preference-fixture failure is not proven fixed. The owner subsequently reported scattered vertices/an unusable VMF view;
that failure is confirmed, not acceptance. Do not use this kit for the checks below.

- [ ] Select one supported terrain face and one native grid sample in
  **Displacement vertex**. Try **Alpha0** then **Alpha255** with **Apply alpha**;
  confirm its neighboring triangles change between the two material layers
  without moving terrain. Undo/redo and check controls at normal window size.
- [ ] Save As a NEW VMF, reopen and confirm the stored weight and local blend.
  Compare that weight in matching Hammer++/Source branch if convenient. Record
  the actual material/branch observed; bump/detail/specular/masks and full Source
  shader parity are not promised by this linear preview.

The planned blend checks below were not confirmed; the A5.24 load failed first.
No original VMF writes, painting, missing-row creation or permission
bypass is part of this check. Visual approval is not measured performance or
universal shader/map/native-compiler acceptance.

## A5.23 stored displacement alpha — pending owner/Hammer check

Use the whole A5.23 kit and a copied VMF with a supported native displacement.
Keep the whole A5.22 kit as unmerged recovery. Automated Model294/current alpha
inspector and source-preservation checks do not confirm these observations.
Full UI58/59 retains the known unchanged Windows preference-fixture failure.
No A5.23 owner observation has been received.

- [ ] Select one terrain face, choose a native grid row/column in **Displacement
  vertex**, set a fractional **Alpha (0..255)** and press **Apply alpha**. Undo/redo
  and confirm the controls are clear at normal window size without moving terrain.
  Distance is a separate action; stored weight does not promise shader blending.
- [ ] Save As a NEW VMF, reopen and compare its stored terrain alpha in matching
  Hammer++/Source branch if available. Record only the actual observed control,
  weight and branch; paint/smoothing/material parity are not implied.
- [ ] If a map lacks complete alpha rows, alpha controls should stay unavailable
  without disabling otherwise valid distance editing. Do not add/strip native
  blocks or overwrite an original to force the check.

All pending. Visual approval is not numerical performance or all-map/native-
compiler acceptance; no new material-blending or complete Source claim.

## A5.22 explicit visgroup RGB — pending owner/Hammer check

Use the whole A5.22 kit and a copied VMF; keep the whole A5.21 kit as recovery.
Automated Model292/UI59 and source/build checks do not confirm these observations.
No A5.22 owner observation has been received.

- [ ] Select a supported record in **Map → LIVE native manual visgroups**. Change
  R/G/B to17/128/255, press **Set RGB on this visgroup only**, then undo/redo.
  Confirm the RGB column/controls and record identity remain understandable.
- [ ] Save As a NEW VMF, reopen and inspect that record's RGB in matching Hammer++.
  This is own-record metadata, not promised member/object tint or propagation.
- [ ] If testing a new/colorless record, confirm **not stored** remains true until
  explicit Set; white controls are a draft, and name/reparent must not store it.
  Unsupported/repeated scalar color should preserve bytes/refuse only RGB, with
  general protected-record restrictions still explicit. Do not overwrite originals.

All pending. Visual approval confirms only the actual observed case, not full
Source branch/renderer/performance compatibility or data-integrity measurements.

## Existing A5.22 plain-brush affine behavior — pending owner/Hammer check

- [ ] On a copied VMF with a plain uncached non-displacement brush, try unequal
  axis scaling, shear and mirror/flip, undo/redo and Save As/reopen in Hammer++.
  Check both UV-lock modes only if useful. This capability already existed; the
  six-arm automated proof corrects broad docs, not a new product feature. Cached,
  opaque, displacement and prop restrictions remain separate. No observation
  received; never use original files or infer all-map acceptance from this case.

## A5.21 live manual visgroups — pending owner/Hammer check

Use a **copied editable** CSS/Momentum VMF in the whole A5.21 kit. Keep the whole
A5.20 kit as recovery; never overwrite an original or merge DLL sets. Automated
Model289/UI58/App50/Render28, actual cache128 and preservation checks do not check
these boxes. No new A5.21 owner observation has been received.

- [ ] In **Map**, scroll to **LIVE native manual visgroups**. Create a root/child,
  rename it and move an early child to Root while another root exists. Confirm
  the panel remains usable at normal window size and the destination order is clear.
- [ ] Assign one ordinary selected object; Hide members then Show members and
  undo/redo. Inspect own/effective native versus ordinary session causes. If useful,
  assign a highlighted hidden unassigned member row to a group then Show it.
  This must not make protected/unsupported geometry pickable. Automatic/wrapper
  causes are displayed but not authored by these controls.
- [ ] Save As a NEW VMF, reopen and inspect the intended hierarchy/membership in
  matching Hammer++. Report the actual tool/branch/diagnostic if it differs.
  Do not infer a complete Hammer checkbox or automatic-visibility implementation.
- [ ] Retry the previously reported ordinary cached brush on a COPY: move/duplicate,
  undo/redo, Save As and reopen. Report any remaining refusal with the exact gesture;
  cached group/clip/reparent/topology and terrain ownership remain separate guards.

All are pending. A visual approval only confirms what was actually observed, not
numerical performance, all-map compatibility or source-preservation measurements.

## Retained A4 independent acceptance

- [ ] **A4 independent Hammer++ reopening.** Open the native-edited authored room
  copy in the appropriate Hammer++ branch (no save needed). Confirm the room,
  ramp and brick texture orientation load without a parse/geometry error.
  Automated native reopen plus CSS/Strata compilation already passed; this is
  the independent editor/visual gate. Record the actual observation and branch.
- [ ] **A4 ramp joins/collision and editor feel.** In the Windows SourceBroom
  candidate, open a copy of the supplied starter, move its existing plain ramp
  one small grid step, undo/redo, Save As, and compile/play with an installed
  matching toolchain. Walk up/along the ramp and inspect the join/UVs. Report
  visible gaps, unexpected collision or surprising controls. Existing automated
  FTESurf compile/load/body logs do not prove this manual feel check.

## A5 owner feedback — observed, limited scope

The owner reports that **some** supplied Momentum Hammer VMFs load, while many
show `Native VMF load refused: VMF entry 938: invalid or unbounded Source solid`.
They describe program startup, map loading and opening the materials window as
very slow. This is actual qualitative owner evidence, not measured latency/FPS,
all-map compatibility or approval of prop/shader/I/O/collision correctness.
The compatibility/performance follow-up is active; the specific checks below
remain pending until actually observed and confirmed.

## A5.1 usability retry — pending owner observation

- [ ] Retry the map that produced entry 938, using the new candidate executable
  and a copied VMF. Report the profile and exact diagnostic if it still refuses.
- [ ] Open the material browser, scroll briefly and select a familiar material.
  Report whether startup/map opening and browser responsiveness feel improved;
  this observation does not establish a measured speedup or visual parity.

## Rendering/material feedback after A5.3 delivery — observed, limited

The owner reports that one previously refused supplied map now loads, but geometry
and prop models appear inside out. They also report an excessive per-folder list
in material settings and request game-named material-root entries. This confirms
that opening observation only: exact binary/profile hashes, complete geometry,
correct rendering and improved texture latency were not independently confirmed.
External map names/content stay out of public notes.

## A5.7 authored cutout appearance — pending

- [ ] Open a **copied** familiar map with a fence/grate using an explicit
  supported alpha-test threshold and compare holes/edges to the matching game.
  Do not change installed VMTs/original maps for this check. Supported thresholds
  are finite and above0 through1; absent uses the prior0.5 preview fallback,
  explicit zero remains unsupported. Report fallback/edge differences. This is
  not scalar opacity, every shader, arbitrary game defaults or performance proof.

## A5.6 material retry — overall visual approval confirmed

After the A5.6 delivery, the owner replied: **"looks dope! please continue!"**
This is qualitative approval of the result they observed. Exact map/profile,
individual two-sided surfaces, full shader/backface-lighting parity, independent
Hammer reopening and numerical performance were not specified or measured.
The detailed comparison below remains pending, not retrospectively passed:

- [ ] In a **copied** map with matching content, look at a familiar two-sided
  fence, foliage or thin prop from both sides and compare the game. Do not edit
  installed VMTs or the original map. Report a remaining fallback or unexpected
  transparency; this is an appearance check, not full backface lighting, all
  models/shaders or numerical performance acceptance. The continuation request
  after A5.5 did not itself confirm A5.5 or A5.6 appearance.

## A5.5 inherited Patch material appearance — pending

- [ ] If a familiar brush/prop uses a Source Patch VMT, open a **copied** map
  with matching content. Confirm the inherited/replaced base image appears where
  a fallback did previously and compare the actual game. Keep original materials
  and maps unchanged. Report a remaining diagnostic, not a full shader/branch
  verdict. This feature does not add displacement geometry or new map editing.

## A5.4 rendering/material retry — overall visual acceptance confirmed

On 2026-10-25, after the A5.4 delivery and requested orientation/material-root
retry, the owner replied: **"Epic! looks good! continue!"** This accepts the
observed visual result. It does not independently attest every detailed action
below, matching Hammer reopening, all models/shaders, complete geometry or any
numerical performance improvement. Unreported detailed subchecks remain explicit:

- [ ] Open a **copy** of the reported map in the new candidate with matching
  content configured. Look at a brush exterior and a familiar prop from outside.
  Confirm faces no longer appear inside out or unexpectedly see-through; report
  any remaining case. Do not infer all-model, lighting or complete-map parity.
- [ ] Inspect Material Collections: confirm the game/profile-named root entry
  replaces the long folder list and familiar materials remain selectable. Existing
  partial selections should not change just by opening settings. On an editable
  copy, optional enable/disable/undo should restore the previous selection.

## A5.3 compatibility and texture feedback — still partly pending

The owner again reports that many maps do not load and textures feel very slow.
This is qualitative prior-build feedback, not A5.3 acceptance or measured timing.

- [ ] On a **copy** of one previously refused VMF, choose the correct game profile,
  accept **Open read-only preview**, and confirm the persistent READ ONLY warning,
  missing-solid/unlocated counts, camera movement and selection/undo. Some geometry
  may intentionally be absent. Confirm entity dragging/creation is unavailable.
  If desired, save an unchanged native copy under a **new** name; do not overwrite
  the input. Report an exact diagnostic if it still refuses.
- [ ] Open a familiar normally editable CSS or Momentum map, browse/scroll a small
  set of materials and select one. Report whether textures progress and interaction
  feels less stuck. Do not turn this into numerical startup/FPS, shader-parity or
  long-session residency evidence.

## A5.2 basic native authoring — pending

- [ ] Create a new CSS or Momentum map from its native starter (or use a copy).
  Draw a small closed brush, move it, duplicate it, delete the duplicate and
  undo/redo. Add a simple point entity from the configured game's definitions.
  Save As a new `.vmf`, reopen in SourceBroom and matching Hammer++, and confirm
  both objects, placement and texture orientation. Do not overwrite an original.
- [ ] On a copy, make a plain brush entity such as `func_detail`, duplicate the
  whole entity and then one selected brush within it. Delete/undo a last brush.
  Confirm native ownership and unrelated metadata survive independent reopening.
  Refusal for opaque/unsupported data is intentional; report the exact action
  and message rather than attempting a MAP export to bypass it.

## A5 authoring-tools candidate — also pending

- [ ] **Ordered native outputs and property hints.** On an owned copy of a CSS or
  Momentum map, select an existing entity with repeated outputs. Change one
  delay, undo/redo, Save As, then reopen in matching Hammer++. Confirm row order,
  repeated events, parameter text and the changed value. Unknown original data
  must remain present. FGD input/output declarations must not appear as invented
  ordinary properties; real stored same-name keys remain real keys.
- [ ] **Familiar prop preview.** With matching installed content configured, open
  one prop-containing copy for CSS and one for Momentum. Report whether the
  expected static mesh/material appears or which diagnostic fallback is shown.
  Do not infer animation, game collision or all-model support from one mesh.
- [ ] **Saved metadata/camera tools.** Save a copy, open Run -> Compile -> Native
  metadata / Custom package, inspect a known group/visgroup/hidden/cordon/camera,
  then try Go to camera eye. Confirm the read-only interpretation and eye-only
  navigation are understandable; no visibility/clipping authoring is promised.
- [ ] **Custom distribution workflow.** With authored/custom-only content you
  have rights to distribute, discover/review a plan, separately approve it, and
  write a new ZIP. Inspect exclusions/unresolved items and extract to an owned
  folder. Confirm no installed stock content or original BSP was changed. This
  visual/manual check is not licence or complete-dependency certification.

## A5.8 native displacements — pending owner check

- [ ] Open a **copy** of a CSS or Momentum VMF with a raised displacement.
  Confirm the terrain is raised (not its old flat base) and face selection lands
  on the visible surface. This is a quick appearance check, not shader parity.
- [ ] Select one displacement face. In **Displacement vertex**, choose a row and
  column with editable source data, apply a small distance change, then undo/redo.
  Save As a new VMF and open the copy in matching Hammer++ to compare that vertex.
  No original map is a write target. Creation, sculpt/power/sew and arbitrary base
  transforms remain unsupported in this foundation; disabled/refused actions are
  not permission to convert through MAP or overwrite the original.

## A5.9 translation / fixed-studio skins — pending owner check

- [ ] On a **copied** VMF, select one whole raised displacement brush in object
  mode. Move it a small grid step, confirm base/raised surface stay together,
  undo/redo and Save As a new file. Compare its position in matching Hammer++.
  Rotation/scale/shear/flip and sculpt/topology remain unsupported.
- [ ] If using a Source FGD with a simple fixed studio/studioprop model path and
  matching multi-skin assets, change the entity skin0->1->0 and compare the preview
  with matching Hammer++. Explicit/legacy/dynamic declarations are separate
  preserved forms, not newly certified. No asset or original VMF is a write target.

## A5.10 native rotation — pending owner/Hammer check

- [ ] On a **copied** VMF, select one whole raised displacement brush in object
  mode, rotate it a small angle, undo/redo, and Save As a new file. Compare its
  position, shading and UVs in matching Hammer++; confirm later distance editing
  still works. Scaling/shear/flip/sculpt/topology remain unsupported. Untyped
  vector metadata and unsafe numeric motion must refuse, not flatten/drop data.
  No original map is a write target; automation does not check this box.

## A5.11 uniform scale / static bodygroups — pending owner check

The question about when VMFs become editable did not confirm an editing or saving
observation. Basic editable VMFs already exist; preview-only windows still refuse
editing. These checks are for the new candidate, not retrospectively passed:

- [ ] On a **copied** editable VMF, select one whole raised displacement brush,
  scale uniformly by2, undo/redo, and Save As a new VMF. Compare terrain position,
  height and UVs in matching Hammer++. Nonuniform/shear/flip and sculpt/topology
  remain guarded; report a refusal rather than bypassing preservation checks.
- [ ] With matching owned/installed content and a known bodygroup prop, change its
  body value and skin separately. Compare visible parts, selection bounds and
  a relevant entity-browser thumbnail with Hammer++/the game. A blank model choice
  should hide that part. No asset/original VMF is a write target; static preview
  does not certify animation, collision, all models or measured GPU memory.

## A5.12 native ordinary clip — pending owner/Hammer check

The candidate receipt identifies the final portable binary. This new human check
is pending, not inferred from source/model/tool/runtime checks:

- [ ] On a **copy** of an editable VMF, select one loaded ordinary world brush
  (no displacement). Clip with Front or Back, undo/redo and Save As a new VMF.
  Compare the shape, cut material/UVs and remaining brush faces in matching
  Hammer++. No original map is a write target. BOTH, generated/duplicated solids,
  brush entities and opaque/reference cases remain guarded in this first slice;
  report a refusal rather than bypassing it through MAP export.

## A5.13 fresh/duplicated/entity clipping — pending owner/Hammer check

- [ ] In a **new editable** CSS/Momentum map or owned copy, draw and duplicate one
  plain closed brush. Clip one with Front or Back before its first save, undo/redo
  and Save As a new VMF. Compare shape, cut material/UVs and surviving faces in
  matching Hammer++. If convenient repeat within a plain `func_detail` entity.
  No original map is a write target. BOTH, displacements and hidden/opaque/reference
  ambiguity remain guarded; report exact refusals rather than bypassing native
  preservation through MAP export. Test counts/review are not this observation.

## A5.20 ordinary polygon-cache ownership — pending owner retry

- [ ] In the whole A5.20 kit, open a COPY of either original/Hammer++ map. Select
  an ordinary NON-terrain brush, duplicate it and move the copy one grid step.
  Undo/redo, Save As a NEW file and reopen. Duplicate/delete the reopened copy
  and undo. Report exact tool/diagnostic on refusal; never strip native blocks.
- [ ] If convenient, inspect that saved copy in matching Hammer++ for intact
  copies/materials/geometry. This is independent editor observation, not inferred
  from SourceBroom's native reopen or automated test counts.

Pending. BOTHactual128/focus91/model260/App50 are automated evidence, NOT a human
pass. Displacement ownership, cached-member groups/target ancestry, reparenting,
clipping/topology and unknown/malformed payloads remain guarded. Existing A5.19
terrain motion remains available. Keep whole A5.19 as recovery without merging DLLs.

## A5.19 native terrain movement — pending owner retry

- [ ] In the whole A5.19 kit, open a COPY of either map. Select a terrain brush,
  move one grid step, undo/redo, Save As a new file and reopen.
- [ ] If useful, try a proper rotation or positive UNIFORM scale and check terrain
  shape/normals in the editor or matching Hammer tools. State the exact tool and
  diagnostic if it refuses; don't strip native payloads or overwrite originals.

Pending. Both actual terrain Map61/focus83/model252/App50 are automated evidence,
NOT human visual/Hammer/compiler passes. Nonuniform/shear/reflection/topology and
unknown/stale/unmatched payloads remain guarded. Keep whole A5.18 as recovery.

## A5.18 original and Hammer++ edits — pending owner retry

- [ ] In the whole A5.18 kit, open a COPY of the original VMF OR its `_h++`
  counterpart. Move the previously refused brush, undo/redo, Save As a new file
  and reopen; report the exact tool and diagnostic if anything still refuses.
- [ ] Optionally inspect the saved copy in matching Strata/Hammer++ for intact
  brush shape/cached vertices/preview settings. No original is a write target.

Pending, not inferred from automated actual-map54/focus80/model249/App50 passes.
Only same-incidence/winding content changes; mixed caches/mirrors/topology/other
opaque operations/combined displacement-base motion stay guarded. Nonempty LUT/
texlight/backgrounds or future root data still refuse. Keep whole A5.17 recovery.

## A5.17 native point-data brush edits — pending owner retry

Reported negative in A5.16: owner could load a Strata map but a native brush-content
edit refused with an opaque attachment error. Tool/gesture was not specified.
Automated reproduction identified side point_data. This is NOT an A5.17 human pass.

- [ ] Open a COPY of the ORIGINAL VMF in the whole A5.17 kit. Select the brush
  that refused, move one grid step, undo/redo, then Save As a new file and reopen.
- [ ] Confirm the exact previously failing tool now works; if it was face/vertex
  editing or clipping rather than movement, state which action and new diagnostic.
- [ ] If useful, inspect the saved copy in matching Hammer/Strata tools for intact
  brush shape/cache data; no automatic compiler/Hammer interpretation claim.

All pending. Same-incidence/winding movement/rotation/positive-scale/content edits
only; topology/mirrors/point-data clone/delete/group/clip and combined displacement
base motion remain guarded. `_h++` uses vertices_plus and was unsupported in
A5.17; A5.18 separately admits the bounded case above. Never strip blocks/overwrite originals as a workaround. Keep the
whole A5.16 kit as recovery. Automated Map42/focus77/model246 are not human passes.

## A5.16 live native cordons — pending owner/Hammer check

Use a **copy**, retaining the whole A5.15 kit as recovery.

- [ ] In map properties find Live native cordons. Add an inactive modern record,
  edit its bounds/flags/name, add a second box, Apply and undo/redo. Confirm the
  panel shows UNSAVED changes and is readable/useful.
- [ ] Fit from an ordinary selected brush; confirm selection and soft-map bounds
  stay unchanged. Apply without changes should not add history or dirty state.
- [ ] Save As a new VMF and open it in matching Hammer++: intended cordon records,
  boxes and flags retained, unrelated map data unchanged.
- [ ] If available, edit a loaded legacy cordon's bounds/active then undo and
  reopen without conversion. Do not overwrite an original to test refusal.

All are pending. This is native metadata authoring, not viewport clipping or a
promise that a compiler applies a region. Any actual selected SDK/cordon compile
behavior needs its own owner observation. Automated UI/model tests are not these
human/Hammer/appearance passes; A5.15 grouping checks below also remain pending.

## A5.15 ordinary native groups — pending owner/Hammer check

Use a **copy** in the whole A5.15 kit, retaining whole A5.14 as recovery.

- [ ] Group two plain brushes with the existing action: no native name prompt;
  label derives from the group ID. Open it, edit a member, undo/redo, close and
  ungroup.
- [ ] Save As a new VMF and reopen in matching Hammer++: same objects, IDs and
  membership, native entity solid ownership, and no lost repeated/unknown data.
- [ ] If convenient, include a point entity and a nested group; Ungroup dissolves
  only the selected immediate container. Reopen member order is native lexical
  order, not a promised arbitrary selection-created sequence.
- [ ] Check grouped ordinary creation/duplication and Front/Back/Both visually if
  useful. Hidden/visgroup/layer/linked/opaque operations must not become an authoring
  workaround; never write the original just to test a refusal.

All observations above are **pending**. Runtime/model/UI checks and static review
are separate evidence, not a human/Hammer pass or complete Source compatibility.

## A5.14 BOTH clipping / actual preview — pending owner/Hammer check

- [ ] On a copied or fresh plain brush, place a plane through it and choose Both.
  Confirm two closed halves are selected; undo restores the original, redo restores
  the halves. Save As a new VMF and compare shape/cut texture placement in matching
  Hammer++. If convenient repeat in a plain brush entity. A missing/tangent plane
  should not delete an untouched brush. No originals are write targets; automated
  data/identity/preview checks do not substitute for this observation.

Pending is not failed, but neither is it complete acceptance. Once confirmed,
record only what was observed; do not infer measured performance, preservation
or support for other Source branches from a visual check.
