# SourceBroom

SourceBroom is an early-alpha fork of [TrenchBroom](https://github.com/TrenchBroom/TrenchBroom)
for **native Source 1 VMF editing across Source games**. Counter-Strike: Source
and Momentum Mod are the first game profiles; generic Source 1 paths are editable.
FTESurf is an additional compile/play verification target, not an editor dependency.

**Status:** Windows A5.2 native-authoring follow-up to the A5 candidate:
native VMF opening, starter/new plain-brush and point-entity creation,
supported duplication/deletion and brush-entity ownership, base-texture and
bounded static-prop preview, existing-brush/entity edits,
FGD-backed properties and ordered Source outputs, preserving saves, external
compile/play, capability preflight, read-only native metadata and an explicit
custom-content distribution ZIP. CSS and Momentum are the first tested profiles,
not the only configurable games. Independent Hammer++/owner acceptance remains
pending; full A5, advanced native authoring and a general release are not claimed.
Native authoring keeps stable independent object/side IDs and preserves original
VMF data without a MAP conversion; arbitrary topology and opaque-reference edits
remain gated. The retained usability fixes remove a redundant Source-content
scan and quadratic material-folder lookup, and accept larger finite Source
preview worlds without weakening geometry/save guards. The supplied scene corpus now constructs **35/38** maps;
all **38/38** remain lossless at the document level.

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
