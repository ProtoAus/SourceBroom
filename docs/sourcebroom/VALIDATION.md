# SourceBroom validation — A5.38 clip and mirror cached brushes

This is a native Source 1 VMF editor fork with configurable game/content/tool
paths. CSS and Momentum/Strata are the first tested profiles; FTESurf is an
optional runtime. Implemented, automated, packaged and manually accepted are
separate states. Full A4/A5 exit and a general release are **not** claimed.

## A5.38 — clip and mirror Strata/Hammer++ cached brushes

- Red: on A5.37 code a cube cached on every side refused clip, corner clip,
  Both and mirror in both schemas (8 of 8) after an acting move control.
- Green: the same cases plus a centred mirror and a second clip pass, with each
  cache checked independently (polygon, winding, numpts first, start at plane
  point 0), untouched faces byte-identical, save/strict reopen/further edit and
  exact Undo/Redo. Partial and mixed caches refuse atomically, next IDs unchanged.
- Discrimination: a reversed-ring mutant passes reopen and re-edit and fails only
  the winding check (116 assertions); the centred-mirror review defect failed 6
  start checks before its fix.
- Real maps: 634 clips on 31 of 38 owner maps verified after save and reopen;
  originals unchanged. Refusal reasons are recorded in DEVELOPMENT.md.
- Full Model332/App50/GL21/Render28 pass; UI 63/64 only the known
  QPreferenceStore rename (not retried); VMF 117 cases/24132 assertions;
  seven targets build with zero compiler diagnostics. One independent review.

Not claimed: Strata/Hammer++ reopening of a clipped copy, vertex edits that
add faces, or brushes whose caches are imprecise (17% of corpus brushes).

## Retained A5.37 — mixed-power displacement position seams

- Red: against A5.36 production the promoted fixture (two power-2 tiles and a
  power-3 tile) failed only its eligibility assertion (12 assertions, 11 passed),
  after the same-power pair acted and the strict pair API refused the mixed pair.
  The same test case passes after the change.
- Focused core: all 21 SourceDisplacement cases pass (35537 assertions): analytic
  curves (powers 2/3, 2/6, 3/5, both frames and selection orders, native
  save/reopen), 2/3/4/6 three- and four-face corners against independently
  computed positions, protected-data refusals after an acting control, and a late
  world-bound failure that leaves every input unchanged; T-junction refusals, a
  change of finer-only samples alone, and a rotated inexact pair closed within
  1e-9.
- Review: one independent read-only review (predicate, control flow, saved
  data). Its T-junction finding was reproduced first: the unfixed planner
  admitted both layouts and the sew opened cracks of 4.0 and 1.0 units. The
  refusal makes both tests pass. Two calculated rounding quirks are in BACKLOG,
  not fixed.
- Discrimination: a mutant ignoring a second mixed-power edge match fails exactly
  the pair-level ambiguity assertion (77 assertions, 1 failed).
- Native inspector: 410 assertions pass, including a 2/3/4/6 four-tile corner
  through the button (64 + mean(4,8,12,16) = 74; fine-only samples at 69, 71.5,
  72.75 and 71.5), one command, no-op repeat, Undo/Redo, stale intent, Save As and
  strict/read-only-preview reopen.
- Full suites: Model330/UI64/App50/GL21/Render28 pass. The intermittent
  QPreferenceStore Unicode rename failure recorded at A5.36 did not occur in this
  run; it was not fixed or retried. Seven targets build with zero compiler
  diagnostics.
- Boundaries: all source hunks are in the sew planner and executor; reader, rebuild,
  transforms and serializer are untouched, so the A5.31 corpus evidence carries
  without a rerun.

Position sewing only. No re-powering, normal welding, neighbor generation, SDK or
Hammer parity, or human acceptance.

## Retained A5.36 — connected displacement position-seam batches

- Actual tests-only native inspector build0/runtime199:198 controls pass, ONE
  desired connected-three eligibility failure in OLD exactly-two production.
  Pair sewing ACTS/Undo restores bytes before that assertion. The EXACT SAME
  inspector SHA then passes281/1; current whitespace-only-formatted version also
  passes281, including three/four corners, one command, Undo/Redo/idempotence,
  stale eligible selection, Save As strict/preview reopen and read-only refusal.
  Original red/green bytes/logs retained; no compile/stale/silence credit.
- Focused core2888/13 passes: old pair behavior, independent common means and
  selection order, interiors/alpha/distance/topology/base preservation, native
  lexical roundtrip, no-op and original-at-mean member without new offset rows.
  Positive sewing precedes invalid/alias/partial/mixed-power/disconnected/
  non-manifold/tolerance-chain guards.64-mesh strip acts,65 cap refuses; maximum65
  grids sew and a later THIRD world-box failure leaves ALL originals unchanged.
- Initial core2644/2639pass/5fail were OWN new-test errors, not feature failures:
  four table expectations for original8 already at mean8, one nonuniform fixture
  from one-component vec initialization. Corrected expectations/explicitXYZ scale;
  product unchanged. Own C4456 shadow warning renamed, not suppressed; no runtime
  credit before warning correction. LSP caught own vec+= before compiling.
- CURRENT actual full Model322/App50/GL21/Render28 pass; full UI63/64 only the known
  untouched QPreferenceStore Unicode rename AccessDenied. Native inspector281
  passes in that full run. No skip/fix/retry for green. Fresh LastTest records
  classified; stale LastTestsFailed files are not current failures or deleted.
- Seven actual builds0 compiler diagnostics/six held code fingerprints. Complete
  direct parent review, changed-range format/UTF8/diffcheck and explicit LSP are
  not independent/clean-LSP/tidy/human/Hammer. Whole VmfDocument/File/Scene and
  SourceDisplacement read/derived-rebuild/transform+native-serializer suffix, plus
  private object fields, match A531. SOURCE checkout-EOL comparison only, never
  original/asset/evidence normalization. The whole displacement file DOES change.
  Retain corpus document38/strict35same3reasons/preview38, not fresh/re-dated;
  original38SHA, installed authored strict probe and portable checks stay separate.

