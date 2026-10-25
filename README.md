# SourceBroom

SourceBroom is an early-alpha fork of [TrenchBroom](https://github.com/TrenchBroom/TrenchBroom)
for **native Source 1 VMF editing across Source games**. Counter-Strike: Source
and Momentum Mod are the first game profiles; generic Source 1 paths are editable.
FTESurf is an additional compile/play verification target, not an editor dependency.

**Status:** Windows A5.5 Patch-material follow-up to the A5 candidate:
native VMF opening, starter/new plain-brush and point-entity creation,
supported duplication/deletion and brush-entity ownership, base-texture and
bounded static-prop preview, existing-brush/entity edits,
FGD-backed properties and ordered Source outputs, preserving saves, external
compile/play, capability preflight, read-only native metadata and an explicit
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

The A5.10 candidate adds native displacement **rotation** through the normal whole-
brush command, preserving vector fields, UV policy, undo and native saves. It
retains A5.9 movement/fixed-studio skins and A5.8 mesh/distance/VTF tools; keep A5.9
as recovery. Unsafe numeric overflow/precision loss refuses before world mutation.
Native scale/shear/flip, sculpt/create/power/sew, blend materials and full Source
support remain incomplete. See the roadmap/validation for exact controls and the
retained preference failure; native appearance/Hammer acceptance remains pending.

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
