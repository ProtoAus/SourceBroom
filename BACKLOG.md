# SourceBroom known findings

Later requested feature work belongs in [the roadmap](docs/sourcebroom/ROADMAP.md).
These are observed limitations/follow-ups, not a claim that all have a product fix.

- **Editable scene compatibility — `lib/TbMdlLib/src/VmfScene.cpp`.** Three of
  38 supplied external VMFs still refuse strict editable loading: duplicate IDs,
  invalid UV scale and an incomplete brush. A5.3 explicitly previews all 38 read-only,
  preserving native bytes and reporting unprojected solids without invented
  geometry. This is not a strict-loader fix or repair. All 38 pass lossless checks.
  Preserve diagnostics/input hashes and obtain a minimal licensed/authored
  reproducer before changing strict tolerances. Falsifier: production strict
  `SourceBroomVmfCheck --scene` accepts correct bounds/UVs with no silently dropped
  object; malformed controls still refuse. External names/content/logs stay private.
- **Initial Source content latency — `mdl/GameFileSystem.cpp`,
  `mdl/LoadMaterialCollections.cpp`.** A5.1 removes the redundant initial rescan
  and quadratic browser membership lookup, without dropping catalog entries.
  A5.3 fixes queue starvation, quadratic retirement and redundant browser relayouts;
  catalog enumeration measured about 3.14s instead of 3.89s for 66,741 registrations.
  Initial mounting remained several seconds and was slower in those subject runs
  (about 6.51s vs 5.23s). Whole-startup improvement is not established. These are
  two fresh processes per arm on one host with OS caches unflushed, not a latency
  guarantee or Hammer++ comparison.
  Falsifier for further optimization: same complete catalog/name signature,
  zero metadata-triggered pixel decodes, explicit-reload add/remove freshness,
  and lower measured initial latency. Do not use a stale cache or prune content
  to manufacture an improvement.
- **Source alpha/shader fidelity — `mdl/SourceContent.cpp`, `shader/AlphaFunc.fragsh`.**
  A5.4 fixes shader-mask opacity and prop winding; its basic alpha hints do not
  implement arbitrary shader opacity, combined translucent/cutout blending,
  scalar `$alpha`, proxies or full shader effects. A5.5 resolves bounded scalar
  Patch inheritance; nested/conditional/proxy/deletion and overlapping operations
  remain unsupported rather than guessed. A5.6 adds basic `$nocull` and scoped
  polygon-state restoration, not backface lighting. A5.7 honors explicit finite
  `$alphatestreference` in (0,1] through both demanded loaders/Patch inheritance;
  absent0.5 is an editor fallback, not universal Source-default parity. Zero's
  literal/sentinel meaning remains uncertified and explicit zero refuses.
  Falsifier for expanding support: authored controls against the matching Source shader
  retain raw cutout ordering, actual opacity, editor fade, non-Source draw state
  and deferred loading without inventing unsupported effects.
- **Long-session texture residency — `gl/Resource.h`, `mdl/Map.cpp`.** Source
  catalogs no longer force pixel decoding for metadata. Pixels requested by
  scene references or viewport activation can still accumulate over a long
  session; broad browsing/cache eviction is not measured. Falsifier: an
  explicitly bounded repeated browse/open/close workload with real peak/resident
  measurements, correct release behavior and unchanged texture appearance.
  A successful small-room preview is not that evidence.
- **Alpha operation affordances — `ui/MapWindow.cpp`, `mdl/VmfScene.cpp`.**
  A5.2 enables basic native creation/duplication/deletion/brush-entity ownership
  and gates common unsupported clipboard/CSG/clip/content changes before mutation.
  A5.14 enables ordinary Front/Back/Both clipping for loaded/fresh/template and
  duplicated world/plain brush-entity solids. A5.15 adds ordinary native
  Group/Ungroup capability predicates/actions, guarded loaded projection and
  per-component membership isolation; native rename and opened-group removal stay
  unavailable. A5.16 adds live native cordon preflight/history/UI with strict
  controller/box/stale guards; it does not clip the viewport or certify compiler
  regions. Other CSG/topology and opaque/displacement/reference cases still refuse.
  Some upstream topology/hierarchy tools remain visible.
  Follow-up: complete capability-specific affordances before full A5, not merely save-time
  refusal. Falsifier: each unsupported action is unavailable or explains refusal
  before mutation; supported authoring, transforms, selection and undo still work.
- **Native polygon-cache operation boundary — `mdl/VmfScene.cpp:polygonCacheEdits`.**
  A5.17 fixes indexed point_data content edits; A5.18 separately supports implicit
  vertices_plus and strict observed inert preview roots. A5.19 composes already
  parsed displacement/cache motion for typed proper similarities. A5.20 adds
  validated ordinary NON-displacement clone/delete and safe closed entity-parent
  additions, including generated/reopened continued authoring. Cached-member
  groups/current or target ancestors, reparenting, clipping, mirrors/topology,
  mixed/malformed caches and unsupported displacement transforms/ownership/payloads
  remain guarded. Nonempty LUT/texlight/backgrounds and future root fields
  need their own evidenced contract, not a wider opaque/root whitelist. Falsifier: independent
  accepted/refused indexed/ring controls plus actual source bindings, surgical
  save/reopen/further edit/undo and unchanged original hashes for each new arm.
