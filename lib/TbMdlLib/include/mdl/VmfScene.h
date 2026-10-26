/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/VmfDocument.h"
#include "mdl/WorldNode.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace tb::mdl
{
class NodeContents;

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
  std::unique_ptr<WorldNode> m_world;
  std::unique_ptr<WorldNode> m_original;
  std::vector<VmfSceneDiagnostic> m_diagnostics;
  bool m_readOnlyPreview = false;
  size_t m_unprojectedSolidCount = 0;

  enum class GeneratedKind
  {
    Entity,
    Solid,
    Side
  };
  struct GeneratedBinding
  {
    GeneratedKind kind;
    std::string nativeId;
    std::optional<VmfEntryId> prototype;
    size_t owner = 0;
    /** Solid: immutable admitted side count; side ownership + uniqueness makes
     * a count match the exact registered binding set without scanning history. */
    size_t sideCount = 0;
  };
  std::map<size_t, GeneratedBinding> m_generated;
  // Exact operation-admitted side sets, including parked undo/redo states.
  std::map<size_t, std::vector<std::set<size_t>>> m_clipStates;
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
  void nodeEdits(
    const Node& before,
    const Node& after,
    std::vector<VmfValueEdit>& values,
    std::vector<VmfStructuralEdit>& structure,
    std::map<VmfEntryId, std::string>& appended,
    bool protectExistingGeometry) const;
  std::string emitNode(
    const Node& node, const std::map<size_t, const Node*>& original) const;

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

  /** Admission precedes command mutation. Generated bindings/IDs survive undo,
   * redo and repeated saves; serialization never allocates identities. */
  void validateAdditions(
    const Node& parent, const std::vector<Node*>& nodes, bool duplication) const;
  void registerAdditions(const std::vector<Node*>& nodes, bool duplication);
  void validateDuplication(const std::vector<Node*>& nodes) const;
  void validateRemoval(const std::vector<Node*>& removalClosure) const;
  void validateReparent(const Node& parent, const std::vector<Node*>& nodes) const;
  void validateContents(
    const Node& node, const NodeContents& contents, const vm::bbox3d& worldBounds) const;

  /** Command-owned ordinary, one-output clips only. Validates the complete batch
   * before assigning any cut-side identity. Never called by serialization. */
  void registerClips(
    std::vector<std::pair<Node*, NodeContents>>& nodes, const vm::bbox3d& worldBounds);
  void validateClipContents(
    const Node& node, const NodeContents& contents, const vm::bbox3d& worldBounds) const;

  /** Immutable-source delta. Plain native additions/deletions and brush-entity
   * wrapping retain unrelated bytes and opaque payloads. Unsupported edits fail
   * closed with std::invalid_argument; no MAP re-export or filesystem writes. */
  std::string serialize(const WorldNode& world) const;
};
} // namespace tb::mdl
