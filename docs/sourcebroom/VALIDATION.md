# SourceBroom validation — A5.5 bounded Patch-material follow-up

This is a native Source 1 VMF editor fork with configurable game/content/tool
paths. CSS and Momentum/Strata are the first tested profiles; FTESurf is an
optional runtime. Implemented, automated, packaged and manually accepted are
separate states. Full A4/A5 exit and a general release are **not** claimed.

## Current implementation boundary

| Area | Implemented and checked | Not implied |
| --- | --- | --- |
| Native document | Ordered/repeated keys, comments, unknown blocks, numeric text, byte-exact no-edit serialization; surgical token edits and entry/nesting bounds | All VMF branch semantics or independent Hammer acceptance |
| Scene/edit/save | Stable bindings; native starter/new plain brushes and point entities; supported duplication/deletion and brush-entity ownership; strict geometry/UV/side-identity admission; undo/redo and transactional preserving save/Save As | Arbitrary topology, native editor-group/layer/displacement/overlay/instance authoring |
| Compatibility preview | Explicit opt-in immutable partial reader; duplicate IDs/bytes unchanged; omitted-solid counts and labelled plane samples; selection-only commands; create-new unchanged copy | Editable repair, complete geometry, proxy hulls, normal save/export/build or relaxed syntax/resource limits |
| Entity tools | FGD property hints, choices/flags and I/O metadata; ordered/repeated outputs; unknown data retained, definition defaults not blindly inserted | FGD completeness or correctness for every game; malformed data silently repaired |
| Props/materials | Bounded static MDL44/48/49 with matching VVD/VTX, root/default pose, installed loose/VPK material lookup; explicit fallbacks | Animation, physics/bodygroup/shader parity or every model variant |
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
| Current filesystem / graphics / model CTest suites | **20/20 + 21/21 + 173/173 passed** |
| Current normal UI / preference history | **42/43**; unchanged QPreferenceStore Windows rename failure also failed its isolated retry. Unfixed; earlier reviewed A5.4 Fs20/Gl21/Mdl171/Ui43 full255 pass remains historical, not an all-runs-green claim |
| Focused Patch and rendering controls | **974 assertions /5 cases** and retained shared-alpha GPU **394 assertions /1 case** passed |
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

### A5.5 on-demand scalar Patch materials

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
