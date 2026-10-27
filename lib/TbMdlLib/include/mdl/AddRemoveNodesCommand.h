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
#include "mdl/UpdateLinkedGroupsCommandBase.h"
#include "mdl/VmfScene.h"

#include "kd/flat_map.h"

#include <memory>
#include <vector>

namespace tb::mdl
{
class Node;

class AddRemoveNodesCommand : public UpdateLinkedGroupsCommandBase
{
private:
  enum class Action
  {
    Add,
    Remove
  };

  Action m_action;
  VmfScene* m_scene = nullptr;
  std::optional<bool> m_managed;
  std::optional<PreparedVmfRemoval> m_removal;
  kdl::flat_map<Node*, std::vector<Node*>> m_nodesToAdd;
  kdl::flat_map<Node*, std::vector<Node*>> m_nodesToRemove;

public:
  static std::unique_ptr<AddRemoveNodesCommand> add(
    const kdl::flat_map<Node*, std::vector<Node*>>& nodes);
  static std::unique_ptr<AddRemoveNodesCommand> remove(
    const kdl::flat_map<Node*, std::vector<Node*>>& nodes);

  AddRemoveNodesCommand(
    Action action, const kdl::flat_map<Node*, std::vector<Node*>>& nodes);
  ~AddRemoveNodesCommand() override;

private:
  static std::string makeName(Action action);

  bool doPerformDo(Map& map) override;
  bool doPerformUndo(Map& map) override;

  bool apply(Map& map, bool adding);

  deleteCopyAndMove(AddRemoveNodesCommand);
};

} // namespace tb::mdl