Connected supported same-power2..64 POSITION seams only, one selected partner per
full base edge. No normal weld, neighbor generation, mixed-power resampling,
sculpt/create/power/topology or SDK/Hammer/compiler certification. No vertex layout
or shader change; retained A535 RGB evidence is not a new GPU claim. Keep whole
A5.35/A5.33/A5.25 recovery; complete native Source work continues.

## Retained A5.35 — bounded static material RGB modulation

- Schema-only actual build0/runtime289:228 acting controls,60 desired RGB parse/
  demand failures and ONE own wrong unknown-shader fallback assumption. Corrected
  control to existing refusal, no admission widening or credit for that failure.
- Parser/loader focused cases pass with literal RGB, byte-scale/color*color2/Patch,
  alpha/raw-buffer/budget/preservation and invalid/strict/unknown/coupled guards.
- With RGB metadata but OLD shared host/stages, actual desktop GPU3847 has3788
  controls pass/59 desired RGB failures at only3 channel assertions. Final SAME
  compiled test SHA3847/1 passes across five configurations +masked terrain;
  raw cutout/opacity/editor fades and ordinary/Q2/raw/null/disabled resets act.
  Five fragment arms use a fixture vertex stage/client arrays; separately, masked
  Face uses synthetic shared-cache Vertex values +REAL production holder/VBO.
  Neither is an actual native mesh/cache instance. No new vertex bytes.
- Full Model315/316 initially caught2 untouched scalar1 base-image checks; product
  now preserves that neutral omission, old test unchanged. At A5.35 delivery,
  Model316/UI64/App50/GL21/Render28 pass with matching actual Passed/zeroFailed.
  Seven builds0 compiler diagnostics,12 held code fingerprints. Shader/host/GPUtest
  unchanged by parser compatibility fix; retain3847 without gratuitous rerun.
- Changed-range format/UTF8/diffcheck, explicit LSP/direct parent review; not
  independent/clean-LSP/tidy/human/Hammer. Native source files remain unchanged;
  A5.31 body/corpus38/35same3reasons/38 proof retained, not rerun/re-dated. Fresh
  original38SHA/installed authored probe/whole-kit checks are separate boundaries.

Normalized static RGB only. Scalar exactly1 is legacy neutral omission, not general
scalar semantics. HDR/proxies/per-pixel tint coupling/complete gamma/shader/branch
parity remain unclaimed/guarded. Raw pixels/alpha/budget/geometry/layout/native-save
permissions unchanged. Keep whole A5.34/A5.33/A5.25 recovery. Source work continues.

## Retained A5.34 — bounded standard terrain blend-modulation masks

- Actual tests-only parser build0/runtime26:23 acting plain/two-layer/strict/refusal
  controls pass,3 desired mask admissions fail. No compile/input-error credit.
- Mask path/schema/storage/loading with OLD helper/Face stages builds0. Actual
  Windows/desktop GL has3587 assertions:3459 pass,128 failures solely the desired
  remapped RGB/fade checks. Normal Catch summary, not inactive uniforms or abort.
- Final helper/Face wiring passes the SAME test SHA,3587/1: uniform/spatial masks,
  R/G remap, B/alpha independence, zero-width stability, fade, following ordinary/
  two-only/disabled paths, unit1/unit2 binding and active/client cleanup. Five
  existing fragment configurations still act. These are SYNTHETIC shared cache
  Vertex values +actual production holder/VBO/Face stages, NOT an actual native
  mesh/cache instance; older "native GPU" shorthand is clarified on that basis.
  Separate native CPU/history/cache and BrushRenderer upload141 are not this arm.
- Literal three-VTF demand/Patch/budget controls act, including exact12-byte
  allowance and0/3/4/7/8/11 refusals, missing/broken masks, original fixture bytes
  unchanged. Lifecycle covers3 uploads/deletes, move/readiness/no-op and flat/data
  guards. Strict/base-only APIs, bad/duplicate/block paths, transforms, proxies,
  masked blending and enabled self-illumination stay refused.
- Full Model315/UI64/App50/GL21/Render28 pass, seven actual target builds0 compiler
  diagnostics; eleven code fingerprints held. Current LastTest has matching Passed
  records and ZERO Failed. CTest's old LastTestsFailed files survive success and
  were classified stale by timestamp, not counted as current failures or removed.
  Earlier preference245 rename failure remains historical, not fixed/suppressed.
- Complete direct parent review, changed-range format/diff-check and LSP inspected;
  not independent/clean-LSP/tidy/all-layout/human/Hammer. Own input/discovery/Qt PATH/
  guessed-target/CRLF-fixture/console-encoding failures remain recorded, not credit.
- Native VMF source files remain unchanged A5.33; its A5.31 reader/preserving-writer
  source-body proof and corpus38/35+same3 reasons/38 stay RETAINED, not rerun/re-dated.
  Existing checkout CRLF is not raw Git LF; no original/evidence bytes normalized.
  Fresh installed-loader/owned-probe/whole-kit checks are separate delivery gates.

No full shader/branch/DX/compiler/gamma, performance or new owner acceptance. A
zero-width interval chooses lower exactly at threshold; it is not a native-DX bit
parity claim. No vertex layout, map geometry, stored weight or authoring authority
change. Full Source support continues; whole A5.33/A5.32/A5.25 remains recovery.

## Retained A5.33 — same-power single-edge position seams

- Tests-only actual inspector build0 and runtime missing-button RED after120
  acting legacy/native base movement/height controls. No compile-failure credit.
- Model177/2 passes: same/reversed unique edge, absent offset initialization,
  unchanged interiors/distance/alpha/baseUV/triangles, lexical save/reopen, no-op,
  ambiguous/disconnected/partial/aliased/self/power refusal, maximum65x65 and
  atomic later bounds failure. Planner admission is not SDK neighbor/topology proof.
- Actual inspector170/1 includes one-command edge averaging, idempotence, Undo/
  Redo, stale selection before queued refresh, Save As/reopen/repeat save and
  actual readonly UI/backend refusal. New button starts disabled until valid
  selection notification/deferred refresh; tests connect before selection.
