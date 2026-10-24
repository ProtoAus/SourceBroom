/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/VmfDocument.h"
#include "mdl/WorldNode.h"

#include <memory>
#include <string>
#include <vector>

namespace tb::mdl
{
struct VmfSceneDiagnostic
{
  VmfEntryId entry;
  std::string message;
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

  explicit VmfScene(VmfDocument document);

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

  WorldNode& world();
  const WorldNode& world() const;
  std::unique_ptr<WorldNode> takeWorld();
  const VmfDocument& document() const;
  const std::vector<VmfSceneDiagnostic>& diagnostics() const;
  std::string nativeId(const Node& node) const;

  /** No-op is byte-identical. Refuses additions/deletions, reparenting, lost or
   * duplicate bindings, and edits to opaque attachments. Existing plain entity
   * values, face planes/materials/UVs can change. Throws std::invalid_argument.
   */
  std::string serialize(const WorldNode& world) const;
};
} // namespace tb::mdl
