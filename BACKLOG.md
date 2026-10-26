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
  vertices_plus and strict observed inert preview roots. Mixed caches, mirrors/
  topology/clone/delete/group/reparent/clip and combined displacement/base-cache
  motion remain guarded. Nonempty LUT/texlight/backgrounds and future root fields
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
