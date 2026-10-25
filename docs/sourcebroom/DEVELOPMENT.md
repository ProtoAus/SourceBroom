# SourceBroom development

Native A5.2 authoring is exercised through the production model/command path,
not a MAP export. `VmfMap`/`VmfScene` cover fresh and native-template identities,
side-set/ownership admission, duplication/deletion, command history and preserving
save/reopen. The existing authored-room fixture may additionally opt into
`SOURCEBROOM_A52_AUTHORING=1` alongside its explicit `SOURCEBROOM_A4_INPUT` and
create-new `SOURCEBROOM_A4_OUTPUT`; it emits a production-saved room with a fresh
brush/point after duplicate/delete controls for external compiler checks.

A5.3 adds explicit immutable compatibility loading; strict admission remains the
default. Read-only commands/undo default-deny except selection-only operations,
native admissions reject before allocating identities, and serialization rejects
all native deltas. Normal save/export/build are not unchanged-copy operations.
The separately labelled copy uses create-new atomic native saving, does not
rebind history/path, and refuses an existing target. Scene omissions stay outside
the editable node hierarchy; diagnostics are samples, never proxy hulls.

Read [the ordered alpha roadmap](ROADMAP.md) and [alpha limits](ALPHA.md) first.
The existing CMake application target remains `TrenchBroom`; the Windows alpha
output, configuration namespace, and package identity are `SourceBroom`.
A normal upstream build is not evidence of VMF GUI support.

## Build

Use the existing [BUILD.md](../../BUILD.md), CMake dependency declarations, and
Catch2 test infrastructure. Do not create a separate parser implementation for
tests or bypass the production library.

Example for Windows with a suitable Visual Studio toolchain, Qt 6.10 kit,
CMake, Git, and Pandoc already installed (substitute local paths):

```powershell
cmake -S . -B ../build -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_PREFIX_PATH=C:/Qt/6.10.3/msvc2022_64 `
  -DCMAKE_CONFIGURATION_TYPES=Release
