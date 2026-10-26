# Native Source authoring tools — bounded alpha candidates

SourceBroom edits native Source 1 VMF, not a converted Quake MAP. CSS and
Momentum/Strata are the first tested profiles; generic Source paths and external
tools remain configurable. This is an experimental editor candidate, not full
Source branch, Hammer++, shader or performance compatibility.

## Read-only compatibility preview

Normal editable loading remains strict. A native load refusal can offer an
explicit **Open read-only preview** choice, with Cancel as the default. This
mode preserves duplicate IDs and the original document, but failed solids are
reported and **not projected**. It does not invent UVs, repair a hull, replace an
all-failed brush entity with a point, or claim that the displayed scene is complete.
Syntax, structural and resource bounds still apply.

The title/status/viewport persistently identify the mode and omission count.
Labelled orange points are only source-plane samples; unlocated omissions are
counted and at most 128 markers draw. Camera movement and selection/undo/redo
are allowed; native authoring, entity drops, normal save/overwrite, export and
compile/play are not. The saved-file build/metadata/package workflow below is
for normally editable documents, not an escape from preview restrictions.

**Save unchanged native copy…** is a separate new-file operation. It copies the
loaded original bytes, refuses existing destinations or any native delta, and
does not rebind the preview to the copy. Repair in a matching source editor and
reopen through the normal loader before attempting edits.

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

## Material progress and base-image limits

For native Source maps, Material Collections settings group the configured root
and gameinfo-mounted content under the active **game/profile name**. This is a
browsing entry, not one entry per nested folder or a new physical mount manager.
All registered material names/paths remain available. Existing saved partial
folder choices are unchanged by opening settings; a partial mount can be listed
on both sides. Explicitly enabling the entry selects the full mount, disabling
selects none, and undo restores the previous subset. Read-only previews cannot
change these saved choices. Non-Source folder selection remains unchanged.

VMT opacity is separate from a VTF's alpha-channel flag, which may describe a
shader mask. The basic preview uses explicit `$translucent`/`$alphatest` hints;
opaque mask alpha is not blended as transparency. Raw cutout rejection happens
before surviving pixels become opaque, and editor whole-brush fading remains
independent. Normal graduated opacity and non-Source material behavior remain.
This is not arbitrary `$alphatestreference`, dual-mode blending, scalar `$alpha`
or full shader equivalence; basic cutout uses the existing 0.5 threshold.

Source catalogs remain complete and demand-loaded. Texture processing resumes
across frames rather than repeatedly servicing a busy prefix; unused catalog
resources compact in one pass. Browser resource notifications repaint without a
full relayout unless a tracked texture's dimensions changed. This avoids several
real stalls, but large first-time content indexing still takes seconds.

The base-image preview accepts additional enumerated optional roughness/AO,
tint/color and tree-sway parameters without resolving those extra textures or
reproducing their effects. Strict material inspection still rejects those
unsupported features. Unknown shaders/parameters, dynamic base selection and
unsupported proxy blocks are not blanket-accepted. A visible base image is not
Source lighting, blending, animation or full shader support.

A5.5 brush/prop image loading resolves a bounded Source **Patch VMT** through
configured mounted paths. Scalar insert adds/overwrites and replace changes only
existing keys; final supported-shader/base-image/alpha policy still applies.
Includes must stay in relative `materials/... .vmt` paths, with 1 MiB/file,
4 MiB combined text and 16-include limits. Resolution is on demand, without a global include scan/cache
or material-file writes. Nested/proxy, empty-value deletion, duplicate/overlapping
operations and unsafe/missing/cyclic includes refuse explicitly. This does not
add geometry patches, displacements or full shader/conditional semantics.

A5.6 also honors basic `$nocull` for demanded brush/prop base-image previews,
including resolved Patch flags. Only configured Default derives two-sided
culling from a tagged Source hint; explicit editor material overrides win.
Previous polygon enable/mode/front-face state is restored after drawing, nested
with existing blend/depth state. No catalog-time decoding, geometry/winding
change or original material rewrite is introduced. Absent/false flags retain
normal culling; invalid/duplicate flags refuse and strict inspection still
rejects unsupported features. Two-sided preview is not Source backface lighting.

A5.7 adds explicit authored `$alphatestreference` for existing alpha-tested
brush/prop previews, including Patch inheritance. One finite value in (0,1] is
supported; zero/-zero/invalid/duplicate/block values refuse. Without the parameter,
the prior0.5 preview fallback remains; game-wide default/sentinel parity is not
claimed. Merely specifying a reference does not enable alpha testing. Configured
alpha-function overrides win; raw cutoff precedes editor fade and `$nocull`
state still restores. Scalar$alpha/full opacity effects remain unsupported and
combined flags retain existing cutout precedence. Original assets are unchanged.

