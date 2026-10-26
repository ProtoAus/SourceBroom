/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/Map_NativeCordons.h"

#include "mdl/Map.h"
#include "mdl/NativeCordonCommand.h"
#include "mdl/VmfScene.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace tb::mdl
{
NativeCordonSnapshot nativeCordons(const Map& map)
{
  if (const auto* scene = map.vmfScene())
  {
    return scene->cordons();
  }
  auto snapshot = NativeCordonSnapshot{};
  snapshot.refusalReason = "Cordon authoring requires a native Source VMF document";
  return snapshot;
}

NativeCordonApplyResult applyNativeCordons(Map& map, const NativeCordonRequest& request)
{
  const auto snapshot = nativeCordons(map);
  if (!snapshot.refusalReason.empty())
  {
    return {NativeCordonApplyStatus::Refused, snapshot.refusalReason};
  }
  try
  {
    auto change = map.vmfScene()->prepareCordonChange(request);
    if (!change)
    {
      return {NativeCordonApplyStatus::Unchanged, {}};
    }
    if (map.executeAndStore(std::make_unique<NativeCordonCommand>(std::move(*change))))
    {
      return {NativeCordonApplyStatus::Changed, {}};
    }
    return {
      NativeCordonApplyStatus::Refused, "Native cordon state changed before dispatch"};
  }
  catch (const std::invalid_argument& e)
  {
    return {NativeCordonApplyStatus::Refused, e.what()};
  }
}
} // namespace tb::mdl
