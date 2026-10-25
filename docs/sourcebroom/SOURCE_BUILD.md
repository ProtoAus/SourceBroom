# Source build / launch seam (A5 tools candidate)

This is an external compiler seam, not by itself evidence that A4 is complete.
Native save/application integration and automated compile/runtime checks have
been exercised; independent Hammer reopening and owner visual/in-game acceptance
remain pending. See [current validation](VALIDATION.md). No compiler or game
asset is bundled.

## Read-only compatibility previews are not build inputs

A5.3 can explicitly open an immutable, possibly partial compatibility preview
after strict editable loading refuses. That document's normal save, export,
compile, rerun and launch actions are disabled. **Save unchanged native copy**
is not a repair or a successful preserving edit; it only writes original loaded
bytes to a new filename. Repair in a suitable source editor and reopen normally
before using this integrated build workflow. The standalone backend still
preflights its actual input bytes; previewing a file does not certify it.

## Integration API

`ui/SourceBuildDialog.h` exports:

```cpp
QDialog* showSourceBuildDialog(
  QWidget* parent, const QString& savedVmf, bool initialPlay = false);
```

Call after the native document has successfully saved to a VMF, using the saved
path. Unsaved edits are **not** exported or compiled. `initialPlay` chooses the
default button; neither showing the dialog nor selecting a preset starts a build.
The dialog owns its backend and uses delete-on-close. Build and Build & Play
buttons require a deliberate click. It cannot close while a compiler is running.
`MapWindow` routes native VMF compilation here after a successful save; it does
not send the native document through the upstream Quake MAP export runner.

`ui/SourceBuild.h` also exports a standalone asynchronous `SourceBuild` QObject:

- `start(savedVmf, settings, play)` validates, stages, and starts the pipeline.
- `output(QString)` streams both compiler channels and backend diagnostics.
- `finished(bool success, QString bspPath)` reports success only after all three
  compilers exit normally with code zero and each successful stage leaves a
  complete minimum Source BSP header matching the explicitly selected target.
  Generic header acceptance does not certify a particular branch. On any
  failure the signal has an empty path; a failed runtime launch logs the retained
  staged BSP path for manual recovery.
- Calling start on a running backend returns false without disturbing that run.
  Other immediate failures emit finished synchronously; connect signals first.
- Keep the backend alive until finished. Its child QProcess belongs to the build,
  never to an existing game. The default per-tool timeout is five minutes.

## Settings and profiles

Momentum and Counter-Strike: Source are primary Windows presets. FTESurf is an
optional runtime preset, with Momentum compiler/content defaults. All executable,
content, staging, runtime cwd, and runtime maps paths are user-editable with browse
buttons. Extra compiler args and launch args are **one argument per line**; no
shell quoting, shell expansion, or combined command strings are used. Compiler
cwd is the owned stage directory; absolute executable/input/game paths are used.
Tools++ CSS lighting defaults to `-cpu`: the tested GPU startup probe stalled on
one local driver. Users may change it explicitly. Other compiler args default empty.

Presets are conveniences for known Windows installation layouts, not Steam game
or tool discovery. Missing paths fail before compiling. The dialog stores the
last chosen settings only in the editor's QSettings `SourceBroom/SourceBuild`
group, never in a game config. Selecting a preset replaces the current form;
starting a build persists the form. It does not change existing upstream
CompilationProfiles.cfg files.

The existing CompilationRunner pattern (Qt child process, sequential tasks,
streamed output, stop on failure) is followed. Direct reuse of its Map/camera
context and CompilationExportMap is intentionally avoided: that context exports
Quake maps and cannot safely stage an authoritative saved native VMF.

## Filesystem safety and runtime contract

Each start exclusively creates a new `sourcebroom_<uuid>` directory below the
chosen staging root. The input VMF is copied there and never passed directly to a
compiler. A fresh run cannot see an old BSP; freshness does not rely on timestamp
resolution. Compilers receive the staged VMF/BSP and `-game` content directory.
No output is copied to a runtime until VBSP, VVIS, and VRAD all succeed. Each step
checks a staged BSP's 1036-byte minimum Source header and `VBSP` magic, plus
BSP20 for CSS or BSP25 for Strata when that target is selected. Generic checks
do not infer a version contract. None of these is whole-BSP semantic validation
or a promise of branch/runtime compatibility.
Relative instance/include dependencies are not copied to staging; this first seam
is checked for self-contained brush fixtures, not A6 instance compilation.

Before the first child process, capability preflight parses the **actual staged
VMF snapshot** through the bounded saved-document reader (128 MiB, one million
entries, native nesting guard). Native syntax/structure or budget failures stop
the pipeline; preview-only/unassessed features remain intact with diagnostics.
Warnings do not certify compiler/game support. The generic profile never claims
all Source branches are compatible.

