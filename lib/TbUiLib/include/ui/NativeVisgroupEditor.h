/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QWidget>

#include "base/NotifierConnection.h"
#include "mdl/Map_NativeVisgroups.h"

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace tb::ui
{
class MapDocument;
class NativeVisgroupEditor : public QWidget
{
  Q_OBJECT
private:
  MapDocument& m_document;
  NotifierConnection m_connections;
  mdl::NativeVisgroupView m_view;
  uint64_t m_rowRevision = 0;
  std::function<void()> m_pendingAction;
  QPushButton* m_pendingButton = nullptr;
  QTreeWidget* m_tree;
  QTreeWidget* m_members;
  QComboBox* m_parent;
  QLineEdit* m_name;
  QLabel* m_summary;
  QLabel* m_status;
  QPushButton* m_create;
  QPushButton* m_apply;
  QPushButton* m_delete;
  QPushButton* m_show;
  QPushButton* m_hide;
  QPushButton* m_assignSelection;
  QPushButton* m_unassignSelection;
  QPushButton* m_assignMembers;
  QPushButton* m_unassignMembers;

public:
  explicit NativeVisgroupEditor(MapDocument& document, QWidget* parent = nullptr);

private:
  void refresh();
  void showRows();
  int recordIndex() const;
  void dispatch(const mdl::NativeVisgroupRequest& request);
};
} // namespace tb::ui
