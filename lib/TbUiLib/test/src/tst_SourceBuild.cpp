/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTextStream>

#include "SourceBuildTestTool.h"
#include "mdl/PointTrace.h"
#include "ui/SourceBuild.h"
#include "ui/SourceBuildDialog.h"

#include <cmath>
#include <memory>
#include <sstream>
#include <tuple>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::ui
{
namespace
{
void writeFile(const QString& path, const QByteArray& bytes)
{
  auto file = QFile{path};
  REQUIRE(file.open(QIODevice::WriteOnly));
  REQUIRE(file.write(bytes) == bytes.size());
}

QByteArray readFile(const QString& path)
{
  auto file = QFile{path};
  REQUIRE(file.open(QIODevice::ReadOnly));
  return file.readAll();
}
} // namespace

TEST_CASE("SourceBuild installed toolchain opt-in", "[.SourceBuildInstalled]")
{
  const auto vmf = qEnvironmentVariable("SOURCEBROOM_REAL_VMF");
  if (vmf.isEmpty())
  {
    SKIP("Installed compiler check requires explicit owned VMF/staging environment");
  }
  const auto staging = qEnvironmentVariable("SOURCEBROOM_REAL_STAGING");
  const auto target = qEnvironmentVariable("SOURCEBROOM_REAL_TARGET");
  REQUIRE(QFileInfo{vmf}.isAbsolute());
  REQUIRE_FALSE(staging.isEmpty());
  REQUIRE(QFileInfo{staging}.isAbsolute());
  REQUIRE((target == "css" || target == "momentum"));
  REQUIRE(QDir{}.mkpath(staging));
  const auto original = readFile(vmf);
  auto settings = defaultSourceBuildSettings(
    target == "css" ? SourceBuildTarget::CounterStrikeSource
                    : SourceBuildTarget::Momentum);
  settings.stagingDirectory = staging;
  settings.toolTimeoutMs = 60000;
  auto build = SourceBuild{};
  auto finished = QSignalSpy{&build, &SourceBuild::finished};
  auto output = QTextStream{stdout};
  QObject::connect(&build, &SourceBuild::output, [&](const QString& text) {
    output << text;
    output.flush();
  });
  REQUIRE(build.start(vmf, settings, false));
  if (finished.isEmpty())
  {
    REQUIRE(finished.wait(190000));
  }
  REQUIRE(finished.size() == 1);
  REQUIRE(finished[0][0].toBool());
  const auto bsp = finished[0][1].toString();
  auto message = QString{};
  CHECK(SourceBuild::validateBsp(bsp, settings.bspTarget, message));
  CHECK(bsp.startsWith(staging));
  CHECK(readFile(vmf) == original);
  CHECK_FALSE(build.running());
}

TEST_CASE("SourceBuild")
{
  // The normal UI test runner already owns QApplication.
  int argc = 1;
  char name[] = "SourceBuildTest";
  char* argv[] = {name, nullptr};
  auto app = std::unique_ptr<QApplication>{};
  if (!QCoreApplication::instance())
  {
    app = std::make_unique<QApplication>(argc, argv);
  }

  SECTION("argument arrays preserve paths and spaces without shell quoting")
  {
    auto settings = SourceBuildSettings{};
    settings.vbsp = "C:/tools with spaces/vbsp.exe";
    settings.vvis = "C:/tools with spaces/vvis.exe";
    settings.vrad = "C:/tools with spaces/vrad.exe";
    settings.gameDirectory = "C:/game with spaces/game";
    settings.launchArguments = {"-game", "{game}", "+map", "{map}", "literal argument"};
    const auto vmf = QString{"C:/owned staging/ramp test.vmf"};
    const auto command = SourceBuild::compilerCommand(settings, 0, vmf);
    CHECK(command.program == settings.vbsp);
    CHECK(command.workingDirectory == "C:/owned staging");
    CHECK(command.arguments == QStringList{"-game", settings.gameDirectory, vmf});
    CHECK(
      SourceBuild::compilerCommand(settings, 1, vmf).arguments.last()
      == "C:/owned staging/ramp test.bsp");
    CHECK(SourceBuild::compilerCommand(settings, 2, vmf).program == settings.vrad);
    CHECK(
      SourceBuild::engineArguments(settings, "unique_map")
      == QStringList{
        "-game", settings.gameDirectory, "+map", "unique_map", "literal argument"});
  }

  SECTION("presets separate compiler content from optional runtime")
  {
    const auto momentum = defaultSourceBuildSettings(SourceBuildTarget::Momentum);
    const auto css = defaultSourceBuildSettings(SourceBuildTarget::CounterStrikeSource);
    const auto fte = defaultSourceBuildSettings(SourceBuildTarget::FteSurf);
    CHECK(momentum.gameDirectory.endsWith("/momentum"));
    CHECK(css.gameDirectory.endsWith("/cstrike"));
    CHECK(fte.gameDirectory == momentum.gameDirectory);
    CHECK(css.vradArguments == QStringList{"-cpu"});
    CHECK(fte.launchArguments == QStringList{"+cfg_save_auto", "0", "+map", "{map}"});
  }

  SECTION("native preflight refuses invalid structure before any compiler child")
  {
    const auto bytes = GENERATE(
      QByteArray{"entity { \"id\" \"1\" }"},
      QByteArray{"world {} world {}"},
      QByteArray{"world { \"id\""});
    auto root = QTemporaryDir{};
    REQUIRE(root.isValid());
    auto settings = SourceBuildSettings{};
    settings.vbsp = settings.vvis = settings.vrad = SOURCE_BUILD_TEST_TOOL_PATH;
    settings.gameDirectory = root.path() + "/game";
    settings.stagingDirectory = root.path() + "/staging";
    REQUIRE(QDir{}.mkpath(settings.gameDirectory));
    writeFile(settings.gameDirectory + "/gameinfo.txt", "success");
    const auto vmf = root.path() + "/invalid.vmf";
    writeFile(vmf, bytes);
    auto build = SourceBuild{};
    auto finished = QSignalSpy{&build, &SourceBuild::finished};
    auto output = QString{};
    QObject::connect(
      &build, &SourceBuild::output, [&](const QString& text) { output += text; });
    CHECK_FALSE(build.start(vmf, settings, false));
    REQUIRE(finished.size() == 1);
    CHECK_FALSE(finished[0][0].toBool());
    // Give queued work an event-loop opportunity; the paired successful-tool
    // controls below prove that this helper really writes calls.txt when run.
    QTest::qWait(50);
    CHECK_FALSE(output.contains("Running: "));
    CHECK_FALSE(QFileInfo::exists(settings.gameDirectory + "/calls.txt"));
    CHECK(readFile(vmf) == bytes);
    CHECK((output.contains("Native") || output.contains("Preflight")));
    CHECK_FALSE(build.running());
  }

  SECTION("owned staging, real child process exits, missing output and stale controls")
  {
    const auto mode = GENERATE(
      "success",
      "diagnostic",
      "leak-failure",
      "wrong-version",
      "missing",
      "invalid",
      "failure",
      "failure-vis",
      "failure-rad",
      "timeout");
    auto root = QTemporaryDir{};
    REQUIRE(root.isValid());
    auto settings = SourceBuildSettings{};
    settings.bspTarget = SourceBspTarget::Css20;
    settings.gameDirectory = root.path() + "/game with spaces";
    settings.stagingDirectory = root.path() + "/owned staging";
    settings.mapsDirectory = root.path() + "/runtime maps";
    settings.vbsp = SOURCE_BUILD_TEST_TOOL_PATH;
    settings.vvis = settings.vbsp;
    settings.vrad = settings.vbsp;
    settings.engine = settings.vbsp;
    settings.engineWorkingDirectory = root.path();
    settings.launchArguments = {"+map", "{map}"};
    settings.toolTimeoutMs = QString{mode} == "timeout" ? 100 : 5000;
    REQUIRE(QDir{}.mkpath(settings.gameDirectory));
    REQUIRE(QDir{}.mkpath(settings.stagingDirectory));
    writeFile(settings.gameDirectory + "/gameinfo.txt", mode);
    const auto original = root.path() + "/owner map.vmf";
    const auto originalBytes =
      QByteArray{"// synthetic document\nworld { \"id\" \"1\" }\n"};
    writeFile(original, originalBytes);
    // A valid-looking stale BSP beside the original must never become this run's output.
    auto stale = QByteArray(1036, '\0');
    stale.replace(0, 4, "VBSP");
    writeFile(root.path() + "/owner map.bsp", stale);
    writeFile(root.path() + "/owner map.lin", "0 0 0\n1 1 1\n");
    auto dialog = SourceBuildDialog{nullptr, original, false};
    auto* child = dialog.findChild<SourceBuild*>();
    REQUIRE(child);
    auto& build = *child;
    auto navigation = QSignalSpy{&dialog, &SourceBuildDialog::navigateToPosition};
    auto showLeak = QSignalSpy{&dialog, &SourceBuildDialog::showLeak};
    auto finished = QSignalSpy{&build, &SourceBuild::finished};
    auto leaks = QSignalSpy{&build, &SourceBuild::leakReady};
    auto diagnostics = QSignalSpy{&build, &SourceBuild::diagnostic};
    auto output = QString{};
    QObject::connect(
      &build, &SourceBuild::output, [&](const QString& text) { output += text; });
    const auto success = QString{mode} == "success" || QString{mode} == "diagnostic";
    REQUIRE(build.start(original, settings, !success));
    if (finished.isEmpty())
    {
      REQUIRE(finished.wait(10000));
    }
    REQUIRE(finished.size() == 1);
    if (QString{mode} == "diagnostic" || QString{mode} == "leak-failure")
    {
      REQUIRE(leaks.size() == 1);
      CHECK(leaks[0][0].toString().startsWith(settings.stagingDirectory));
      auto error = QString{};
      CHECK(validateSourceLeak(leaks[0][0].toString(), error));
      REQUIRE_FALSE(diagnostics.isEmpty());
      const auto location = parseSourceBuildDiagnostic(diagnostics[0][0].toString());
      REQUIRE(location.position.has_value());
      CHECK(*location.position == QVector3D{128, -64, 32});
      auto* log = dialog.findChild<QTextBrowser*>("sourceBuildLog");
      REQUIRE(log);
      emit log->anchorClicked(QUrl{"sourcebuild:0"});
      REQUIRE(navigation.size() == 1);
      CHECK(navigation[0][0].value<QVector3D>() == QVector3D{128, -64, 32});
      emit log->anchorClicked(QUrl{"sourcebuild:leak"});
      REQUIRE(showLeak.size() == 1);
      CHECK(showLeak[0][0].toString() == leaks[0][0].toString());
    }
    else
    {
      CHECK(leaks.isEmpty());
    }
    CHECK(finished[0][0].toBool() == success);
    CHECK(build.running() == false);
    CHECK(readFile(original) == originalBytes);
    CHECK(readFile(root.path() + "/owner map.bsp") == stale);
    CHECK_FALSE(QFileInfo::exists(settings.mapsDirectory));
    if (success)
    {
      const auto bsp = finished[0][1].toString();
      REQUIRE(SourceBuild::isSourceBsp(bsp));
      CHECK(bsp.startsWith(settings.stagingDirectory));
      const auto stagedVmf =
        QFileInfo(bsp).dir().filePath(QFileInfo(bsp).completeBaseName() + ".vmf");
      CHECK(readFile(stagedVmf) == originalBytes);
      const auto calls = readFile(settings.gameDirectory + "/calls.txt");
      CHECK(calls.count("-game") == 3);
      CHECK(calls.contains(QFileInfo(bsp).absolutePath().toUtf8()));
      CHECK(calls.contains(settings.gameDirectory.toUtf8()));
      CHECK(
        readFile(QFileInfo(bsp).dir().filePath("build.log")).contains("Build succeeded"));
      // A second successful build must have a distinct path, never reuse old BSPs.
      REQUIRE(build.start(original, settings, false));
      REQUIRE(finished.wait(10000));
      CHECK(finished[1][1].toString() != bsp);
    }
    else
    {
      CHECK(finished[0][1].toString().isEmpty());
      CHECK(output.contains("ERROR:"));
      const auto expectedCalls = QString{mode} == "failure-vis"   ? 2
                                 : QString{mode} == "failure-rad" ? 3
                                                                  : 1;
      CHECK(
        readFile(settings.gameDirectory + "/calls.txt").count("-game") == expectedCalls);
    }
  }

  SECTION("strict leak input rejects malformed, nonfinite and excessive data")
  {
    auto root = QTemporaryDir{};
    REQUIRE(root.isValid());
    const auto path = root.path() + "/test.lin";
    auto error = QString{};
    CHECK_FALSE(validateSourceLeak(path, error));
    const auto invalid = GENERATE(
      QByteArray{},
      QByteArray{"0 0 0\n"},
      QByteArray{"0 0 0\n1 2\n"},
      QByteArray{"0 0 0\nnan 0 1\n"},
      QByteArray{"0 0 0\ninf 0 1\n"},
      QByteArray{"0 0 0\n1000001 0 1\n"},
      QByteArray{"0 0 0\n0 0 0\n"},
      QByteArray{"0 0 0\n1 2 3 extra\n"},
      QByteArray(4 * 1024 * 1024 + 1, '0'));
    writeFile(path, invalid);
    CHECK_FALSE(validateSourceLeak(path, error));
    CHECK_FALSE(error.isEmpty());
    auto tooMany = QByteArray{};
    for (auto i = 0; i < 65537; ++i)
    {
      tooMany += i % 2 ? "1 0 0\n" : "0 0 0\n";
    }
    writeFile(path, tooMany);
    CHECK_FALSE(validateSourceLeak(path, error));
    writeFile(path, "-1000000 0 0\n1000000 0 0\n-1000000 0 0\n1000000 0 0\n");
    CHECK_FALSE(validateSourceLeak(path, error));
    writeFile(path, "0 0 0\n128 -64 32\n");
    CHECK(validateSourceLeak(path, error));
  }

  SECTION("leak validation checks production pointfile smoothing and navigation")
  {
    using T = std::tuple<QByteArray, bool, bool>;
    const auto [contents, finiteDirections, accepted] = GENERATE(
      values<T>({
        {"0 0 0\n128 0 0\n0 0 0\n", false, false},
        {"0 0 0\n128 0 0\n0 0 0\n128 0 0\n", true, true},
        {"0 0 0\n128 0 0\n0 0 0\n128 0 0\n0 0 0\n", false, false},
        {"0 0 0\n0 0 0\n128 0 0\n", true, false},
        {"0 0 0\n128 0 0\n128 0 0\n", true, false},
        {"0 0 0\n128 0 0\n128 128 0\n0 0 0\n", true, true},
        {"0 0 0\n128 0 0\n256 0 0\n", true, true},
      }));
    CAPTURE(contents.constData());
    auto root = QTemporaryDir{};
    REQUIRE(root.isValid());
    const auto path = root.path() + "/staged.lin";
    writeFile(path, contents);

    // Load the actual owned file through the production parser, not a test
    // approximation of smoothPoints. Include both endpoints in both directions.
    auto stream = std::istringstream{readFile(path).toStdString()};
    auto result = mdl::loadPointFile(stream);
    REQUIRE(result.is_success());
    auto trace = std::move(result).value();
    const auto finiteDirection = [&] {
      const auto direction = trace.currentDirection();
      return std::isfinite(direction.x()) && std::isfinite(direction.y())
             && std::isfinite(direction.z()) && vm::squared_length(direction) > 0.0f;
    };
    auto allFinite = finiteDirection();
    while (trace.hasNextPoint())
    {
      trace.advance();
      allFinite = finiteDirection() && allFinite;
    }
    while (trace.hasPreviousPoint())
    {
      trace.retreat();
      allFinite = finiteDirection() && allFinite;
    }
    CHECK(allFinite == finiteDirections);

    auto error = QString{};
    CHECK(validateSourceLeak(path, error) == accepted);
    CHECK(error.isEmpty() == accepted);
  }

  SECTION("BSP target contract is explicit and generic does not infer compatibility")
  {
    auto root = QTemporaryDir{};
    REQUIRE(root.isValid());
    const auto path = root.path() + "/test.bsp";
    auto data = QByteArray(1036, '\0');
    data.replace(0, 4, "VBSP");
    data[4] = 25;
    writeFile(path, data);
    auto message = QString{};
    CHECK(SourceBuild::validateBsp(path, SourceBspTarget::Generic, message));
    CHECK(message.contains("compatibility unknown"));
    CHECK(SourceBuild::validateBsp(path, SourceBspTarget::Strata25, message));
    CHECK_FALSE(SourceBuild::validateBsp(path, SourceBspTarget::Css20, message));
    writeFile(path, data.left(1035));
    CHECK_FALSE(SourceBuild::validateBsp(path, SourceBspTarget::Generic, message));
  }

  SECTION("diagnostic parsing keeps indices distinct from VMF IDs")
  {
    const auto diagnostic =
      parseSourceBuildDiagnostic("Error: Entity 2, Brush 3 at (1.5, -2, 3e2)");
    REQUIRE(diagnostic.position.has_value());
    CHECK(*diagnostic.position == QVector3D{1.5f, -2, 300});
    CHECK(diagnostic.reference == "Entity 2, Brush 3");
    CHECK_FALSE(parseSourceBuildDiagnostic("WARNING: (nan 0 0)").position);
    CHECK_FALSE(parseSourceBuildDiagnostic("ERROR: (1e99 0 0)").position);
  }

  SECTION("dialog diagnostic link emits the actual navigation coordinate")
  {
    auto dialog = SourceBuildDialog{nullptr, "unused.vmf", false};
    auto* build = dialog.findChild<SourceBuild*>();
    auto* log = dialog.findChild<QTextBrowser*>("sourceBuildLog");
    REQUIRE(build);
    REQUIRE(log);
    auto navigation = QSignalSpy{&dialog, &SourceBuildDialog::navigateToPosition};
    emit build->diagnostic("WARNING: entity 2 at (128 -64 32)");
    CHECK(log->toHtml().contains("sourcebuild:0"));
    emit log->anchorClicked(QUrl{"sourcebuild:0"});
    REQUIRE(navigation.size() == 1);
    CHECK(navigation[0][0].value<QVector3D>() == QVector3D{128, -64, 32});
    emit log->anchorClicked(QUrl{"sourcebuild:999"});
    CHECK(navigation.size() == 1);
  }

  SECTION("missing compiler and unsaved input fail before staging")
  {
    auto build = SourceBuild{};
    auto finished = QSignalSpy{&build, &SourceBuild::finished};
    CHECK_FALSE(build.start("not-a-saved-map.vmf", SourceBuildSettings{}, false));
    REQUIRE(finished.size() == 1);
    CHECK_FALSE(finished[0][0].toBool());
    auto root = QTemporaryDir{};
    REQUIRE(root.isValid());
    const auto vmf = root.path() + "/saved.vmf";
    writeFile(vmf, "world {}");
    CHECK_FALSE(build.start(vmf, SourceBuildSettings{}, false));
    REQUIRE(finished.size() == 2);
    CHECK_FALSE(finished[1][0].toBool());
  }
}
} // namespace tb::ui
