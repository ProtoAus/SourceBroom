/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/UndoableCommand.h"

#include <memory>
#include <vector>

namespace tb::mdl
{
class GroupNode;
class Node;
class SelectionCommand;

class NativeGroupCommand : public UndoableCommand
{
private:
  struct Placement
  {
    Node* node;
    Node* parent;
    size_t index;
  };
  std::vector<Placement> m_before;
  std::vector<Placement> m_after;
  struct ClosureNode
  {
    Node* node;
    size_t binding;
    std::vector<Node*> children;
  };
  std::vector<ClosureNode> m_closure;
  void captureClosure(Node& node);
  std::vector<GroupNode*> m_containers;
  std::vector<size_t> m_containerBindings;
  std::vector<std::unique_ptr<GroupNode>> m_owners;
  std::vector<std::pair<Node*, std::unique_ptr<Node>>> m_parkedMembers;
  std::unique_ptr<SelectionCommand> m_deselect;
  std::unique_ptr<SelectionCommand> m_select;
  bool m_grouping;
  bool m_registered = false;

  void reconcileOwnership();
  bool validate(Map& map, const std::vector<Placement>& expected) const;
  void move(
    Map& map, const std::vector<Placement>& from, const std::vector<Placement>& to);
  bool doPerformDo(Map& map) override;
  bool doPerformUndo(Map& map) override;

public:
  NativeGroupCommand(std::vector<Node*> nodes, Node& parent);
  explicit NativeGroupCommand(std::vector<GroupNode*> groups);
  ~NativeGroupCommand() override;
  GroupNode* createdGroup() const;
};
} // namespace tb::mdl
