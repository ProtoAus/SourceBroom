/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QWidget>

#include "base/NotifierConnection.h"
#include "mdl/NativeCordons.h"

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace tb::ui
{
class MapDocument;
class NativeCordonEditor : public QWidget
{
private:
  MapDocument& m_document;
  std::function<void()> m_pendingAction;
  uint64_t m_rowRevision = 0;
  NotifierConnection m_connections;
  mdl::NativeCordonSnapshot m_snapshot;
  QComboBox* m_records;
  QComboBox* m_boxes;
  QComboBox* m_global;
  QComboBox* m_active;
  QLineEdit* m_name;
  QLineEdit* m_mins;
  QLineEdit* m_maxs;
  QLabel* m_status;
  QLabel* m_form;
  QPushButton* m_addRecord;
  QPushButton* m_addBox;
  QPushButton* m_deleteRecord;
  QPushButton* m_deleteBox;
  QPushButton* m_clear;
  QPushButton* m_apply;
  QPushButton* m_fit;

public:
  explicit NativeCordonEditor(MapDocument& document, QWidget* parent = nullptr);

private:
  void refresh();
  void showRows();
  void dispatch(const mdl::NativeCordonRequest& request);
};
} // namespace tb::ui
