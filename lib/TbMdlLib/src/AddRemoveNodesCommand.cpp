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

#include "mdl/AddRemoveNodesCommand.h"

#include "base/Logger.h"
#include "base/Macros.h"
#include "mdl/AddRemoveNodesUtils.h"
#include "mdl/Map.h"
#include "mdl/Node.h"

#include "kd/map_utils.h"

#include <algorithm>

namespace tb::mdl
{

std::unique_ptr<AddRemoveNodesCommand> AddRemoveNodesCommand::add(
  const kdl::flat_map<Node*, std::vector<Node*>>& nodes)
{
  return std::make_unique<AddRemoveNodesCommand>(Action::Add, nodes);
}

std::unique_ptr<AddRemoveNodesCommand> AddRemoveNodesCommand::remove(
  const kdl::flat_map<Node*, std::vector<Node*>>& nodes)
{
  return std::make_unique<AddRemoveNodesCommand>(Action::Remove, nodes);
}

AddRemoveNodesCommand::~AddRemoveNodesCommand()
{
  if (m_removal)
    m_scene->disposeRemoval(*m_removal);
  kdl::map_clear_and_delete(m_nodesToAdd);
}

AddRemoveNodesCommand::AddRemoveNodesCommand(
  const Action action, const kdl::flat_map<Node*, std::vector<Node*>>& nodes)
  : UpdateLinkedGroupsCommandBase{makeName(action), true}
  , m_action{action}
{
  switch (m_action)
  {
  case Action::Add:
    m_nodesToAdd = nodes;
    break;
  case Action::Remove:
    m_nodesToRemove = nodes;
    break;
    switchDefault();
  }
}

std::string AddRemoveNodesCommand::makeName(const Action action)
{
  switch (action)
  {
  case Action::Add:
    return "Add Objects";
  case Action::Remove:
    return "Remove Objects";
    switchDefault();
  }
}

bool AddRemoveNodesCommand::doPerformDo(Map& map)
{
  return apply(map, m_action == Action::Add);
}
bool AddRemoveNodesCommand::doPerformUndo(Map& map)
{
  return apply(map, m_action == Action::Remove);
}
bool AddRemoveNodesCommand::apply(Map& map, const bool adding)
{
  const auto& nodes = adding ? m_nodesToAdd : m_nodesToRemove;
  auto roots = std::vector<Node*>{};
  auto placements = std::vector<std::pair<Node*, Node*>>{};
  for (const auto& [parent, children] : nodes)
    for (auto* node : children)
    {
      roots.push_back(node);
      placements.emplace_back(node, parent);
    }
  try
  {
    if (auto* scene = map.vmfScene())
    {
      auto closure = std::vector<Node*>{};
      const auto collect = [&](auto&& self, Node& node) -> void {
        closure.push_back(&node);
        for (auto* child : node.children())
          self(self, *child);
      };
      for (auto* root : roots)
        collect(collect, *root);
      const auto registered = std::ranges::count_if(
        closure, [&](const auto* node) { return scene->hasVisgroupMember(*node); });
      if (!m_managed)
      {
        if (
          adding && registered != 0
          && registered != static_cast<ptrdiff_t>(closure.size()))
          throw std::invalid_argument{"mixed managed/unmanaged native additions"};
        m_managed = !adding || registered != 0;
      }
      if (!*m_managed && registered != 0)
        throw std::invalid_argument{"unmanaged native command became registered"};
      if (*m_managed && adding && m_removal)
        scene->validateActivation(*m_removal, placements);
      else if (*m_managed && !adding)
        m_removal = scene->prepareRemoval(roots);
      m_scene = scene;
    }
  }
  catch (const std::invalid_argument& e)
  {
    map.logger().error() << "Native node lifecycle refused: " << e.what();
    return false;
  }
  if (adding)
  {
    addNodesAndNotify(nodes, map, [&] {
      if (m_removal)
      {
        m_scene->activateRemoval(*m_removal, roots);
        m_removal.reset();
      }
    });
  }
  else
  {
    removeNodesAndNotify(nodes, map, [&] {
      if (m_removal)
        m_scene->publishRemoval(*m_removal, roots);
    });
  }
  std::swap(m_nodesToAdd, m_nodesToRemove);
  return true;
}
} // namespace tb::mdl
