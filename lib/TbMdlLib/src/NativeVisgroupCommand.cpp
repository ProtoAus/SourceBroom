/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/NativeVisgroupCommand.h"

#include "base/Logger.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/EditorContext.h"
#include "mdl/EntityNode.h"
#include "mdl/GroupNode.h"
#include "mdl/Map.h"
#include "mdl/SelectionChange.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace tb::mdl
{
NativeVisgroupCommand::NativeVisgroupCommand(PreparedNativeVisgroupChange change)
  : UndoableCommand{"Edit Native Visgroups", true}
  , m_change{std::move(change)}
{
}

bool NativeVisgroupCommand::apply(Map& map, const NativeVisgroupDirection direction)
{
  try
  {
    auto* scene = map.vmfScene();
    if (!scene)
      return false;
    const auto forward = direction == NativeVisgroupDirection::Forward;
    const auto projection = scene->visgroupVisibility(m_change, direction);
    auto hidden = std::map<const Node*, bool>{};
    auto newlyHidden = std::set<const Node*>{};
    for (const auto& [node, causes] : projection)
    {
      hidden.emplace(node, causes.hidden());
      if (causes.hidden() && !node->effectiveNativeVisibility().hidden())
        newlyHidden.insert(node);
    }
    auto live = std::set<const Node*>{};
    const auto collect = [&](auto&& self, const Node& node) -> void {
      live.insert(&node);
      for (auto* child : node.children())
        self(self, *child);
    };
    collect(collect, map.worldNode());
    const auto unavailable = [&](const Node* node) {
      if (!live.contains(node))
        throw std::invalid_argument{"selection/context is no longer live"};
      const auto found = hidden.find(node);
      return found != hidden.end() && found->second;
    };
    const auto visibleContent = [&](auto&& self, const Node& node) -> bool {
      if (unavailable(&node) || !node.sessionVisible())
        return false;
      if (
        dynamic_cast<const GroupNode*>(&node)
        || (dynamic_cast<const EntityNode*>(&node) && node.hasChildren()))
      {
        return std::ranges::any_of(
          node.children(), [&](const auto* child) { return self(self, *child); });
      }
      return map.editorContext().visible(node);
    };
    auto* context = map.editorContext().currentGroup();
    for (auto* ancestor = static_cast<Node*>(context); ancestor;
         ancestor = ancestor->parent())
    {
      static_cast<void>(unavailable(ancestor));
      if (newlyHidden.contains(ancestor))
        throw std::invalid_argument{"native visibility would hide editing context"};
    }
    const auto contextDecreases =
      context && std::ranges::any_of(newlyHidden, [&](const auto* node) {
        return node == context || context->isAncestorOf(*node);
      });
    if (contextDecreases && !visibleContent(visibleContent, *context))
      throw std::invalid_argument{
        "native visibility would hide last editing-context content"};

    if (!m_planned)
    {
      m_beforeNodes = map.selection().nodes;
      m_beforeFaces = map.selection().brushFaces;
      for (const auto& face : m_beforeFaces)
        m_faceBindings.push_back(face.face().lineNumber());
      m_context = context;
      for (auto* node : m_beforeNodes)
      {
        static_cast<void>(unavailable(node));
        bool keep = !newlyHidden.contains(node);
        const auto closure = [&](auto&& self, const Node& member) -> void {
          if (newlyHidden.contains(&member))
            keep = false;
          for (const auto* child : member.children())
            self(self, *child);
        };
        closure(closure, *node);
        for (auto* required : node->nodesRequiredForViewSelection())
          if (newlyHidden.contains(required))
            keep = false;
        if (keep)
          m_afterNodes.push_back(node);
      }
      for (const auto& face : m_beforeFaces)
        if (!newlyHidden.contains(&face.node()))
          m_afterFaces.push_back(face);
    }
    const auto& expectedNodes = forward ? m_beforeNodes : m_afterNodes;
    const auto& expectedFaces = forward ? m_beforeFaces : m_afterFaces;
    const auto& desiredNodes = forward ? m_afterNodes : m_beforeNodes;
    const auto& desiredFaces = forward ? m_afterFaces : m_beforeFaces;
    if (
      context != m_context || map.selection().nodes != expectedNodes
      || map.selection().brushFaces != expectedFaces)
      throw std::invalid_argument{"native visgroup replay selection/context differs"};
    for (auto* node : desiredNodes)
      if (unavailable(node))
        throw std::invalid_argument{"native visgroup replay would select hidden node"};
    for (const auto& face : desiredFaces)
      if (
        unavailable(&face.node()) || face.faceIndex() >= face.node().brush().faceCount())
        throw std::invalid_argument{"native visgroup replay face differs"};

    for (size_t i = 0; i < m_beforeFaces.size(); ++i)
    {
      const auto& face = m_beforeFaces[i];
      if (
        !live.contains(&face.node())
        || face.faceIndex() >= face.node().brush().faceCount()
        || face.face().lineNumber() != m_faceBindings[i])
        throw std::invalid_argument{"native visgroup selected face identity changed"};
    }

    // Scene validates first-dispatch freshness / semantic replay BEFORE publishing.
    const auto changed = scene->applyVisgroupChange(m_change, direction);
    if (expectedNodes != desiredNodes || expectedFaces != desiredFaces)
    {
      auto selection = SelectionChange{};
      selection.deselectedNodes = expectedNodes;
      selection.deselectedBrushFaces = expectedFaces;
      selection.selectedNodes = desiredNodes;
      selection.selectedBrushFaces = desiredFaces;
      map.selectionWillChangeNotifier();
      for (auto* node : expectedNodes)
        node->deselect();
      for (const auto& face : expectedFaces)
        face.node().deselectFace(face.faceIndex());
      for (auto* node : desiredNodes)
        node->select();
      for (const auto& face : desiredFaces)
        face.node().selectFace(face.faceIndex());
      map.selectionDidChangeNotifier(selection);
    }
    map.nodeVisibilityDidChangeNotifier(changed);
    m_planned = true;
    return true;
  }
  catch (const std::invalid_argument& e)
  {
    map.logger().warn() << "Native visgroup edit refused: " << e.what();
    return false;
  }
}
bool NativeVisgroupCommand::doPerformDo(Map& map)
{
  return apply(map, NativeVisgroupDirection::Forward);
}
bool NativeVisgroupCommand::doPerformUndo(Map& map)
{
  return apply(map, NativeVisgroupDirection::Backward);
}
} // namespace tb::mdl
