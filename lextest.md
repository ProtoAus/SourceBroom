# Human acceptance — SourceBroom A4/A5/A5.5 candidates

Only the owner observations explicitly recorded below are confirmed. Do not turn
an automated build/screenshot or a general visual approval into unrelated passes.
Use newly authored maps/copies; never overwrite an original user/corpus VMF.

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

Pending is not failed, but neither is it complete acceptance. Once confirmed,
record only what was observed; do not infer measured performance, preservation
or support for other Source branches from a visual check.
