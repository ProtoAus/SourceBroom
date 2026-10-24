/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDialog>
#include <QString>
#include <QVector3D>

class QCloseEvent;

namespace tb::ui
{
class SourceBuild;

// Hook Build / Build & Play actions here only after successfully saving native VMF.
// initialPlay selects the primary button; nothing compiles or launches until clicked.
class SourceBuildDialog;
SourceBuildDialog* showSourceBuildDialog(
  QWidget* parent, const QString& savedVmf, bool initialPlay = false);

class SourceBuildDialog : public QDialog
{
  Q_OBJECT
private:
  SourceBuild* m_build;

public:
  SourceBuildDialog(QWidget* parent, const QString& savedVmf, bool initialPlay);
  void reject() override;

signals:
  void navigateToPosition(const QVector3D& position);
  void showLeak(const QString& path);

protected:
  void closeEvent(QCloseEvent* event) override;
};
} // namespace tb::ui