- Full Model314/App50 pass on eight held compiled fingerprints. Earlier UI64
  passed; latest full UI63/64 only has untouched known QPreferenceStore245 Unicode
  rename AccessDenied. No skip/suppression/fix or new isolated claim; seam170 still
  passes in that actual full run. Current targets build without compiler diagnostics. Own test-shadow warning renamed,
  not suppressed; failure/default-state/setup logs retained. Changed-range format,
  diff-check/direct parent review are not independent/clean-LSP/tidy/human/Hammer.
- Reader and serializer function bodies match A5.31 exactly; retain corpus
  document38/strict35+same3 reasons/preview38, not fresh runs. New batch seam
  writes use its existing offsets and structural emitter; loader proof is separate.
  A527 renderer/GPU results remain retained boundaries, not new equivalence.

No smooth-normal weld, neighbor generation, general sculpt/power/topology editing,
performance/compiler or full Source claim. Owner check pending in lextest.md.

## Retained A5.32 — read-only capture into a live camera draft

- Tests-only Root UI build0 and missing-capture runtime RED after35 acting native
  camera/history/view/input assertions; no compile failure or test toggle.
- Native-camera UI175/3 and full UI64/App50 pass. Actual owned-window focus routing
  is simulated through Qt's signal; active3D captures eye+absolute64-unit look,
  active2D refuses. Capture fills the draft only; map count/redo/native bytes and
  view stay unchanged until explicit Apply acts; Undo restores. Invalid/no-provider and guarded modes deny
  fields/writes; capture cancels older pressed write intent. No FOV/roll import.
- Nine UI-only compiled fingerprints, zero compiler diagnostics, changed-range
  format/diff-check and direct parent review; no independent/clean-LSP/tidy or
  human/all-layout/Hammer claim. The known preference rename passed THIS full run,
  not a fix; retained failures stay open. Core/reader/serializer/shader/flags are
  unchanged: retain A531 Model312 and corpus38/35+same3 reasons/38, not fresh runs.
- Whole portable/native smoke and clean installed-loader evidence are separate
  delivery records. No original data or old profiles/kits are overwritten.

Full Source remains incomplete; see pending small owner check in lextest.md.

## Retained A5.31 — independent per-vertex XYZ offsets

- Tests-only actual inspector compiles0/no diagnostics and runtime RED follows64
  acting legacy distance/alpha/init/history assertions: missing offset Apply.
  Separate alias regressions show unsafe typed alpha/offset authority. No missing
  C++ API/compile failure, test toggle or suppression counts as the falsifier.
- Final offset Model449 across3 cases passes: one-vector lexical surgery, exact
  parsed no-op/revert, immutable original, derived geometry/normals/bounds/picking,
  independent distance/alpha/baseUV/triangles, invalid atomic rollback, row provenance,
  explicit absent-only initialization, full65x65 budget and protected empty/partial/
  aliased/malformed data. Alpha alias regression is included among six related
  model cases totaling1153 assertions. Independent admitted base motion composes.
- Production history includes absent and original complete tables: edit/init undo/
  redo, collation boundary, Save As/reopen/repeat save and later base movement.
  No-op/refusal retains redo; no/plain selection and read-only preview refuse.
  Both newly initialized alpha and offset tables share ONE parent append; original
  input and unknown tokens/IDs remain unchanged. No SDK/Hammer claim from self-read.
- Actual inspector116 includes partial protection, independent field drafts,
  frozen pressed XYZ, stale row/backend and selection refresh before queued widget
  refresh, cross-button releases, initialization and undo/redo. It does not stand
  in for an owner visual check or all-layout/read-only-UI matrix.
- Full Model312/App50 pass. UI63/64 has only untouched known QPreferenceStore Windows
  Unicode-fixture rename (existing379 site). Prior A530 isolation remains retained;
  no fresh isolated pass/fail, skip/suppression or64/64 claim. Five targets build0
  with zero compiler diagnostics on nine fingerprints; changed-range format,
  diff-check and complete direct parent review are not independent/clean-LSP/tidy.
- Fresh current production checker corpus: document38/strict35+same3 refusal set
  AND reasons/preview38; acting generic value-edit probes38/35/38. All38 original
  hashes unchanged. Clean installed binary/loader proof is recorded separately at
  delivery; no external offset authoring, original writes or new GPU equivalence.

Own combined-table overlap was fixed in the displacement structural seam, not by
weakening the preserving emitter. Own missing direct UI test include was fixed,
not counted as runtime RED. Full Source, arbitrary-deformation compiler validity,
sculpt/sew/new/power/normal edits and matching Hammer/human remain unproven.
Keep whole A5.30/A5.29/A5.25 recovery; see lextest.md.

## Retained A5.30 — scoped LIVE camera records and active-camera persistence

- Actual13-file protected read-only prototype builds0, zero diagnostics; source/
  fingerprints retained. Runtime scene8/9, history6/7 and RootUI6/7 expose the
  missing writer/panel after existing entity edit/undo and camera-reader controls
  act. No compile failure, test toggle or raw-index bypass counts as RED.
- Final new Model262/10 and LIVE RootUI136/3 assertions pass: pose/record CRUD,
  absent/-1/selected activation, delete-before-active remapping, explicit clear,
  pure creation/deletion/recreation, exact lexical no-op/refusal/undo, staged
  non-consumption, stale/foreign/duplicate/reordered/bad-later requests, scope-preserving
  move/replay, generated entity ID independence and camera/cordon/root composition.
  Production history Save As/reopen/repeat save retains original input; refused/
  no-op calls preserve redo. Preview/version/non-native authoring remains denied.
- Actual UI freezes pressed scope/row AND field intent and pending-button identity.
  Stale row/backend refresh and cross-button release cannot retarget a request;
  invalid drafts/no-op do not write. Eye/heading of the actual perspective view
  remain unchanged during metadata authoring. Saved Tools remains saved/read-only.
- Current full Model309/App50 pass. Regular offscreen UI is63/64, NOT64/64: untouched
  known QPreferenceStore Windows Unicode-fixture rename fails, also isolated.
  No test skip/suppression or unrelated repair. All new camera UI cases pass.
  Five actual build targets have zero compiler warning/error lines on17 compiled
  fingerprints. Direct parent review is not independent or clean-LSP/tidy proof.
