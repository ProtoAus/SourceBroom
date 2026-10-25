# Native Source authoring tools — bounded A5.2 candidate

SourceBroom edits native Source 1 VMF, not a converted Quake MAP. CSS and
Momentum/Strata are the first tested profiles; generic Source paths and external
tools remain configurable. This is an experimental editor candidate, not full
Source branch, Hammer++, shader or performance compatibility.

## Basic native object authoring

New Source maps start from the profile's native VMF starter, not a Quake MAP.
On a new map or owned copy, normal tools can create closed plain brushes and
simple point entities, transform them, duplicate supported plain objects and
remove them. Brush-to-entity creation and whole/partial duplication of plain
brush entities preserve native solid ownership. Undo/redo and Save As remain
available. Opaque/geometry-dependent attachments or native references can make
an operation unsafe; refusal is intentional, not permission to strip that data.

New object and side IDs are allocated at admission, independently, above all
existing IDs in their namespaces, including unprojected groups. Saved IDs do not
change on each write and undo does not recycle an issued ID. Untouched native
text, repeated keys, unknown blocks and metadata remain authoritative. Duplicates
use new IDs, preserve supported template metadata and refuse unsafe opaque data.
Removing a last brush also checks its implicitly removed native owner.

This is not arbitrary topology support. An admitted brush keeps its complete,
unique side-identity set; face splitting, missing/aliased sides and unsupported
clipboard/CSG/clip operations refuse before mutation, with a preserving-save
backstop. Use Save As and independently reopen the copy in matching Hammer++.

## Entity properties and ordered outputs

Select an **existing** native entity and use the Entity inspector. FGD metadata
provides typed property hints, choices and flag labels where definitions exist.
Unknown keys and unknown flag bits are retained. A definition default is a hint,
not permission to insert every missing property into the VMF. FGD input/output
declarations are not fabricated as ordinary keyvalue rows; a genuine stored
keyvalue with the same name is still retained.

For a single selected existing entity, its Source outputs table keeps the native
order and repeated event names. Edit target/input/parameter/delay/fire-count
fields, add or remove ordinary rows, then use normal undo/redo. Special targets
such as `!self` remain literal. Unrecognized or malformed original rows are
preserved rather than silently repaired; opaque connection children can make
editing unavailable. FGD hints do not prove that an entity/input/event exists in
all Source games. Native save guards remain authoritative.

## Static prop preview

The bounded reader accepts supported MDL 44/48/49 layouts with matching
VVD/VTX sidecars, root/default pose and a limited static preview path. Installed
loose/VPK materials and VTF data are read through the configured Source content
paths; nothing from an installed game is bundled with this editor. Unsupported
versions/layouts or missing/mismatched/oversized data use a diagnostic fallback
instead of pretending the model rendered. Check the console.

This does not add skeletal animation, physics/collision authoring, full bodygroup
or shader equivalence, or instance/displacement/overlay authoring. A static pose or
visible mesh is not proof of the game's collision hull or runtime behavior.
Invalid/out-of-range skin/frame numbers fall back safely without rewriting the
original native property; this is not validation of every model's skin table.

## Read-only editor metadata and camera eyes

Save the VMF, then open **Run -> Compile** and choose
**Native metadata / Custom package…**. These tools read a snapshot of the
**saved file**, not unsaved editor changes. Reload explicitly after saving again.

The metadata tab interprets groups, nested visgroups, object membership, hidden
containers/flags, camera eye/look data and cordon bounds/active state. Invalid,
duplicate, dangling or cyclic known references are reported; unknown data stays
opaque. The display is capped, with an explicit notice if not all rows fit.

These fields are **read-only**. Native group IDs are not TrenchBroom scene parent
bindings, so group creation/reparenting and visibility/cordon authoring are not
silently enabled. SourceBroom does not apply cordon clipping itself; unchanged
native metadata is still passed to external tools whose behavior must be tested.

**Go to camera eye** moves only the eye and preserves the current view direction;
it does not import Hammer's whole camera state. Invalid/degenerate cameras or
eyes outside the safe preview-navigation range are not targets. The range is a
UI safety policy, not a Source branch/world-bound assertion.

## Capability preflight before external compilers

Builds assess the **actual owned staged VMF bytes** before starting a compiler.
Native syntax/structure and read budgets can refuse a build. Known preview-only
or unassessed data is disclosed without stripping it from the staged document.
Warnings are not a claim that an external compiler or game supports the feature.
Generic targets never certify arbitrary Source branches.

The shared Source-tools reader bounds a saved document to 128 MiB and one million
entries while parsing, with the existing native nesting guard. Larger documents
are explicitly refused by this tool path, not partially compiled or silently
truncated; use an appropriate external toolchain. Concrete CSS20/Strata25 BSP
header contracts are checked after every successful compiler stage. Neither a
preflight pass nor a header match is whole-map/game acceptance.

## Explicit custom-content distribution ZIP

The package tab writes an ordinary **distribution ZIP**, not automatic BSP
embedding and not an automatic pack of installed assets.

1. Prepare a **dedicated custom-only root** using game-relative directories such
   as `materials/`, `models/` and `sound/`. Do not choose a Steam library or installed
   game. Only include content for which you have redistribution rights.
2. Optionally choose an existing compiled BSP read-only. It is placed at
   `maps/<safe-name>.bsp`; the exact included snapshot must meet its native header
   and selected CSS20/Strata25 contract. No original BSP is modified.
3. Acknowledge the custom-only content and rights requirement. This defaults to
   **off** and is not a licence verification service.
4. Choose **Discover and review exact plan**. Read every included file and every
   excluded/missing/unresolved/error finding. VMF material/model/sound references
   and a bounded set of VMT/model dependencies are discovered. Scripts, runtime
   lookups, some skin/branch-specific data and unknown dependencies need manual
   review. An apparently small plan does not mean nothing else is needed.
5. Separately approve that exact plan, then choose **Write NEW distribution ZIP…**
   with a new filename. Existing files are never overwritten. Changes to the
   root, BSP, rights acknowledgement or saved snapshot invalidate approval.

The reviewed plan holds immutable binary snapshots and CRCs. Writing packages
those reviewed bytes, not a later reread of potentially changed source files.
Traversal, linked/reparse paths, case collisions, unsafe archive names and budget
failures are refused. Rights, review and output checks are not bypass switches.
Installed game/VPK stock content is not automatically resolved or redistributed.

**Discovery is deliberately incomplete.** A successfully written ZIP is not proof
of licence rights, a self-contained map, valid gameplay or compatibility with
another branch. Review exclusions and runtime dependencies, then test the package
in the intended Source game with only its declared prerequisites installed.

## Still outside this candidate

Basic plain-object authoring does not imply arbitrary topology or native
editor-group/layer/instance/displacement/overlay authoring, full Source material
rendering or all-branch performance. Common unsupported operations are gated;
some upstream actions can remain visible, but unsupported content changes must
refuse before mutation and preserving-save remains the final backstop. Never ignore an error or assume an
unsuccessful save changed the original. Use **Save As** and inspect a copy in
Hammer++ before relying on it. Actual owner Hammer++/game observations belong in
`lextest.md`; automated checks do not fill those human gates.
