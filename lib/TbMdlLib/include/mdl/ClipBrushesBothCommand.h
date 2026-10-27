/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/Brush.h"
#include "mdl/NodeContents.h"
#include "mdl/UndoableCommand.h"
#include "mdl/VmfScene.h"

#include "kd/flat_map.h"

#include <memory>
#include <utility>
#include <vector>

namespace tb::mdl
{
class BrushNode;
class Node;
class SelectionCommand;

/** Native-only, single-history compound clip. Parked siblings belong to this command. */
class ClipBrushesBothCommand : public UndoableCommand
{
private:
  std::vector<BrushNode*> m_subjects;
  std::vector<std::pair<Node*, NodeContents>> m_keepers;
  std::vector<std::unique_ptr<BrushNode>> m_siblingOwners;
  std::vector<BrushNode*> m_siblings;
  std::vector<Node*> m_parents;
  std::vector<Brush> m_originals;
  std::vector<Brush> m_backStates;
  kdl::flat_map<Node*, std::vector<Node*>> m_additions;
  std::unique_ptr<SelectionCommand> m_deselect;
  std::unique_ptr<SelectionCommand> m_select;
  bool m_registered = false;
  VmfScene* m_scene = nullptr;
  std::optional<PreparedVmfRemoval> m_removal;
  std::optional<NativeVisgroupSnapshot> m_beforeVisgroups;
  std::optional<NativeVisgroupSnapshot> m_afterVisgroups;
  std::vector<size_t> m_keeperBindings;
  std::vector<size_t> m_afterBindings;

public:
  ClipBrushesBothCommand(
    std::vector<BrushNode*> subjects,
    std::vector<std::pair<Node*, NodeContents>> keepers,
    std::vector<std::unique_ptr<BrushNode>> siblings);
  ~ClipBrushesBothCommand() override;

private:
  void ownDetachedSiblings();
  bool doPerformDo(Map& map) override;
  bool doPerformUndo(Map& map) override;
};
} // namespace tb::mdl
