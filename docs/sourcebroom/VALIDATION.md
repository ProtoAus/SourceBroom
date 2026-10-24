# SourceBroom validation — A5.1 usability follow-up

This is a native Source 1 VMF editor fork with configurable game/content/tool
paths. CSS and Momentum/Strata are the first tested profiles; FTESurf is an
optional runtime. Implemented, automated, packaged and manually accepted are
separate states. Full A4/A5 exit and a general release are **not** claimed.

## Current implementation boundary

| Area | Implemented and checked | Not implied |
| --- | --- | --- |
| Native document | Ordered/repeated keys, comments, unknown blocks, numeric text, byte-exact no-edit serialization; surgical token edits and entry/nesting bounds | All VMF branch semantics or independent Hammer acceptance |
| Scene/edit/save | Stable native bindings, strict geometry/UV checks, bounded existing-brush/entity edits, undo/redo, transactional preserving save/Save As | New/deleted topology, hierarchy, displacement/overlay/instance authoring |
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
| Full filesystem / graphics / model / normal UI CTest suites | **20/20 + 21/21 + 165/165 + 42/42 passed** (248 entries) |
| Preference replacement history | The untouched test passes this full run; earlier A5 full/narrow runs failed with Windows rename `Access is denied`. One pass is not a fix or a claim of consistent host behavior |
| Focused accepted-fix UI controls | **4/4 passed**: property/I/O namespace controls, production build backend and metadata/package tools |
| Lossless supplied corpus | **38/38 passed**, no-edit plus acted in-memory mutation; original SHA-256 unchanged |
| Scene supplied corpus | **35/38 constructed** under the shared +/-131072 preview envelope, versus **32/38** under the explicit +/-65536 control; three ID/geometry/UV refusals remain, not skipped success |
| Native edited ramp/entity/output save | Production native map save/reopen control changes actual ramp bounds, property data and one repeated-output delay; original/copy separation maintained |
| Installed CSS/Momentum pipeline | Production backend VBSP/VVIS/VRAD completes for an authored native-edited fixture; target headers are BSP20/BSP25, inputs unchanged; no game launched by this check |
| Build-tree A5.1 GUI smoke | Actual Momentum prop fixture and two external copied maps opened and closed with exit zero; inputs unchanged. Title readiness is not visual or owner acceptance |
| Historical A5 installed GUI smoke | CSS/Momentum starter fixtures opened; the installed-prop description was overbroad because an unsupported input environment variable was ignored. Build-tree prop evidence is separate; installed A5.1 coverage belongs to its receipt |
| Packaging implementation | Authored custom files and optional short-name synthetic BSP written to new ZIP; independent reader extracts exact bytes; no-overwrite/approval/snapshot/path/budget refusals exercised |

The corpus was rerun through syntax/mutation, old-envelope and current-envelope
arms for A5.1, with all original hashes unchanged. This sample is useful evidence,
not all-map/all-game coverage. External map names, content and raw private logs
are not public fixtures.

### Usability controls and measured limits

- A closed authored box translated far beyond the old preview envelope has the
  expected six faces/eight vertices and exact bounds in the new envelope; its
  preserving save/reopen is byte-identical. The old envelope refuses it. An open
  authored brush still refuses even at +/-1048576; no missing face is repaired.
- The three remaining external refusals are duplicate native IDs, invalid UV
  scale and an incomplete brush. Larger bounds alone do not conceal them.
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

The historical preference replacement failure predates this follow-up; its
implementation/fixture code remains untouched. Although the latest full run
passes, a file watcher/sharing or host cause is not conclusively diagnosed. It
remains recorded rather than being declared fixed or hidden by weakening a test.
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