cmake --build ../build --config Release --target TrenchBroom SourceBroomVmfCheck TbFsLibTest TbGlLibTest TbMdlLibTest TbUiLibTest --parallel 1
ctest --test-dir ../build/lib/TbMdlLib/test -C Release --output-on-failure
cmake --install ../build --config Release --prefix ../alpha
```

The pinned TinyXML2 dependency fails generation with multiple Visual Studio
configurations under CMake 4.3; selecting one configuration avoids that problem.
Upstream's Ninja/MSVC route is another supported option. The initial local
reconfigure also encountered an already-applied Assimp dependency patch. After
all pinned dependencies were fetched, `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`
allowed reuse without rerunning population/patching. **Do not enable that option
on a fresh tree with missing dependencies.** No dependency source was hand-edited.

Use the build's actual executable location (multi-config builds add `Release`):

```powershell
../build/app/SourceBroomVmfCheck/Release/SourceBroomVmfCheck.exe --probe-edit C:/path/to/maps
```

The checker reads inputs only. It parses every document, verifies unchanged
serialization, reports structure counts, and optionally proves an in-memory
value-token edit took effect without touching any file. `--scene` additionally
checks the production scene bridge and reports explicitly refused geometry.
The default +/-131072 preview envelope matches the GUI. Use, for example,
`--scene --world-bounds 65536` for a smaller-envelope control; the finite positive
extent is bounded by 1048576 and requires a scene mode. It is not an engine limit.
`--read-only-preview` explicitly checks immutable partial projection and reports
`unprojected_solids` plus omission locations/diagnostics. It never promotes the
scene to editable, repairs IDs/UVs, or declares absent geometry present. Strict
`--scene` remains the compatibility control; a partial-preview pass is not a
strict-scene pass. The checker does not invoke compilers, write a VMF, or
decompile BSPs.

After public C++ class/layout changes, verify that affected translation units
actually recompile. One measured MSVC incremental-build failure retained an old
Source connection test object whose tracking log omitted its direct MapFixture,
Map and VmfScene headers, causing three ABI-related crashes. Retiring only that
verified disposable object rebuilt it, restored all three tracked dependencies,
and passed the full model suite. Preserve the failure and inspect tracking or use
a supported clean target rebuild; do not change tests to hide a stale object.

## A5.1 content/startup checks

Normal model tests cover initial loose-material discovery followed by explicit
reload after file addition and removal. Initial Source loading skips only the
redundant rescan of its newly constructed filesystem; explicit reload and other
map formats keep their prior behavior. No cross-document cache is introduced.
Game filesystem mounting supplies a synchronous constructor-only error reporter,
so an initial disk-index traversal failure still has an actionable diagnostic
without a second successful-path scan.

For a read-only installed-content measurement, set `SOURCEBROOM_PROFILE_GAME`
to a game directory containing `gameinfo.txt`, then invoke the hidden Catch2 case
`Source content catalog profiling opt-in` in `TbMdlLibTest.exe`. It reports mount,
reload, catalog and equivalent linear/hashed membership timings, checks identical
ordered selection/name signatures, proves catalog registration did not decode
pixels, then explicitly requests a texture to prove the loader acts. Keep game
files/logs private and report the workload; this is not an end-to-end benchmark
or a substitute for visual acceptance.

## A5.6 two-sided material / polygon-state contracts

Preview-only `$nocull` uses the existing bounded boolean policy after direct or
Patch resolution. Both demanded brush and prop loads carry the Source-only hint
in tagged `SourceEmbeddedDefaults`; Q2/default metadata and VMT cache identity
remain separate. `Material::culling()` is configured policy; `effectiveCulling()`
peeks without requesting loading and derives None only with configured Default.
Explicit overrides win. Do not eagerly inspect the catalog or cache by base-image
identity: two VMTs using the same image may have different draw hints.

A real culling override saves `GL_POLYGON_BIT` before blend/depth attributes and
pops in reverse order. Never guess that prior culling was enabled or GL_BACK.
None/Front/Both are scoped; Default/Back keep existing no-op behavior. Readiness
still gates activation/deactivation. The EmbeddedDefaults variant/API changed;
build all six affected product/test targets and inspect actual dependencies,
not an assumed equal size. The existing opt-in five-config GPU case now includes
160 acted pixel/state culling scenarios alongside the retained alpha/fade arms.

New tests need explicit MockGl disable/blend/depth callbacks, and the base
SyntheticProp has only one skin. Reuse the existing aggregate-budget multi-skin
fixture for same-image metadata isolation rather than assuming skin(1) exists.
Neither fixture correction weakens a product or pixel assertion.

## A5.5 Patch material contracts / measured pitfalls

`loadSourceMaterial` resolves only demanded scalar Patch chains through the
existing mounted filesystem. Keep canonical relative materials VMT paths, cycle
tracking, 1 MiB/file, 4 MiB combined text and 16-include guards, plus final shader/
alpha validation. Byte limits guard explicit reads, not arbitrary `openFile` work.
No global scan/cache, host-relative lookup or guessed missing-file fallback.
Insert adds/overwrites; replace touches existing scalars only. Nested/proxy,
empty/deletion and duplicate/overlapping operations intentionally refuse.

Read exact owned bytes with `Reader::read`: `readString(size)` silently truncates
at NUL and bypasses the parser's explicit rejection. Root and include controls
must keep the valid counterpart. Do not replace the generic Reader contract.
Use a resolver-local ordered index of inherited key positions/occurrence counts;
count/find per operation is quadratic even under text limits. Stable numeric
indices survive vector inserts; the 40k+40k control checks acted override, not a
performance promise. A5.5 did not require a public Material/Texture layout change.

Tests must create fixture parent directories and compare exact before/after
bytes, not LF literals with Windows text-mode files. Capture subprocess output
as bytes/to-file: a default cp1252 reader lost UTF8 preference-failure output.
Preserve native CTest LastTest/LastTestsFailed evidence if capture fails; never
repeat passed suites or keep retrying the unrelated preference flake for green.

## A5.4 rendering / collection contracts

Source VTX indices are already clockwise for the renderer. The authored prop
fixture has explicit outward normals; do not silently change winding based on a
synthetic fixture with unspecified normals. The opt-in `Source installed model
winding opt-in` case accepts `SOURCEBROOM_WINDING_GAME` and
`SOURCEBROOM_WINDING_MODEL` for one bounded read-only installed model. It compares
actual submitted triangle crosses with VVD outward normals, including fixups.
Use exact binary `Reader::read`, not `readString` (which stops at NUL).

`sourceMaterial` separates basic VMT opacity from raw VTF alpha; `sourceBaseTexture`
retains its existing path-only/strict interface. Deferred brush loading and prop
cache hits retain VMT identity. `setAlphaFuncUniforms` sets Source-only opacity
suppression for non-graduated VMTs, case-insensitively, AFTER raw alpha rejection.
Do not disable Source blending globally: that breaks editor transparent-tag fade.

The opt-in `Source alpha preview GPU controls opt-in` UI case uses an owned
compatibility context, all five shared alpha-test program configurations and real material
activation. It checks pixels plus draw blend/depth state for opaque/cutout/real
opacity, editor fade and non-Source controls; forcing mask suppression off is an
acted negative arm. It does not need game assets or owner-window interaction.
ShaderManager passes `shader/name` into its locator, not a bare filename. New
uniform/shader changes must ship together; a compiler-only pass is not GPU proof.
A linked uniform is inactive when its shared function is never used: the first
A5.4 app paint exposed this in material-browser/UV fragments despite Face/Entity
pixel controls passing. Cover every `setAlphaFuncUniforms` caller (Face, Patch,
Entity Model/browser, material browser and UV view), consume the policy in each
fragment, and keep the active-uniform assertion. Do not make setters silently
ignore missing uniforms to conceal an integration error.

Source collection grouping is presentation-only over existing material paths.
Never migrate/expand a saved partial subset on dialog creation. Only explicit
root enable/disable changes the saved choices; normal undo must restore them.
Non-Source folder behavior and read-only-preview command safety remain controls.

## A5.3 texture progress controls

Resource-manager tests prove progress past a busy prefix, cursor survival across
removal/appending, and retirement of an unused on-demand catalog without decoding.
The browser tracks weak texture resources and only invalidates layout when a
tracked size changes; loading/failure/upload/unrelated events still repaint.
Root-mounted VFS lookup keeps original suffix spelling/case and explicit reload
freshness. New optional VMT parameters are enumerated and preview-only.

The existing opt-in catalog profiling case also accepts
`SOURCEBROOM_PROFILE_MATERIAL` for one explicit registered material name. It still
requires one actual decode and unchanged full-catalog signatures, never an empty
or skipped catalog. `SOURCEBROOM_RESOURCE_GC_COUNT` selects the bounded count for
the existing unused-catalog graphics test; its default is small. Timing is
reported, not a portable pass/fail deadline. See [validation](VALIDATION.md) for
both gains and the lack of a whole-startup improvement claim.

## A5 tool checks

Normal `SourceBuild` controls always use an acted synthetic helper; they do not
skip when installed compilers are absent. The separate hidden Catch2 case
`SourceBuild installed toolchain opt-in` requires `SOURCEBROOM_REAL_VMF` (an
absolute owned saved fixture), `SOURCEBROOM_REAL_STAGING` (an absolute owned
staging root), and `SOURCEBROOM_REAL_TARGET` (`css` or `momentum`). Invoke that
exact case in `TbUiLibTest.exe`. It uses the production backend with **no play**,
checks all stages and verifies the original input remains unchanged. Installed
compiler outputs/assets are local evidence, never public fixtures.

The shared saved-tools/compiler reader enforces 128 MiB, one million entries
before insertion, and the native nesting guard. It explicitly refuses excess;
it does not partially parse/compile a document. Metadata remains read-only and
custom dependency discovery remains incomplete. See [SOURCE_TOOLS.md](SOURCE_TOOLS.md)
and [VALIDATION.md](VALIDATION.md) for current controls and the known untouched
Windows preference replacement test failure.

## A1 document architecture

The core lives in `lib/TbMdlLib`, not a second map converter. The diagnostic app
uses that same production code.

- Retain original bytes and token offsets, including comments, BOM, line endings,
  high-bit bytes, and opaque extension data. Do not infer an encoding or reformat.
- Store entries in source order with explicit parent/child IDs. Repeated property
  names remain separate entries.
- Treat values as raw lexical tokens at this stage. Semantic escape decoding,
  geometry parsing, and typed entity values belong to later layers.
- Surgical edits replace an existing property value with exactly one explicitly
  supplied, quoted VMF token. Quotes prevent lexical merging with adjacent tokens
  or comments. Unedited bare tokens remain unchanged. Validate the replacement
  before producing output. A raw token API is deliberately not a user-facing
  text-field API.
- Never retain string_views into a movable source string. Accessors derive views
  from numeric spans when requested; callers must not outlive the document.
- Reject malformed quoting/braces/comments, NUL/binary input, UTF-16, and excessive
  nesting with a source location. Unknown well-formed blocks remain opaque.
- A1 has no file-saving API. Safe Save As/overwrite/backups are an A4 requirement.

The scene bridge keeps the native document and ID mapping alive;
regenerating a VMF solely from the existing Quake scene would lose information.
An edit that changes topology must update known references and invalidate or
block incompatible opaque attachments. Blindly carrying stale metadata through
an arbitrary transform is not a preservation guarantee.

## Pre-registered A1 checks

| Control / subject | Prediction | Falsifier |
|---|---|---|
| Synthetic nested document | Enumerated world/solid/side/connection entries and duplicate order match authored expectations | Missing/mis-parented entries or collapsed duplicate outputs |
| Unedited bytes | Serialization equals input byte-for-byte, including BOM/CRLF/comments/unknown blocks | Any byte difference |
| One selected value-token edit | Exact expected bytes differ only at that token and reparsing exposes the new token | No-op edit, wrong occurrence, dropped bytes, or changed structure |
| Invalid edit | Bad ID, block target, duplicate edit, or extra syntax is rejected | Any output accepted for invalid edit |
| Malformed input | Bad strings/braces/comments/NUL/UTF-16/depth are rejected with location | Accepted malformed input, crash, or hang |
| External corpus | Every original parses; counts show parser activity; no-edit and mutation checks pass; input hashes unchanged | Parse failure, mismatch, invisible mutation, or changed original hash |

A failed corpus input is a compatibility finding, not permission to skip the file
or silently relax grammar. Determine the actual syntax before changing the parser.

## Contribution boundaries

Keep user/third-party VMFs, installed assets, proprietary SDK files, machine paths,
and local logs out of public commits. Add synthetic tests for newly learned
syntax. No promise of whole-VMF semantic validation or target compatibility is
made by the document parser. Record verified outcomes and remaining limits in the
roadmap; do not mark GUI/compiler stages complete based on parser tests.
