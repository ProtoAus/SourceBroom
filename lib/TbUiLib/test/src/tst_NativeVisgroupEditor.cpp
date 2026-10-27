/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTest>
#include <QTreeWidget>

#include "fs/TestEnvironment.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/Map_NativeVisgroups.h"
#include "mdl/Map_NodeVisibility.h"
#include "mdl/Map_Selection.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"
#include "ui/MapDocument.h"
#include "ui/MapDocumentFixture.h"
#include "ui/NativeVisgroupEditor.h"

#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <set>

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{
namespace
{
const auto config = mdl::MapFixtureConfig{.mapFormat = mdl::MapFormat::SourceVMF};
class VisgroupSettingsScope
{
  QSettings::Format m_previous = QSettings::defaultFormat();

public:
  explicit VisgroupSettingsScope(const std::filesystem::path& path)
  {
    QSettings::setDefaultFormat(QSettings::IniFormat);
    for (const auto scope : {QSettings::UserScope, QSettings::SystemScope})
    {
      QSettings::setPath(
        QSettings::IniFormat, scope, QString::fromStdString(path.string()));
    }
  }
  ~VisgroupSettingsScope() { QSettings::setDefaultFormat(m_previous); }
};
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
void edit(QWidget& editor, const char* value)
{
  auto* field = child<QLineEdit>(editor, "visgroupName");
  field->selectAll();
  QTest::keyClicks(field, value);
}
const auto minimal = std::string{
  "visgroups { visgroup { name Loaded visgroupid 007 color \"1 2 3\" } }\n"
  "world { id 1 classname worldspawn }\n"
  "entity { id 0002 classname info_target origin \"0 0 0\" editor { visgroupid 7 } }\n"};
} // namespace
TEST_CASE("NativeVisgroup UI acted live dispatch", "[vmf][visgroups]")
{
  auto env = fs::TestEnvironment{};
  auto settings = VisgroupSettingsScope{env.dir()};
  write(env.dir() / "loaded.vmf", minimal);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(env.dir() / "loaded.vmf", config);
  auto& map = document.map();
  auto editor = NativeVisgroupEditor{document};
  CHECK(
    QSettings{}.fileName().startsWith(
      QString::fromStdString(env.dir().generic_string())));
  CHECK(serialize(map) == minimal);
  const auto count = map.modificationCount();
  click(editor, "visgroupApply");
  CHECK(map.modificationCount() == count);
  CHECK(serialize(map) == minimal);
  child<QComboBox>(editor, "visgroupParent")->setCurrentIndex(1);
  edit(editor, "invalid self parent");
  click(editor, "visgroupApply");
  CHECK(serialize(map) == minimal);
  CHECK(map.modificationCount() == count);
  child<QComboBox>(editor, "visgroupParent")->setCurrentIndex(0);
  edit(editor, "Live UNSAVED");
  QTest::mouseClick(child<QPushButton>(editor, "visgroupApply"), Qt::LeftButton);
  CHECK(mdl::nativeVisgroups(map).records[0].name == "Live UNSAVED");
  CHECK(serialize(map).find("Live UNSAVED") != std::string::npos);
  CHECK(map.modificationCount() == count + 1);
  CHECK(map.undoCommandName() != nullptr);
}
namespace
{
std::string cube(const int id, const std::string& metadata = {})
{
  const auto planes = std::array<std::string, 6>{
    "(0 0 0) (0 64 0) (0 0 64)",
    "(64 0 0) (64 0 64) (64 64 0)",
    "(0 0 0) (0 0 64) (64 0 0)",
    "(0 64 0) (64 64 0) (0 64 64)",
    "(0 0 0) (64 0 0) (0 64 0)",
    "(0 0 64) (0 64 64) (64 0 64)"};
  auto text = "solid { id " + std::to_string(id) + "\n";
  for (size_t i = 0; i < planes.size(); ++i)
  {
    text += "side { id " + std::to_string(id + int(i) + 1) + " plane \"" + planes[i]
      + "\" material synthetic/grid uaxis \"[1 0 0 0] 0.25\" vaxis \"[0 -1 0 0] 0.25\" rotation 0 lightmapscale 16 }\n";
  }
  return text + metadata + "}\n";
}
std::string familySource()
{
  auto source = std::string{
    "// public loaded family; retained numeric text\n"
    "visgroups { visgroup { name Root visgroupid 007 color \"1 2 3\" visgroup { name Overlap visgroupid 8 color \"4 5 6\" } } visgroup { name Isolated visgroupid 9 color \"7 8 9\" } }\n"
    "world { id 1 classname worldspawn group { id 30 editor { visgroupid 7 } } group { id 31 editor { groupid 30 } }\n"}
    + cube(2, "editor { groupid 31 visgroupid 007 visgroupid 8 }")
    + "}\nentity { id 40 classname info_target origin \"128.000 0 0\" editor { visgroupid 8 visgroupautoshown 0 } connections { OnX \"a,b,,0,-1\" OnX \"c,d,,0,-1\" } }\n"
    "hidden { entity { id 41 classname info_target origin \"192 0 0\" editor { visgroupid 8 } } }\n"
    "entity { id 50 classname func_detail " + cube(60) + cube(70) + "}\n"
    "entity { id 99 classname info_target origin \"256 0 0\" editor { visgroupid 9 } future { token \"-001.250\" } }\n";
  auto crlf = std::string{};
  for (const auto c : source)
  {
    crlf += c == '\n' ? "\r\n" : std::string(1, c);
  }
  return crlf;
}
QTreeWidgetItem* row(QWidget& editor, const std::string& name)
{
  const auto rows =
    child<QTreeWidget>(editor, "visgroupTree")
      ->findItems(QString::fromUtf8(name), Qt::MatchExactly | Qt::MatchRecursive, 0);
  REQUIRE(rows.size() == 1);
  return rows.front();
}
void choose(QWidget& editor, const std::string& name)
{
  child<QTreeWidget>(editor, "visgroupTree")->setCurrentItem(row(editor, name));
}
void chooseMember(QWidget& editor, const mdl::Map& map, const std::string& id)
{
  const auto metadata = mdl::nativeVisgroups(map);
  auto* tree = child<QTreeWidget>(editor, "visgroupMembers");
  tree->clearSelection();
  for (int i = 0; i < tree->topLevelItemCount(); ++i)
  {
    auto* item = tree->topLevelItem(i);
    if (metadata.members[size_t(item->data(0, Qt::UserRole).toInt())].nativeId == id)
    {
      tree->setCurrentItem(item);
      item->setSelected(true);
      return;
    }
  }
  FAIL("Missing native member row " << id);
}
mdl::NativeVisgroupMember member(const mdl::Map& map, const std::string& id)
{
  const auto metadata = mdl::nativeVisgroups(map);
  const auto found =
    std::ranges::find(metadata.members, id, &mdl::NativeVisgroupMember::nativeId);
  REQUIRE(found != metadata.members.end());
  return *found;
}
mdl::Node* node(mdl::Map& map, const std::string& id)
{
  mdl::Node* result = nullptr;
  const auto target = member(map, id).id;
  const auto visit = [&](auto&& self, mdl::Node& current) -> void {
    if (map.vmfScene()->registeredVisgroupMember(current) == target)
    {
      result = &current;
    }
    for (auto* child : current.children())
    {
      self(self, *child);
    }
  };
  visit(visit, map.worldNode());
  REQUIRE(result);
  return result;
}
void parentChoice(QWidget& editor, const std::string& nativeId)
{
  auto* combo = child<QComboBox>(editor, "visgroupParent");
  for (int i = 0; i < combo->count(); ++i)
  {
    if (combo->itemText(i).endsWith("[" + QString::fromStdString(nativeId) + "]"))
    {
      combo->setCurrentIndex(i);
      return;
    }
  }
  FAIL("Missing visgroup parent " << nativeId);
}
} // namespace
TEST_CASE("NativeVisgroup UI explicit RGB dispatch and exact history", "[vmf][visgroups]")
{
  auto env = fs::TestEnvironment{};
  auto settings = VisgroupSettingsScope{env.dir()};
  const auto original = familySource();
  write(env.dir() / "rgb.vmf", original);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(env.dir() / "rgb.vmf", config);
  auto& map = document.map();
  auto editor = NativeVisgroupEditor{document};
  // An actual existing UI action controls the absent-new-controls RED.
  edit(editor, "RGB control acted");
  click(editor, "visgroupApply");
  CHECK(mdl::nativeVisgroups(map).records[0].name == "RGB control acted");
  CHECK(serialize(map).find("color \"1 2 3\"") != std::string::npos);
  map.undoCommand();
  CHECK(serialize(map) == original);
  auto* red = child<QSpinBox>(editor, "visgroupRed");
  auto* green = child<QSpinBox>(editor, "visgroupGreen");
  auto* blue = child<QSpinBox>(editor, "visgroupBlue");
  auto* set = child<QPushButton>(editor, "visgroupSetColor");
  CHECK(red->value() == 1);
  CHECK(green->value() == 2);
  CHECK(blue->value() == 3);
  CHECK(row(editor, "Root")->text(2) == "1 2 3");
  const auto count = map.modificationCount();
  SECTION("name edits do not author draft RGB; explicit Set saves and replays")
  {
    red->setValue(17);
    green->setValue(0);
    blue->setValue(255);
    edit(editor, "Name only");
    click(editor, "visgroupApply");
    CHECK(
      mdl::nativeVisgroups(map).records[0].color == mdl::NativeVisgroupColor{1, 2, 3});
    CHECK(serialize(map).find("color \"1 2 3\"") != std::string::npos);
    map.undoCommand();
    CHECK(serialize(map) == original);
    red->setValue(17);
    green->setValue(0);
    blue->setValue(255);
    set->click();
    CHECK(
      mdl::nativeVisgroups(map).records[0].color == mdl::NativeVisgroupColor{17, 0, 255});
    CHECK(map.modificationCount() == count + 1);
    CHECK(row(editor, "Root")->text(2) == "17 0 255");
    const auto saved = serialize(map);
    CHECK(saved != original);
    REQUIRE(map.saveAs(env.dir() / "rgb-saved.vmf").is_success());
    auto reopenedFixture = MapDocumentFixture{};
    auto& reopened = reopenedFixture.load(env.dir() / "rgb-saved.vmf", config);
    CHECK(serialize(reopened.map()) == saved);
    CHECK(
      mdl::nativeVisgroups(reopened.map()).records[0].color
      == mdl::NativeVisgroupColor{17, 0, 255});
    map.undoCommand();
    CHECK(serialize(map) == original);
    map.redoCommand();
    CHECK(serialize(map) == saved);
    CHECK(red->value() == 17);
  }
  SECTION("new record has no invented RGB until explicit Set")
  {
    click(editor, "visgroupCreate");
    choose(editor, "New visgroup");
    CHECK_FALSE(mdl::nativeVisgroups(map).records.back().color);
    CHECK(child<QLabel>(editor, "visgroupColorState")->text().contains("not stored"));
    CHECK(red->value() == 255);
    CHECK(green->value() == 255);
    CHECK(blue->value() == 255);
    const auto created = serialize(map);
    click(editor, "visgroupApply");
    CHECK(serialize(map) == created);
    CHECK_FALSE(mdl::nativeVisgroups(map).records.back().color);
    set->click();
    CHECK(
      mdl::nativeVisgroups(map).records.back().color
      == mdl::NativeVisgroupColor{255, 255, 255});
    CHECK(serialize(map) != created);
    map.undoCommand();
    CHECK(serialize(map) == created);
    map.undoCommand();
    CHECK(serialize(map) == original);
  }
  SECTION("press-time color action cannot retarget another row")
  {
    red->setValue(99);
    Q_EMIT set->pressed();
    choose(editor, "Overlap");
    Q_EMIT set->clicked();
    CHECK(serialize(map) == original);
    CHECK(map.modificationCount() == count);
    CHECK(child<QLabel>(editor, "visgroupStatus")->text().contains("Stale"));
  }
}

TEST_CASE("NativeVisgroup UI early child moves behind retained roots", "[vmf][visgroups]")
{
  auto env = fs::TestEnvironment{};
  auto settings = VisgroupSettingsScope{env.dir()};
  const auto original = familySource();
  write(env.dir() / "family.vmf", original);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(env.dir() / "family.vmf", config);
  auto& map = document.map();
  auto editor = NativeVisgroupEditor{document};
  const auto count = map.modificationCount();
  choose(editor, "Overlap");
  child<QComboBox>(editor, "visgroupParent")->setCurrentIndex(0);
  click(editor, "visgroupApply");
  CHECK(row(editor, "Overlap")->parent() == nullptr);
  auto roots = std::vector<std::string>{};
  for (const auto& record : mdl::nativeVisgroups(map).records)
  {
    if (!record.parent)
    {
      roots.push_back(record.nativeId);
    }
  }
  REQUIRE(roots == std::vector<std::string>{"7", "9", "8"});
  CHECK(map.modificationCount() == count + 1);
  const auto saved = serialize(map);
  CHECK(saved != original);
  REQUIRE(map.saveAs(env.dir() / "reparented.vmf").is_success());
  auto reopenedFixture = MapDocumentFixture{};
  auto& reopened = reopenedFixture.load(env.dir() / "reparented.vmf", config);
  CHECK(serialize(reopened.map()) == saved);
  const auto records = mdl::nativeVisgroups(reopened.map()).records;
  REQUIRE(records.size() == 3);
  CHECK(records.back().nativeId == "8");
  CHECK_FALSE(records.back().parent.has_value());
  map.undoCommand();
  CHECK(serialize(map) == original);
}

TEST_CASE(
  "NativeVisgroup UI loaded complete family and continued saved history",
  "[vmf][visgroups]")
{
  auto env = fs::TestEnvironment{};
  auto settings = VisgroupSettingsScope{env.dir()};
  const auto original = familySource();
  write(env.dir() / "family.vmf", original);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(env.dir() / "family.vmf", config);
  auto& map = document.map();
  auto editor = NativeVisgroupEditor{document};
  CHECK(serialize(map) == original);
  REQUIRE(mdl::nativeVisgroups(map).refusalReason.empty());
  REQUIRE(member(map, "2").editable);
  REQUIRE(member(map, "60").editable);
  REQUIRE_FALSE(member(map, "99").editable);
  choose(editor, "Root");
  edit(editor, "Renamed root");
  click(editor, "visgroupApply");
  CHECK(mdl::nativeVisgroups(map).records[0].name == "Renamed root");
  CHECK(child<QLabel>(editor, "visgroupSummary")->text().contains("not set"));
  parentChoice(editor, "7");
  click(editor, "visgroupCreate");
  const auto afterCreate = mdl::nativeVisgroups(map);
  REQUIRE(afterCreate.records.size() == 4);
  const auto newRecord = afterCreate.records.back();
  choose(editor, "New visgroup");
  edit(editor, "New child");
  click(editor, "visgroupApply");
  choose(editor, "Overlap");
  parentChoice(editor, newRecord.nativeId);
  click(editor, "visgroupApply");
  CHECK(row(editor, "Overlap")->parent() == row(editor, "New child"));
  chooseMember(editor, map, "60");
  click(editor, "visgroupAssignMembers");
  CHECK(member(map, "60").memberships == std::vector<std::string>{"8"});
  CHECK(member(map, "50").memberships.empty());
  CHECK(member(map, "70").memberships.empty());
  CHECK(
    child<QTreeWidget>(editor, "visgroupMembers")
      ->currentItem()
      ->text(0)
      .contains("owner 50"));
  choose(editor, "Renamed root");
  chooseMember(editor, map, "50");
  click(editor, "visgroupAssignMembers");
  mdl::selectNodes(map, {node(map, "70")});
  click(editor, "visgroupUnassignSelection");
  CHECK(member(map, "70").memberships.empty());
  CHECK(member(map, "50").memberships == std::vector<std::string>{"7"});
  click(editor, "visgroupAssignSelection");
  CHECK(member(map, "50").memberships == std::vector<std::string>{"7"});
  CHECK(member(map, "60").memberships.size() == 2);
  CHECK(member(map, "70").memberships == std::vector<std::string>{"7"});
  const auto beforeHide = serialize(map);
  click(editor, "visgroupHide");
  CHECK(member(map, "2").manualShown == false);
  CHECK(member(map, "60").manualShown == false);
  CHECK(map.selection().nodes.empty());
  choose(editor, "Overlap");
  click(editor, "visgroupShow");
  CHECK(member(map, "2").manualShown == true);
  CHECK(member(map, "60").manualShown == true);
  CHECK(member(map, "70").manualShown == false);
  CHECK(member(map, "40").autoShown == false);
  CHECK(member(map, "41").wrapperHidden);
  CHECK(node(map, "2")->effectiveNativeVisibility().manual);
  CHECK_FALSE(node(map, "40")->visible());
  CHECK_FALSE(node(map, "41")->visible());
  CHECK(
    child<QLabel>(editor, "visgroupSummary")
      ->text()
      .contains("effectively native hidden"));
  chooseMember(editor, map, "2");
  click(editor, "visgroupUnassignMembers");
  CHECK(member(map, "2").memberships == std::vector<std::string>{"7"});
  map.undoCommand();
  CHECK(member(map, "2").memberships.size() == 2);
  map.redoCommand();
  choose(editor, "Isolated");
  const auto protectedBefore = serialize(map);
  const auto protectedCount = map.modificationCount();
  click(editor, "visgroupHide");
  CHECK(serialize(map) == protectedBefore);
  CHECK(map.modificationCount() == protectedCount);
  CHECK_FALSE(child<QLabel>(editor, "visgroupStatus")->text().isEmpty());
  click(editor, "visgroupDelete");
  CHECK(serialize(map) == protectedBefore);
  choose(editor, "New child");
  click(editor, "visgroupDelete");
  CHECK(mdl::nativeVisgroups(map).records.size() == 2);
  CHECK(member(map, "60").memberships == std::vector<std::string>{"7"});
  CHECK(member(map, "40").memberships.empty());
  const auto savedBytes = serialize(map);
  CHECK(savedBytes.find("OnX \"a,b,,0,-1\" OnX \"c,d,,0,-1\"") != std::string::npos);
  CHECK(savedBytes.find("-001.250") != std::string::npos);
  CHECK(savedBytes.find("128.000") != std::string::npos);
  REQUIRE(map.saveAs(env.dir() / "saved.vmf").is_success());
  CHECK(serialize(map) == savedBytes);
  auto reopenedFixture = MapDocumentFixture{};
  auto& reopened = reopenedFixture.load(env.dir() / "saved.vmf", config);
  auto reopenedEditor = NativeVisgroupEditor{reopened};
  CHECK(serialize(reopened.map()) == savedBytes);
  choose(reopenedEditor, "Renamed root");
  edit(reopenedEditor, "Continued");
  click(reopenedEditor, "visgroupApply");
  CHECK(mdl::nativeVisgroups(reopened.map()).records[0].name == "Continued");
  reopened.map().undoCommand();
  CHECK(serialize(reopened.map()) == savedBytes);
  while (map.undoCommandName())
  {
    map.undoCommand();
  }
  CHECK(serialize(map) == original);
  CHECK(child<QLineEdit>(editor, "visgroupName")->text() == "Root");
  while (map.redoCommandName())
  {
    map.redoCommand();
  }
  CHECK(serialize(map) == savedBytes);
  CHECK(beforeHide != savedBytes);
}
TEST_CASE(
  "NativeVisgroup UI press identity stale scope and explicit name bytes",
  "[vmf][visgroups]")
{
  auto env = fs::TestEnvironment{};
  auto settings = VisgroupSettingsScope{env.dir()};
  auto original = minimal;
  original.replace(
    original.find("Loaded"), 6, std::string{"bad"} + std::bit_cast<char>(uint8_t{255}));
  write(env.dir() / "raw.vmf", original);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(env.dir() / "raw.vmf", config);
  auto editor = NativeVisgroupEditor{document};
  click(editor, "visgroupApply");
  CHECK(serialize(document.map()) == original);
  click(editor, "visgroupCreate");
  CHECK(
    mdl::nativeVisgroups(document.map()).records[0].name
    == std::string{"bad"} + std::bit_cast<char>(uint8_t{255}));
  document.map().undoCommand();
  CHECK(serialize(document.map()) == original);
  click(editor, "visgroupCreate");
  auto* apply = child<QPushButton>(editor, "visgroupApply");
  edit(editor, "queued");
  QTest::mousePress(apply, Qt::LeftButton);
  SECTION("undo revision")
  {
    document.map().undoCommand();
    document.map().redoCommand();
  }
  SECTION("same name recreation")
  {
    auto request = mdl::NativeVisgroupRequest{mdl::nativeVisgroups(document.map())};
    request.records.clear();
    REQUIRE(
      mdl::applyNativeVisgroups(document.map(), request).status
      == mdl::NativeVisgroupApplyStatus::Changed);
    request = mdl::NativeVisgroupRequest{mdl::nativeVisgroups(document.map())};
    request.records.push_back({mdl::NativeVisgroupDraftKey{1}, std::nullopt, "Loaded"});
    REQUIRE(
      mdl::applyNativeVisgroups(document.map(), request).status
      == mdl::NativeVisgroupApplyStatus::Changed);
  }
  SECTION("document replacement")
  {
    REQUIRE(document
              .load(
                config.environmentConfig,
                config.gameInfo,
                *config.mapFormat,
                vm::bbox3d{8192},
                env.dir() / "raw.vmf")
              .is_success());
  }
  SECTION("row switch")
  {
    choose(editor, "New visgroup");
  }
  const auto before = serialize(document.map());
  const auto count = document.map().modificationCount();
  QTest::mouseRelease(apply, Qt::LeftButton);
  CHECK(serialize(document.map()) == before);
  CHECK(document.map().modificationCount() == count);
  CHECK(child<QLabel>(editor, "visgroupStatus")->text().contains("Stale"));
  edit(editor, "Explicit replacement");
  click(editor, "visgroupApply");
  CHECK(child<QLineEdit>(editor, "visgroupName")->text() == "Explicit replacement");
  const auto crossBefore = serialize(document.map());
  auto* create = child<QPushButton>(editor, "visgroupCreate");
  auto* remove = child<QPushButton>(editor, "visgroupDelete");
  QTest::mousePress(create, Qt::LeftButton);
  QTest::mousePress(remove, Qt::LeftButton);
  QTest::mouseRelease(create, Qt::LeftButton);
  CHECK(serialize(document.map()) == crossBefore);
  // Cancel the still-down other button, then a fresh matched delete must act.
  QTest::mouseRelease(remove, Qt::LeftButton);
  CHECK(serialize(document.map()) == crossBefore);
  remove->click();
  CHECK(serialize(document.map()) != crossBefore);
}

TEST_CASE(
  "NativeVisgroup UI read view inherited session unknown and admission",
  "[vmf][visgroups]")
{
  auto env = fs::TestEnvironment{};
  auto settings = VisgroupSettingsScope{env.dir()};
  const auto original = std::string{
    "visgroups { visgroup { name Root visgroupid 7 color \"1 2 3\" } }\n"
    "world { id 1 classname worldspawn group { id 30 editor { visgroupshown 0 } } }\n"
    "entity { id 40 classname info_target origin \"0 0 0\" editor { groupid 30 "
    "visgroupid 7 visgroupshown 1 } future { x 1 } }\n"
    "entity { id 41 classname info_target origin \"128 0 0\" }\n"};
  write(env.dir() / "view.vmf", original);
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.load(env.dir() / "view.vmf", config);
  auto& map = document.map();
  const auto baseline = mdl::nativeVisgroups(map);
  const auto count = map.modificationCount();
  const auto view = mdl::nativeVisgroupView(map);
  const auto display = [&](const std::string& id) {
    const auto m = member(map, id);
    const auto found =
      std::ranges::find(view.members, m.id, &mdl::NativeVisgroupMemberView::id);
    REQUIRE(found != view.members.end());
    return *found;
  };
  CHECK(display("40").effectiveNativeVisibility->manual);
  CHECK(display("40").sessionVisible == true);
  CHECK_FALSE(display("30").effectiveNativeVisibility.has_value());
  CHECK_FALSE(display("30").sessionVisible.has_value());
  CHECK(display("41").sessionVisible == true);
  CHECK_FALSE(display("41").effectiveNativeVisibility->hidden());
  auto editor = NativeVisgroupEditor{document};
  chooseMember(editor, map, "40");
  auto* selected = child<QTreeWidget>(editor, "visgroupMembers")->currentItem();
  CHECK(selected->text(2).contains("Own manual shown"));
  CHECK(selected->text(2).contains("including inherited): manual"));
  CHECK(selected->text(2).contains("Session shown"));
  CHECK(selected->text(2).contains("Protected"));
  CHECK(mdl::nativeVisgroups(map).revision == baseline.revision);
  CHECK(map.modificationCount() == count);
  // The actual mixed-scene transaction previously refused session visibility.
  // It must now commit without persisting session state or changing native causes.
  chooseMember(editor, map, "41");
  mdl::hideNodes(map, {node(map, "41")});
  REQUIRE(map.undoCommandName());
  CHECK_FALSE(node(map, "41")->sessionVisible());
  CHECK(
    child<QTreeWidget>(editor, "visgroupMembers")
      ->currentItem()
      ->text(2)
      .contains("Session hidden"));
  CHECK(serialize(map) == original);
  CHECK(mdl::nativeVisgroups(map).revision == baseline.revision);
  CHECK(map.modificationCount() == count);
  CHECK(node(map, "40")->effectiveNativeVisibility().manual);
  REQUIRE(map.saveAs(env.dir() / "mixed-hidden.vmf").is_success());
  CHECK(serialize(map) == original);
  map.undoCommand();
  CHECK(node(map, "41")->sessionVisible());
  CHECK(
    child<QTreeWidget>(editor, "visgroupMembers")
      ->currentItem()
      ->text(2)
      .contains("Session shown"));
  CHECK(serialize(map) == original);
  // Isolate genuine ordinary session command+undo on a supported loaded scene.
  write(env.dir() / "session.vmf", minimal);
  auto sessionFixture = MapDocumentFixture{};
  auto& sessionDocument = sessionFixture.load(env.dir() / "session.vmf", config);
  auto& sessionMap = sessionDocument.map();
  auto sessionEditor = NativeVisgroupEditor{sessionDocument};
  chooseMember(sessionEditor, sessionMap, "2");
  const auto sessionBaseline = mdl::nativeVisgroups(sessionMap);
  const auto sessionCount = sessionMap.modificationCount();
  mdl::hideNodes(sessionMap, {node(sessionMap, "2")});
  REQUIRE(sessionMap.undoCommandName());
  CHECK_FALSE(node(sessionMap, "2")->sessionVisible());
  CHECK(
    child<QTreeWidget>(sessionEditor, "visgroupMembers")
      ->currentItem()
      ->text(2)
      .contains("Session hidden"));
  CHECK(mdl::nativeVisgroups(sessionMap).revision == sessionBaseline.revision);
  CHECK(sessionMap.modificationCount() == sessionCount);
  CHECK(serialize(sessionMap) == minimal);
  sessionMap.undoCommand();
  CHECK(
    child<QTreeWidget>(sessionEditor, "visgroupMembers")
      ->currentItem()
      ->text(2)
      .contains("Session shown"));
  CHECK(mdl::nativeVisgroups(sessionMap).revision == sessionBaseline.revision);
  CHECK(sessionMap.modificationCount() == sessionCount);
  CHECK_FALSE(node(sessionMap, "2")->effectiveNativeVisibility().hidden());
  CHECK(serialize(sessionMap) == minimal);
  const auto leadingView = mdl::nativeVisgroupView(sessionMap);
  REQUIRE(leadingView.members.size() == 1);
  REQUIRE(leadingView.members[0].effectiveNativeVisibility);
  CHECK_FALSE(leadingView.members[0].effectiveNativeVisibility->hidden());
  CHECK(leadingView.members[0].sessionVisible == true);
  CHECK(sessionMap.vmfScene()->nativeId(*node(sessionMap, "2")) == "0002");
  click(sessionEditor, "visgroupCreate");
  choose(sessionEditor, "New visgroup");
  mdl::selectNodes(sessionMap, {node(sessionMap, "2")});
  click(sessionEditor, "visgroupAssignSelection");
  CHECK(member(sessionMap, "2").memberships.size() == 2);
  CHECK(serialize(sessionMap).find("id 0002") != std::string::npos);

  // Same numeric object IDs remain protected, but observations use exact handles.
  const auto duplicateSource = std::string{
    "world { id 1 classname worldspawn }\n"
    "entity { id 0002 classname info_target origin \"0 0 0\" editor { visgroupshown 0 } "
    "}\n"
    "entity { id 2 classname info_target origin \"128 0 0\" editor { visgroupautoshown 0 "
    "} }\n"};
  write(env.dir() / "duplicate.vmf", duplicateSource);
  auto duplicateConfig = config;
  duplicateConfig.readOnlyVmfPreview = true;
  auto duplicateFixture = MapDocumentFixture{};
  auto& duplicateDocument =
    duplicateFixture.load(env.dir() / "duplicate.vmf", duplicateConfig);
  auto& duplicateMap = duplicateDocument.map();
  const auto duplicateView = mdl::nativeVisgroupView(duplicateMap);
  REQUIRE(duplicateView.members.size() == 2);
  auto* duplicateLayer = duplicateMap.worldNode().defaultLayer();
  REQUIRE(duplicateLayer->childCount() == 2);
  for (size_t i = 0; i < 2; ++i)
  {
    const auto exact =
      duplicateMap.vmfScene()->registeredVisgroupMember(*duplicateLayer->children()[i]);
    REQUIRE(exact);
    const auto observation = std::ranges::find(
      duplicateView.members, *exact, &mdl::NativeVisgroupMemberView::id);
    REQUIRE(observation != duplicateView.members.end());
    REQUIRE(observation->effectiveNativeVisibility);
    CHECK(observation->effectiveNativeVisibility->manual == (i == 0));
    CHECK(observation->effectiveNativeVisibility->automatic == (i == 1));
    CHECK(observation->sessionVisible == true);
    CHECK_FALSE(duplicateView.metadata.members[i].editable);
  }
  auto duplicateEditor = NativeVisgroupEditor{duplicateDocument};
  CHECK_FALSE(
    child<QPushButton>(duplicateEditor, "visgroupAssignSelection")->isEnabled());
  CHECK(serialize(duplicateMap) == duplicateSource);
  const auto a = member(map, "40").id;
  const auto b = member(map, "41").id;
  auto otherFixture = MapDocumentFixture{};
  auto& other = otherFixture.load(env.dir() / "view.vmf", config);
  const auto foreign = member(other.map(), "40").id;
  CHECK_FALSE(a < a);
  CHECK_FALSE(a < mdl::NativeVisgroupMemberId{a});
  CHECK((a < b) != (b < a));
  CHECK((a < foreign) != (foreign < a));
  auto ids = std::set<mdl::NativeVisgroupMemberId>{a, a, b, foreign};
  CHECK(ids.size() == 3);
  CHECK(ids.contains(a));
  auto previewConfig = config;
  previewConfig.readOnlyVmfPreview = true;
  auto previewFixture = MapDocumentFixture{};
  auto& preview = previewFixture.load(env.dir() / "view.vmf", previewConfig);
  auto previewEditor = NativeVisgroupEditor{preview};
  CHECK_FALSE(child<QPushButton>(previewEditor, "visgroupCreate")->isEnabled());
  CHECK_FALSE(child<QPushButton>(previewEditor, "visgroupShow")->isEnabled());
  auto nonNativeFixture = MapDocumentFixture{};
  auto& nonNative = nonNativeFixture.create();
  auto nonNativeEditor = NativeVisgroupEditor{nonNative};
  CHECK(nonNativeEditor.isHidden());
}
} // namespace tb::ui
