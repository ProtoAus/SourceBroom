/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTest>

#include "fs/TestEnvironment.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/Map_Entities.h"
#include "mdl/Map_NativeCordons.h"
#include "mdl/Map_Selection.h"
#include "mdl/Map_World.h"
#include "mdl/Selection.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"
#include "ui/MapDocument.h"
#include "ui/MapDocumentFixture.h"
#include "ui/MapInspector.h"
#include "ui/NativeCordonEditor.h"

#include <bit>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{
namespace
{
const auto config = mdl::MapFixtureConfig{.mapFormat = mdl::MapFormat::SourceVMF};
const auto original = std::string{
  "world { \"id\" \"1\" \"classname\" \"worldspawn\" }\n"
  "cordons { cordon { \"name\" \"Loaded\" box { \"mins\" \"(-01.00 0 0)\" \"maxs\" \"(64 "
  "64 64)\" } "
  "box { \"mins\" \"(100 100 100)\" \"maxs\" \"(200 200 200)\" } } "
  "cordon { \"name\" \"Other\" \"active\" \"0\" box { \"mins\" \"(1 2 3)\" \"maxs\" \"(4 "
  "5 6)\" } } }\n"};
void write(const std::filesystem::path& path, const std::string& bytes)
{
  auto stream = std::ofstream{path, std::ios::binary};
  stream << bytes;
}
std::string serialize(const mdl::Map& map)
{
  return map.vmfScene()->serialize(map.worldNode());
}
template <typename T>
T* child(QWidget& editor, const char* name)
{
  auto* result = editor.findChild<T*>(name);
  REQUIRE(result);
  return result;
}
void click(QWidget& editor, const char* name)
{
  child<QPushButton>(editor, name)->click();
}
void edit(QWidget& editor, const char* name, const char* value)
{
  auto* field = child<QLineEdit>(editor, name);
  field->selectAll();
  QTest::keyClicks(field, value);
}
} // namespace
TEST_CASE("NativeCordon UI acted live dispatch", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "original.vmf";
  write(path, original);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(path, config);
  auto& map = document.map();
  auto editor = NativeCordonEditor{document};
  CHECK(serialize(map) == original);
  mdl::selectNodes(map, {&map.worldNode()});
  REQUIRE(mdl::setEntityProperty(map, "message", "safe control"));
  CHECK(serialize(map).find("safe control") != std::string::npos);
  map.undoCommand();
  CHECK(serialize(map) == original);
  const auto count = map.modificationCount();
  click(editor, "cordonApply");
  CHECK(map.modificationCount() == count);
  CHECK(serialize(map) == original);
  edit(editor, "cordonName", "Live UNSAVED");
  edit(editor, "cordonMaxs", "128 64 64");
  child<QComboBox>(editor, "cordonGlobal")->setCurrentIndex(2);
  click(editor, "cordonApply");
  CHECK(mdl::nativeCordons(map).records[0].name == "Live UNSAVED");
  CHECK(mdl::nativeCordons(map).records[0].boxes[0].maxs == vm::vec3d{128, 64, 64});
  CHECK(map.modificationCount() == count + 1);
  CHECK(child<QLineEdit>(editor, "cordonName")->text() == "Live UNSAVED");
  auto expected = original;
  expected.replace(expected.find("Loaded"), 6, "Live UNSAVED");
  expected.replace(expected.find("(64 64 64)"), 10, "(128 64 64)");
  expected.insert(expected.rfind('}'), "\n\"active\" \"1\"\n\n");
  CHECK(serialize(map) == expected);
  const auto saved = env.dir() / "saved.vmf";
  REQUIRE(map.saveAs(saved).is_success());
  auto reopenedFixture = MapDocumentFixture{};
  auto& reopened = reopenedFixture.load(saved, config);
  CHECK(mdl::nativeCordons(reopened.map()).records[0].name == "Live UNSAVED");
  CHECK(serialize(reopened.map()) == expected);
  auto reopenedEditor = NativeCordonEditor{reopened};
  edit(reopenedEditor, "cordonMaxs", "256 64 64");
  click(reopenedEditor, "cordonApply");
  CHECK(mdl::nativeCordons(reopened.map()).records[0].boxes[0].maxs.x() == 256);
  reopened.map().undoCommand();
  CHECK(serialize(reopened.map()) == expected);
  map.undoCommand();
  CHECK(serialize(map) == original);
  CHECK(child<QLineEdit>(editor, "cordonName")->text() == "Loaded");
  map.redoCommand();
  CHECK(child<QLineEdit>(editor, "cordonName")->text() == "Live UNSAVED");
  CHECK(serialize(map) == expected);
}
TEST_CASE("NativeCordon UI modern rows atomic refusal and removal", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "modern.vmf";
  write(path, original);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(path, config);
  auto& map = document.map();
  auto editor = NativeCordonEditor{document};
  CHECK(child<QComboBox>(editor, "cordonRecords")->count() == 2);
  CHECK(child<QComboBox>(editor, "cordonBoxes")->count() == 2);
  CHECK(child<QComboBox>(editor, "cordonGlobal")->currentText() == "Not set");
  CHECK(child<QComboBox>(editor, "cordonActive")->currentText() == "Not set");
  child<QComboBox>(editor, "cordonBoxes")->setCurrentIndex(1);
  CHECK(child<QLineEdit>(editor, "cordonMins")->text() == "100 100 100");
  const auto before = serialize(map);
  edit(editor, "cordonName", "must not publish");
  edit(editor, "cordonMaxs", "99 200 200");
  child<QComboBox>(editor, "cordonActive")->setCurrentIndex(2);
  click(editor, "cordonApply");
  CHECK(serialize(map) == before);
  CHECK_FALSE(map.modified());
  CHECK_FALSE(child<QLabel>(editor, "cordonStatus")->text().isEmpty());
  edit(editor, "cordonMaxs", "300 200 200");
  click(editor, "cordonApply");
  CHECK(mdl::nativeCordons(map).records[0].boxes[1].maxs.x() == 300);
  CHECK(mdl::nativeCordons(map).records[0].boxes[0].maxs.x() == 64);
  CHECK(child<QComboBox>(editor, "cordonBoxes")->currentIndex() == 1);
  const auto count = map.modificationCount();
  click(editor, "cordonApply");
  CHECK(map.modificationCount() == count);
  click(editor, "cordonDeleteBox");
  CHECK(mdl::nativeCordons(map).records[0].boxes.size() == 1);
  CHECK_FALSE(child<QPushButton>(editor, "cordonDeleteBox")->isEnabled());
  click(editor, "cordonAddBox");
  CHECK(mdl::nativeCordons(map).records[0].boxes.size() == 2);
  child<QComboBox>(editor, "cordonRecords")->setCurrentIndex(1);
  CHECK(child<QLineEdit>(editor, "cordonName")->text() == "Other");
  CHECK(child<QComboBox>(editor, "cordonBoxes")->count() == 1);
  click(editor, "cordonDeleteRecord");
  CHECK(mdl::nativeCordons(map).records.size() == 1);
  click(editor, "cordonDeleteRecord");
  CHECK(mdl::nativeCordons(map).form == mdl::NativeCordonForm::None);
  map.undoCommand();
  CHECK(mdl::nativeCordons(map).records.size() == 1);
  map.redoCommand();
  click(editor, "cordonAddRecord");
  CHECK(mdl::nativeCordons(map).active == false);
  CHECK(mdl::nativeCordons(map).records[0].active == false);
}
TEST_CASE("NativeCordon UI legacy empty and untouched fields", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "legacy.vmf";
  const auto source = std::string{
    "world { id 1 classname worldspawn }\n"
    "cordon { name legacy active 0 mins \"(-01.00 0 0)\" maxs \"(64 64 64)\" }\n"};
  write(path, source);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(path, config);
  auto& map = document.map();
  auto editor = NativeCordonEditor{document};
  CHECK_FALSE(child<QComboBox>(editor, "cordonGlobal")->isEnabled());
  CHECK_FALSE(child<QLineEdit>(editor, "cordonName")->isEnabled());
  CHECK_FALSE(child<QPushButton>(editor, "cordonAddRecord")->isEnabled());
  click(editor, "cordonApply");
  CHECK(serialize(map) == source);
  child<QComboBox>(editor, "cordonActive")->setCurrentIndex(2);
  edit(editor, "cordonMaxs", "96 64 64");
  click(editor, "cordonApply");
  CHECK(mdl::nativeCordons(map).records[0].active == true);
  CHECK_FALSE(mdl::nativeCordons(map).active.has_value());
  CHECK(serialize(map).find("cordons") == std::string::npos);
  map.undoCommand();
  CHECK(serialize(map) == source);
  map.redoCommand();
  click(editor, "cordonDeleteRecord");
  click(editor, "cordonAddRecord");
  CHECK(mdl::nativeCordons(map).form == mdl::NativeCordonForm::Modern);
  CHECK(mdl::nativeCordons(map).records[0].active == false);
  click(editor, "cordonAddRecord");
  CHECK(mdl::nativeCordons(map).records.size() == 2);
  const auto empty = env.dir() / "empty.vmf";
  write(empty, "world { id 1 classname worldspawn } cordons {}");
  REQUIRE(document
            .load(
              config.environmentConfig,
              config.gameInfo,
              *config.mapFormat,
              vm::bbox3d{8192},
              empty)
            .is_success());
  CHECK(child<QPushButton>(editor, "cordonClear")->isEnabled());
  click(editor, "cordonClear");
  CHECK(mdl::nativeCordons(document.map()).form == mdl::NativeCordonForm::None);
  document.map().undoCommand();
  CHECK(mdl::nativeCordons(document.map()).form == mdl::NativeCordonForm::Modern);
  const auto raw = env.dir() / "raw.vmf";
  auto bytes = original;
  bytes.replace(
    bytes.find("Loaded"), 6, std::string{"bad"} + std::bit_cast<char>(uint8_t{255}));
  write(raw, bytes);
  REQUIRE(document
            .load(
              config.environmentConfig,
              config.gameInfo,
              *config.mapFormat,
              vm::bbox3d{8192},
              raw)
            .is_success());
  click(editor, "cordonApply");
  CHECK(serialize(document.map()) == bytes);
  edit(editor, "cordonMaxs", "64 64 64");
  click(editor, "cordonApply");
  CHECK(serialize(document.map()) == bytes);
}
TEST_CASE("NativeCordon UI stale pressed actions cannot redirect", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "stale.vmf";
  write(path, original);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(path, config);
  auto editor = NativeCordonEditor{document};
  auto* button = child<QPushButton>(editor, "cordonApply");
  const auto press = [&] { QTest::mousePress(button, Qt::LeftButton); };
  const auto release = [&] { QTest::mouseRelease(button, Qt::LeftButton); };
  SECTION("row switch refuses old field application")
  {
    edit(editor, "cordonName", "queued stale");
    press();
    child<QComboBox>(editor, "cordonRecords")->setCurrentIndex(1);
    release();
    CHECK(serialize(document.map()) == original);
    CHECK_FALSE(child<QLabel>(editor, "cordonStatus")->text().isEmpty());
  }
  SECTION("deleted record does not retarget next record")
  {
    edit(editor, "cordonName", "queued stale");
    press();
    auto request = mdl::NativeCordonRequest{mdl::nativeCordons(document.map())};
    request.records.erase(request.records.begin());
    REQUIRE(
      mdl::applyNativeCordons(document.map(), request).status
      == mdl::NativeCordonApplyStatus::Changed);
    const auto bytes = serialize(document.map());
    release();
    CHECK(serialize(document.map()) == bytes);
    CHECK(mdl::nativeCordons(document.map()).records[0].name == "Other");
    CHECK_FALSE(child<QLabel>(editor, "cordonStatus")->text().isEmpty());
  }
  SECTION("undo revision invalidates pressed request")
  {
    click(editor, "cordonAddBox");
    edit(editor, "cordonName", "queued stale");
    press();
    document.map().undoCommand();
    release();
    CHECK(serialize(document.map()) == original);
    CHECK(document.map().redoCommandName() != nullptr);
    CHECK_FALSE(child<QLabel>(editor, "cordonStatus")->text().isEmpty());
  }
  SECTION("document replacement invalidates pressed request")
  {
    edit(editor, "cordonName", "queued stale");
    press();
    REQUIRE(document
              .load(
                config.environmentConfig,
                config.gameInfo,
                *config.mapFormat,
                vm::bbox3d{8192},
                path)
              .is_success());
    release();
    CHECK(serialize(document.map()) == original);
    CHECK_FALSE(document.map().modified());
    CHECK_FALSE(child<QLabel>(editor, "cordonStatus")->text().isEmpty());
  }
}
TEST_CASE("NativeCordon UI fit capability and isolated map settings", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto oldFormat = QSettings::defaultFormat();
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(
    QSettings::IniFormat,
    QSettings::UserScope,
    QString::fromStdString(env.dir().string()));
  QSettings::setPath(
    QSettings::IniFormat,
    QSettings::SystemScope,
    QString::fromStdString(env.dir().string()));
  struct Restore
  {
    QSettings::Format format;
    ~Restore() { QSettings::setDefaultFormat(format); }
  } restore{oldFormat};
  const auto path = env.dir() / "fit.vmf";
  write(path, original + "entity { id 10 classname info_target origin \"16 32 48\" }\n");
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(path, config);
  CHECK(
    QSettings{}.fileName().startsWith(
      QString::fromStdString(env.dir().generic_string())));
  auto inspector = MapInspector{document};
  auto* field = child<QLineEdit>(inspector, "cordonName");
  auto* editor = static_cast<NativeCordonEditor*>(field->parentWidget());
  const auto before = serialize(document.map());
  click(*editor, "cordonFit");
  CHECK(serialize(document.map()) == before);
  auto& map = document.map();
  auto* point = map.worldNode().defaultLayer()->children().front();
  mdl::selectNodes(map, {point});
  REQUIRE(map.selectionBounds());
  const auto bounds = *map.selectionBounds();
  REQUIRE(bounds.min.x() < bounds.max.x());
  const auto selection = map.selection().nodes;
  const auto soft = mdl::softMapBounds(map);
  click(*editor, "cordonFit");
  CHECK(mdl::nativeCordons(map).records[0].boxes[0].mins == bounds.min);
  CHECK(mdl::nativeCordons(map).records[0].boxes[0].maxs == bounds.max);
  CHECK(map.selection().nodes == selection);
  CHECK(mdl::softMapBounds(map).bounds == soft.bounds);
  map.undoCommand();
  CHECK(serialize(map) == before);
  auto previewConfig = config;
  previewConfig.readOnlyVmfPreview = true;
  auto previewFixture = MapDocumentFixture{};
  auto& preview = previewFixture.load(path, previewConfig);
  auto previewEditor = NativeCordonEditor{preview};
  CHECK_FALSE(child<QPushButton>(previewEditor, "cordonApply")->isEnabled());
  CHECK_FALSE(child<QPushButton>(previewEditor, "cordonAddRecord")->isEnabled());
  auto nonNativeFixture = MapDocumentFixture{};
  auto& nonNative = nonNativeFixture.create();
  auto nonNativeEditor = NativeCordonEditor{nonNative};
  CHECK(nonNativeEditor.isHidden());
  CHECK_FALSE(child<QPushButton>(nonNativeEditor, "cordonApply")->isEnabled());
}

