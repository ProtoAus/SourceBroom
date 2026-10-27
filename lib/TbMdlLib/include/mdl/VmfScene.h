/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/NativeCordons.h"
#include "mdl/NativeVisgroups.h"
#include "mdl/NativeVisibility.h"
#include "mdl/VmfDocument.h"
#include "mdl/WorldNode.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tb::mdl
{
class NodeContents;
class BrushNode;
class GroupNode;
class PreparedVmfRemoval
{
private:
  struct Data;
  std::shared_ptr<Data> m_data;
  explicit PreparedVmfRemoval(std::shared_ptr<Data> data)
    : m_data{std::move(data)}
  {
  }
  friend class VmfScene;
};

struct VmfSceneDiagnostic
{
  VmfEntryId entry;
  std::string message;
  // A labelled source-plane sample, never a proxy hull or an editable node.
  std::optional<vm::vec3d> planeSample = std::nullopt;
  bool unprojectedSolid = false;
};

/** Native-backed preview and restricted existing-object editing. No filesystem writes.
 * filePosition is reserved for native entry bindings, not physical source lines.
 * Keep this state alive when transferring the world to a MapDocument.
 */
class VmfScene
{
private:
  VmfDocument m_document;
  NativeCordons m_cordons;
  NativeVisgroups m_visgroups;
  std::shared_ptr<const void> m_lifecycleScope = std::make_shared<int>(0);
  // Pointer-free committed deletion evidence survives history disposal.
  std::map<size_t, NativeVisgroupMember> m_removals;
  std::map<size_t, std::shared_ptr<PreparedVmfRemoval::Data>> m_parkedTickets;
  std::map<size_t, NativeVisgroupMemberId> m_memberIds;
  std::map<uint64_t, size_t> m_memberBindings;
  // Compare addresses only against a freshly traversed live tree; never dereference.
  std::map<size_t, const Node*> m_memberNodes;
  mutable NativeVisgroupContext m_lastVisgroupContext;
  mutable std::set<size_t> m_pendingMembers;
  NativeVisgroupContext visgroupContext() const;
  void projectVisgroups();
  NativeVisibility memberVisibility(const NativeVisgroupMember& member) const;
  // Visibility only for known, unambiguous source ancestry withheld from projection.
  std::map<size_t, NativeVisibility> m_sourceNativeMasks;
  void validateVisgroupProjection(const Node& node) const;
  void registerVisgroupMembers(
    const std::vector<std::tuple<Node*, size_t, std::string>>& members, bool copy);
  std::unique_ptr<WorldNode> m_world;
  std::unique_ptr<WorldNode> m_original;
  // Retains ancestry authority after takeWorld transfers ownership to Map.
  const WorldNode* m_activeWorld = nullptr;
  std::vector<VmfSceneDiagnostic> m_diagnostics;
  bool m_readOnlyPreview = false;
  size_t m_unprojectedSolidCount = 0;

  enum class GeneratedKind
  {
    Entity,
    Group,
    Solid,
    Side
  };
  struct GeneratedBinding
  {
    GeneratedKind kind;
    std::string nativeId;
    std::optional<VmfEntryId> prototype;
    size_t owner = 0;
  };
  std::map<size_t, GeneratedBinding> m_generated;
  std::set<size_t> m_nativeGroups;
  std::map<size_t, const GroupNode*> m_groupNodes;
  std::map<size_t, std::string> m_groupLinks;
  std::map<size_t, std::set<size_t>> m_groupStates;
  std::set<size_t> m_unsupportedGroupObjects;
  const Node* effectiveParent(const Node& node) const;
  size_t parentBinding(const Node& node) const;
  void projectGroups();
  void validateRegisteredGroup(const GroupNode& group) const;
  void membershipEdits(
    VmfEntryId source,
    const Node& node,
    const NativeVisgroupEmissionSession& session,
    std::vector<VmfValueEdit>& values,
    std::vector<VmfStructuralEdit>& structure,
    std::map<VmfEntryId, std::string>& appended) const;
  // Exact operation-admitted side sets, including parked undo/redo states.
  std::map<size_t, std::vector<std::set<size_t>>> m_clipStates;
  // Counts generated initial sets and retained clip states; source baselines
  // are derived from the immutable document. Never evict undoable states.
  size_t m_sideStateMemberships = 0;
  size_t m_nextBinding = 0;
  size_t m_generatedIdBytes = 0;
  std::optional<std::string> m_geometryReferenceProblem;
  std::string m_nextObjectId = "1";
  std::string m_nextSideId = "1";

  explicit VmfScene(VmfDocument document);
  static VmfScene readImpl(
    std::string source,
    const vm::bbox3d& worldBounds,
    EntityPropertyConfig config,
    bool readOnlyPreview);
  void requireEditing() const;
  std::optional<VmfEntryId> prototype(size_t binding) const;
  void checkGeometryReferences(VmfEntryId object) const;
  void validateSideState(const BrushNode& node) const;
  void validateReparentImpl(
    const Node& parent, const std::vector<Node*>& nodes, bool operation) const;
  void validateClipOwner(
    const BrushNode& node, bool clipping = true, bool operation = true) const;
  void validateContentsImpl(
    const Node& node, const NodeContents& contents, const vm::bbox3d& worldBounds) const;
  void nodeEdits(
    const Node& before,
    const Node& after,
    std::vector<VmfValueEdit>& values,
    std::vector<VmfStructuralEdit>& structure,
    std::map<VmfEntryId, std::string>& appended,
    bool protectExistingGeometry) const;
  std::string emitNode(
    const Node& node,
    const std::map<size_t, const Node*>& original,
    const NativeVisgroupEmissionSession& session) const;

public:
  // Finite editor preview envelope, not an engine/branch compatibility claim.
  static constexpr double PreviewWorldExtent = 131072.0;

  VmfScene(VmfScene&&) noexcept;
  VmfScene& operator=(VmfScene&&) noexcept;
  ~VmfScene();

  /** Throws std::invalid_argument for invalid geometry/unsupported numeric values.
   * Never returns a scene with silently dropped solids or sides.
   */
  static VmfScene read(
    std::string source, const vm::bbox3d& worldBounds, EntityPropertyConfig config = {});

  /** Explicit partial, immutable preview. Duplicate native IDs stay unchanged;
   * invalid solids are reported and not projected. Never an editable fallback. */
  static VmfScene readReadOnlyPreview(
    std::string source, const vm::bbox3d& worldBounds, EntityPropertyConfig config = {});
  bool isReadOnlyPreview() const { return m_readOnlyPreview; }
  size_t unprojectedSolidCount() const { return m_unprojectedSolidCount; }

  WorldNode& world();
  const WorldNode& world() const;
  std::unique_ptr<WorldNode> takeWorld();
  const VmfDocument& document() const;
  const std::vector<VmfSceneDiagnostic>& diagnostics() const;
  std::string nativeId(const Node& node) const;

  NativeVisgroupSnapshot visgroups() const;
  std::optional<PreparedNativeVisgroupChange> prepareVisgroupChange(
    const NativeVisgroupRequest& request) const;
  std::vector<std::pair<Node*, NativeVisibility>> visgroupVisibility(
    const PreparedNativeVisgroupChange& change, NativeVisgroupDirection direction) const;
  // Complete projected visibility invalidation closure; no geometry changes.
  std::vector<Node*> applyVisgroupChange(
    PreparedNativeVisgroupChange& change, NativeVisgroupDirection direction);

  NativeCordonSnapshot cordons() const;
  std::optional<PreparedNativeCordonChange> prepareCordonChange(
    const NativeCordonRequest& request) const;
  void applyCordonChange(
    PreparedNativeCordonChange& change, NativeCordonDirection direction);

  /** Admission precedes command mutation. Generated bindings/IDs survive undo,
   * redo and repeated saves; serialization never allocates identities. */
  void validateAdditions(
    const Node& parent, const std::vector<Node*>& nodes, bool duplication) const;
  void registerAdditions(
    const std::vector<Node*>& nodes, bool duplication, const Node* parent = nullptr);
  void validateDuplication(const std::vector<Node*>& nodes) const;
  void registerAdditionBatch(
    const std::vector<std::pair<const Node*, std::vector<Node*>>>& additions,
    bool duplication);
  void validateGroupClosure(const std::vector<Node*>& nodes, bool creation = false) const;
  /** Container admission only for a registered loaded/generated group with no
   * parent. The caller must own its lifetime and validate its parked closure.
   * Never admits binding-zero staging nodes or makes detached nodes live.
   * Throws std::invalid_argument without mutation on invalid admission.
   */
  void validateRegisteredDetachedGroup(const GroupNode& group) const;
  void registerGroupChanges(
    Node* created, const std::vector<std::pair<Node*, Node*>>& parents);
  void validateRemoval(const std::vector<Node*>& removalClosure) const;
  PreparedVmfRemoval prepareRemoval(const std::vector<Node*>& roots) const;
  PreparedVmfRemoval prepareGroupRemoval(const std::vector<Node*>& groups) const;
  void validateVisgroupReplay(
    const NativeVisgroupSnapshot& expected, const std::vector<size_t>& bindings) const;
  bool hasVisgroupMember(const Node& node) const;
  // Exact registered node identity, not a numeric native-ID lookup.
  std::optional<NativeVisgroupMemberId> registeredVisgroupMember(const Node& node) const;
  // Explicit command-owned detach boundary; preflight alone publishes nothing.
  void publishRemoval(
    const PreparedVmfRemoval& proof, const std::vector<Node*>& parkedRoots);
  void validateActivation(
    const PreparedVmfRemoval& proof,
    const std::vector<std::pair<Node*, Node*>>& placements) const;
  void activateRemoval(
    const PreparedVmfRemoval& proof, const std::vector<Node*>& restoredRoots);
  void disposeRemoval(const PreparedVmfRemoval& proof);
  void validateReparent(const Node& parent, const std::vector<Node*>& nodes) const;
  void validateContents(
    const Node& node, const NodeContents& contents, const vm::bbox3d& worldBounds) const;

  /** Command-owned ordinary, one-output clips only. Validates the complete batch
   * before assigning any cut-side identity. Never called by serialization. */
  void registerClips(
    std::vector<std::pair<Node*, NodeContents>>& nodes, const vm::bbox3d& worldBounds);
  void validateClipContents(
    const Node& node, const NodeContents& contents, const vm::bbox3d& worldBounds) const;

  /** One publication for both outputs; siblings are detached and correspond to keepers.
   * Subjects includes unchanged brushes, which must pass the same live admission. */
  void registerBothClips(
    const std::vector<BrushNode*>& subjects,
    std::vector<std::pair<Node*, NodeContents>>& keepers,
    const std::vector<BrushNode*>& siblings,
    const vm::bbox3d& worldBounds);
  void validateBothClipReplay(
    const std::vector<BrushNode*>& subjects,
    const std::vector<std::pair<Node*, NodeContents>>& keepers,
    const std::vector<BrushNode*>& siblings,
    const vm::bbox3d& worldBounds) const;

  /** Immutable-source delta of the authoritative registered live scene, not a
   * detached clone that merely reuses source IDs. Plain native additions/deletions
   * and brush-entity wrapping retain unrelated bytes and opaque payloads. Unsupported
   * edits fail closed with std::invalid_argument; no MAP re-export or filesystem writes.
   */
  std::string serialize(const WorldNode& world) const;
};
} // namespace tb::mdl
