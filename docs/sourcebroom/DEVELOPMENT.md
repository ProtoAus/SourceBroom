# SourceBroom development

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
The checker does not invoke compilers, write a VMF, or decompile BSPs.

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
