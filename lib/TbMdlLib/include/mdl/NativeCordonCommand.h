/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/NativeCordons.h"
#include "mdl/UndoableCommand.h"

namespace tb::mdl
{
class NativeCordonCommand : public UndoableCommand
{
private:
  PreparedNativeCordonChange m_change;
  bool apply(Map& map, NativeCordonDirection direction);
  bool doPerformDo(Map& map) override;
  bool doPerformUndo(Map& map) override;

public:
  explicit NativeCordonCommand(PreparedNativeCordonChange change);
};
} // namespace tb::mdl
