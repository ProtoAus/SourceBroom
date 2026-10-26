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

#include "mdl/SwapNodeContentsCommand.h"

#include "base/Logger.h"
#include "base/Notifier.h"
#include "mdl/EditorContext.h"
#include "mdl/GameInfo.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/Map_Assets.h"
#include "mdl/Map_World.h"
#include "mdl/Node.h"
#include "mdl/NodeQueries.h"
#include "mdl/VmfScene.h"

#include "kd/ranges/to.h"

#include <ranges>
#include <stdexcept>

namespace tb::mdl
{
namespace
{

auto notifySpecialWorldProperties(
  const std::vector<std::pair<Node*, NodeContents>>& nodesToSwap)
{
  for (const auto& [node, contents] : nodesToSwap)
  {
    if (const auto* worldNode = dynamic_cast<const WorldNode*>(node))
    {
      const auto& oldEntity = worldNode->entity();
      const auto& newEntity = std::get<Entity>(contents.get());

      const auto* oldWads = oldEntity.property(EntityPropertyKeys::Wad);
      const auto* newWads = newEntity.property(EntityPropertyKeys::Wad);

      const bool notifyWadsChange =
        (oldWads == nullptr) != (newWads == nullptr)
        || (oldWads != nullptr && newWads != nullptr && *oldWads != *newWads);

      const auto oldEntityDefinitionSpec = entityDefinitionFile(oldEntity);
      const auto newEntityDefinitionSpec = entityDefinitionFile(newEntity);
      const bool notifyEntityDefinitionsChange =
        oldEntityDefinitionSpec != newEntityDefinitionSpec;

      const auto oldMods = enabledMods(oldEntity);
      const auto newMods = enabledMods(newEntity);
      const bool notifyModsChange = oldMods != newMods;

      return std::tuple{
        notifyWadsChange, notifyEntityDefinitionsChange, notifyModsChange};
    }
  }

  return std::tuple{false, false, false};
}

void doSwapNodeContents(
  std::vector<std::pair<Node*, NodeContents>>& nodesToSwap, Map& map)
{
  const auto nodes = nodesToSwap | std::views::keys | kdl::ranges::to<std::vector>();

  auto notifyNodes =
    NotifyBeforeAndAfter{map.nodesWillChangeNotifier, map.nodesDidChangeNotifier, nodes};

  const auto [notifyWadsChange, notifyEntityDefinitionsChange, notifyModsChange] =
    notifySpecialWorldProperties(nodesToSwap);
  auto notifyWads = NotifyBeforeAndAfter{
    notifyWadsChange,
    map.materialCollectionsWillChangeNotifier,
    map.materialCollectionsDidChangeNotifier};
  auto notifyEntityDefinitions = NotifyBeforeAndAfter{
    notifyEntityDefinitionsChange,
    map.entityDefinitionsWillChangeNotifier,
    map.entityDefinitionsDidChangeNotifier};
  auto notifyMods = NotifyBeforeAndAfter{
    notifyModsChange, map.modsWillChangeNotifier, map.modsDidChangeNotifier};

  for (auto& pair : nodesToSwap)
  {
    auto* node = pair.first;
    auto& contents = pair.second.get();

    pair.second = node->accept(
      kdl::overload(
        [&](WorldNode& worldNode) {
          return NodeContents{worldNode.setEntity(std::get<Entity>(std::move(contents)))};
        },
        [&](LayerNode& layerNode) {
          return NodeContents(layerNode.setLayer(std::get<Layer>(std::move(contents))));
        },
        [&](GroupNode& groupNode) {
          return NodeContents{groupNode.setGroup(std::get<Group>(std::move(contents)))};
        },
        [&](EntityNode& entityNode) {
          return NodeContents{
            entityNode.setEntity(std::get<Entity>(std::move(contents)))};
        },
        [&](BrushNode& brushNode) {
          return NodeContents{brushNode.setBrush(std::get<Brush>(std::move(contents)))};
        },
        [&](PatchNode& patchNode) {
          return NodeContents{
            patchNode.setPatch(std::get<BezierPatch>(std::move(contents)))};
        }));
  }
}

} // namespace

SwapNodeContentsCommand::SwapNodeContentsCommand(
  std::string name,
  std::vector<std::pair<Node*, NodeContents>> nodes,
  const bool nativeClip)
  : UpdateLinkedGroupsCommandBase{std::move(name), true}
  , m_nodes{std::move(nodes)}
  , m_nativeClip{nativeClip}
{
}

SwapNodeContentsCommand::~SwapNodeContentsCommand() = default;

bool SwapNodeContentsCommand::doPerformDo(Map& map)
{
  if (auto* scene = map.vmfScene())
  {
    try
    {
      if (m_nativeClip)
      {
        for (const auto& [node, contents] : m_nodes)
        {
          if (
            !node || node->parent() != map.worldNode().defaultLayer()
            || !map.editorContext().visible(*node))
          {
            throw std::invalid_argument{
              "native clip target is not a visible world brush"};
          }
        }
      }
      if (m_nativeClip && !m_clipRegistered)
      {
        scene->registerClips(m_nodes, map.worldBounds());
        m_clipRegistered = true;
      }
      else
      {
        for (const auto& [node, contents] : m_nodes)
        {
          if (m_nativeClip)
          {
            scene->validateClipContents(*node, contents, map.worldBounds());
          }
          else
          {
            scene->validateContents(*node, contents, map.worldBounds());
          }
        }
      }
    }
    catch (const std::exception& e)
    {
      map.logger().error() << "Native VMF content edit refused: " << e.what();
      return false;
    }
  }
  doSwapNodeContents(m_nodes, map);
  return true;
}

bool SwapNodeContentsCommand::doPerformUndo(Map& map)
{
  doSwapNodeContents(m_nodes, map);
  return true;
}

bool SwapNodeContentsCommand::doCollateWith(UndoableCommand& command)
{
  if (m_nativeClip)
  {
    return false;
  }
  if (auto* other = dynamic_cast<SwapNodeContentsCommand*>(&command))
  {
    if (other->m_nativeClip)
    {
      return false;
    }
    auto myNodes = m_nodes | std::views::keys | kdl::ranges::to<std::vector>();
    auto theirNodes = other->m_nodes | std::views::keys | kdl::ranges::to<std::vector>();

    std::ranges::sort(myNodes);
    std::ranges::sort(theirNodes);

    return myNodes == theirNodes;
  }

  return false;
}

} // namespace tb::mdl
