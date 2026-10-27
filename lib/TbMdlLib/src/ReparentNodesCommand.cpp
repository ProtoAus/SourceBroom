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

#include "mdl/ReparentNodesCommand.h"

#include "base/Logger.h"
#include "mdl/AddRemoveNodesUtils.h"
#include "mdl/Map.h"
#include "mdl/Node.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"

#include <algorithm>
#include <set>

namespace tb::mdl
{

std::unique_ptr<ReparentNodesCommand> ReparentNodesCommand::reparent(
  kdl::flat_map<Node*, std::vector<Node*>> nodesToAdd,
  kdl::flat_map<Node*, std::vector<Node*>> nodesToRemove)
{
  return std::make_unique<ReparentNodesCommand>(
    std::move(nodesToAdd), std::move(nodesToRemove));
}

ReparentNodesCommand::ReparentNodesCommand(
  kdl::flat_map<Node*, std::vector<Node*>> nodesToAdd,
  kdl::flat_map<Node*, std::vector<Node*>> nodesToRemove)
  : UpdateLinkedGroupsCommandBase{"Reparent Objects", true}
  , m_nodesToAdd{std::move(nodesToAdd)}
  , m_nodesToRemove{std::move(nodesToRemove)}
{
}

std::vector<ReparentNodesCommand::ReplayNode> ReparentNodesCommand::captureReplayNodes(
  Map& map) const
{
  auto live = std::set<Node*>{};
  const auto collect = [&](auto&& self, Node& node) -> void {
    live.insert(&node);
    for (auto* child : node.children())
      self(self, *child);
  };
  collect(collect, map.worldNode());
  auto involved = std::set<Node*>{};
  for (const auto* placements : {&m_nodesToAdd, &m_nodesToRemove})
    for (const auto& [parent, roots] : *placements)
    {
      if (!live.contains(parent))
        throw std::invalid_argument{"reparent parent no longer live"};
      involved.insert(parent);
      for (auto* root : roots)
      {
        if (!live.contains(root))
          throw std::invalid_argument{"reparent child no longer live"};
        const auto closure = [&](auto&& self, Node& node) -> void {
          involved.insert(&node);
          for (auto* child : node.children())
            self(self, *child);
        };
        closure(closure, *root);
      }
    }
  auto result = std::vector<ReplayNode>{};
  for (auto* node : involved)
    result.push_back({node, node->lineNumber(), node->parent(), node->children()});
  return result;
}
bool ReparentNodesCommand::preflight(Map& map, const bool forward)
{
  try
  {
    if (auto* scene = map.vmfScene())
    {
      const auto rows = captureReplayNodes(map);
      const auto& expected = forward ? m_beforeNodes : m_afterNodes;
      if (m_beforeVisgroups)
      {
        scene->validateVisgroupReplay(
          forward ? *m_beforeVisgroups : *m_afterVisgroups, m_bindings);
        if (rows.size() != expected.size())
          throw std::invalid_argument{"reparent closure changed"};
        for (size_t i = 0; i < rows.size(); ++i)
          if (
            rows[i].node != expected[i].node || rows[i].binding != expected[i].binding
            || rows[i].parent != expected[i].parent
            || rows[i].children != expected[i].children)
            throw std::invalid_argument{"reparent owner/children/identity changed"};
        auto roots = std::vector<Node*>{};
        for (const auto& [parent, children] : m_nodesToRemove)
          roots.insert(roots.end(), children.begin(), children.end());
        scene->validateGroupClosure(roots);
      }
      else
      {
        for (const auto& [parent, nodes] : m_nodesToAdd)
          scene->validateReparent(*parent, nodes);
        m_beforeVisgroups = scene->visgroups();
        m_beforeNodes = rows;
        for (const auto& row : rows)
          for (auto* node = row.node; node; node = node->parent())
            m_bindings.push_back(node->lineNumber());
      }
    }
    return true;
  }
  catch (const std::invalid_argument& e)
  {
    map.logger().error() << "Native reparent replay refused: " << e.what();
    return false;
  }
}
void ReparentNodesCommand::moveNative(Map& map, const bool forward)
{
  const auto& from = forward ? m_nodesToRemove : m_nodesToAdd;
  const auto& to = forward ? m_nodesToAdd : m_nodesToRemove;
  auto moved = std::vector<Node*>{};
  for (const auto& [parent, children] : from)
    moved.insert(moved.end(), children.begin(), children.end());
  map.nodesWillBeRemovedNotifier(moved);
  for (const auto& [parent, children] : from)
    parent->removeChildren(children.begin(), children.end());
  for (const auto& [parent, children] : to)
  {
    if (forward)
      parent->addChildren(children);
    else
    {
      const auto before = std::ranges::find(m_beforeNodes, parent, &ReplayNode::node);
      auto ordered = children;
      const auto index = [&](auto* node) {
        return std::ranges::find(before->children, node) - before->children.begin();
      };
      std::ranges::sort(ordered, [&](auto* a, auto* b) { return index(a) < index(b); });
      for (auto* child : ordered)
        parent->insertChild(static_cast<size_t>(index(child)), child);
    }
  }
  // These nodes moved; they were never committed native removals.
  map.nodesWereRemovedNotifier(moved);
  map.nodesWereAddedNotifier(moved);
}
bool ReparentNodesCommand::doPerformDo(Map& map)
{
  if (!preflight(map, true))
    return false;
  if (map.vmfScene())
    moveNative(map, true);
  else
  {
    removeNodesAndNotify(m_nodesToRemove, map);
    addNodesAndNotify(m_nodesToAdd, map);
  }
  if (map.vmfScene())
  {
    m_afterVisgroups = map.vmfScene()->visgroups();
    m_afterNodes = captureReplayNodes(map);
  }
  return true;
}
bool ReparentNodesCommand::doPerformUndo(Map& map)
{
  if (!preflight(map, false))
    return false;
  if (map.vmfScene())
    moveNative(map, false);
  else
  {
    removeNodesAndNotify(m_nodesToAdd, map);
    addNodesAndNotify(m_nodesToRemove, map);
  }
  return true;
}
} // namespace tb::mdl
