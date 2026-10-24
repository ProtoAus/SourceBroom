/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/SourceBuild.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QtEndian>

#include "mdl/PointTrace.h"
#include "mdl/SourceCapabilities.h"
#include "ui/SourceDocumentReader.h"

#include <cmath>
#include <exception>
#include <sstream>
#include <utility>

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
  s.bspTarget = target == SourceBuildTarget::CounterStrikeSource
                  ? SourceBspTarget::Css20
                  : SourceBspTarget::Strata25;
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
  // Old settings have no explicit compiler contract; do not infer one from paths.
  const auto target = prefs.value("bspTarget", 0).toInt();
  s.bspTarget = target == 1   ? SourceBspTarget::Css20
                : target == 2 ? SourceBspTarget::Strata25
                              : SourceBspTarget::Generic;
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
  prefs.setValue("bspTarget", static_cast<int>(s.bspTarget));
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
    consumeOutput(m_process.readAllStandardOutput(), m_stdoutPending);
  });
  connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
    consumeOutput(m_process.readAllStandardError(), m_stderrPending);
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
    consumeOutput(m_process.readAllStandardOutput(), m_stdoutPending, true);
    consumeOutput(m_process.readAllStandardError(), m_stderrPending, true);
    if (!m_running)
    {
      return;
    }
    emit output(QString("Compiler exit code: %1\n").arg(code));
    if (m_step == 0)
    {
      inspectLeak();
    }
    if (m_timedOut || status != QProcess::NormalExit || code != 0)
    {
      fail("Compiler failed; no BSP will be promoted or launched.");
      return;
    }
    auto headerMessage = QString{};
    if (!validateBsp(m_bsp, m_settings.bspTarget, headerMessage))
    {
      fail(headerMessage);
      return;
    }
    emit output(headerMessage + "\n");
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

bool SourceBuild::validateBsp(
  const QString& path, const SourceBspTarget target, QString& message)
{
  if (!isSourceBsp(path))
  {
    message = "Compiler did not leave a complete VBSP header; refusing promotion.";
    return false;
  }
  auto file = QFile{path};
  if (!file.open(QIODevice::ReadOnly) || !file.seek(4))
  {
    message = "Cannot read staged BSP version.";
    return false;
  }
  const auto bytes = file.read(4);
  if (bytes.size() != 4)
  {
    message = "Truncated BSP version.";
    return false;
  }
  const auto version = qFromLittleEndian<qint32>(bytes.constData());
  const auto expected = target == SourceBspTarget::Css20      ? 20
                        : target == SourceBspTarget::Strata25 ? 25
                                                              : 0;
  message = QString("Staged VBSP header version %1; %2.")
              .arg(version)
              .arg(
                expected ? QString("required target version %1").arg(expected)
                         : QString("generic target, compatibility unknown"));
  if (expected && version != expected)
  {
    message += " Target mismatch: refusing promotion and launch.";
    return false;
  }
  return true;
}

namespace
{
std::optional<QVector3D> sourcePosition(const QStringList& fields)
{
  if (fields.size() != 3)
  {
    return std::nullopt;
  }
  auto values = QVector3D{};
  for (auto i = 0; i < 3; ++i)
  {
    auto ok = false;
    const auto value = fields[i].toDouble(&ok);
    if (!ok || !std::isfinite(value) || std::abs(value) > 1000000.0)
    {
      return std::nullopt;
    }
    values[i] = static_cast<float>(value);
  }
  return values;
}
} // namespace

SourceBuildDiagnostic parseSourceBuildDiagnostic(const QString& line)
{
  auto result = SourceBuildDiagnostic{line, std::nullopt, {}};
  static const auto coordinates =
    QRegularExpression{R"(\(\s*([-+\d.eE]+)[,\s]+([-+\d.eE]+)[,\s]+([-+\d.eE]+)\s*\))"};
  const auto match = coordinates.match(line);
  if (match.hasMatch())
  {
    result.position =
      sourcePosition({match.captured(1), match.captured(2), match.captured(3)});
  }
  static const auto reference = QRegularExpression{
    R"(\b(?:entity|brush)\s*(?:#|:)?\s*\d+\b)",
    QRegularExpression::CaseInsensitiveOption};
  auto matches = reference.globalMatch(line);
  while (matches.hasNext())
  {
    if (!result.reference.isEmpty())
    {
      result.reference += ", ";
    }
    result.reference += matches.next().captured();
  }
  return result;
}

