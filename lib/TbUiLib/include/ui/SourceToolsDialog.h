/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/SourceDependencyPack.h"
#include "mdl/SourceEditorMetadata.h"
#include "ui/SourceBuild.h"

#include <QDialog>
#include <QVector3D>

#include <optional>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace tb::ui
{
/** Saved-native metadata and explicit custom-content packaging. No metadata save
 * or installed-game asset resolver is exposed by this dialog. */
class SourceToolsDialog : public QDialog
{
  Q_OBJECT
private:
  QString m_savedVmf;
  SourceBspTarget m_target;
  std::optional<mdl::VmfDocument> m_document;
  mdl::SourceEditorMetadata m_metadata;
  std::optional<mdl::SourceDependencyPackPlan> m_plan;
  QPlainTextEdit* m_metadataText;
  QComboBox* m_cameras;
  QPushButton* m_goCamera;
  QLineEdit* m_customRoot;
  QLineEdit* m_bsp;
  QCheckBox* m_rights;
  QCheckBox* m_approval;
  QPlainTextEdit* m_packText;
  QPushButton* m_review;
  QPushButton* m_export;

public:
  SourceToolsDialog(QWidget* parent, QString savedVmf, SourceBspTarget target);

signals:
  void navigateToPosition(const QVector3D& position);

private:
  void reloadDocument();
  void invalidatePlan();
  void updateExportEnabled();
  void createPlan();
  void exportPlan();
  void updateCameraButton();
  void navigateCamera();
  std::optional<QVector3D> cameraEye() const;
};
} // namespace tb::ui
