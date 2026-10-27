/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include "mdl/NativeCameras.h"

#include <string>
namespace tb::mdl
{
class Map;
enum class NativeCameraApplyStatus
{
  Unchanged,
  Changed,
  Refused
};
struct NativeCameraApplyResult
{
  NativeCameraApplyStatus status;
  std::string reason;
};
NativeCameraSnapshot nativeCameras(const Map& map);
NativeCameraApplyResult applyNativeCameras(Map& map, const NativeCameraRequest& request);
} // namespace tb::mdl
