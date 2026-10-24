/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/SourceBuild.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

namespace tb::ui
{
namespace
{
const auto settingsKeys = QStringList{
  "vbsp",
  "vvis",
  "vrad",
  "gameDirectory",
  "stagingDirectory",
  "engine",
  "engineWorkingDirectory",
  "mapsDirectory"};

QStringList settingsPaths(const SourceBuildSettings& s)
{
  return {
    s.vbsp,
    s.vvis,
    s.vrad,
    s.gameDirectory,
    s.stagingDirectory,
    s.engine,
    s.engineWorkingDirectory,
    s.mapsDirectory};
}

void setSettingsPaths(SourceBuildSettings& s, const QStringList& paths)
{
  s.vbsp = paths[0];
  s.vvis = paths[1];
  s.vrad = paths[2];
  s.gameDirectory = paths[3];
  s.stagingDirectory = paths[4];
  s.engine = paths[5];
  s.engineWorkingDirectory = paths[6];
  s.mapsDirectory = paths[7];
}
} // namespace

SourceBuildSettings defaultSourceBuildSettings(const SourceBuildTarget target)
{
  auto s = SourceBuildSettings{};
  const auto common = QStringLiteral("C:/Program Files (x86)/Steam/steamapps/common/");
  const auto momentum = common + "Momentum Mod Playtest";
  const auto css = common + "Counter-Strike Source";
  const auto tools =
    target == SourceBuildTarget::CounterStrikeSource
      ? QStringLiteral("C:/HammerDream/Hammer++ Compilers/tools++_windows")
      : momentum + "/bin/win64";
  const auto suffix =
    target == SourceBuildTarget::CounterStrikeSource ? "++.exe" : ".exe";
  s.vbsp = tools + "/vbsp" + suffix;
  s.vvis = tools + "/vvis" + suffix;
  s.vrad = tools + "/vrad" + suffix;
  if (target == SourceBuildTarget::CounterStrikeSource)
  {
    // Tools++ otherwise probes Vulkan/GPU on startup, which may stall on some drivers.
    s.vradArguments = {"-cpu"};
  }
  s.gameDirectory = target == SourceBuildTarget::CounterStrikeSource
                      ? css + "/cstrike"
                      : momentum + "/momentum";
  s.stagingDirectory =
    QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/source-builds";
  s.engine = target == SourceBuildTarget::CounterStrikeSource
               ? css + "/cstrike_win64.exe"
               : momentum + "/bin/win64/momentum.exe";
  s.engineWorkingDirectory =
    target == SourceBuildTarget::CounterStrikeSource ? css : momentum;
  s.mapsDirectory = s.gameDirectory + "/maps";
  s.launchArguments = {"-game", "{game}", "+map", "{map}"};
  if (target == SourceBuildTarget::FteSurf)
  {
    // FTESurf is an optional runtime, not the compiler's game/content directory.
    s.engine = "C:/FTESurf/ftesurf64.exe";
    s.engineWorkingDirectory = "C:/FTESurf";
    s.mapsDirectory = "C:/FTESurf/ftesurf/maps";
    s.launchArguments = {"+cfg_save_auto", "0", "+map", "{map}"};
  }
  return s;
}

SourceBuildSettings loadSourceBuildSettings()
{
  auto s = defaultSourceBuildSettings(SourceBuildTarget::Momentum);
  auto prefs = QSettings{};
  prefs.beginGroup("SourceBroom/SourceBuild");
  auto paths = settingsPaths(s);
  for (auto i = 0; i < paths.size(); ++i)
  {
    paths[i] = prefs.value(settingsKeys[i], paths[i]).toString();
  }
  setSettingsPaths(s, paths);
  s.vbspArguments = prefs.value("vbspArguments", s.vbspArguments).toStringList();
  s.vvisArguments = prefs.value("vvisArguments", s.vvisArguments).toStringList();
  s.vradArguments = prefs.value("vradArguments", s.vradArguments).toStringList();
  s.launchArguments = prefs.value("launchArguments", s.launchArguments).toStringList();
  return s;
}

void saveSourceBuildSettings(const SourceBuildSettings& s)
{
  auto prefs = QSettings{};
  prefs.beginGroup("SourceBroom/SourceBuild");
  const auto paths = settingsPaths(s);
  for (auto i = 0; i < paths.size(); ++i)
  {
    prefs.setValue(settingsKeys[i], paths[i]);
  }
  prefs.setValue("vbspArguments", s.vbspArguments);
  prefs.setValue("vvisArguments", s.vvisArguments);
  prefs.setValue("vradArguments", s.vradArguments);
  prefs.setValue("launchArguments", s.launchArguments);
}

SourceBuild::SourceBuild(QObject* parent)
  : QObject{parent}
{
  m_timeout.setSingleShot(true);
  connect(this, &SourceBuild::output, this, [this](const QString& text) {
    if (m_log.isOpen())
    {
      m_log.write(text.toUtf8());
      m_log.flush();
    }
  });
  connect(&m_timeout, &QTimer::timeout, this, [this] {
    m_timedOut = true;
    emit output("Compiler timed out; terminating this build's child process.\n");
    m_process.kill();
  });
  connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
    emit output(QString::fromLocal8Bit(m_process.readAllStandardOutput()));
  });
  connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
    emit output(QString::fromLocal8Bit(m_process.readAllStandardError()));
  });
  connect(&m_process, &QProcess::errorOccurred, this, [this](auto error) {
    emit output("Process error: " + m_process.errorString() + "\n");
    if (m_running && error == QProcess::FailedToStart)
    {
      fail("Could not start compiler: " + m_process.errorString());
    }
  });
  connect(&m_process, &QProcess::finished, this, [this](int code, auto status) {
    m_timeout.stop();
    emit output(QString::fromLocal8Bit(m_process.readAllStandardOutput()));
    emit output(QString::fromLocal8Bit(m_process.readAllStandardError()));
    if (!m_running)
    {
      return;
    }
    emit output(QString("Compiler exit code: %1\n").arg(code));
    if (m_timedOut || status != QProcess::NormalExit || code != 0)
    {
      fail("Compiler failed; no BSP will be promoted or launched.");
      return;
    }
    if (!isSourceBsp(m_bsp))
    {
      fail("Compiler did not leave a valid staged Source BSP.");
      return;
    }
    ++m_step;
    runNext();
  });
}

