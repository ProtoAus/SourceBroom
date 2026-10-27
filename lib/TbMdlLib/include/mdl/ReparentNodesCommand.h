/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "base/Macros.h"
#include "mdl/NativeVisgroups.h"
#include "mdl/UpdateLinkedGroupsCommandBase.h"

#include "kd/flat_map.h"

#include <memory>
#include <vector>

namespace tb::mdl
{
class GroupNode;
class Map;
class Node;

class ReparentNodesCommand : public UpdateLinkedGroupsCommandBase
{
private:
  kdl::flat_map<Node*, std::vector<Node*>> m_nodesToAdd;
  kdl::flat_map<Node*, std::vector<Node*>> m_nodesToRemove;
  std::optional<NativeVisgroupSnapshot> m_beforeVisgroups, m_afterVisgroups;
  struct ReplayNode
  {
    Node* node;
    size_t binding;
    Node* parent;
    std::vector<Node*> children;
  };
  std::vector<ReplayNode> m_beforeNodes, m_afterNodes;
  std::vector<size_t> m_bindings;
  std::vector<ReplayNode> captureReplayNodes(Map& map) const;
  bool preflight(Map& map, bool forward);
  void moveNative(Map& map, bool forward);

public:
  static std::unique_ptr<ReparentNodesCommand> reparent(
    kdl::flat_map<Node*, std::vector<Node*>> nodesToAdd,
    kdl::flat_map<Node*, std::vector<Node*>> nodesToRemove);

  ReparentNodesCommand(
    kdl::flat_map<Node*, std::vector<Node*>> nodesToAdd,
    kdl::flat_map<Node*, std::vector<Node*>> nodesToRemove);

private:
  bool doPerformDo(Map& map) override;
  bool doPerformUndo(Map& map) override;

  deleteCopyAndMove(ReparentNodesCommand);
};

} // namespace tb::mdl