- Fresh current production-checker, system-only PATH: document38/strict35+same3
  refusal set AND reasons/preview38; actual generic value-edit probes38/35/38.
  Original38 SHA unchanged. This proves the changed Scene constructor/serializer
  boundary, not every external camera mutation, Hammer or shader equivalence.
- Own by-value-vector-accessor compile error was fixed, not counted as runtime RED.
  Link OOM was environmental commit pressure; only proven current-host auxiliary
  ownership was stopped, peers/game untouched and flags unchanged. cp1252 capture/
  console exceptions were corrected at the bytes/ASCII-summary seam; raw actual
  CTest failure logs retained. No fabricated suite pass or preference fix.

No capture-current-view, viewport motion, projection/FOV or arbitrary unknown camera
fields; no full Source/branch/Hammer/human/all-layout acceptance. Renderer/GPU are
retained A5.27 boundaries. Whole-kit/native smoke and pending owner checks remain
separate evidence; see lextest.md.

## Retained A5.29 — direct saved tools without compile or autosave authority

- Actual tests-only Tools115/116 reaches real rights/discovery/approval before
  failing the absent target selector. Corrected Root18/22 has four missing-action
  failures across saved editable/readonly and new native/non-native documents.
  The first new-native fixture lacked the required profile starter; adding an
  owned binary starter fixes the fixture, not the product. Failed logs remain.
- First implementation Tools132 passes; Root53/54 exposes the genuine exact-path
  readonly filter omission. Whitelist ONLY Source Tools, not Run/Compile/Launch.
  Final native Qt QAction -> SourceTools -> root camera75 assertions pass: saved
  eye and view act, repeat entry reuses the dialog, readonly build commands stay
  denied, new/non-native guard works even on direct controller calls. Unsaved
  live serialization/modification count and original binary file stay unchanged.
- Target changes clear plan approval, invalid target blocks discovery, existing
  Generic/CSS20/Strata25 validators remain and global build settings stay unchanged.
  Metadata reading does not need compiler/launch or rights consent.
- Final product/UI build has zero compiler warning/error lines. The own shadowed
  test variable was renamed without suppression. Full regular offscreen UI61/61
  passes on eight compiled code fingerprints and complete direct parent review.
  Initial Catch discovery without Qt PATH and empty-hunk formatter range errors
  were infrastructure, not feature RED. Omitting the prior offscreen UI contract
  caused the existing shown-visgroup test to lack native shader resources; the
  supported invocation was recovered and restored. This was NOT established as
  intermittent preference IO; old failures remain, no unrelated product repair.
- No new metadata/activecamera/native-authoring, geometry, rendering/performance,
  full-Hammer or all-layout acceptance. Prior App50 (A5.28), Model299/Render28/GL21/
  GPU2693 (A5.27) and native38/35+same3/38 controls (A5.26) remain retained, not new
  or re-dated measurements. Existing preference rename issue is not proven fixed.

Whole-kit native smoke, exact source/artifact receipt and small owner direct-menu
check are separate boundaries; see lextest.md. Full Source support is not complete.

## Retained A5.28 — readonly saved camera view navigation

- Actual proto/default-only Tools78/80: old eye/control forwarding acts; new
  buttons absent. Concrete MapWindow19/22: actual eye position moves but two
  requested headings remain unchanged. The third failure is a fixture oracle,
  not product corruption: text-mode TestEnvironment creates CRLF while expected
  source was LF. Binary native fixture fixes only that mismatch; logs retained.
- After wiring, Tools118 and actual MapWindow26 assertions pass. Known eye+look
  direction is normalized in double before float conversion. Safe oblique,
  vertical, close-at-large-eye and tiny valid targets act; invalid/out-of-range/
  missing/degenerate/nonfinite targets do not emit. Legacy eye-only stays separate.
- Concrete camera checks cover opposite/vertical heading, existing world-up
  fallback, pending position-animation replacement, zoom preservation, legacy
  calls and unchanged map modification count/native serialization/file bytes.
  Qt dialog forwarding and root API are separate actual arms, not a claimed
  single clicked-through GUI/whole-Hammer camera-state acceptance.
- Two actual product/UI consumers build with zero compiler warning/error lines.
  Full UI60/60 and App50/50 pass on18 matching code fingerprints. UTF8 changed-range
  format/diff checks and complete direct parent review are not independent review,
  clean clang-tidy or new human/Hammer acceptance. Preference issue remains open.
- This writes no camera metadata, active-camera field or native model/history.
  Projection/FOV are not imported. Compile/read-only/source budgets remain as
  before. No new native-authoring, rendering/performance or all-map claim.

A5.27 Model299/Render28/GL21/GPU2693 and A5.26 VMF38/35+same3/38 remain retained
unchanged-boundary evidence, not new/re-dated A5.28 runs. Current portable proof
and optional owner view check are separate; full Source support is not complete.

## Retained A5.27 — bounded scalar opacity in demanded material rendering

- Before semantics, only defaulted schema fields/tests: actual Model64/81,17
  expected scalar-admission failures with ordinary translucency/raw-image controls
  acting. Desktop synthetic shared-Vertex/production-holder/VBO GPU2661/2693: exactly32
  new red/blue failures for ignored0/.25; factor1 and prior controls pass. This is
  not client-array/stub proof. First test API copy error and offscreen-plugin GL
  creation failure are retained infrastructure attempts, NOT product RED.
- After wiring, material136, brush/prop10224 (flags, Patch, skin, no-cull, reference
  and opacity permutations), and shared-Vertex desktop GPU2693 assertions pass. Pixels
  exercise0/.25/1 with alpha255/128; blend/depth-mask restoration and null/plain
  following-state reset act. Existing five fragment programs and real packed
  production Face path remain covered; not full native prop vertex-stage proof.
