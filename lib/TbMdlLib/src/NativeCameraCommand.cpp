/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/NativeCameraCommand.h"

#include "base/Logger.h"
#include "mdl/Map.h"
#include "mdl/VmfScene.h"

#include <stdexcept>
#include <utility>
namespace tb::mdl
{
NativeCameraCommand::NativeCameraCommand(PreparedNativeCameraChange change)
  : UndoableCommand{"Edit Native Cameras", true}
  , m_change{std::move(change)}
{
}
bool NativeCameraCommand::apply(Map& map, NativeCameraDirection direction)
{
  if (auto* scene = map.vmfScene())
  {
    try
    {
      scene->applyCameraChange(m_change, direction);
      return true;
    }
    catch (const std::invalid_argument& e)
    {
      map.logger().warn() << "Native camera edit refused: " << e.what();
    }
  }
  return false;
}
bool NativeCameraCommand::doPerformDo(Map& map)
{
  return apply(map, NativeCameraDirection::Forward);
}
bool NativeCameraCommand::doPerformUndo(Map& map)
{
  return apply(map, NativeCameraDirection::Backward);
}
} // namespace tb::mdl