## Static prop preview

The bounded reader accepts supported MDL 44/48/49 layouts with matching
VVD/VTX sidecars, root/default pose and a limited static preview path. Installed
loose/VPK materials and VTF data are read through the configured Source content
paths; nothing from an installed game is bundled with this editor. Unsupported
versions/layouts or missing/mismatched/oversized data use a diagnostic fallback
instead of pretending the model rendered. Check the console.

A5.4 preserves the Source VTX clockwise order instead of reversing its faces.
Actual outward-normal checks on one real model and authored list/strip controls
cover this correction; native brush renderer winding and coordinates were left
unchanged. This is orientation evidence, not all-model visual acceptance.

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

## A5.10 native displacement movement, rotation and distance editing

The A5.10 candidate renders and picks admitted native power2..6 displacement
meshes rather than their flat base faces. In object mode, select the whole
brush and move or rotate it normally: the base and raised grid transform together.
UV-lock policy, undo/redo and preserving Save As retain the mesh; later distance
edits still work. Scaling, shear/flip and face/vertex topology remain refused.
Legacy untyped offset-normal rows can load/translate but refuse rotation. Numeric
overflow or precision-losing motion also refuses without changing the world.

Select exactly one displacement **face**
to show a **Displacement vertex** row/column/distance section in the face inspector.
Choose a grid row and column, enter a distance and click **Apply distance**. This
uses normal undo/redo and preserving Save As. The row/column order is the native
Source grid, not a screen-space brush. The tool is disabled when the original
normal/distance data is incomplete or cannot safely express a distance change.

Use a copy until manual/Hammer acceptance is recorded. Unknown attachments and
references remain guarded; missing or malformed meshes are not silently replaced
by a flat editable proxy. This is not a sculpt brush, creation/power/sew tool,
blend-material preview or permission to transform arbitrary displacement bases.
A5.9 remains recovery with native movement/distance tools but without rotation.

A5.11 also permits **positive uniform scaling** of whole
native displacement brushes through the normal numeric and bounds-based scale
commands. Positions, offsets, elevation and distances follow the scale while
normal-like metadata retains magnitude. UV locks, undo/redo, native save/reopen
and continued distance edits have automated controls. Bounds-based authorization
is conservative: ratios must be exactly uniform; near-uniform arithmetic is not
permission for a nonuniform transform. Shear, reflection, topology changes and
unsafe numeric motion remain refused. Legacy untyped offset-normal rows also
refuse this new operation. A5.10 remains unmerged recovery; source and portable
checks do not fill native appearance/Hammer acceptance.

A5.11 also previews supported static Source model **bodygroups**.
An entity's bounded `body` value selects each decoded bodypart's model using its
actual base/count, independently of skin and animation-frame index. Source studio
FGDs default to the entity's body0; explicit definitions and other games are not
globally reinterpreted. Blank choices hide the relevant geometry. Bounds, picking,
renderer caches and browser centering follow the same selection without copying
all geometry for every combination. This remains static/root-pose preview, not
animation, body controllers, game collision or complete model/shader parity.
Source checks pass and candidate receipts identify exact portable artifacts;
no owner bodygroup/browser appearance acceptance is implied.

## Source model skins on fixed FGD paths

In a SourceVMF map, a simple fixed nonempty `studio` or `studioprop` model path
can now use the entity's `skin` property. With matching model/material assets and
multiple skin families, changing `0 -> 1 -> 0` selects the corresponding preview skin.
No MDL/VMT asset is edited. Other games, explicit skin/frame declarations and
legacy/dynamic/model()/sprite forms keep their prior semantics; this is not
animation, bodygroup or universal FGD/SDK parity.

## Still outside this candidate

Basic plain-object authoring does not imply arbitrary topology or native
editor-group/layer/instance/complete-displacement/overlay authoring, full Source material
rendering or all-branch performance. Common unsupported operations are gated;
some upstream actions can remain visible, but unsupported content changes must
refuse before mutation and preserving-save remains the final backstop. Never ignore an error or assume an
unsuccessful save changed the original. Use **Save As** and inspect a copy in
Hammer++ before relying on it. Actual owner Hammer++/game observations belong in
`lextest.md`; automated checks do not fill those human gates.
