/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/SourceBuildDialog.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

#include "ui/SourceBuild.h"

#include <array>

namespace tb::ui
{
QDialog* showSourceBuildDialog(
  QWidget* parent, const QString& savedVmf, const bool initialPlay)
{
  auto* dialog = new SourceBuildDialog{parent, savedVmf, initialPlay};
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
  return dialog;
}

SourceBuildDialog::SourceBuildDialog(
  QWidget* parent, const QString& savedVmf, const bool initialPlay)
  : QDialog{parent}
  , m_build{new SourceBuild{this}}
{
  setWindowTitle(tr("Source Build / Build & Play"));
  resize(850, 800);
  auto* layout = new QVBoxLayout{this};
  auto* description = new QLabel{
    tr(
      "Builds a new staged copy of the saved VMF, not unsaved editor changes.\n"
      "Build & Play copies a uniquely named BSP; never overwrites a map.\n"
      "Runtime may write its normal game settings. Close the game yourself."),
    this};
  layout->addWidget(description);
  auto* source = new QLineEdit{savedVmf, this};
  source->setReadOnly(true);
  layout->addWidget(source);
  auto* presets = new QComboBox{this};
  presets->addItems(
    {tr("Load preset…"),
     tr("Momentum"),
     tr("Counter-Strike: Source"),
     tr("FTESurf (optional)")});
  layout->addWidget(presets);
  auto* fields = new QWidget{this};
  auto* form = new QFormLayout{fields};
  layout->addWidget(fields);
  using PathMember = QString SourceBuildSettings::*;
  const auto members = std::array<PathMember, 8>{
    &SourceBuildSettings::vbsp,
    &SourceBuildSettings::vvis,
    &SourceBuildSettings::vrad,
    &SourceBuildSettings::gameDirectory,
    &SourceBuildSettings::stagingDirectory,
    &SourceBuildSettings::engine,
    &SourceBuildSettings::engineWorkingDirectory,
    &SourceBuildSettings::mapsDirectory};
  const auto labels = QStringList{
    tr("VBSP"),
    tr("VVIS"),
    tr("VRAD"),
    tr("Compiler game directory"),
    tr("Owned staging root"),
    tr("Runtime executable"),
    tr("Runtime working directory"),
    tr("Runtime maps directory")};
  auto edits = std::array<QLineEdit*, 8>{};
  for (auto i = 0; i < 8; ++i)
  {
    auto* row = new QWidget{fields};
    auto* rowLayout = new QHBoxLayout{row};
    rowLayout->setContentsMargins(0, 0, 0, 0);
    auto* edit = new QLineEdit{row};
    edits[i] = edit;
    auto* browse = new QPushButton{tr("Browse…"), row};
    rowLayout->addWidget(edit);
    rowLayout->addWidget(browse);
    form->addRow(labels[i], row);
    connect(browse, &QPushButton::clicked, this, [this, edit, i] {
      const auto directory = i == 3 || i == 4 || i == 6 || i == 7;
      const auto selected =
        directory
          ? QFileDialog::getExistingDirectory(this, tr("Choose directory"), edit->text())
          : QFileDialog::getOpenFileName(this, tr("Choose executable"), edit->text());
      if (!selected.isEmpty())
      {
        edit->setText(selected);
      }
    });
  }
  using ArgumentMember = QStringList SourceBuildSettings::*;
  const auto argumentMembers = std::array<ArgumentMember, 3>{
    &SourceBuildSettings::vbspArguments,
    &SourceBuildSettings::vvisArguments,
    &SourceBuildSettings::vradArguments};
  auto compilerArguments = std::array<QPlainTextEdit*, 3>{};
  for (auto i = 0; i < 3; ++i)
  {
    compilerArguments[i] = new QPlainTextEdit{fields};
    compilerArguments[i]->setMaximumHeight(55);
    form->addRow(labels[i] + tr(" extra args (one per line)"), compilerArguments[i]);
  }
  auto* arguments = new QPlainTextEdit{fields};
  arguments->setMaximumHeight(110);
  form->addRow(tr("Launch args (one per line; {map}, {game})"), arguments);
  const auto populate = [edits, members, arguments, compilerArguments, argumentMembers](
                          const SourceBuildSettings& s) {
    for (auto i = 0; i < 8; ++i)
    {
      edits[i]->setText(s.*members[i]);
    }
    arguments->setPlainText(s.launchArguments.join('\n'));
    for (auto i = 0; i < 3; ++i)
    {
      compilerArguments[i]->setPlainText((s.*argumentMembers[i]).join('\n'));
    }
  };
  populate(loadSourceBuildSettings());
  connect(presets, &QComboBox::currentIndexChanged, this, [populate](int index) {
    if (index > 0)
    {
      populate(defaultSourceBuildSettings(static_cast<SourceBuildTarget>(index - 1)));
    }
  });
  auto* log = new QPlainTextEdit{this};
  log->setReadOnly(true);
  layout->addWidget(log, 1);
  auto* buttons = new QHBoxLayout{};
  layout->addLayout(buttons);
  auto* build = new QPushButton{tr("Build"), this};
  auto* play = new QPushButton{tr("Build & Play"), this};
  auto* close = new QPushButton{tr("Close"), this};
  build->setDefault(!initialPlay);
  play->setDefault(initialPlay);
  buttons->addWidget(build);
  buttons->addWidget(play);
  buttons->addStretch();
  buttons->addWidget(close);
  const auto start = [this,
                      savedVmf,
                      edits,
                      members,
                      arguments,
                      compilerArguments,
                      argumentMembers,
                      fields,
                      presets,
                      build,
                      play,
                      close](bool run) {
    auto settings = SourceBuildSettings{};
    for (auto i = 0; i < 8; ++i)
    {
      settings.*members[i] = edits[i]->text();
    }
    for (auto i = 0; i < 3; ++i)
    {
      settings.*argumentMembers[i] =
        compilerArguments[i]->toPlainText().split('\n', Qt::SkipEmptyParts);
    }
    settings.launchArguments = arguments->toPlainText().split('\n', Qt::KeepEmptyParts);
    saveSourceBuildSettings(settings);
    fields->setEnabled(false);
    presets->setEnabled(false);
    build->setEnabled(false);
    play->setEnabled(false);
    close->setEnabled(false);
    m_build->start(savedVmf, settings, run);
  };
  connect(build, &QPushButton::clicked, this, [start] { start(false); });
  connect(play, &QPushButton::clicked, this, [start] { start(true); });
  connect(close, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_build, &SourceBuild::output, this, [log](const QString& text) {
    log->moveCursor(QTextCursor::End);
    log->insertPlainText(text);
    log->ensureCursorVisible();
  });
  connect(
    m_build,
    &SourceBuild::finished,
    this,
    [fields, presets, build, play, close](bool, const QString&) {
      fields->setEnabled(true);
      presets->setEnabled(true);
      build->setEnabled(true);
      play->setEnabled(true);
      close->setEnabled(true);
    });
}

void SourceBuildDialog::reject()
{
  if (!m_build->running())
  {
    QDialog::reject();
  }
}

void SourceBuildDialog::closeEvent(QCloseEvent* event)
{
  if (m_build->running())
  {
    event->ignore();
  }
  else
  {
    QDialog::closeEvent(event);
  }
}
} // namespace tb::ui
