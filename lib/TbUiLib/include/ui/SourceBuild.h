/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QFile>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace tb::ui
{

enum class SourceBuildTarget
{
  Momentum,
  CounterStrikeSource,
  FteSurf
};

struct SourceBuildSettings
{
  QString vbsp;
  QString vvis;
  QString vrad;
  QStringList vbspArguments;
  QStringList vvisArguments;
  QStringList vradArguments;
  QString gameDirectory;
  QString stagingDirectory;
  QString engine;
  QString engineWorkingDirectory;
  QString mapsDirectory;
  // One array element per argument; {map} and {game} are substituted, not shell parsed.
  QStringList launchArguments;
  int toolTimeoutMs = 300000;
};

SourceBuildSettings defaultSourceBuildSettings(SourceBuildTarget target);
SourceBuildSettings loadSourceBuildSettings();
void saveSourceBuildSettings(const SourceBuildSettings& settings);

struct SourceBuildCommand
{
  QString program;
  QStringList arguments;
  QString workingDirectory;
};

// Each invocation owns a new staging directory. The supplied saved VMF is read only.
// This intentionally does not use CompilationExportMap or mdl::Map's Quake exporter.
class SourceBuild : public QObject
{
  Q_OBJECT
private:
  QProcess m_process;
  QFile m_log;
  QTimer m_timeout;
  SourceBuildSettings m_settings;
  QString m_stagedVmf;
  QString m_bsp;
  QString m_runDirectory;
  int m_step = 0;
  bool m_running = false;
  bool m_play = false;
  bool m_timedOut = false;

public:
  explicit SourceBuild(QObject* parent = nullptr);
  bool running() const;
  // Returns false on validation/staging failure (also emits finished(false, "")).
  bool start(const QString& savedVmf, const SourceBuildSettings& settings, bool play);

  static SourceBuildCommand compilerCommand(
    const SourceBuildSettings& settings, int step, const QString& stagedVmf);
  static QStringList engineArguments(
    const SourceBuildSettings& settings, const QString& mapName);
  static bool isSourceBsp(const QString& path);

signals:
  void output(const QString& text);
  // bspPath is nonempty only after all compiler steps and output validation succeed.
  void finished(bool success, const QString& bspPath);

private:
  void runNext();
  void fail(const QString& message);
  void complete();
};

} // namespace tb::ui
