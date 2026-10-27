/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include <QWidget>

#include "base/NotifierConnection.h"
#include "mdl/NativeCameras.h"
#include "ui/NativeCameraCapture.h"

#include <functional>
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
namespace tb::ui
{
class MapDocument;
class NativeCameraEditor : public QWidget
{
private:
  MapDocument& m_document;
  NativeCameraCapture m_capture;
  NotifierConnection m_connections;
  mdl::NativeCameraSnapshot m_snapshot;
  uint64_t m_rowRevision = 0;
  std::function<void()> m_pendingAction;
  QPushButton* m_pendingButton = nullptr;
  QComboBox* m_records;
  QComboBox* m_active;
  QLineEdit* m_eye;
  QLineEdit* m_look;
  QLabel* m_status;
  QPushButton* m_captureView;
  QPushButton* m_add;
  QPushButton* m_applyPose;
  QPushButton* m_delete;
  QPushButton* m_applyActive;
  QPushButton* m_remove;
  void refresh();
  void showRow();
  void dispatch(const mdl::NativeCameraRequest& request);

public:
  explicit NativeCameraEditor(
    MapDocument& document, QWidget* parent = nullptr, NativeCameraCapture capture = {});
};
} // namespace tb::ui