- All13 C++/shader fingerprints match tested source. Seven-target actual build
  has zero compiler warning/error lines. Full Model299/UI59/App50/Render28/GL21
  pass. Prior intermittent Windows preference failure remains unfixed. UTF8
  changed-range format/diff checks and complete direct parent review are not
  independent review, clean clang-tidy or human/Hammer acceptance.
- Explicit translucent basic preview only; strict/nontranslucent/cutout/terrain/
  malformed/repeated/block/proxy policies remain. Scalar0 is not cutoff sentinel0
  or terrain blend weight. Raw decoded bytes and image budgets remain unchanged;
  no catalog decode, native geometry/save authority or performance claim.

Standalone upload141 and A5.26 VMF38/35+same3/38 original-preservation evidence
remain unchanged-boundary controls, not new/re-dated A5.27 measurements. The current
checker relink differs in bytes; no identical-binary assertion. Current portable
installation and new owner view/Hammer observations are separate real boundaries.
Full Source support remains in progress; pending human checks are in lextest.md.

## Retained A5.26 — explicit table creation without geometry widening

- Actual partial wiring: Core80 passes; Map54/55 and UI45/47 fail at explicit
  initialization while distance controls act. Passing the existing structural
  sink advances Map72/73 and UI61/62. The UI exposes same-node command collation;
  the Map clone-positive oracle incorrectly assumed existing displacement clone
  support. Source/documented clone guards were inspected and kept unchanged.
- A stored default-true collation option lets only initialization opt out in
  both directions. Normal scalar edits still collate. The corrected clone arm
  proves a plain native clone acts and displacement clone refuses before/after.
  No native identity/geometry/opaque/reference authority was relaxed.
- Final actual focused **Core80 / Map124 / UI64 assertions** pass. A wrong Core
  regex previously selected no tests and was caught; only the corrected exact
  title with --no-tests=error counts. Failed green-named logs are not green proof.
- Actual seven-target product/checker/test build has zero compiler warning/error
  lines. Full **Model298/298, UI59/59, App50/50, Render28/28, GL21/21** pass on
  the same15-code-file fingerprints. Prior intermittent preference rename failure
  remains unfixed/not erased. Full direct parent review and changed-range UTF8
  format/diff checks are not independent review or a clean clang-tidy claim.
- Absent power2..6 tables initialize; complete/present empty/partial/unknown data
  does not. Source bytes, fractional weight, Save As/reopen, separate undo/redo,
  original inputs, positions/base positions/normals/indices/bounds/picking and
  subsequent native translation have acted controls. Read-only, ordinary/multiple
  faces and stale selection stay guarded. No painting/sculpt/topology/duplication.

A5.25 upload141/GPU2597 remain valid unchanged-boundary evidence, not new A5.26
runs. Installed package/corpus/portable checks are separate real-boundary records.
The owner confirmed A5.25's same-map view; A5.26 controls and Hammer semantics
remain pending in ../../lextest.md. Full Source support is not complete.

## Retained A5.25 — actual packed vertex upload

Owner observation: A5.24 scattered vertices and the map was not usable. Root
cause is verified at the production boundary: cached vertices36bytes, holder/
copy/GL stride32bytes. Prior shader/client-array controls were insufficient.

Actual pre-fix runs show the size mismatch, corrupt uploaded attributes and wrong
packed-VBO pixels. Both then suffered a separate test teardown contract, so their
abnormal exit is preserved, not presented as a clean counted RED. After correcting
only test cleanup and sharing the complete cache type/typed production copy,
**141 production-upload assertions** and **2597 real-GPU assertions** pass with
exit0, including actual position/normal/UV/weight bytes and four geometry/blend
pixels. Four affected product/checker/test build targets have zero warning/error
lines. Full affected Render28/28 and UI59/59 pass; the old intermittent preference
issue is not proven fixed. Model296/GL21/App50 are retained unchanged-source
coverage, not newly measured A5.25 runs. Native source/geometry/edit/save guards
remain unchanged. A hash-verified
copy of the supplied map loads strictly; the original and copy are unchanged.

The owner confirmed the requested A5.25 same-map geometry/edges view in
../../lextest.md. That observation does not confirm other editing, performance,
maps or Hammer behavior. A5.24 stays a failed candidate, not a fallback.

## Retained A5.24 — bounded two-layer terrain preview (known broken upload)

- Actual compiled PARTIAL wiring contains the parser/dual demand budget/private
  child lifecycle/VBO/production binding helper and tests; Face host/stages are
  still old. **2545 GPU controls pass**, including old five-shader alpha/culling
  checks and the real Face vertex/primary pixel, before one missing new uniform
  yields **two nested contract failures**, observed exit42. A missing test include
  and the original expected-exit1 wrapper mismatch are preserved separately, not
  called product RED. No uniform guard or failure policy was weakened.
- After wiring the real Face host and both stages, the same native-context GPU
  case passes **2580 assertions**. First/intermediate/second RGB, raw child alpha17
  not becoming terrain weight/opacity, separate editor fade, following plain
  reset, saved unit1 binding, active server unit and client-unit0 cleanup act.
- Parser22/demand25 controls verify explicit opt-in, default/base-only rejection,
  safe complete second paths, Patch inheritance, both image bytes, one remaining
  retained-byte budget and unchanged VMT input. Child76 controls verify single
  image parity, loaded/unnested ownership, ready/upload/drop and metadata guards.
  Native cache/history117 verifies ordinary0 and terrain0/128/255 plus scalar
  update/undo/redo without geometry or preserving-save permission changes.
- Actual final seven-target client/checker/test build has **zero warning/error
  lines**. Full **Model296/296, GL21/21, UI59/59, App50/50, Render28/28** pass on
  the formatted20-code-file cut. This current UI pass does NOT establish that
  the known intermittent Windows preference rename failure is fixed.
- Complete20-file direct parent review/source fingerprints and explicit-UTF8
  changed-range formatting identify the cut. Existing LSP modernize hints remain;
  no independent-review or clean clang-tidy claim. Installed package/corpus receipts
  are separate real-boundary evidence, not inferred from source tests.

