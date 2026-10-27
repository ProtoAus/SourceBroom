/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/Map_NativeCameras.h"

#include "mdl/Map.h"
#include "mdl/NativeCameraCommand.h"
#include "mdl/VmfScene.h"

#include <memory>
#include <stdexcept>
#include <utility>
namespace tb::mdl
{
NativeCameraSnapshot nativeCameras(const Map& map)
{
  if (const auto* scene = map.vmfScene())
  {
    return scene->cameras();
  }
  auto snapshot = NativeCameraSnapshot{};
  snapshot.refusalReason = "Camera authoring requires a native Source VMF document";
  return snapshot;
}
NativeCameraApplyResult applyNativeCameras(Map& map, const NativeCameraRequest& request)
{
  const auto snapshot = nativeCameras(map);
  if (!snapshot.refusalReason.empty())
  {
    return {NativeCameraApplyStatus::Refused, snapshot.refusalReason};
  }
  try
  {
    auto change = map.vmfScene()->prepareCameraChange(request);
    if (!change)
    {
      return {NativeCameraApplyStatus::Unchanged, {}};
    }
    if (map.executeAndStore(std::make_unique<NativeCameraCommand>(std::move(*change))))
    {
      return {NativeCameraApplyStatus::Changed, {}};
    }
    return {NativeCameraApplyStatus::Refused, "Native camera command was refused"};
  }
  catch (const std::invalid_argument& e)
  {
    return {NativeCameraApplyStatus::Refused, e.what()};
  }
}
} // namespace tb::mdl
