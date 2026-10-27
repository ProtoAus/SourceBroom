/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "ui/SourceBuildDialog.h"
#include "ui/SourceDocumentReader.h"
#include "ui/SourceToolsDialog.h"

#include <cmath>

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{
namespace
{
void writeToolFixture(const QString& path, const QByteArray& bytes)
{
  auto file = QFile{path};
  REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::NewOnly));
  REQUIRE(file.write(bytes) == bytes.size());
}
} // namespace

TEST_CASE("SourceToolsDialog", "[SourceTools]")
{
  auto root = QTemporaryDir{};
  REQUIRE(root.isValid());
  const auto vmf = root.path() + "/native.vmf";
  const auto bytes = QByteArray{
    "world { \"id\" \"1\" \"classname\" \"worldspawn\" solid { \"id\" \"2\" "
    "side { \"id\" \"3\" \"material\" \"custom/test\" } } }\n"
    "cameras { \"activecamera\" \"0\" camera { \"position\" \"[8 16 24]\" "
    "\"look\" \"[32 32 32]\" } camera { \"position\" \"[1000001 0 0]\" "
    "\"look\" \"[1000001 1 0]\" } }\n"};
  writeToolFixture(vmf, bytes);

  SECTION(
    "build dialog tools affordance opens metadata and forwards navigation without "
    "compiling")
  {
    auto buildDialog = SourceBuildDialog{nullptr, vmf, false};
    auto* openTools = buildDialog.findChild<QPushButton*>("sourceToolsButton");
    REQUIRE(openTools);
    auto navigated = QSignalSpy{&buildDialog, &SourceBuildDialog::navigateToPosition};
    openTools->click();
    auto* tools = buildDialog.findChild<SourceToolsDialog*>();
    REQUIRE(tools);
    auto* go = tools->findChild<QPushButton*>("sourceMetadataGoCamera");
    REQUIRE(go);
    REQUIRE(go->isEnabled());
    go->click();
    REQUIRE(navigated.size() == 1);
    CHECK(navigated[0][0].value<QVector3D>() == QVector3D{8, 16, 24});
    auto* build = buildDialog.findChild<SourceBuild*>();
    REQUIRE(build);
    CHECK_FALSE(build->running());
    auto viewNavigation =
      QSignalSpy{&buildDialog, &SourceBuildDialog::navigateToCameraView};
    auto* view = tools->findChild<QPushButton*>("sourceMetadataGoCameraView");
    REQUIRE(view);
    REQUIRE(view->isEnabled());
    view->click();
    REQUIRE(viewNavigation.size() == 1);
    CHECK(viewNavigation[0][0].value<QVector3D>() == QVector3D{8, 16, 24});
    CHECK(
      (viewNavigation[0][1].value<QVector3D>() - QVector3D{24, 16, 8}.normalized())
        .length()
      < 0.00001f);
    CHECK(navigated.size() == 1);
    CHECK_FALSE(build->running());
  }
  SECTION("saved snapshot has read-only metadata and acted safe camera navigation")
  {
    auto dialog = SourceToolsDialog{nullptr, vmf, SourceBspTarget::Css20};
    auto* display = dialog.findChild<QPlainTextEdit*>("sourceMetadataText");
    REQUIRE(display);
    CHECK(display->isReadOnly());
    CHECK(display->toPlainText().contains("cameras: 2"));
    auto* cameras = dialog.findChild<QComboBox*>("sourceMetadataCameras");
    auto* go = dialog.findChild<QPushButton*>("sourceMetadataGoCamera");
    REQUIRE(cameras);
    REQUIRE(go);
    auto navigated = QSignalSpy{&dialog, &SourceToolsDialog::navigateToPosition};
    REQUIRE(go->isEnabled());
    go->click();
    REQUIRE(navigated.size() == 1);
    CHECK(navigated[0][0].value<QVector3D>() == QVector3D{8, 16, 24});
    cameras->setCurrentIndex(1);
    CHECK_FALSE(go->isEnabled());
    go->click();
    CHECK(navigated.size() == 1);
    auto original = QFile{vmf};
    REQUIRE(original.open(QIODevice::ReadOnly));
    CHECK(original.readAll() == bytes);
  }
  SECTION("saved camera view preserves double direction and refuses unsafe targets")
  {
    const auto cameraPath = root.path() + "/camera-views.vmf";
    const auto cameraBytes = QByteArray{R"(world { id 1 classname worldspawn }
      cameras { activecamera 0
        camera { position "[8 16 24]" look "[32 32 32]" }
        camera { position "[1000001 0 0]" look "[1000001 1 0]" }
        camera { position "[1 2 3]" look "[1 2 3]" }
        camera { position "[1 2 3]" }
        camera { position "[0 0 8]" look "[0 0 9]" }
        camera { position "[999999.99 0 0]" look "[999999.98 0 0]" }
        camera { position "[0 0 0]" look "[1e-150 0 0]" }
        camera { position "[0 0 0]" look "[inf 0 0]" }
      }
    )"};
    writeToolFixture(cameraPath, cameraBytes);
    auto dialog = SourceToolsDialog{nullptr, cameraPath, SourceBspTarget::Css20};
    auto* cameras = dialog.findChild<QComboBox*>("sourceMetadataCameras");
    auto* eye = dialog.findChild<QPushButton*>("sourceMetadataGoCamera");
    REQUIRE(cameras);
    REQUIRE(eye);
    auto eyeNavigation = QSignalSpy{&dialog, &SourceToolsDialog::navigateToPosition};
    REQUIRE(eye->isEnabled());
    eye->click();
    REQUIRE(eyeNavigation.size() == 1);
    CHECK(eyeNavigation[0][0].value<QVector3D>() == QVector3D{8, 16, 24});
    auto* view = dialog.findChild<QPushButton*>("sourceMetadataGoCameraView");
    REQUIRE(view);
    auto viewNavigation = QSignalSpy{&dialog, &SourceToolsDialog::navigateToCameraView};
    for (const auto row : {0, 4, 5, 6})
    {
      cameras->setCurrentIndex(row);
      REQUIRE(view->isEnabled());
      const auto previous = viewNavigation.size();
      view->click();
      REQUIRE(viewNavigation.size() == previous + 1);
      const auto& emitted = viewNavigation.back();
      const auto expected = row == 0   ? QVector3D{24, 16, 8}.normalized()
                            : row == 4 ? QVector3D{0, 0, 1}
                            : row == 5 ? QVector3D{-1, 0, 0}
                                       : QVector3D{1, 0, 0};
      CHECK((emitted[1].value<QVector3D>() - expected).length() < 0.00001f);
      CHECK(std::abs(emitted[1].value<QVector3D>().length() - 1.0f) < 0.00001f);
      CHECK(eyeNavigation.size() == 1);
    }
    const auto accepted = viewNavigation.size();
    for (const auto row : {1, 2, 3, 7, -1})
    {
      cameras->setCurrentIndex(row);
      CHECK_FALSE(view->isEnabled());
      view->click();
      CHECK(viewNavigation.size() == accepted);
    }
    auto original = QFile{cameraPath};
    REQUIRE(original.open(QIODevice::ReadOnly));
    CHECK(original.readAll() == cameraBytes);
  }
  SECTION(
    "default deny rights and exact plan approval gate real custom dependency discovery")
  {
    const auto content = root.path() + "/custom-content";
    REQUIRE(QDir{}.mkpath(content + "/materials/custom"));
    writeToolFixture(
      content + "/materials/custom/test.vmt",
      "UnlitGeneric { \"$basetexture\" \"custom/test\" }");
    writeToolFixture(
      content + "/materials/custom/test.vtf", QByteArray::fromHex("56544600000100ff"));
    auto dialog = SourceToolsDialog{nullptr, vmf, SourceBspTarget::Css20};
    auto* customRoot = dialog.findChild<QLineEdit*>("sourcePackRoot");
    auto* rights = dialog.findChild<QCheckBox*>("sourcePackRights");
    auto* review = dialog.findChild<QPushButton*>("sourcePackReview");
    auto* approval = dialog.findChild<QCheckBox*>("sourcePackApproval");
    auto* package = dialog.findChild<QPushButton*>("sourcePackExport");
    auto* report = dialog.findChild<QPlainTextEdit*>("sourcePackReport");
    REQUIRE(customRoot);
    REQUIRE(rights);
    REQUIRE(review);
    REQUIRE(approval);
    REQUIRE(package);
    REQUIRE(report);
    CHECK(customRoot->text().isEmpty());
    CHECK_FALSE(rights->isChecked());
    CHECK_FALSE(review->isEnabled());
    CHECK_FALSE(package->isEnabled());
    customRoot->setText(content);
    CHECK_FALSE(review->isEnabled());
    rights->setChecked(true);
    REQUIRE(review->isEnabled());
    review->click();
    CHECK(report->toPlainText().contains("INCLUDE materials/custom/test.vmt"));
    CHECK(report->toPlainText().contains("INCLUDE materials/custom/test.vtf"));
    CHECK(report->toPlainText().contains("INCOMPLETE"));
    CHECK_FALSE(package->isEnabled());
    REQUIRE(approval->isEnabled());
    approval->setChecked(true);
    CHECK(package->isEnabled());
    auto* target = dialog.findChild<QComboBox*>("sourcePackTarget");
    REQUIRE(target);
    CHECK(target->currentIndex() == static_cast<int>(SourceBspTarget::Css20));
    const auto compilerTarget = loadSourceBuildSettings().bspTarget;
    target->setCurrentIndex(static_cast<int>(SourceBspTarget::Strata25));
    CHECK_FALSE(approval->isChecked());
    CHECK_FALSE(approval->isEnabled());
    CHECK_FALSE(package->isEnabled());
    REQUIRE(review->isEnabled());
    review->click();
    REQUIRE(approval->isEnabled());
    approval->setChecked(true);
    REQUIRE(package->isEnabled());
    target->setCurrentIndex(-1);
    CHECK_FALSE(review->isEnabled());
    CHECK_FALSE(approval->isChecked());
    CHECK_FALSE(approval->isEnabled());
    CHECK_FALSE(package->isEnabled());
    target->setCurrentIndex(static_cast<int>(SourceBspTarget::Generic));
    CHECK(review->isEnabled());
    CHECK(loadSourceBuildSettings().bspTarget == compilerTarget);
    customRoot->setText(content + "/changed");
    CHECK_FALSE(approval->isChecked());
    CHECK_FALSE(approval->isEnabled());
    CHECK_FALSE(package->isEnabled());
  }
  SECTION("bounded saved reader refuses syntax and oversized files before parsing")
  {
    auto error = QString{};
    const auto parsed = readSavedSourceDocument(vmf, error);
    REQUIRE(parsed);
    CHECK(parsed->source() == bytes.toStdString());
    writeToolFixture(root.path() + "/bad.vmf", "world {");
    CHECK_FALSE(readSavedSourceDocument(root.path() + "/bad.vmf", error));
    CHECK(error.contains("parse refused"));
    auto huge = QFile{root.path() + "/over-budget.vmf"};
    REQUIRE(huge.open(QIODevice::WriteOnly | QIODevice::NewOnly));
    REQUIRE(huge.resize(128ll * 1024ll * 1024ll + 1ll));
    huge.close();
    CHECK_FALSE(readSavedSourceDocument(huge.fileName(), error));
    CHECK(error.contains("budget"));
  }
}
} // namespace tb::ui