bool SourceBuild::running() const
{
  return m_running;
}

SourceBuildCommand SourceBuild::compilerCommand(
  const SourceBuildSettings& s, const int step, const QString& stagedVmf)
{
  const auto program = step == 0 ? s.vbsp : step == 1 ? s.vvis : s.vrad;
  const auto input = step == 0 ? stagedVmf
                               : QFileInfo(stagedVmf).dir().filePath(
                                   QFileInfo(stagedVmf).completeBaseName() + ".bsp");
  auto arguments = step == 0   ? s.vbspArguments
                   : step == 1 ? s.vvisArguments
                               : s.vradArguments;
  arguments.append({"-game", s.gameDirectory, input});
  return {program, arguments, QFileInfo(stagedVmf).absolutePath()};
}

QStringList SourceBuild::engineArguments(
  const SourceBuildSettings& settings, const QString& mapName)
{
  auto result = settings.launchArguments;
  for (auto& arg : result)
  {
    arg.replace("{game}", settings.gameDirectory);
    arg.replace("{map}", mapName);
  }
  return result;
}

bool SourceBuild::isSourceBsp(const QString& path)
{
  auto file = QFile{path};
  // A Source BSP has a 1036-byte header (magic, version, 64 lump records, revision).
  return QFileInfo(path).isFile() && file.open(QIODevice::ReadOnly) && file.size() >= 1036
         && file.read(4) == "VBSP";
}

