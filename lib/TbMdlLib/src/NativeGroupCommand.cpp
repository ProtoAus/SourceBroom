/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/NativeGroupCommand.h"

#include "base/Logger.h"
#include "mdl/AddRemoveNodesUtils.h"
#include "mdl/EditorContext.h"
#include "mdl/GroupNode.h"
#include "mdl/Map.h"
#include "mdl/SelectionCommand.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h" // IWYU pragma: keep

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace tb::mdl
{
namespace
{
size_t position(const Node& node)
{
  const auto& siblings = node.parent()->children();
  return static_cast<size_t>(std::ranges::find(siblings, &node) - siblings.begin());
}
} // namespace

NativeGroupCommand::NativeGroupCommand(std::vector<Node*> nodes, Node& parent)
  : UndoableCommand{"Group Selected Objects", true}
  , m_grouping{true}
{
  auto group = std::make_unique<GroupNode>(Group{""});
  auto* container = group.get();
  m_containers.push_back(container);
  m_containerBindings.push_back(0);
  m_owners.push_back(std::move(group));
  auto siblings = parent.children();
  for (auto* node : nodes)
  {
    captureClosure(*node);
    m_before.push_back({node, node->parent(), position(*node)});
    if (node->parent() == &parent)
    {
      std::erase(siblings, node);
    }
    m_after.push_back({node, container, m_after.size()});
  }
  m_before.push_back({container, nullptr, 0});
  m_after.push_back({container, &parent, siblings.size()});
  for (auto* node : nodes)
  {
    m_parkedMembers.emplace_back(node, nullptr);
  }
  m_deselect = SelectionCommand::deselectAll();
  m_select = SelectionCommand::select({container});
}

NativeGroupCommand::NativeGroupCommand(std::vector<GroupNode*> groups)
  : UndoableCommand{"Ungroup", true}
  , m_grouping{false}
{
  auto final = std::map<Node*, std::vector<Node*>>{};
  auto selected = std::vector<Node*>{};
  for (auto* group : groups)
  {
    captureClosure(*group);
    m_containers.push_back(group);
    m_containerBindings.push_back(group->lineNumber());
    m_owners.emplace_back();
    final.try_emplace(group->parent(), group->parent()->children());
  }
  for (auto& [parent, children] : final)
  {
    auto result = std::vector<Node*>{};
    for (auto* child : children)
    {
      if (std::ranges::find(groups, child) == groups.end())
      {
        result.push_back(child);
      }
      else
      {
        result.insert(result.end(), child->children().begin(), child->children().end());
      }
    }
    children = std::move(result);
  }
  for (auto* group : groups)
  {
    for (auto* child : group->children())
    {
      m_before.push_back({child, group, position(*child)});
      const auto& siblings = final.at(group->parent());
      m_after.push_back(
        {child,
         group->parent(),
         static_cast<size_t>(std::ranges::find(siblings, child) - siblings.begin())});
      selected.push_back(child);
    }
    m_before.push_back({group, group->parent(), position(*group)});
    m_after.push_back({group, nullptr, 0});
  }
  for (auto* node : selected)
  {
    m_parkedMembers.emplace_back(node, nullptr);
  }
  m_deselect = SelectionCommand::deselectAll();
  m_select = SelectionCommand::select(std::move(selected));
}

void NativeGroupCommand::captureClosure(Node& node)
{
  m_closure.push_back({&node, node.lineNumber(), node.children()});
  for (auto* child : node.children())
  {
    captureClosure(*child);
  }
}

NativeGroupCommand::~NativeGroupCommand() = default;
GroupNode* NativeGroupCommand::createdGroup() const
{
  return m_grouping ? m_containers.front() : nullptr;
}

void NativeGroupCommand::reconcileOwnership()
{
  for (auto& [node, owner] : m_parkedMembers)
  {
    if (node->parent())
    {
      owner.release();
    }
    else if (!owner)
    {
      owner.reset(node);
    }
  }
  for (size_t i = 0; i < m_containers.size(); ++i)
  {
    if (m_containers[i]->parent())
    {
      m_owners[i].release();
    }
    else if (!m_owners[i])
    {
      m_owners[i].reset(m_containers[i]);
    }
  }
}

bool NativeGroupCommand::validate(Map& map, const std::vector<Placement>& expected) const
{
  try
  {
    auto* scene = map.vmfScene();
    if (!scene || scene->isReadOnlyPreview())
    {
      return false;
    }
    auto available = std::set<const Node*>{};
    const auto collect = [&](auto&& self, const Node& node) -> void {
      if (!available.insert(&node).second)
      {
        return;
      }
      for (const auto* child : node.children())
      {
        self(self, *child);
      }
    };
    collect(collect, map.worldNode());
    for (const auto& owner : m_owners)
    {
      if (owner)
      {
        collect(collect, *owner);
      }
    }
    for (const auto& [node, owner] : m_parkedMembers)
    {
      if (owner)
      {
        collect(collect, *owner);
      }
    }
    for (size_t i = 0; i < m_containers.size(); ++i)
    {
      if (
        !available.contains(m_containers[i])
        || m_containers[i]->lineNumber() != m_containerBindings[i])
      {
        throw std::invalid_argument{
          "native group container is not live or uniquely parked"};
      }
    }
    for (const auto& placement : expected)
    {
      if (
        !available.contains(placement.node)
        || (placement.parent && !available.contains(placement.parent)))
      {
        throw std::invalid_argument{
          "native group replay member/parent is no longer live"};
      }
    }
    for (const auto* placements : {&m_before, &m_after})
    {
      for (const auto& placement : *placements)
      {
        if (placement.parent && !available.contains(placement.parent))
        {
          throw std::invalid_argument{"native group destination is no longer live"};
        }
      }
    }
    for (const auto& entry : m_closure)
    {
      if (!available.contains(entry.node))
      {
        throw std::invalid_argument{"native group closure member is no longer live"};
      }
      if (entry.node->lineNumber() != entry.binding
          || (std::ranges::find(m_containers, entry.node) == m_containers.end()
              && entry.node->children() != entry.children))
      {
        throw std::invalid_argument{"native group closure identity/children changed"};
      }
    }
    for (size_t i = 0; i < m_containers.size(); ++i)
    {
      const auto* container = m_containers[i];
      auto children = std::vector<Placement>{};
      for (const auto& placement : expected)
      {
        if (placement.parent == container)
        {
          children.push_back(placement);
        }
      }
      std::ranges::sort(children, {}, &Placement::index);
      auto nodes = std::vector<Node*>{};
      for (const auto& placement : children)
      {
        nodes.push_back(placement.node);
      }
      if (container->children() != nodes)
      {
        throw std::invalid_argument{"native group container acquired foreign members"};
      }
      if (!container->parent())
      {
        if (m_owners[i].get() != container)
        {
          throw std::invalid_argument{"native group container is not uniquely parked"};
        }
        // Only the initial command-private blank GROUP container is unregistered.
        if (!(m_grouping && !m_registered && !container->lineNumber()))
        {
          if (container->locked())
          {
            throw std::invalid_argument{"parked native group is locked"};
          }
          scene->validateRegisteredDetachedGroup(*container);
        }
      }
    }
    const auto& destination = &expected == &m_before ? m_after : m_before;
    if (const auto* current = map.editorContext().currentGroup())
    {
      for (const auto& placement : destination)
      {
        if (
          !placement.parent
          && (placement.node == current || placement.node->isAncestorOf(*current)))
        {
          throw std::invalid_argument{
            "native group operation would detach editing context"};
        }
      }
    }
    auto live = std::vector<Node*>{};
    for (const auto& placement : expected)
    {
      if (
        placement.node->parent() != placement.parent
        || (placement.parent && position(*placement.node) != placement.index))
      {
        throw std::invalid_argument{"native group replay tree/order changed"};
      }
      if (placement.parent)
      {
        live.push_back(placement.node);
      }
      else if (placement.node->childCount() != 0)
      {
        throw std::invalid_argument{"parked native group acquired foreign children"};
      }
    }
    scene->validateGroupClosure(live);
    return true;
  }
  catch (const std::exception& e)
  {
    map.logger().error() << "Native VMF group operation refused: " << e.what();
    return false;
  }
}

void NativeGroupCommand::move(
  Map& map, const std::vector<Placement>& from, const std::vector<Placement>& to)
{
  try
  {
    for (const auto& placement : from)
    {
      if (placement.parent)
      {
        removeNodesAndNotify({{placement.parent, {placement.node}}}, map);
        reconcileOwnership();
      }
    }
    auto pending = to;
    std::ranges::stable_sort(pending, {}, &Placement::index);
    // Containers may receive their children while detached. Node propagates the
    // complete descendant counts when each container is subsequently attached.
    for (const auto& placement : pending)
    {
      if (placement.parent)
      {
        placement.parent->insertChild(placement.index, placement.node);
        reconcileOwnership();
      }
    }
    // Map recursively indexes added descendants. Publish only nonoverlapping
    // roots after ALL placements are live, never a detached child and its later
    // attached container. Indexed insertion still leaves unrelated siblings alone.
    auto added = std::vector<Node*>{};
    for (const auto& placement : to)
    {
      if (placement.parent && std::ranges::none_of(to, [&](const auto& other) {
            return other.parent && other.node->isAncestorOf(*placement.node);
          }))
      {
        added.push_back(placement.node);
      }
    }
    map.nodesWereAddedNotifier(added);
  }
  catch (...)
  {
    reconcileOwnership();
    throw;
  }
}

bool NativeGroupCommand::doPerformDo(Map& map)
{
  if (!validate(map, m_before))
  {
    return false;
  }
  if (!m_registered)
  {
    auto parents = std::vector<std::pair<Node*, Node*>>{};
    for (const auto& placement : m_after)
    {
      parents.emplace_back(placement.node, placement.parent);
    }
    try
    {
      map.vmfScene()->registerGroupChanges(
        m_grouping ? m_containers.front() : nullptr, parents);
    }
    catch (const std::exception& e)
    {
      map.logger().error() << "Native VMF group allocation refused: " << e.what();
      return false;
    }
    if (m_grouping)
    {
      m_containerBindings.front() = m_containers.front()->lineNumber();
    }
    m_registered = true;
  }
  m_deselect->performDo(map);
  move(map, m_before, m_after);
  m_select->performDo(map);
  return true;
}

bool NativeGroupCommand::doPerformUndo(Map& map)
{
  if (!validate(map, m_after))
  {
    return false;
  }
  m_select->performUndo(map);
  move(map, m_after, m_before);
  m_deselect->performUndo(map);
  return true;
}
} // namespace tb::mdl