The dialog can load a bounded native leak file for display and navigate to a
finite compiler diagnostic coordinate. Compiler entity/brush indices are not
VMF IDs. Navigation is not proof of a repaired leak or selection of a native
object. **Native metadata / Custom package…** opens the separate saved-file
[metadata/package tools](SOURCE_TOOLS.md); opening them does not compile or launch.

Staged VMF/BSP/compiler artifacts and `build.log` are retained. Compile output
may include copyrighted content packed by the installed compiler. These local
artifacts must **not** be committed or redistributed as public fixtures.

Build & Play copies the successful BSP to the selected runtime maps directory
under the same unique name. `QFile::copy` refuses an existing destination; there
is no overwrite fallback. Files are never automatically deleted while a game
might be using them. Users may remove owned stage/runtime files manually later.
The runtime is detached and is never killed by this seam. Successful process
creation means **launch requested**, not that the map loaded or plays correctly.

Launch arguments substitute `{map}` (unique basename, no `.bsp`) and `{game}`
(compiler content directory) within individual argument-array elements:

- Source defaults: `-game`, `{game}`, `+map`, `{map}`. Choose the game's executable,
  runtime cwd, and maps directory for the same content/branch being compiled.
- FTESurf: `+cfg_save_auto`, `0`, `+map`, `{map}`; cwd is its install root and maps
  destination is its `ftesurf/maps`. The config guard precedes map loading.
  FTESurf can be paired with the CSS compiler preset by retaining the CSS tools
  and game directory and changing only runtime settings/args. Actual authored
  CSS v20 and Momentum v25 outputs have loaded in isolated FTESurf clients;
  that is not a guarantee for arbitrary branch output or maps.

Owner permission is required for runtime testing: engines may read/write normal
runtime data even with the FTESurf auto-config guard. Parent integration used
owned isolated rigs with no owner config/player data and did not modify canonical
installs. Owner should still inspect the unique map in the selected engine, ramp
joins/collision, materials, and compiler logs; no owner maps or game configs need
replacement.

## Initial independent backend evidence (before parent integration)

- Focused CMake/Catch2 lane build compiled the actual backend and dialog against
  Qt 6; Catch2 ran the production backend against a synthetic compiler executable.
  Paths/spaces/argument arrays, success, repeat-run freshness, nonzero exit after
  writing output, missing/invalid BSP, stale sibling BSP, timeout, untouched VMF,
  no runtime promotion on failures at each compiler step, and missing paths are
  covered (190 assertions in one sectioned/generated Catch2 case).
- `test/fixture/sourcebuild/sealed-ramp.vmf` is newly authored public synthetic
  geometry: six sealing slabs, one slope, spawn entities, and a light. Only this
  textual authored fixture is tracked, not generated BSPs or asset data.
- The production backend ran installed Tools++ VBSP/VVIS/VRAD with CSS content on
  that control and a separately authored changed-slope subject (ramp high vertex
  128 to 160). All three compiler exits were zero on each final run. A separate
  Momentum preset control also passed all three compilers. The inspected CSS
  outputs were BSP v20; Momentum output was BSP v25. Source VMF hashes were
  unchanged. These are compiler acceptance checks, **not editor edit/save tests**.
- Two bounded negative real-compiler observations correctly stopped the pipeline:
  an initial all-nodraw control produced VVIS exit 1, and default Tools++ VRAD GPU
  probing timed out. Neither was promoted. The drawable fixture and `-cpu` default
  are the final successful CSS control/subject configuration.
- Installed CSS content emitted missing optional language-VPK and GameData-key
  warnings. They were not suppressed or interpreted as full content validation.
- Subsequent A4 parent integration built the full application and passed all 40
  TbUiLib CTest entries. A production native-authored ramp edit was compiled
  with both CSS and Momentum toolchains and all three control/edited BSPs loaded
  in isolated FTESurf clients. Native-edited compile/load is now evidenced, not
  just the separately authored backend subject. Independent Hammer++ and owner
  feel/collision acceptance remain pending; no full A4 completion is claimed.
- The A5 production backend adds acted synthetic controls for preflight refusal
  before child launch, real helper-call markers, per-stage target checks, leak
  bounds and diagnostic navigation. Its installed-toolchain opt-in test runs
  the same backend with an explicit owned VMF/staging root and **never play**.
  Current totals, real CSS/Momentum checks and the unrelated Windows preference
  watcher test failure are recorded in [VALIDATION.md](VALIDATION.md).