Only the brush demand path opts into basic WorldVertexTransition. Shared UV linear
blend is not modulation/independent transforms/proxies, bump/detail/specular,
lightmap/model parity, paint/smoothing, row creation or topology. Default strict
material modes and all native geometry/save guards remain. Human appearance and
matching-Hammer checks remain pending in ../../lextest.md; Source support is not
complete.

## Retained A5.23 — stored alpha without geometry or permission widening

- Actual compiled PARTIAL-wiring RED has API, owning row bindings, local setter
  and Map route present; the existing emitter refusal/missing UI remain. Model
  **180 assertions /176 controls pass /4 desired failures**, UI **24/23/1**. Old
  distance and local alpha controls act. This is not an all-code-withheld/stub RED.
- After the bounded emitter/UI changes, focused **2Model/1UI** pass. Actual final
  four-target client/checker/test build has zero new compiler diagnostics; full
  **Model294/294** passes. Full **UI58/59**: ONLY the unchanged Windows
  QPreferenceStore Unicode-fixture rename AccessDenied fails. The current log
  was inspected; this known prior failure is preserved/unfixed, not waived or
  repeatedly rerun to claim59/59. The new inspector case passes.
- Nine source files match explicit-UTF8 changed-range formatting; final source/
  four-executable hashes and complete **669-line parent review** identify the
  cut. No independent alpha review claim. Actual-source clang DB refreshed from
  **889 TUs/30 Release CL logs** after bounding compiler concurrency. Core has
  modernize hints, not C++ errors; this is not a clean clang-tidy/self-test claim.
- A fractional alpha changes ONLY its bound original numeric token. Stable
  Save As/reopen, no-op, exact undo/redo and untouched original bytes act through
  Core and Map. Positions/base positions/normals/indices/bounds and raised ray
  picking remain unchanged; a subsequent supported translation still works.
- Missing/partial rows refuse alpha creation while acted distance editing remains
  available. Same-value foreign alpha row text, invalid coordinates/range,
  ordinary/multiple-face selection and stale queued UI indices remain guarded.
  Actual Qt Apply, no extra no-op history, undo/redo and65-to-5 selection refresh
  pass. Existing frame/corner/elevation/vector/cache/world/opaque/topology checks
  are not relaxed. No material/shader blending or paint/smoothing implementation.

App50/Render28 remain retained A5.21 evidence, not new alpha results. Installed
corpus/portable receipts are refreshed only at the real new-reader boundary;
source checks alone do not claim installation. Owner control/weight/Hammer checks
in ../../lextest.md remain pending. Full A6/Source support is not complete.

## Retained A5.22 — own-record RGB without implicit defaults

- API/tests compiled BEFORE product wiring, zero new compiler diagnostics. Actual
  ModelRGB **169 assertions / 123 controls pass / 46 desired failures** and actual
  UI **6 / 5 / 1** retained: existing rename/save/undo acted before desired RGB.
- First wired3Model+1UI cases pass. A5.22 full **Model292/292**, **UI59/59** and
  actual client/checker/test4target build pass with0new compiler diagnostics.
  A5.21 **App50/Render28** are retained history, not rerun/current RGB claims.
- Tested6-file source/executable fingerprints and complete721-line parent review
  delta identify the cut. This was direct parent review, not independent review.
  All6 sources match clang-format with explicit UTF8; the earlier mixed cp1252
  preview changed no source. Fresh LSP has no C++ errors; prior tidy/self-test
  limitations are not a clean-LSP claim.
- Controls exercise parsed noncanonical no-op, known/absent/new-draft color,
  own/moved-root/retained-child emission, stable save/reopen and exact undo/redo.
  Malformed/repeated scalar color blocks ONLY RGB while acted rename preserves
  raw bytes and an independent sibling accepts RGB. Invalid channels, foreign/
  deleted target and forged expected state refuse before publish.
- Actual UI verifies current RGB, separate explicit Set, name edits not applying
  draft values, missing field not silently becoming white, one-command history,
  Save As/reopen and press-time stale-row refusal. No member editor.color,
  visibility, geometry ownership, automatic classification or policy widening.

Portable/installed corpus receipts are separate from source validation; refresh
at that real boundary. Original corpus remains read-only. Owner/Hammer checks in
../../lextest.md are pending; RGB authoring is not object tint/full Source support.

### Existing plain-brush affine clarification — no product change

A separate VmfMap section on unchanged A5.22 production code runs actual
scaleSelection(2,1.5,.5), shear transformSelection and X flipSelection, each with
UV lock off/on. All six arms act changed geometry, independently verify plane
covariance and locked texel mapping, preserve native bindings, Save As/reopen/
stable bytes, exact undo/redo and original input. Actual Model target build0new
compiler warnings and the existing VmfMap case pass. Only tests/docs changed;
no new alpha, transform implementation or repeated full-suite claim. Global
nonuniform/shear/flip wording is narrowed to the displacement/cache/opaque/prop
restrictions it actually describes. This is not arbitrary topology/world-bound
permission or independent Hammer/visual acceptance.

## Retained A5.21 — manual visgroups, native visibility and replay

Current final post-review production:

- Full **Model289/289**, **UI58/58**, **App50/50**, **Render28/28** pass. Existing
  product/checker/API targets build with zero new MSVC diagnostics. Source and
  executable hashes identify the tested cut; later installation is a separate gate.
- Four serial model/projection/Map/UI leases were individually parent-reviewed.
  A fresh whole-family code review blocked on two P1s and one P2. Actual compiled
  pre-fix controls reproduced them: early-child UI reparent9 assertions/7controls/
  2fail; Both stale metadata replay64/46/18; ordered-reference save10/8/2. Parent
  fixed the request order, operational+semantic replay preflight, and reference
  emission order. Focused independent post-fix review found no remaining blocker.
- Source0002/canonical2 and numeric-duplicate rows resolve by registered scoped
  handles, not numeric aliases. Actual selection assignment, live observations,
  protected duplicates and raw original ID spelling are covered. Press/release
  document/row/button changes refuse, hidden-member metadata recovery is explicit,
  and the actual MapWindow's scroll/bounded-name controls act at normal bounds.
