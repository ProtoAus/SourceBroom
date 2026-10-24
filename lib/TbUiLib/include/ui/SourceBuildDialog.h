/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDialog>
#include <QString>

class QCloseEvent;

namespace tb::ui
{
class SourceBuild;

// Hook Build / Build & Play actions here only after successfully saving native VMF.
// initialPlay selects the primary button; nothing compiles or launches until clicked.
QDialog* showSourceBuildDialog(
  QWidget* parent, const QString& savedVmf, bool initialPlay = false);

class SourceBuildDialog : public QDialog
{
private:
  SourceBuild* m_build;

public:
  SourceBuildDialog(QWidget* parent, const QString& savedVmf, bool initialPlay);
  void reject() override;

protected:
  void closeEvent(QCloseEvent* event) override;
};
} // namespace tb::ui
