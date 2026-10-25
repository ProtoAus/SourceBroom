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
  implement arbitrary `$alphatestreference`, combined translucent/cutout blending,
  scalar `$alpha`, proxies or full shader effects. Basic cutout uses 0.5. Falsifier
  for expanding support: authored controls against the matching Source shader
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
  Some upstream topology/hierarchy tools can remain visible. Follow-up: complete
  capability-specific affordances before the full A5 exit, not merely save-time
  refusal. Falsifier: each unsupported action is unavailable or explains refusal
  before mutation; supported authoring, transforms, selection and undo still work.
- **Windows preference regression check — `test/src/tst_QPreferenceStore.cpp`,
  replacement/reload sections.** The A5 full UI run passed 41/42, with this
  untouched test throwing rename `Access is denied` in its task-owned Unicode
  fixture; one narrow rerun repeated it. Later A5.1/A5.3 full runs pass 42/42,
  but A5.3 also observed another full-run failure followed by an isolated pass.
  A5.4 again failed the first full run, then passed alone and full 43/43.
  These passes do not establish a fix. Earlier runs observed intermittent sharing
  failures. The host/watcher cause is not conclusively diagnosed and no
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
