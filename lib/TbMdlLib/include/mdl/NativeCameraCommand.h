/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include "mdl/NativeCameras.h"
#include "mdl/UndoableCommand.h"
namespace tb::mdl
{
class NativeCameraCommand : public UndoableCommand
{
private:
  PreparedNativeCameraChange m_change;
  bool apply(Map& map, NativeCameraDirection direction);
  bool doPerformDo(Map& map) override;
  bool doPerformUndo(Map& map) override;

public:
  explicit NativeCameraCommand(PreparedNativeCameraChange change);
};
} // namespace tb::mdl
