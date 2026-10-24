/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "SourceBuildTestTool.h"
#include "ui/SourceBuild.h"

#include <memory>

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

TEST_CASE("SourceBuild")
{
  // The normal UI test runner already owns QApplication. Also supports a focused
  // Core-only run of these exact production sources/tests in an isolated build.
  int argc = 1;
  char name[] = "SourceBuildTest";
  char* argv[] = {name, nullptr};
  auto app = std::unique_ptr<QCoreApplication>{};
  if (!QCoreApplication::instance())
  {
    app = std::make_unique<QCoreApplication>(argc, argv);
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

  SECTION("owned staging, real child process exits, missing output and stale controls")
  {
    const auto mode = GENERATE(
      "success",
      "missing",
      "invalid",
      "failure",
      "failure-vis",
      "failure-rad",
      "timeout");
    auto root = QTemporaryDir{};
    REQUIRE(root.isValid());
    auto settings = SourceBuildSettings{};
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
    auto build = SourceBuild{};
    auto finished = QSignalSpy{&build, &SourceBuild::finished};
    auto output = QString{};
    QObject::connect(
      &build, &SourceBuild::output, [&](const QString& text) { output += text; });
    REQUIRE(build.start(original, settings, QString{mode} != "success"));
    if (finished.isEmpty())
    {
      REQUIRE(finished.wait(10000));
    }
    REQUIRE(finished.size() == 1);
    const auto success = QString{mode} == "success";
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
