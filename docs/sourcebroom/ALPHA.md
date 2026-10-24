# Try the Windows SourceBroom alpha

This is the first bounded interactive-alpha candidate, not a replacement for
Hammer yet. Native VMF remains authoritative; no Quake MAP conversion is involved.
CSS and Momentum are the first profiles. FTESurf is optional and only one play
target. Generic Source 1 installations can use manual game/compiler paths.

## Open, change, save

1. Extract the complete Windows package and run `SourceBroom.exe` beside its Qt
   DLLs/resources. Keep the whole folder together. No Valve content is bundled.
2. **Work on a copy.** Open an existing `.vmf`, or copy the supplied
   `games/Source-common/starter.vmf` to your own map-work folder first. Do not use
   upstream File > New as an assertion that arbitrary new Source geometry is
   supported at this stage.
3. Choose the matching **Counter-Strike: Source**, **Momentum Mod**, or generic
   **Source 1** profile. Set its game path to the installed mod
   folder containing `gameinfo.txt` (for example `cstrike` or `momentum`), not
   the Steam root or a compiler `bin` folder. Missing textures need the correct
   content mount; the editor does not download them.
4. The supplied starter is a newly authored sealed room with a ramp and spawn.
   Select an existing plain brush and move it a small grid step; undo/redo it.
   Existing plain convex brushes support affine transforms and material/UV
   edits. Existing entity property values can be changed within preserving-save
   restrictions.
5. Use **Save As** to a new `.vmf`. No-op saves retain original bytes. Supported
   edits patch only affected native values. Unsafe topology/opaque-data changes
   or externally changed backing files produce errors rather than a misleading
   successful save. Inspect the error/console; never treat it as a saved map.
6. Open the saved copy in your normal Hammer/Hammer++ branch. Check the ramp,
   texture orientation, entities and bounds before trusting a new map workflow.

## Compile and play

Use the **Run** menu's compile action. Build without play first; configure
an installed external VBSP/VVIS/VRAD toolchain and the correct game directory.
Use the build-and-play action once that works. Tool paths, game/content directory,
output/staging directory and client executable are editable. Review them when
switching presets; source-game content is not implicitly interchangeable with a
compiler branch. No proprietary compiler is included.

[Source build configuration](SOURCE_BUILD.md) explains the fields and safety
contract. The log contains each tool's output. A stage failure stops the chain;
a stale BSP is not accepted as a new successful build. The map is staged under
an owned unique name rather than overwriting your normal BSP.

For standard CSS/Momentum play, choose that game's client and correct branch.
For optional FTESurf testing, choose its executable and runtime gamedir. A VMF
being readable does not mean every game/engine understands every BSP version.
Classic CSS version-20 and Momentum Strata version-25 authored outputs have
both been compiled and loaded in isolated FTESurf clients on the development
machine; that is not a blanket claim for all Source branches or maps.

## Deliberately limited

- Start with **existing plain brush maps**. Some supplied real maps are refused
  for invalid/incompatible geometry or UVs; nothing is silently stripped.
- No new/deleted/duplicated brush topology, arbitrary hierarchy/group changes,
  displacement sculpting, overlay/instance editing, props, or full FGD/I/O
  authoring yet. Some upstream tools remain visible; unsupported changes cause
  preserving-save refusal. Undo the change or reopen your original copy.
- Displacement/opaque data can survive unedited, but that does not make the
  affected geometry freely transformable. Never rely on a thumbnail to establish
  that an opaque object rendered correctly.
- Materials are a **VMT/VTF base-texture preview**, not Source lighting/PBR/shader
  emulation. Missing or unsupported materials may fall back with diagnostics.
- Source pixel resources load on scene/viewport demand instead of decoding an
  entire installed game's catalog. Long-session cache eviction and performance
  across huge maps/libraries remain unverified.
- Windows x64 only has been built/packaged here. Do not infer tested Linux/macOS
  compatibility from upstream's cross-platform support.
- SourceBroom uses a separate settings namespace and disables the upstream
  replacement updater. It does not migrate or alter your TrenchBroom settings.

Please report the operation and diagnostic rather than supplying proprietary
assets or a full private map publicly. [Validation](VALIDATION.md),
[roadmap](ROADMAP.md), [known findings](../../BACKLOG.md) and
[pending human checks](../../lextest.md) separate evidence from unfinished work.
