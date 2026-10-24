# Try the Windows SourceBroom A5 tools candidate

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

- Existing plain brush/entity edits are the safe native surface. Arbitrary new
  geometry/entities, deletion, topology, hierarchy, displacements, overlays and
  instances are not supported authoring features. Some upstream controls remain
  visible; a refusing save is not a successful export.
- Metadata interpretation is not group/visibility/cordon authoring. SourceBroom
  does not apply cordon clipping itself; unchanged VMF is passed to external tools
  whose behavior must be checked.
- Base texture/static model preview is not full Source shader/game parity.
- All 38 supplied maps pass lossless syntax/no-edit checks; six are explicitly
  refused by scene geometry/UV validation rather than silently dropped.
- Current regression evidence is **164/164 model tests and 41/42 UI tests**;
  the untouched Windows preference replacement test has an access-denied failure.
  Details and prior A4 evidence are in [VALIDATION.md](VALIDATION.md).
- The package includes no Valve assets, Source compilers or external user maps.
  Keep the older A4 package as recovery; do not merge different package DLL sets.

For the quickest feedback, check one native output edit in Hammer++ and the
appearance of one familiar prop-containing map. Report the game/profile and the
exact observation, not a blanket compatibility verdict. No performance advantage
over Hammer++ or all-branch compatibility has been measured.
