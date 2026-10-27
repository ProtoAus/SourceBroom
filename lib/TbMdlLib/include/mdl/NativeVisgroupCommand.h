/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include "mdl/BrushFaceHandle.h"
#include "mdl/NativeVisgroups.h"
#include "mdl/UndoableCommand.h"
namespace tb::mdl
{
class GroupNode;
class Node;
class NativeVisgroupCommand : public UndoableCommand
{
private:
  PreparedNativeVisgroupChange m_change;
  std::vector<Node*> m_beforeNodes, m_afterNodes;
  std::vector<BrushFaceHandle> m_beforeFaces, m_afterFaces;
  std::vector<size_t> m_faceBindings;
  GroupNode* m_context = nullptr;
  bool m_planned = false;
  bool apply(Map& map, NativeVisgroupDirection direction);
  bool doPerformDo(Map& map) override;
  bool doPerformUndo(Map& map) override;

public:
  explicit NativeVisgroupCommand(PreparedNativeVisgroupChange change);
};
} // namespace tb::mdl