TEST_CASE("NativeCordon UI fresh document and unsupported controller", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto starter = env.dir() / "starter.vmf";
  write(starter, "world { id 1 classname worldspawn }");
  auto freshConfig = config;
  freshConfig.gameInfo.gameConfig.fileFormats = {{"Source VMF", starter}};
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create(freshConfig);
  auto editor = NativeCordonEditor{document};
  click(editor, "cordonAddRecord");
  CHECK(mdl::nativeCordons(document.map()).records.size() == 1);
  CHECK(child<QComboBox>(editor, "cordonActive")->currentText() == "Inactive");
  edit(editor, "cordonName", "fresh authoring");
  click(editor, "cordonApply");
  const auto saved = env.dir() / "new.vmf";
  REQUIRE(document.map().saveAs(saved).is_success());
  document.map().undoCommand();
  CHECK(child<QLineEdit>(editor, "cordonName")->text() == "New cordon");
  document.map().redoCommand();
  auto reopenedFixture = MapDocumentFixture{};
  auto& reopened = reopenedFixture.load(saved, config);
  CHECK(mdl::nativeCordons(reopened.map()).records[0].name == "fresh authoring");
  const auto invalid = env.dir() / "unsupported.vmf";
  write(invalid, "world { id 1 classname worldspawn } cordons { unknown {} }");
  REQUIRE(document
            .load(
              config.environmentConfig,
              config.gameInfo,
              *config.mapFormat,
              vm::bbox3d{8192},
              invalid)
            .is_success());
  CHECK_FALSE(child<QPushButton>(editor, "cordonAddRecord")->isEnabled());
  CHECK_FALSE(child<QPushButton>(editor, "cordonApply")->isEnabled());
  CHECK_FALSE(child<QLabel>(editor, "cordonStatus")->text().isEmpty());
  mdl::selectNodes(document.map(), {&document.map().worldNode()});
  REQUIRE(mdl::setEntityProperty(document.map(), "message", "independent"));
  document.map().undoCommand();
  CHECK_FALSE(document.map().modified());
}
} // namespace tb::ui