- **Windows preference regression check — `test/src/tst_QPreferenceStore.cpp`,
  replacement/reload sections.** The A5 full UI run passed 41/42, with this
  untouched test throwing rename `Access is denied` in its task-owned Unicode
  fixture; one narrow rerun repeated it. Later A5.1/A5.3 full runs pass 42/42,
  but A5.3 also observed another full-run failure followed by an isolated pass.
  A5.4 again failed the first full run, then passed alone and full 43/43.
  A5.5 observed full 42/43 and an isolated failure; both were preserved rather
  than retried for green. A5.6's full UI run passed 43/43 with no preference-code
  change. These passes do not establish a fix. Earlier runs observed intermittent
  sharing failures. The host/watcher cause is not conclusively diagnosed and no
  unrelated product/test code was changed to conceal it. Falsifier: a bounded
  same-host control consistently performs both atomic replacements, observes
  the actual reload notifications/values, and preserves missing/malformed-file
  controls without weakening the test or touching owner preferences.
- **Non-Windows identity/packaging — application resources and CMake install.**
  Only Windows has a tested SourceBroom output/package. Check desktop launchers,
  associations, bundle names and runtime dependencies before offering Linux or
  macOS binaries. Falsifier: a clean installed package launches through the OS's
  normal entrypoint with correct identity and resources, not just a build-tree
  executable on a developer machine.
- **Displacement sew rounding — `SourceDisplacement::sewTogether`.** Found by the
  A5.37 review by reading, not observed. (1) A group member whose position differs
  from the computed mean only by rounding (for example Z values 0.1,
  0.15000000000000002, 0.2) still gets an exact-difference offset write, so a mesh
  that did not visibly move can gain an offsets table holding a ~1e-17 entry. This
  predates A5.37. (2) Suspected: a finer-only sample skipped because it lies
  within TransformEpsilon of its target could exceed epsilon against the rebuilt
  coarse curve, refusing a sew that `canSewTogether` admitted. A rotated, inexact
  A5.37 fixture did not show it. Falsifier: a fixture that reproduces either case,
  then a fix that leaves current exact no-op/no-table tests passing.
- **Displacement sew contact refusal is conservative.** A5.37 refuses a selection
  where a boundary touches another selected mesh without being sewn to it and
  either side belongs to a seam group, even when that group would not move.
  Falsifier for loosening it: the same T-junction tests still refuse, and a
  selection whose touching samples provably stay put sews.
- **Rewritten polygon caches — `polygonCacheEdits`/`polygonCacheRows` in VmfScene.cpp.**
  A5.38 rewrites a changed face's point_data/vertices_plus from its new polygon,
  wound like the written plane points and starting at plane point 0 when that point
  is a vertex (the owner's corpus does this on 99.98% of point_data sides). Not
  done: making ring[1..2] equal plane points 1..2, which the corpus also shows; a
  cut side's plane points are the user's clip points and need not be vertices. A
  review suspected Strata may depend on that and could not establish it; 32 corpus
  sides already break it. Rewritten rows also reformat values (`64.00` becomes `64`,
  computed values print 17 digits), leave whitespace-only lines where rows were
  removed and add LF lines to CRLF files. Falsifier: a matching Strata/Hammer++ load
  of an A5.38-clipped copy shows a wrong or rejected shape, or a check of that
  editor's loader shows it reads ring order beyond winding.
- **Vertex edits that change a brush's faces — VmfScene side bindings.** Moving a
  vertex usually splits or merges faces; the new faces have no native side binding,
  so the edit refuses for every brush, cached or not. A5.38 also keeps refusing
  cache rewrites for incidence changes other than a clip or mirror. Falsifier for a
  fix: a vertex edit that adds a face saves new sides with fresh IDs, UVs and (on a
  cached brush) a rewritten cache, and reopens and re-edits like a clip does.
- **Imprecise polygon caches block all geometry edits — `polygonCacheEdits`.** Every
  edit matches each cached row to a plane-derived vertex within 1e-6. Measured on
  the owner's 38 maps (`artifacts/full-source/a538_cache_deviation.py`): 83.05% of
  41,245 non-displacement brushes match exactly, but 6,993 (16.95%) have cached
  vertices 0.0001 to over 1 unit from what their planes imply (2.4% up to 0.01,
  9.7% up to 0.1, 4.2% up to 1, 0.4% beyond). 85 of 153 sampled real-map clip
  refusals were this; 63 of those 85 brushes also refused a plain move and 22
  accepted one, which is not yet explained (find out first). Likely the
  editors keep precise vertices while plane points are rounded. Falsifier for a
  fix: such brushes move, clip and mirror with caches kept consistent (moves
  carry each cached point by its vertex's motion, rewrites follow the planes),
  while ambiguous or far-off (stale) caches still refuse.
