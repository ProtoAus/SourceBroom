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
#include <QVector3D>

#include <optional>

namespace tb::ui
{

enum class SourceBuildTarget
{
  Momentum,
  CounterStrikeSource,
  FteSurf
};

enum class SourceBspTarget
{
  Generic,
  Css20,
  Strata25
};

struct SourceBuildDiagnostic
{
  QString text;
  std::optional<QVector3D> position;
  QString reference;
};

// Compiler indices are not native VMF IDs. Never use these references to select nodes.
SourceBuildDiagnostic parseSourceBuildDiagnostic(const QString& line);
// Strict, bounded Source .lin input, before handing it to the existing pointfile view.
bool validateSourceLeak(const QString& path, QString& error);

struct SourceBuildSettings
{
  SourceBspTarget bspTarget = SourceBspTarget::Generic;
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
  QString m_stdoutPending;
  QString m_stderrPending;

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
  static bool validateBsp(const QString& path, SourceBspTarget target, QString& message);

signals:
  void output(const QString& text);
  void diagnostic(const QString& line);
  // Only a validated .lin adjacent to this attempt's uniquely staged VMF.
  void leakReady(const QString& path);
  // bspPath is nonempty only after all compiler steps and output validation succeed.
  void finished(bool success, const QString& bspPath);

private:
  void consumeOutput(const QByteArray& bytes, QString& pending, bool flush = false);
  void inspectLeak();
  void runNext();
  void fail(const QString& message);
  void complete();
};

} // namespace tb::ui
