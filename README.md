# SourceBroom

SourceBroom is an early-alpha fork of [TrenchBroom](https://github.com/TrenchBroom/TrenchBroom)
for **native Source 1 VMF editing across Source games**. Counter-Strike: Source
and Momentum Mod are the first game profiles; generic Source 1 paths are editable.
FTESurf is an additional compile/play verification target, not an editor dependency.

**Status:** Windows A5.27 explicit translucent material opacity:
native VMF opening, starter/new plain-brush and point-entity creation,
supported duplication/deletion and brush-entity ownership, base-texture and
bounded static-prop preview, existing-brush/entity edits,
FGD-backed properties and ordered Source outputs, preserving saves, external
compile/play, capability preflight, read-only saved metadata, live native cordons,
manual visgroups/own-record RGB, stored terrain weights and an explicit
custom-content distribution ZIP. CSS and Momentum are the first tested profiles,
not the only configurable games. Independent Hammer++ and detailed authoring
acceptance remain pending; full A5, advanced native authoring and a general
release are not claimed.
Native authoring keeps stable independent object/side IDs and preserves original
VMF data without a MAP conversion; arbitrary topology and opaque-reference edits
remain gated. The retained usability fixes remove a redundant Source-content
scan and quadratic material-folder lookup, and accept larger finite Source
preview worlds without weakening geometry/save guards. A5.3 adds fair texture
queue progress, linear unused-resource cleanup and fewer full-browser relayouts.
Initial content indexing can still take seconds; a whole-startup speedup is not
established.

A5.4 fixes reversed Source prop triangles and separates VMT opacity from VTF
shader-mask alpha for both brushes and props, while retaining editor fades.
Source Material Collections settings now show a game/profile-named entry instead
of every nested folder. The complete catalog and saved partial folder selections
remain intact; choosing the entry explicitly enables/disables the whole mount.
The owner accepted the observed A5.4 visual retry; all-map/model/shader parity
and the separate Hammer/authoring checks are not implied.

A5.5 resolves bounded Source **Patch VMTs** on demand for brush and prop textures,
including scalar inheritance/insert/replace through existing mounted search paths.
No material-wide scan or persistent include cache is added. Unsupported nested,
proxy, deletion or ambiguous operations still refuse; file/chain limits and final
shader/alpha validation remain explicit.

A5.27 previews finite scalar VMT `$alpha`0..1 on supported basic shaders with
explicit `$translucent 1` and no cutout/terrain layers. The multiplier travels
with demanded brush and prop pixels, including inherited Patch values; image
bytes/budgets and native geometry/save permissions stay unchanged. Strict mode,
missing translucency, cutout/terrain combinations, invalid/repeated/block values
and proxies still refuse. Zero really is invisible; this is not the unsupported
zero alpha-test-reference sentinel or terrain blend weight.

Actual seven-target build has no compiler warning/error lines; focused material136,
brush/prop10224 and native packed production GPU2693 assertions pass. Full
Model299/UI59/App50/Render28/GL21 pass. Parent review is not independent/Hammer,
new human or full shader acceptance. Use the whole A5.27 kit on copies, keep
whole A5.26 and owner-confirmed A5.25 as recovery; full Source support is ongoing.

Retained A5.26 adds **Initialize zero alpha rows** for a supported displacement whose
native `alphas` table is entirely absent. This deliberate action enables normal
per-sample editing without moving terrain. Present empty/partial/unknown data
is not repaired. Initialization is a separate undo step from surrounding height
and alpha edits; Save As/reopen and exact history preserve geometry and source.
Displacement clone/topology/paint permissions are not widened. Actual seven-target
build has zero compiler warning/error lines; full Model298/UI59/App50/Render28/
GL21 pass. Prior intermittent preference failures remain unfixed. Parent review
is not independent/Hammer or human acceptance. Use whole A5.26 on copies; keep
the owner-confirmed whole A5.25 kit as recovery. Full Source support is ongoing.

Retained A5.25 fixes **scrambled brush geometry introduced by A5.24**: the cache gained a
terrain-weight component but the upload holder still used the old32-byte vertex.
The holder now uses the cache's full36-byte type, and typed copying makes future
producer/consumer mismatches a compile error. Actual production upload141 and real
packed-VBO GPU2597 checks pass. The owner's unusable A5.24 view is a confirmed
failed observation; **avoid A5.24**, keep A5.23 as recovery, and use copied VMFs.
The owner confirmed A5.25's requested same-map geometry/edges view works, not
other editing, maps/performance or Hammer behavior. Prior draw-count/shader checks
missed this byte-layout boundary. Native data/edit/save guards are unchanged.

Retained A5.24 adds **basic WorldVertexTransition terrain blending**. Mounted VMTs can
supply two VTF layers; native displacement alpha0..255 blends first-to-second
using the existing UVs, independently of image opacity and editor fade. The
selected-face alpha controls update that preview through normal undo/redo.
This is a bounded linear preview, not blend masks/transforms, bump/detail/specular
shading, paint/smoothing or full Source renderer parity. Strict material readers
remain strict and native geometry/save permissions are unchanged. Current full
Model296, GL21, UI59, App50 and Render28 pass; real GPU controls pass2580 assertions
and all seven build targets have zero warnings/errors. The prior intermittent
Windows preference failure is not established fixed. Parent review is not
independent visual/Hammer acceptance; use copies and keep the whole A5.23 kit.

Retained A5.23 adds **stored displacement alpha weights (0..255)** in the selected-face
inspector, independently of distance/height. Complete original rows only: one
explicit Apply supports no-op, native Save As/reopen and exact undo/redo without
changing terrain positions/normals/bounds/picking. Missing alpha does not disable
valid height editing. This is not full Source material blending, paint/smoothing,
missing-row creation or topology. Model294/294 and the inspector pass; full UI58/59
retains the known unchanged Windows preference rename failure. Actual client/
checker/test builds have zero new compiler diagnostics; parent review is not
independent Hammer/owner acceptance. Keep the whole A5.22 kit as recovery.

Retained A5.21/A5.22 add live native manual-visgroup tree/membership/Show/Hide and
explicit own-record RGB, not automatic/wrapper visibility or member object tint.
See [current alpha instructions](docs/sourcebroom/ALPHA.md),
[tool limits](docs/sourcebroom/SOURCE_TOOLS.md) and [pending human checks](lextest.md).
Complete Source support and a general release remain unclaimed.

Retained A5.20 enables **duplication and deletion of ordinary non-terrain brushes with
validated point_data or vertices_plus polygons**, including supported whole/partial
brush entities and safe brush additions to those entities. Copies retain stable
new native IDs and remain movable, duplicable and removable after Save As/reopen.
Both actual map variants passed the acted ownership/history path (128 assertions);
full model260/260, affected91 and App50 pass with zero new compiler warnings.
Unknown/malformed caches, displacement ownership, cached-member groups, reparenting,
clipping and topology remain guarded. Human/Hammer retry pending; keep whole A5.19
as recovery. This is not complete Source support.

Retained A5.19 lets already-parsed **displacement terrain and its native polygon cache**
move together: typed translation, proper rotation and positive uniform scaling.
Both actual map variants passed a production-discovered terrain-brush move/save/
reopen/further-edit/undo/redo path. Existing exact shared-frame, base/mesh, cache,
reference and opacity guards remain; no new mesh schema or tolerance is admitted.
Full model252/252, affected83, actual-map61 assertions and App50 pass with no new
compiler warnings. Human retry pending; keep whole A5.18 as recovery.

Retained A5.18 adds the corresponding **Hammer++ vertices_plus** content-edit path and
strictly admits the observed scalar preview-settings roots without rewriting them.
Both the original Strata map and its native-ID-matched Hammer++ counterpart passed
real Map move/save/reopen/further-edit/undo/redo checks. Polygon caches use exact
validated-block allowances, not a broad opaque whitelist. Full model249/249,
affected80, actual-map54 assertions and App50 pass; production builds have no new
warnings. Mixed caches, mirrors/topology and nonempty LUT/texlight/background
settings remain guarded. Owner/Hammer retry is pending; keep whole A5.17 as recovery.

Retained A5.17 fixes an actual Strata VMF brush-edit refusal caused by native **point_data**
face polygons. Supported movement, rotation, positive scaling, material/UV and
boundary edits keep those cached vertices synchronized without re-exporting VMF.
Only unchanged face/vertex incidence and winding are admitted; unknown/malformed
caches, mirrors, topology changes, and point-data clone/delete/group/clip remain
protected. Hammer++ **vertices_plus** is a different, still-unsupported schema.
The exact reported brush passed real Map move/Save As/reopen/undo/redo; full model
246/246, affected77 and App50 pass with zero new compiler warnings. Corpus load and
metadata-edit checks are NOT arbitrary brush-edit acceptance. Human retry is pending;
keep the whole A5.16 kit as recovery. Full Source support is not claimed.

Retained A5.16 adds a **live native cordon editor** beside map properties. Edit modern
loaded/new records, multiple boxes, names and optional activation; legacy bounds,
activation and removal stay in their original form. The editor shows UNSAVED state,
uses document-scoped handles, supports atomic Apply/Fit/undo/redo and preserves
untouched source text. It does **not** clip the viewport or guarantee external
compiler-region behavior. Preview/unsupported/ambiguous controllers remain guarded.
Full model243, post-helper-rename model24, full App50 and scoped affected UI13 pass;
actual app/checker/UI builds have no new compiler warnings and independent review
found no focused blockers. Owner/Hammer interpretation remains pending. Keep the
whole A5.15 kit as recovery, without merging DLLs.

Retained A5.15 adds native **Group/Ungroup** through the existing actions for supported
loaded, fresh and duplicated ordinary groups. Nested groups, plain world brushes
and whole ordinary entities retain native membership/IDs without changing VMF
lexical ownership. Undo/redo restores exact live nodes, sibling order and selection;
reopening follows deterministic native lexical order, not arbitrary selection order.
Native labels are derived as `Group <id>`; rename, visgroup/layer/hidden/linked
instance authoring remain unavailable. Ambiguous components stay preserved and
uneditable without blocking independent ordinary brushes; true opaque geometry
references remain guarded. Full model219/App50 and affected UI5 pass, actual app/
checker/UI builds have no new compiler warnings, and focused independent review
found no remaining blockers. Owner/Hammer acceptance remains pending. Keep the
whole A5.14 kit as recovery; never merge DLL sets.

The retained A5.14 candidate adds **BOTH-halves native clipping** for visible loaded,
fresh/template and duplicated plain brushes, world-owned or in supported plain
brush entities. Front keeps the original solid; Back is a new same-parent sibling
with stable remapped IDs. One batch admission/command preserves undo/redo, source
attributes and native save/reopen; no-op subjects remain untouched. Actual native
preview cut material/UV parity is now tested and repaired, including first-nearest
normal ties. Front/Back and its narrow retry certificate remain; generic failed
redo/undo/transaction policies do not change. Displacements, other CSG/arbitrary
topology and hidden/opaque/reference ambiguity remain guarded. Portable provenance
belongs to the candidate receipt; owner/Hammer acceptance remains pending.
Keep the whole A5.13 kit as recovery.

It retains A5.11's **positive uniform displacement scaling**, static Source
model **bodygroup selection**, and short safe-save transaction names for valid
Windows filenames near the path budget. Native movement/rotation, UV locks,
distance editing, undo and preserving saves remain available. Body choices agree
across rendering, picking, bounds and caches without duplicating GPU geometry for
every body/skin combination. Keep A5.10 as unmerged recovery.
Unsafe numeric overflow/precision loss refuses before mutation. Nonuniform scale,
shear/flip, sculpt/create/power/sew, blend materials, animation and full Source
support remain incomplete. See roadmap/validation and the package receipt for
source versus portable checks. Native appearance/Hammer acceptance and the known
untouched Windows preference-test failure remain pending/unfixed.

Strict editable loading still constructs **35/38** supplied maps. After explicit
permission, a clearly labelled **read-only compatibility preview** opens all
**38/38**, preserving duplicate IDs and original bytes while counting any solids
it cannot project. It does not repair data or pretend that omitted geometry is
present. Editing, normal save, export and compile are disabled in this mode;
**Save unchanged native copy** creates a new byte-identical file only. All
**38/38** remain lossless at the document level.

- [Try the Windows alpha and understand its limits](docs/sourcebroom/ALPHA.md)
- [Native authoring, properties, outputs, props, metadata and custom packages](docs/sourcebroom/SOURCE_TOOLS.md)
- [Measured validation and pending acceptance](docs/sourcebroom/VALIDATION.md)
- [Ordered alpha roadmap and acceptance gates](docs/sourcebroom/ROADMAP.md)
- [Development, build, and preservation contract](docs/sourcebroom/DEVELOPMENT.md)

VMF will remain the authoritative editable file, not an export through Quake MAP.
Valve assets, compiler executables, and user maps are not bundled. The upstream
GPL-3.0-or-later license and attribution are retained.

---

## Upstream TrenchBroom

[![TrenchBroom Icon](app/TrenchBroom/resources/graphics/images/AppIcon.png)](https://www.youtube.com/watch?v=shcAvnYp9ow)

TrenchBroom is a modern cross-platform level editor for Quake-engine based games.

- Trailer:   https://www.youtube.com/watch?v=shcAvnYp9ow
- Website:   https://github.com/TrenchBroom/TrenchBroom
- Discord:   https://discord.gg/WGf9uve
- Mastodon:  https://mastodon.gamedev.place/@trenchbroom
- Bluesky:   https://bsky.app/profile/trenchbroom.bsky.social
- Video Tutorial Series:  https://www.youtube.com/playlist?list=PLgDKRPte5Y0AZ_K_PZbWbgBAEt5xf74aE
- Manual:    https://trenchbroom.github.io/manual/latest

## Features
* **General**
  - Full support for editing in 3D and in up to three 2D views
  - High performance renderer with support for huge maps
  - Unlimited Undo and Redo
  - Macro-like command repetition
  - Issue browser with automatic quick fixes
  - Point file support
  - Automatic backups
  - .obj file export
  - Free and cross platform
* **Brush Editing**
  - Robust vertex editing with edge and face splitting and manipulating multiple vertices together
  - Clipping tool with two and three points
  - Scaling and shearing tools
  - CSG operations: merge, subtract, intersect
  - UV view for easy texture manipulations
  - Precise texture lock for all brush editing operations
  - Multiple material collections
* **Entity Editing**
  - Entity browser with drag and drop support
  - Support for FGD and DEF files for entity definitions
  - Mod support
  - Entity link visualization
  - Displays 3D models in the editor
  - Smart entity property editors
* **Supported Games**
  - Quake (Standard and Valve 220 file formats)
  - Quake 2
  - Quake 3 (partial, no patches or brush primitives yet)
  - Hexen 2
  - Daikatana
  - Generic (for custom engines)
  - More games can be supported with custom game configurations


## Releases

Binary builds are available from [releases](https://github.com/kduske/TrenchBroom/releases).

## Compiling

Read [BUILD.md](BUILD.md) for instructions.

# Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for more information.

# Credits
- [Qt](https://www.qt.io/)
- [FreeType](https://www.freetype.org/)
- [FreeImage](https://freeimage.sourceforge.io/)
- [TinyXML](http://www.grinninglizard.com/tinyxml/)
- miniz
- [Assimp](https://www.assimp.org/)
- [Catch2](https://github.com/catchorg/Catch2)
- [CMake](https://cmake.org/)
- [Pandoc](https://www.pandoc.org/)
- Quake icons by [Th3 ProphetMan](https://www.deviantart.com/th3-prophetman)
- Hexen 2 icon by [thedoctor45](https://www.deviantart.com/thedoctor45)
- [Source Sans Pro](https://fonts.google.com/specimen/Source+Sans+Pro) font

## Changes

See [releases](https://github.com/TrenchBroom/TrenchBroom/releases) for latest changes.