bool validateSourceLeak(const QString& path, QString& error)
{
  auto file = QFile{path};
  const auto reject = [&error] {
    error =
      "Missing or invalid .lin: expected 2..65536 finite XYZ points, bounded "
      "coordinates and path length, and finite nonzero navigation directions; "
      "leak display refused.";
    return false;
  };
  if (
    QFileInfo(path).isSymLink() || !file.open(QIODevice::ReadOnly)
    || file.size() > 4 * 1024 * 1024)
  {
    return reject();
  }
  auto contents = QByteArray{};
  auto count = 0;
  auto length = 0.0;
  auto previous = QVector3D{};
  while (!file.atEnd())
  {
    const auto line = file.readLine(1025);
    if (line.size() > 1024)
    {
      return reject();
    }
    const auto fields = QString::fromLatin1(line).trimmed().split(
      QRegularExpression{"\\s+"}, Qt::SkipEmptyParts);
    const auto point = sourcePosition(fields);
    if (!point || ++count > 65536)
    {
      return reject();
    }
    if (count > 1)
    {
      const auto distance = (*point - previous).length();
      // Existing pointfile segmentation normalizes each segment; reject duplicates.
      if (distance == 0.0f)
      {
        return reject();
      }
      length += distance;
      if (length > 4000000.0)
      {
        return reject();
      }
    }
    previous = *point;
    contents += line;
  }
  if (count < 2)
  {
    return reject();
  }
  // Validate the exact bytes above using the same smoothing and segmentation as
  // navigation. Nonadjacent repeats can collapse into zero-length segments.
  auto stream = std::istringstream{contents.toStdString()};
  auto result = mdl::loadPointFile(stream);
  if (!result.is_success())
  {
    return reject();
  }
  auto trace = std::move(result).value();
  while (true)
  {
    const auto direction = trace.currentDirection();
    if (
      !std::isfinite(direction.x()) || !std::isfinite(direction.y())
      || !std::isfinite(direction.z()) || vm::squared_length(direction) == 0.0f)
    {
      return reject();
    }
    if (!trace.hasNextPoint())
    {
      break;
    }
    trace.advance();
  }
  error.clear();
  return true;
}

void SourceBuild::consumeOutput(
  const QByteArray& bytes, QString& pending, const bool flush)
{
  const auto text = QString::fromLocal8Bit(bytes);
  emit output(text);
  pending += text;
  while (pending.contains('\n'))
  {
    const auto end = pending.indexOf('\n');
    emit diagnostic(pending.left(end).left(16384).trimmed());
    pending.remove(0, end + 1);
  }
  // Avoid unbounded diagnostics buffering from malformed tool output.
  if (flush || pending.size() > 16384)
  {
    if (!pending.isEmpty())
    {
      emit diagnostic(pending.left(16384));
    }
    pending.clear();
  }
}

void SourceBuild::inspectLeak()
{
  const auto path =
    QDir(m_runDirectory).filePath(QFileInfo(m_stagedVmf).completeBaseName() + ".lin");
  if (!QFileInfo::exists(path))
  {
    return;
  }
  auto error = QString{};
  if (
    QFileInfo(path).canonicalPath() != QFileInfo(m_runDirectory).canonicalFilePath()
    || !validateSourceLeak(path, error))
  {
    emit output("WARNING: Leak display refused. " + error + "\n");
    return;
  }
  emit output("Validated leak path from this build: " + path + "\n");
  emit leakReady(path);
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
  m_stdoutPending.clear();
  m_stderrPending.clear();
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
  // Assess the actual staged bytes, not a different read of the original. Refusal
  // leaves its owned log/VMF for diagnosis and starts no external process.
  try
  {
    auto error = QString{};
    const auto document = readSavedSourceDocument(m_stagedVmf, error);
    if (!document)
    {
      fail(error);
      return false;
    }
    auto target = mdl::SourceCapabilityTarget::Generic;
    switch (settings.bspTarget)
    {
    case SourceBspTarget::Generic:
      break;
    case SourceBspTarget::Css20:
      target = mdl::SourceCapabilityTarget::ClassicCss;
      break;
    case SourceBspTarget::Strata25:
      target = mdl::SourceCapabilityTarget::Strata;
      break;
    default:
      fail("Unknown compiler target contract; choose Generic, CSS20 or Strata25.");
      return false;
    }
    const auto assessment = mdl::assessSourceCapabilities(*document, target);
    for (const auto& diagnostic : assessment.diagnostics)
    {
      const auto severity =
        diagnostic.severity == mdl::SourceCapabilitySeverity::Error ? QString{"ERROR"}
        : diagnostic.severity == mdl::SourceCapabilitySeverity::Warning
          ? QString{"WARNING"}
          : QString{"INFO"};
      auto location = QString{};
      if (diagnostic.line)
      {
        location =
          QString{" line %1, column %2"}.arg(*diagnostic.line).arg(*diagnostic.column);
      }
      if (diagnostic.nativeId)
      {
        location += " (native id " + QString::fromStdString(*diagnostic.nativeId) + ")";
      }
      emit output(
        "Preflight " + severity + location + ": "
        + QString::fromStdString(diagnostic.message) + "\n");
    }
    if (!assessment.mayCompile())
    {
      fail(
        "Native preflight refused compilation; inspect the findings above. No compiler "
        "started.");
      return false;
    }
  }
  catch (const std::exception& exception)
  {
    fail(
      "Native preflight failed before compiler start: "
      + QString::fromUtf8(exception.what()));
    return false;
  }
  emit output(
    "Native VMF format is independent of BSP target. Header checking is not feature "
    "validation; displacements, instances, packing and branch extensions are not "
    "certified by this build dialog.\n");
  if (settings.bspTarget == SourceBspTarget::Generic)
  {
    emit output("WARNING: Generic target: no game/version compatibility is inferred.\n");
  }
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