- Native wrapper/manual/automatic and inherited causes remain separate from
  ordinary session visibility. Actual session Hide/save/Undo updates the UI and
  preserves source/native revision. Three obsolete legacy save-throw expectations
  were corrected ONLY to exact-byte persistence; native-wrapper and operational
  clip/opaque/provenance negatives remain. No default failure-policy waiver.
- Terminal removal, parked activation, stale/foreign handles, semantic replay,
  true no-op, unknown/protected branch locality, native selected/showHidden vetoes,
  tree append order, membership8,7 roundtrip and exact full undo have acting controls.
- Both original point_data and Hammer++ vertices_plus cached ownership chains
  pass **128 assertions** on current production (move/clone/further move/reclone/
  delete/save/reopen/undo/redo). All original input hashes remain unchanged.
- Current read-only corpus: **38/38 document**, **35/38 strict scene** (identical
  three refusals), **38/38 explicitly partial preview**. The in-memory surgical
  value-edit control acts; all38 original SHA256 values remain unchanged. This is
  load/metadata evidence, NOT universal geometry/Hammer/branch acceptance.

Historical intermediate failures remain evidence, not final passes: the first
whole Model286/287 exposed two obsolete session-save generator expectations;
UI source gates included composed runs and one known untouched QPreferenceStore
Windows rename AccessDenied failure. The current full58 pass does NOT establish
that preference bug is fixed. Clangd parses current ASTs without source errors,
but its feature self-tests exit nonzero; this is not a clean-LSP claim. Test-only
parked-Back lifetime and hidden-ancestor layout oracle mistakes were corrected,
not converted into product RED or policy exceptions.

RGB/wrapper/automatic flag authoring, automatic classification and reconstructed
Hammer-checkbox semantics remain unsupported. Protected/opaque/cache geometry
guards are not widened. Owner/Hammer appearance, real-map authoring feel and
independent compiler behavior remain pending in ../../lextest.md. Source/build,
commit/push, installed receipt and manual observation must be recorded separately.

## Retained A5.20 — closed ordinary polygon-cache ownership

Ordinary NON-displacement point_data/vertices_plus duplication/removal and safe
whole/partial brush-entity/addition paths use forced unchanged cache validation,
canonical source-owned side prototypes and exact operation-local block allowances.
Generated content/save and the entity-parent publication scan consume the SAME
closed-owner allowance; clipping=true retains default opacity. No global whitelist,
header, parser/schema/tolerance, displacement ownership or ID-policy change.

- Desired compiled synthetic RED210/202 controls/eight ownership failures and
  BOTH actual RED49/47 controls/two ownership refusals retained before wiring.
- Final focus **91/91**, ONE fullmodel **260/260**, final BOTH actual **128
  assertions** pass: production native-ID binding, acted move/clone/move/reclone/
  delete/repeated save/reopen/further ownership/undo/redo/exact source restore.
  Both original hashes unchanged; writes owned, no installed-original edits.
- Synthetic both-schema/no-cache and already-edited controls cover full/partial
  two-brush entities, child removal/owner clone/delete, fresh entity-parent adds,
  actual save/reopen/second edit/removal/history/cache counts and stable IDs.
  Invalid later batch retains nodes/selection/history/count/dirty/bytes; nextID12
  and reserved unprojected IDs600/700 -> objects601..603/sides701..713 are checked.
- Unchanged malformed/mixed/misplaced/unknown caches; foreign/stolen/missing/
  duplicate generated side provenance; reference/root policy; hidden native-ID
  routes; current/destination group ancestry; displacement and actual clipping/
  registerClips remain refused. Plain uncached group/registered-clip controls ACT.
- Parent independently read the whole final production/test diff and matched all
  three frozen hashes. Intermediate findings (destination-group ancestors, no-op
  before Brush copy, all current prototypes covered/no live state on archived
  originals) are resolved with regressions; no focused blockers remain.
- Actual app/checker/App build0/zero new compiler warnings, App **50/50**. Rebuilt
  production corpus document38/strict35/preview38 retains EXACT three strict
  refusals/all38 original hashes. This is load/metadata, not arbitrary ownership
  or complete-map acceptance. No UI-source changes or fresh full-UI claim.

LSP has modernization hints and inherited later-test warnings, not a blanket clean
claim; actual compiler is the zero-new-warning gate. Human editor/Hammer/compiler
acceptance remains pending. Cached group/reparent/clip/topology and displacement
ownership remain guarded. Final receipt records installed provenance separately.

## Retained A5.19 — parsed terrain and validated polygon-cache composition

Production change is ONLY relocating existing immutable-before parsed displacement
side collection before the polygon preguard and passing that same set instead of
null. Later exact shared proper-similarity frame, base/mesh agreement, every plane,
structure and editsFrom validation are unchanged. No raw/unparsed mesh allowance,
new schema/tolerance/policy, matrix inference or other-operation relaxation.

- Compilable synthetic desired positive RED619/613 acted controls/six preguard
  failures; actual installed-input desired RED13/11 controls/two failures covered
  direct admission and real Map motion. All evidence retained before wiring.
- Early BOTH actual terrain-input GREEN **61 assertions**: production discovery
  requires parsed displacement AND native polygon, same native ID resolves H++;
  move/save/reopen/second ACTED move/undo/redo/exact source restore. No ordinary
  brush ID or equal-entry assumption. Original hashes unchanged, writes test-owned.
- Final affected **83/83** and ONE fullmodel **252/252** after final source, actual
  compiler0/zero new warnings. Separate/no-cache and both cache-schema controls
  cover typed translation/proper rotation/positive uniform scale, renderer position/
  normal, exact mesh frame and raw emitted normals/offsets/offset_normals rows,
  polygon motion, topology/alphas retained and acted reopened continuation.
- Tiny base-only/mesh-only pose, stale cache, unknown/raw/misplaced mesh, extra
  opaque/ref/root data, inconsistent exact multi-displacement frames, nonuniform/
  shear/reflection/unclassified transforms/topology remain refused. Invalid Map
  batches preserve independent entity/brush/selection/native IDs/history/dirty/
  counts/bytes. Only previously unsupported legitimate coupled positives changed.
