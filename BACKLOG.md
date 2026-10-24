# SourceBroom known findings

Later requested feature work belongs in [the roadmap](docs/sourcebroom/ROADMAP.md).
These are observed limitations/follow-ups, not a claim that all have a product fix.

- **Scene compatibility — `lib/TbMdlLib/src/VmfScene.cpp`.** Six of the 38 supplied
  external VMFs are refused by strict geometry/UV validation; all 38 still pass
  the lossless syntax/no-edit checks. Preserve diagnostics and input hashes;
  obtain a minimal licensed/authored reproducer before changing tolerances.
  Falsifier: production `SourceBroomVmfCheck --scene` must accept the reproducer
  with correct bounds/UVs and no silently dropped object; malformed controls
  must remain refused. External map contents/names/logs stay out of public Git.
- **Long-session texture residency — `gl/Resource.h`, `mdl/Map.cpp`.** Source
  catalogs no longer force pixel decoding for metadata. Pixels requested by
  scene references or viewport activation can still accumulate over a long
  session; broad browsing/cache eviction is not measured. Falsifier: an
  explicitly bounded repeated browse/open/close workload with real peak/resident
  measurements, correct release behavior and unchanged texture appearance.
  A successful small-room preview is not that evidence.
- **Alpha operation affordances — `ui/MapWindow.cpp`, `mdl/VmfScene.cpp`.** Some
  upstream new/topology/hierarchy tools remain visible although native
  preserving-save refuses unsupported edits. Follow-up: disable those actions
  per native capability before A5 rather than letting users discover every
  restriction at save time. Falsifier: unsupported actions are unavailable or
  explain refusal before mutation; supported transforms and undo still work.
- **Non-Windows identity/packaging — application resources and CMake install.**
  Only Windows has a tested SourceBroom output/package. Check desktop launchers,
  associations, bundle names and runtime dependencies before offering Linux or
  macOS binaries. Falsifier: a clean installed package launches through the OS's
  normal entrypoint with correct identity and resources, not just a build-tree
  executable on a developer machine.
