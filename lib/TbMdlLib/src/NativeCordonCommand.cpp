/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/NativeCordonCommand.h"

#include "base/Logger.h"
#include "mdl/Map.h"
#include "mdl/VmfScene.h"

#include <stdexcept>
#include <utility>

namespace tb::mdl
{
NativeCordonCommand::NativeCordonCommand(PreparedNativeCordonChange change)
  : UndoableCommand{"Edit Native Cordons", true}
  , m_change{std::move(change)}
{
}

bool NativeCordonCommand::apply(Map& map, const NativeCordonDirection direction)
{
  if (auto* scene = map.vmfScene())
  {
    try
    {
      scene->applyCordonChange(m_change, direction);
      return true;
    }
    catch (const std::invalid_argument& e)
    {
      map.logger().warn() << "Native cordon edit refused: " << e.what();
    }
  }
  return false;
}

bool NativeCordonCommand::doPerformDo(Map& map)
{
  return apply(map, NativeCordonDirection::Forward);
}

bool NativeCordonCommand::doPerformUndo(Map& map)
{
  return apply(map, NativeCordonDirection::Backward);
}
} // namespace tb::mdl