- Parent independently read final three-file diff and checked frozen raw hashes:
  no focused blocker. Actual app/checker/App build0/newwarnings0 and App50 pass.
  Production load/metadata corpus38/35/38, same three strict refusals/all38 original
  hashes unchanged. These metadata checks are NOT arbitrary brush acceptance.

Test-only pointer/setter compile failure, stale-binary focus and two test-oracle
faults are preserved and NOT runtime RED evidence. Corrected cache-relative fault
injection and local brush lookup did not weaken production checks. LSP hints and
inherited later-test warnings remain; actual MSVC is the zero-new-warning gate.
Human terrain/visual/Hammer/compiler acceptance pending. Source/build evidence is
not a deployed artifact; final candidate receipt records clean installed provenance.

## Retained A5.18 — Hammer++ cache and inert-root admission

Thin schema-specific row parsing now supports implicit-order `vertices_plus` and
indexed `point_data` using one per-brush correspondence. Only EXACT validated
native block IDs enter the per-content opaque allowance; duplicate/mixed/malformed
blocks cannot borrow a side's approval. Default other-operation/ref/displacement
checks remain. No global transform guess, cache deletion or loader-authority change.

- Compilable early desired H++ positive RED32:31 acted controls including the
  original positive, one opaque-side failure. Side-only wiring then produced the
  expected ROOT-RED32/31 controls/one unknown-root refusal, both retained.
- Approved strict root wiring produced early BOTH-positive46 assertions; final
  actual Map opt-in **54 assertions** passes for original and native-ID-resolved
  H++ counterpart, including Save As/reopen/second ACTED edit/undo/redo/exact source
  restore. Both original input SHA-256 hashes unchanged, writes test-owned only.
- Final focused **80/80**, ONE fullmodel **249/249** after final code. Synthetic
  cache/no-cache translate/rotate/positive-scale/boundary/UV/material, surgical
  spelling, actual reopened edit and exact restore; malformed/nonfinite/noncyclic/
  duplicate/misplaced/mixed cache, cardinality, reference, mirror and other-operation
  refusals. Point-data behavior retained across shared tests.
- Safe-root/no-cache motion acts. Unknown/future/nested/duplicate/malformed fields,
  scalar known-root aliases, scalar+block root duplicates, color00/name00 slot
  aliases, nonempty LUT/texlight/backgrounds refuse. Map later-invalid batches keep
  brush, second entity, selection, native ID, history, dirty count and bytes.
- Roots admitted only by exact scalar schema: integer RGB0..255; nonnegative
  int32 sample/count-shaped values (ambiguous bounced/supersample/cubemap NOT claimed
  booleans); explicit toggles0/1; finite other scalars; paired LUT slots with EMPTY
  names; EMPTY texlight and EMPTY bgimages. All original root text stays raw.
- Parent independently reviewed final three source files and raw hashes; no focused
  blockers. Actual app/checker/App targets build0/zero new warnings and App50 passes.
  Production load/metadata corpus38/35/38 retains exact same three strict refusals
  and all38 input hashes. This corpus is NOT arbitrary brush-edit evidence.

An early test-only compile failure/stale-binary42-assertion run is NOT RED evidence;
fixed compilable desired subject and two runtime REDs above are the valid gates.
No UI-source change/full-UI claim; LSP observations remain hints/inherited warnings,
not a blanket clean claim. Same1e-6/cache-incidence/winding limits remain. Mixed
caches, mirrors/topology/clone/remove/group/reparent/clip, combined displacement/base
cache motion and unobserved root/resource shapes remain guarded. Owner/Hammer/
visual/compiler retry and full Source support pending. Final receipt records the
installed source/artifact/runtime hashes separately.

## Retained A5.17 — strict indexed face-polygon content edits

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
| Scene/edit/save | Stable bindings; native starter/new plain brushes and point entities; supported duplication/deletion and brush-entity ownership; bounded ordinary groups and Front/Back/Both clips; strict geometry/UV/side-identity admission; undo/redo and transactional preserving save/Save As | Other CSG/arbitrary topology, layers, displacement ownership, overlays or instances |
| Compatibility preview | Explicit opt-in immutable partial reader; duplicate IDs/bytes unchanged; omitted-solid counts and labelled plane samples; selection-only commands; create-new unchanged copy | Editable repair, complete geometry, proxy hulls, normal save/export/build or relaxed syntax/resource limits |
| Entity tools | FGD property hints, choices/flags and I/O metadata; ordered/repeated outputs; unknown data retained, definition defaults not blindly inserted | FGD completeness or correctness for every game; malformed data silently repaired |
| Props/materials | Bounded static MDL44/48/49 with matching VVD/VTX, root/default pose, installed loose/VPK materials; Source fixed-studio entity skins and static body selection; explicit fallbacks | Animation, physics/full shader parity or every model variant |
| Native terrain | Bounded native meshes/picking; existing distance and stored alpha samples; admitted whole-base translation/rotation/positive uniform scale, including validated coupled cache motion; native history/save | Missing-row creation, paint/smoothing, topology/power/sew, nonuniform/shear/flip, full material blending or independent native acceptance |
| Native metadata | Saved-file read-only inspection/camera eye; live bounded cordons and manual visgroup tree/membership/Show/Hide/own-record RGB; known-reference diagnostics | Automatic/wrapper flag authoring, automatic group predicates, arbitrary metadata or compiler clipping behavior |
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
| Current A5.23 product/checker/model/UI build | Four actual Release targets, zero new compiler diagnostics; source/executable fingerprinted cut |
| Current A5.23 full model / normal UI suites | **294/294 + 58/59**; new alpha inspector passes, known unchanged QPreferenceStore Windows rename failure preserved |
| Retained earlier filesystem / graphics / model / UI suites | **20/20 + 21/21 + 175/175 + 43/43 passed** (259 historical entries), not rerun A5.23 results |
| Retained A5.21 App / renderer suites | **50/50 + 28/28**, not new alpha results |
| Preference history | Prior full42/43 and isolated Windows rename failures remain preserved/unfixed; A5.23 full UI58/59 contains that same known failure. No preference-code fix or all-runs-green claim |
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
