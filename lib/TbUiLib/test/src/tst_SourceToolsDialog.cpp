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
