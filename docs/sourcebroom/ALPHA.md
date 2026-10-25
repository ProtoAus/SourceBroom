# Try the Windows SourceBroom A5.10 native-rotation candidate

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

## A5.10: rotate native terrain without flattening it

Select a whole displacement brush in object mode and use the normal rotation
command. Its base, raised samples and authored vectors now rotate together,
including arbitrary angles/pivots, UV-lock behavior, picking/bounds and undo/redo.
Save As a new file, reopen, then continue moving or distance editing normally.
Scale/shear/flip, face/vertex topology and sculpt/create/power/sew remain refused.
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
  bodygroup and shader parity are not claimed.
- Save, then use **Run -> Compile -> Native metadata / Custom package…**. The
  metadata tab reads the **saved** snapshot: groups, visgroups, hidden state,
  cameras and cordons are read-only. **Go to camera eye** moves only the eye,
  not the full Hammer camera orientation. Reload after saving changes.
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
  CSG/clipping, native editor-group/layer authoring, displacement creation/sculpt/
  power/sew, native scale/shear/flip, overlays and instances remain unsupported. Common unsafe paths refuse before mutation;
  some upstream controls can remain visible and save guards stay authoritative.
- Metadata interpretation is not group/visibility/cordon authoring. SourceBroom
  does not apply cordon clipping itself; unchanged VMF is passed to external tools
  whose behavior must be checked.
- Base texture/static model preview is not full Source shader/game parity.
- All 38 supplied maps pass lossless syntax/no-edit checks; **35/38** construct
  strict editable scenes. **38/38** open with explicit read-only preview, where
  omitted solids remain in the original document but are not scene geometry.
  This is not all-map compatibility or editable repair.
- All **184 model** entries pass after rotation integration; native focus passes
  **4,009 assertions/41 cases**, including real Map/cache/UV/guard controls.
  The review overflow red-to-green passes **364 assertions/2 cases**. Affected
  UI **129/3** and existing five-config GPU **2,539** evidence remain valid.
  Earlier filesystem20/graphics21 remain valid. The prior full UI **42/43**
  Windows preference-file replacement failure is preserved/unfixed; these
  targeted passes are not an all-UI-green or manual/Hammer acceptance claim. [VALIDATION.md](VALIDATION.md) and package
  receipts distinguish source checks, installed reruns and historical evidence.
- The package includes no Valve assets, Source compilers or external user maps.
  Keep prior A5.9/A5.8/A5.7/A5.6/A5.5 and accepted A5.4 recovery packages; do not merge DLL sets.

For additional compatibility feedback, retry one previously refused copied VMF, accept the
read-only preview and confirm the warning/counts and camera/selection behavior.
Then open a familiar map and briefly scroll the material browser: report whether
textures progress and the UI feels less stuck. These observations are not measured
speedup, complete-map rendering or shader parity. The native authoring/Hammer++
checks above also remain pending. Report the game/profile and exact observation,
not a blanket compatibility verdict. No performance advantage
over Hammer++ or all-branch compatibility has been measured.
