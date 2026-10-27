/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/Map_NativeVisgroups.h"

#include "mdl/BrushNode.h"
#include "mdl/Map.h"
#include "mdl/NativeVisgroupCommand.h"
#include "mdl/Node.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

namespace tb::mdl
{
NativeVisgroupSnapshot nativeVisgroups(const Map& map)
{
  if (const auto* scene = map.vmfScene())
  {
    return scene->visgroups();
  }
  auto snapshot = NativeVisgroupSnapshot{};
  snapshot.refusalReason = "Visgroup authoring requires a native Source VMF document";
  return snapshot;
}

NativeVisgroupView nativeVisgroupView(const Map& map)
{
  auto view = NativeVisgroupView{nativeVisgroups(map), {}};
  auto indices = std::map<NativeVisgroupMemberId, size_t>{};
  for (const auto& member : view.metadata.members)
  {
    if (member.active && member.projected)
      indices.emplace(member.id, view.members.size());
    view.members.push_back({member.id, std::nullopt, std::nullopt});
  }
  if (const auto* scene = map.vmfScene())
  {
    if (scene->isReadOnlyPreview())
      view.metadata.refusalReason =
        "Visgroup authoring is unavailable in read-only VMF preview";
    const auto visit = [&](auto&& self, const Node& node) -> void {
      if (const auto member = scene->registeredVisgroupMember(node))
      {
        const auto found = indices.find(*member);
        if (found != indices.end())
        {
          auto& display = view.members[found->second];
          display.effectiveNativeVisibility = node.effectiveNativeVisibility();
          display.sessionVisible = node.sessionVisible();
        }
      }
      for (const auto* child : node.children())
        self(self, *child);
    };
    visit(visit, map.worldNode());
  }
  return view;
}

NativeVisgroupApplyResult applyNativeVisgroups(
  Map& map, const NativeVisgroupRequest& request)
{
  const auto snapshot = nativeVisgroups(map);
  if (!snapshot.refusalReason.empty())
  {
    return {NativeVisgroupApplyStatus::Refused, snapshot.refusalReason};
  }
  try
  {
    auto change = map.vmfScene()->prepareVisgroupChange(request);
    if (!change)
    {
      return {NativeVisgroupApplyStatus::Unchanged, {}};
    }
    if (map.executeAndStore(std::make_unique<NativeVisgroupCommand>(std::move(*change))))
    {
      return {NativeVisgroupApplyStatus::Changed, {}};
    }
    return {
      NativeVisgroupApplyStatus::Refused,
      "Native visgroup edit refused; see diagnostic log"};
  }
  catch (const std::invalid_argument& e)
  {
    return {NativeVisgroupApplyStatus::Refused, e.what()};
  }
}
std::vector<NativeVisgroupMemberId> nativeVisgroupSelection(const Map& map)
{
  auto result = std::vector<NativeVisgroupMemberId>{};
  if (!map.vmfScene())
    return result;
  const auto snapshot = nativeVisgroups(map);
  auto roots = map.selection().nodes;
  for (const auto& face : map.selection().brushFaces)
    if (std::ranges::find(roots, &face.node()) == roots.end())
      roots.push_back(&face.node());
  auto members = std::set<NativeVisgroupMemberId>{};
  for (const auto& member : snapshot.members)
    if (member.active && member.projected)
      members.insert(member.id);
  for (const auto* node : roots)
  {
    const auto member = map.vmfScene()->registeredVisgroupMember(*node);
    if (member && members.contains(*member))
      result.push_back(*member);
  }
  return result;
}
} // namespace tb::mdl