bool SourceBuild::start(
  const QString& savedVmf, const SourceBuildSettings& settings, const bool play)
{
  if (m_running || m_process.state() != QProcess::NotRunning)
  {
    emit output("A build is already running.\n");
    return false;
  }
  m_log.close();
  m_settings = settings;
  m_play = play;
  m_running = true;
  m_step = 0;
  if (
    !QFileInfo(savedVmf).isFile()
    || QFileInfo(savedVmf).suffix().compare("vmf", Qt::CaseInsensitive) != 0)
  {
    fail("Choose an existing saved VMF; the editor must save native edits first.");
    return false;
  }
  for (const auto& tool : {settings.vbsp, settings.vvis, settings.vrad})
  {
    if (!QFileInfo(tool).isAbsolute() || !QFileInfo(tool).isExecutable())
    {
      fail("Compiler is missing or not an absolute executable path: " + tool);
      return false;
    }
  }
  if (
    !QFileInfo(settings.gameDirectory + "/gameinfo.txt").isFile()
    || !QDir::isAbsolutePath(settings.gameDirectory)
    || !QDir::isAbsolutePath(settings.stagingDirectory) || settings.toolTimeoutMs <= 0)
  {
    fail(
      "Choose an absolute game directory with gameinfo.txt and an absolute staging "
      "directory.");
    return false;
  }
  if (play && (!QFileInfo(settings.engine).isAbsolute()
      || !QFileInfo(settings.engine).isExecutable()
      || !QFileInfo(settings.engineWorkingDirectory).isDir()
      || !QDir::isAbsolutePath(settings.engineWorkingDirectory)
      || !QDir::isAbsolutePath(settings.mapsDirectory)
      || !settings.launchArguments.join('\n').contains("{map}")))
  {
    fail(
      "Choose an engine, absolute working/maps directories and launch arguments "
      "containing {map}.");
    return false;
  }
  const auto name = "sourcebroom_" + QUuid::createUuid().toString(QUuid::Id128);
  if (
    !QDir{}.mkpath(settings.stagingDirectory)
    || !QDir(settings.stagingDirectory).mkdir(name))
  {
    fail("Cannot create a new owned staging directory.");
    return false;
  }
  m_runDirectory = QDir(settings.stagingDirectory).filePath(name);
  m_stagedVmf = QDir(m_runDirectory).filePath(name + ".vmf");
  m_bsp = QDir(m_runDirectory).filePath(name + ".bsp");
  if (!QFile::copy(savedVmf, m_stagedVmf))
  {
    fail("Cannot copy saved VMF into staging.");
    return false;
  }
  m_log.setFileName(QDir(m_runDirectory).filePath("build.log"));
  if (!m_log.open(QIODevice::WriteOnly | QIODevice::NewOnly))
  {
    fail("Cannot create build log in staging.");
    return false;
  }
  emit output("Owned build staging (retained for diagnostics): " + m_runDirectory + "\n");
  runNext();
  return true;
}

void SourceBuild::runNext()
{
  if (m_step == 3)
  {
    complete();
    return;
  }
  // Freshness is established by exclusive creation of an empty per-run directory,
  // not filesystem timestamps (which may be coarse). No previous BSP is ever reused.
  const auto command = compilerCommand(m_settings, m_step, m_stagedVmf);
  emit output(
    "Running: " + command.program + "\nArguments: [" + command.arguments.join("] [")
    + "]\nWorking directory: " + command.workingDirectory + "\n");
  m_timedOut = false;
  m_process.setWorkingDirectory(command.workingDirectory);
  m_process.start(command.program, command.arguments);
  if (m_running)
  {
    m_timeout.start(m_settings.toolTimeoutMs);
  }
}

void SourceBuild::fail(const QString& message)
{
  m_timeout.stop();
  m_running = false;
  emit output("ERROR: " + message + "\n");
  emit finished(false, {});
}

void SourceBuild::complete()
{
  if (m_play)
  {
    const auto mapName = QFileInfo(m_bsp).completeBaseName();
    const auto destination = QDir(m_settings.mapsDirectory).filePath(mapName + ".bsp");
    if (!QDir{}.mkpath(m_settings.mapsDirectory) || !QFile::copy(m_bsp, destination))
    {
      fail(
        "Cannot copy BSP to runtime maps directory (existing files are never "
        "overwritten): "
        + destination);
      return;
    }
    emit output(
      "Runtime copy (retained; remove manually when no game uses it): " + destination
      + "\n");
    // Detached engine is never terminated by this object or used to infer play success.
    if (!QProcess::startDetached(
          m_settings.engine,
          engineArguments(m_settings, mapName),
          m_settings.engineWorkingDirectory))
    {
      fail("BSP built, but engine launch failed. Staged BSP: " + m_bsp);
      return;
    }
    emit output(
      "Engine launch requested; in-game compatibility requires owner verification.\n");
  }
  m_running = false;
  emit output("Build succeeded: " + m_bsp + "\n");
  emit finished(true, m_bsp);
}
} // namespace tb::ui
