/*
 Copyright (C) 2026 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_2_1>
#include <QOpenGLVersionFunctionsFactory>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QTest>
#include <QTimer>

#include "TestEnvironment.h"
#include "base/Result.h"
#include "gl/ActiveShader.h"
#include "gl/GlManager.h"
#include "gl/Material.h"
#include "gl/MaterialCollection.h"
#include "gl/MaterialManager.h"
#include "gl/MaterialRenderFunc.h"
#include "gl/Resource.h"
#include "gl/ResourceManager.h"
#include "gl/ShaderManager.h"
#include "gl/Shaders.h"
#include "gl/TestGl.h"
#include "gl/TestUtils.h"
#include "gl/Texture.h"
#include "gl/VertexAttributeType.h"
#include "mdl/BrushBuilder.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushFaceHandle.h"
#include "mdl/BrushNode.h"
#include "mdl/EditorContext.h"
#include "mdl/EntityDefinition.h"
#include "mdl/EntityDefinitionManager.h"
#include "mdl/GameConfigFixture.h"
#include "mdl/Grid.h"
#include "mdl/GroupNode.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_Assets.h"
#include "mdl/Map_Groups.h"
#include "mdl/Map_NativeVisgroups.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"
#include "ui/AppControllerFixture.h"
#include "ui/CatchConfig.h"
#include "ui/FaceAttribsEditor.h"
#include "ui/GlQt.h"
#include "ui/MapDocument.h"
#include "ui/MapDocumentFixture.h"
#include "ui/MapViewBase.h"
#include "ui/MapViewToolBox.h"
#include "ui/MapWindow.h"
#include "ui/MaterialCollectionEditor.h"
#include "ui/NativeVisgroupEditor.h"
#include "ui/TabBook.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::ui
{

// Keep even default-constructed QSettings used by MapWindow inside the owned fixture.
class NativeGroupSettingsScope
{
  QSettings::Format m_previous = QSettings::defaultFormat();

public:
  explicit NativeGroupSettingsScope(const std::filesystem::path& path)
  {
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(
      QSettings::IniFormat, QSettings::UserScope, QString::fromStdString(path.string()));
    QSettings::setPath(
      QSettings::IniFormat,
      QSettings::SystemScope,
      QString::fromStdString(path.string()));
  }
  ~NativeGroupSettingsScope() { QSettings::setDefaultFormat(m_previous); }
};

TEST_CASE(
  "NativeVisgroup UI actual MapWindow live inspector and history", "[vmf][visgroups]")
{
  auto env = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{env.dir()};
  auto fixture = AppControllerFixture{};
  auto& app = fixture.appController();
  const auto source = std::string{
    "visgroups { visgroup { name Loaded visgroupid 7 color \"1 2 3\" } }\n"
    "world { id 1 classname worldspawn }\n"
    "entity { id 2 classname info_target editor { visgroupid 7 } }\n"};
  {
    auto stream = std::ofstream{env.dir() / "inspector.vmf", std::ios::binary};
    stream << source;
    REQUIRE(stream);
  }
  auto loaded = MapDocument::loadDocument(
    app.environmentConfig(),
    mdl::QuakeGameInfo,
    mdl::MapFormat::SourceVMF,
    vm::bbox3d{8192},
    env.dir() / "inspector.vmf",
    app.taskManager(),
    app.glManager().resourceManager());
  REQUIRE(loaded.is_success());
  auto window = MapWindow{app, std::move(loaded) | kdl::value()};
  auto& map = window.document().map();
  auto* editor = window.findChild<NativeVisgroupEditor*>();
  REQUIRE(editor);
  auto* field = editor->findChild<QLineEdit*>("visgroupName");
  auto* apply = editor->findChild<QPushButton*>("visgroupApply");
  auto* hide = editor->findChild<QPushButton*>("visgroupHide");
  REQUIRE(field);
  REQUIRE(apply);
  REQUIRE(hide);
  auto* inspectorBook = window.findChild<TabBook*>("Inspector_TabBook");
  REQUIRE(inspectorBook);
  inspectorBook->switchToPage(0);
  window.resize(1000, 700);
  CHECK(window.minimumSizeHint().height() <= 700);
  CHECK(window.minimumSizeHint().width() <= 1000);
  auto* inspectorScroll = window.findChild<QScrollArea*>("mapInspectorScroll");
  REQUIRE(inspectorScroll);
  // Process the real window layout without putting a test window on the desktop.
  window.setAttribute(Qt::WA_DontShowOnScreen);
  window.show();
  QApplication::processEvents();
  CHECK(window.height() <= 700);
  CHECK(window.width() <= 1000);
  inspectorScroll->ensureWidgetVisible(editor);
  CHECK(inspectorScroll->verticalScrollBar()->maximum() > 0);
  auto* editorScroll = editor->findChild<QScrollArea*>();
  REQUIRE(editorScroll);
  editorScroll->ensureWidgetVisible(hide);
  inspectorScroll->ensureWidgetVisible(hide, 0, 0);
  const auto inside =
    QRect{hide->mapTo(editorScroll->viewport(), QPoint{}), hide->size()};
  const auto outside =
    QRect{hide->mapTo(inspectorScroll->viewport(), QPoint{}), hide->size()};
  INFO(
    "inner viewport " << editorScroll->viewport()->width() << ","
                      << editorScroll->viewport()->height() << " target " << inside.x()
                      << "," << inside.y() << "," << inside.width() << ","
                      << inside.height());
  INFO(
    "outer viewport " << inspectorScroll->viewport()->width() << ","
                      << inspectorScroll->viewport()->height() << " target "
                      << outside.x() << "," << outside.y() << "," << outside.width()
                      << "," << outside.height());
  CHECK(editorScroll->viewport()->rect().contains(inside.center()));
  CHECK(inspectorScroll->viewport()->rect().contains(outside.center()));
  CHECK(inside.top() >= 0);
  CHECK(inside.bottom() < editorScroll->viewport()->height());
  CHECK(outside.top() >= 0);
  CHECK(outside.bottom() < inspectorScroll->viewport()->height());
  auto* parentCombo = editor->findChild<QComboBox*>("visgroupParent");
  REQUIRE(parentCombo);
  CHECK(
    parentCombo->sizeAdjustPolicy() == QComboBox::AdjustToMinimumContentsLengthWithIcon);
  const auto widthBefore = parentCombo->sizeHint().width();
  parentCombo->addItem(QString(1024, 'x'));
  CHECK(parentCombo->sizeHint().width() == widthBefore);
  parentCombo->removeItem(parentCombo->count() - 1);
  field->selectAll();
  QTest::keyClicks(field, "Unsaved inspector");
  apply->click();
  CHECK(mdl::nativeVisgroups(map).records[0].name == "Unsaved inspector");
  CHECK(
    map.vmfScene()->serialize(map.worldNode()).find("Unsaved inspector")
    != std::string::npos);
  auto bytesOnDisk = std::ifstream{env.dir() / "inspector.vmf", std::ios::binary};
  CHECK(std::string{std::istreambuf_iterator<char>{bytesOnDisk}, {}} == source);
  map.undoCommand();
  CHECK(field->text() == "Loaded");
  map.redoCommand();
  CHECK(field->text() == "Unsaved inspector");
  auto* point = map.worldNode().defaultLayer()->children().front();
  REQUIRE(point->visible());
  mdl::selectNodes(map, {point});
  hide->click();
  CHECK_FALSE(point->visible());
  CHECK(map.selection().nodes.empty());
  map.undoCommand();
  CHECK(point->visible());
  CHECK(map.selection().nodes == std::vector<mdl::Node*>{point});
  REQUIRE(map.saveAs(env.dir() / "saved.vmf").is_success());
  field->selectAll();
  QTest::keyClicks(field, "Continued inspector");
  apply->click();
  CHECK(mdl::nativeVisgroups(map).records[0].name == "Continued inspector");
  map.undoCommand();
  CHECK(field->text() == "Unsaved inspector");
  REQUIRE(window.document()
            .load(
              app.environmentConfig(),
              mdl::QuakeGameInfo,
              mdl::MapFormat::SourceVMF,
              vm::bbox3d{8192},
              env.dir() / "inspector.vmf",
              true)
            .is_success());
  CHECK_FALSE(editor->findChild<QPushButton*>("visgroupCreate")->isEnabled());
  CHECK_FALSE(hide->isEnabled());
}

TEST_CASE("NativeGroup native action flow and capability safety")
{
  const auto preview = GENERATE(false, true);
  const auto unsupported = GENERATE(false, true);
  auto env = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{env.dir()};
  auto fixture = AppControllerFixture{};
  auto& app = fixture.appController();
  const auto source =
    std::string{
      "world { id 1 classname worldspawn group { id 10 } "
      "group { id 11 editor { groupid 10 } } }\n"
      "entity { id 20 classname info_target editor { groupid 11 } }\n"
      "entity { id 21 classname info_target "}
    + (unsupported ? "editor { groupid 999 }" : "") + " }\n";
  {
    auto stream = std::ofstream{env.dir() / "groups.vmf", std::ios::binary};
    stream << source;
    REQUIRE(stream);
  }
  auto loaded = MapDocument::loadDocument(
    app.environmentConfig(),
    mdl::QuakeGameInfo,
    mdl::MapFormat::SourceVMF,
    vm::bbox3d{8192},
    env.dir() / "groups.vmf",
    app.taskManager(),
    app.glManager().resourceManager(),
    preview);
  REQUIRE(loaded.is_success());
  auto window = MapWindow{app, std::move(loaded) | kdl::value()};
  auto& map = window.document().map();
  REQUIRE(map.isReadOnlyVmfPreview() == preview);
  auto* layer = map.worldNode().defaultLayer();
  REQUIRE(layer->childCount() == 2);
  const auto groupIt = std::ranges::find_if(
    layer->children(), [](auto* node) { return dynamic_cast<mdl::GroupNode*>(node); });
  REQUIRE(groupIt != layer->children().end());
  auto* outer = static_cast<mdl::GroupNode*>(*groupIt);
  auto* inner = dynamic_cast<mdl::GroupNode*>(outer->children()[0]);
  REQUIRE(inner);
  auto* point = *std::ranges::find_if(
    layer->children(), [outer](auto* node) { return node != outer; });
  mdl::selectNodes(map, {outer, point});
  CHECK(window.canGroupSelectedObjects() == (!preview && !unsupported));
  QApplication::processEvents();
  auto* groupAction = window.findAction("Menu/Edit/Group");
  REQUIRE(groupAction);
  CHECK(groupAction->isEnabled() == (!preview && !unsupported));
  CHECK(!groupAction->shortcut().isEmpty());
  auto modalObserved = false;
  auto watchdog = QTimer{};
  QObject::connect(&watchdog, &QTimer::timeout, [&]() {
    if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))
    {
      modalObserved = true;
      dialog->reject();
    }
  });
  watchdog.start(1);
  const auto count = map.modificationCount();
  groupAction->trigger();
  if (preview || unsupported)
  {
    window.groupSelectedObjects(); // The action entry point also refuses directly.
  }
  CHECK_FALSE(modalObserved);
  if (preview || unsupported)
  {
    CHECK(map.modificationCount() == count);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }
  else
  {
    REQUIRE(map.selection().hasOnlyGroups());
    REQUIRE(map.selection().groups.size() == 1);
    auto* created = map.selection().groups.front();
    REQUIRE(created != outer);
    CHECK(created->name().starts_with("Group "));
    CHECK(created->children() == std::vector<mdl::Node*>{outer, point});
    CHECK(map.vmfScene()->serialize(map.worldNode()) != source);
    CHECK(map.modificationCount() == count + 1);
    CHECK_FALSE(window.canRenameSelectedGroups());
    window.renameSelectedGroups();
    CHECK_FALSE(modalObserved);
    CHECK_FALSE(mdl::canCreateLinkedDuplicate(map));
    CHECK_FALSE(mdl::canExtractLinkedGroups(map));
    QApplication::processEvents();
    for (const auto* path :
         {"Menu/Edit/Rename Groups",
          "Menu/Edit/Create Linked Duplicate",
          "Menu/Edit/Separate Linked Groups",
          "Menu/Edit/Extract Linked Groups"})
    {
      auto* action = window.findAction(path);
      REQUIRE(action);
      CHECK_FALSE(action->isEnabled());
    }
    auto mergeObserved = false;
    QTimer::singleShot(0, [&]() {
      auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      CHECK(popup);
      if (popup)
      {
        for (auto* action : popup->actions())
        {
          if (action->text().startsWith("Merge Groups"))
          {
            mergeObserved = true;
            CHECK_FALSE(action->isEnabled());
          }
        }
        popup->close();
      }
    });
    window.currentMapViewBase()->showPopupMenuLater();
    CHECK(mergeObserved);
    mdl::openGroup(map, *created);
    CHECK(map.editorContext().currentGroup() == created);
    mdl::selectNodes(map, {point});
    CHECK(window.canGroupSelectedObjects());
    window.toolBox().toggleRotateTool();
    REQUIRE(window.toolBox().anyModalToolActive());
    CHECK_FALSE(window.canGroupSelectedObjects());
    CHECK_FALSE(window.canUngroupSelectedObjects());
    window.toolBox().deactivateAllTools();
    window.groupSelectedObjects();
    REQUIRE(map.selection().hasOnlyGroups());
    auto* nested = map.selection().groups.front();
    REQUIRE(nested != created);
    CHECK(nested->parent() == created);
    CHECK(nested->children() == std::vector<mdl::Node*>{point});
    window.ungroupSelectedObjects();
    CHECK(point->parent() == created);
    mdl::closeGroup(map);
    mdl::selectNodes(map, {created});
    REQUIRE(window.canUngroupSelectedObjects());
    QApplication::processEvents();
    auto* ungroupAction = window.findAction("Menu/Edit/Ungroup");
    REQUIRE(ungroupAction);
    REQUIRE(ungroupAction->isEnabled());
    ungroupAction->trigger();
    CHECK(map.selection().nodes == std::vector<mdl::Node*>{outer, point});
    CHECK(inner->parent() == outer);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
    map.undoCommand();
    CHECK(map.selection().groups == std::vector<mdl::GroupNode*>{created});
    map.redoCommand();
    CHECK(map.selection().nodes == std::vector<mdl::Node*>{outer, point});
  }
  mdl::deselectAll(map);
  mdl::selectNodes(map, {outer});
  CHECK_FALSE(window.canRenameSelectedGroups());
  window.renameSelectedGroups();
  CHECK_FALSE(modalObserved);
  CHECK(window.canUngroupSelectedObjects() == !preview);
  if (preview)
  {
    window.ungroupSelectedObjects();
    CHECK(outer->parent() == layer);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }
  watchdog.stop();
}

TEST_CASE("NativeGroup nonnative name prompt and retained group controls")
{
  auto env = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{env.dir()};
  auto fixture = AppControllerFixture{};
  auto& app = fixture.appController();
  auto document = MapDocument::createDocument(
                    app.environmentConfig(),
                    mdl::QuakeGameInfo,
                    mdl::MapFormat::Valve,
                    vm::bbox3d{8192},
                    app.taskManager(),
                    app.glManager().resourceManager())
                  | kdl::value();
  auto window = MapWindow{app, std::move(document)};
  auto& map = window.document().map();
  auto builder = mdl::BrushBuilder{map.worldNode().mapFormat(), map.worldBounds()};
  auto* brush = new mdl::BrushNode{builder.createCube(64, "test") | kdl::value()};
  mdl::addNodes(map, {{map.worldNode().defaultLayer(), {brush}}});
  const auto nodes = map.worldNode().defaultLayer()->children();
  REQUIRE(!nodes.empty());
  mdl::selectNodes(map, nodes);
  REQUIRE(window.canGroupSelectedObjects());
  auto prompts = 0;
  auto watchdog = QTimer{};
  QObject::connect(&watchdog, &QTimer::timeout, [&]() {
    if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget()))
    {
      dialog->setTextValue(prompts++ == 0 ? "Control group" : "Renamed control");
      dialog->accept();
    }
  });
  watchdog.start(1);
  window.groupSelectedObjects();
  REQUIRE(prompts == 1);
  REQUIRE(map.selection().hasOnlyGroups());
  CHECK(map.selection().groups.front()->name() == "Control group");
  REQUIRE(window.canRenameSelectedGroups());
  window.renameSelectedGroups();
  CHECK(prompts == 2);
  CHECK(map.selection().groups.front()->name() == "Renamed control");
  window.toolBox().toggleRotateTool();
  REQUIRE(window.toolBox().anyModalToolActive());
  CHECK_FALSE(window.canGroupSelectedObjects());
  CHECK_FALSE(window.canUngroupSelectedObjects());
  window.toolBox().deactivateAllTools();
  REQUIRE(window.canUngroupSelectedObjects());
  window.ungroupSelectedObjects();
  CHECK(map.selection().nodes == nodes);
  watchdog.stop();
}

// Simple resource type for testing resource processing
struct TestResource
{
  void upload(gl::Gl&) const {}
  void drop(gl::Gl&) const {}
};

TEST_CASE("MapWindow")
{
  auto settingsEnvironment = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{settingsEnvironment.dir()};
  auto appControllerFixture = AppControllerFixture{};
  auto& appController = appControllerFixture.appController();

  auto document = MapDocument::createDocument(
                    appController.environmentConfig(),
                    mdl::QuakeGameInfo,
                    mdl::MapFormat::Valve,
                    vm::bbox3d{8192.0},
                    appController.taskManager(),
                    appController.glManager().resourceManager())
                  | kdl::value();

  auto window = MapWindow{appController, std::move(document)};

  SECTION("load resets grid size dropdown")
  {
    auto* gridChoice = window.findChild<QComboBox*>("MapWindow_GridChoice");
    REQUIRE(gridChoice != nullptr);

    const auto changedGridSize = 5;
    const auto changedGridIndex = changedGridSize - mdl::Grid::MinSize;
    const auto path = getFixtureRoot() / "test/ui/MapDocument/emptyValveMap.map";

    window.setGridSize(changedGridSize);
    QApplication::processEvents();

    REQUIRE(window.document().map().grid().size() == changedGridSize);
    REQUIRE(gridChoice->currentIndex() == changedGridIndex);

    REQUIRE(window.document().load(
      appController.environmentConfig(),
      mdl::QuakeGameInfo,
      mdl::MapFormat::Unknown,
      vm::bbox3d{8192.0},
      path));

    QApplication::processEvents();

    const auto loadedGridSize = window.document().map().grid().size();
    const auto loadedGridIndex = loadedGridSize - mdl::Grid::MinSize;

    CHECK(loadedGridSize != changedGridSize);
    CHECK(gridChoice->currentIndex() == loadedGridIndex);
    CHECK(gridChoice->currentData().toInt() == loadedGridSize);
  }

  SECTION("native preview rejects creation gestures without stranding selection history")
  {
    const auto preview = GENERATE(false, true);
    INFO("read-only preview: " << preview);
    auto env = fs::TestEnvironment{};
    auto source = std::string{
      "versioninfo { \"formatversion\" \"100\" }\n"
      "world { \"id\" \"1\" \"classname\" \"worldspawn\" solid { \"id\" \"2\"\n"};
    const auto planes = {
      "(0 0 0) (0 64 0) (0 0 64)",
      "(64 0 0) (64 0 64) (64 64 0)",
      "(0 0 0) (0 0 64) (64 0 0)",
      "(0 64 0) (64 64 0) (0 64 64)",
      "(0 0 0) (64 0 0) (0 64 0)",
      "(0 0 64) (0 64 64) (64 0 64)"};
    auto id = 3;
    for (const auto* plane : planes)
    {
      source += "side { \"id\" \"" + std::to_string(id++) + "\" \"plane\" \"" + plane
                + "\" \"material\" \"test/preview\" \"uaxis\" \"[1 0 0 0] 1\" "
                  "\"vaxis\" \"[0 -1 0 0] 1\" \"rotation\" \"0\" \"lightmapscale\" \"16\" }\n";
    }
    source += "} }\n";
    {
      auto stream = std::ofstream{env.dir() / "preview.vmf", std::ios::binary};
      REQUIRE(stream);
      stream.write(source.data(), std::streamsize(source.size()));
      REQUIRE(stream);
    }
    static const auto DefinitionFreeGame = []() {
      auto game = mdl::QuakeGameInfo;
      game.gameConfig.entityConfig.defFilePaths.clear();
      return game;
    }();
    const auto loaded = window.document().load(
      appController.environmentConfig(),
      DefinitionFreeGame,
      mdl::MapFormat::SourceVMF,
      vm::bbox3d{8192.0},
      env.dir() / "preview.vmf",
      preview);
    if (loaded.is_error())
    {
      INFO(std::get<Error>(loaded.error()).msg);
      REQUIRE(loaded.is_success());
    }
    auto& map = window.document().map();
    REQUIRE(map.entityDefinitionManager().definitions().empty());
    map.entityDefinitionManager().setDefinitions({
      mdl::EntityDefinition{
        .name = "test_point",
        .pointEntityDefinition = mdl::PointEntityDefinition{.bounds = vm::bbox3d{8.0}}},
      mdl::EntityDefinition{.name = "test_brush"},
    });
    REQUIRE(map.entityDefinitionManager().definition("test_point"));
    REQUIRE(map.entityDefinitionManager().definition("test_brush"));
    auto* brush =
      dynamic_cast<mdl::BrushNode*>(map.worldNode().defaultLayer()->children().front());
    REQUIRE(brush);
    mdl::selectNodes(map, {brush});
    REQUIRE(map.canUndoCommand());
    auto* view = window.currentMapViewBase();
    REQUIRE(view);
    const auto originalCount = map.worldNode().defaultLayer()->childCount();
    auto mime = QMimeData{};
    mime.setText("entity:test_point");
    auto enter = QDragEnterEvent{
      QPoint{16, 16}, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(view, &enter);
    if (preview)
    {
      CHECK_FALSE(enter.isAccepted());
      CHECK(map.canUndoCommand());
      CHECK(map.worldNode().defaultLayer()->childCount() == originalCount);
    }
    else
    {
      REQUIRE(enter.isAccepted());
      CHECK_FALSE(map.canUndoCommand());
      CHECK(map.worldNode().defaultLayer()->childCount() == originalCount + 1);
    }
    auto leave = QDragLeaveEvent{};
    QApplication::sendEvent(view, &leave);
    const auto historyRecovered = map.canUndoCommand();
    CHECK(historyRecovered);
    if (!historyRecovered)
    {
      // Clean up the broken control's stranded transaction after recording red.
      map.cancelTransaction();
    }
    REQUIRE(map.canUndoCommand());
    CHECK(map.worldNode().defaultLayer()->childCount() == originalCount);
    map.undoCommand();
    CHECK_FALSE(brush->selected());
    map.redoCommand();
    REQUIRE(brush->selected());
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);

    // Inspect the actual raw context menus, not the main Action allowlist.
    auto creationMenus = 0;
    auto creationActions = 0;
    QTimer::singleShot(0, [&]() {
      auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
      CHECK(popup != nullptr);
      if (!popup)
      {
        return;
      }
      for (auto* action : popup->actions())
      {
        auto* menu = action->menu();
        if (
          menu
          && (menu->title() == "Create Point Entity" || menu->title() == "Create Brush Entity"))
        {
          ++creationMenus;
          CHECK(menu->menuAction()->isEnabled() == !preview);
          for (auto* group : menu->actions())
          {
            auto* groupMenu = group->menu();
            CHECK(groupMenu != nullptr);
            if (groupMenu)
            {
              for (auto* creation : groupMenu->actions())
              {
                ++creationActions;
                CHECK(creation->isEnabled() == !preview);
              }
            }
          }
        }
      }
      popup->close();
    });
    view->showPopupMenuLater();
    CHECK(creationMenus == 2);
    CHECK(creationActions == 2);
    CHECK(map.canUndoCommand());
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }

  SECTION("canReloadMaterialCollections returns false when resources need processing")
  {
    using TestResourceT = gl::Resource<TestResource>;

    // Verify initial state: no resources pending processing
    REQUIRE(!appController.glManager().resourceManager().needsProcessing());
    CHECK(window.canReloadMaterialCollections());

    // Add a resource that needs processing
    auto testResource = std::make_shared<TestResourceT>(
      []() { return Result<TestResource>{TestResource{}}; });
    appController.glManager().resourceManager().addResource(testResource);

    REQUIRE(appController.glManager().resourceManager().needsProcessing());
    CHECK(!window.canReloadMaterialCollections());

    // Process all resources synchronously
    auto testGl = gl::TestGl{};
    const auto processContext = gl::ProcessContext{testGl, [](auto, auto) {}};
    gl::processResourcesSync(appController.glManager().resourceManager(), processContext);

    REQUIRE(!appController.glManager().resourceManager().needsProcessing());
    CHECK(window.canReloadMaterialCollections());
  }

  SECTION("canReloadEntityDefinitions returns false when resources need processing")
  {
    using TestResourceT = gl::Resource<TestResource>;

    // Verify initial state: no resources pending processing
    REQUIRE(!appController.glManager().resourceManager().needsProcessing());
    CHECK(window.canReloadEntityDefinitions());

    // Add a resource that needs processing
    auto testResource = std::make_shared<TestResourceT>(
      []() { return Result<TestResource>{TestResource{}}; });
    appController.glManager().resourceManager().addResource(testResource);

    REQUIRE(appController.glManager().resourceManager().needsProcessing());
    CHECK(!window.canReloadEntityDefinitions());

    // Process all resources synchronously
    auto testGl = gl::TestGl{};
    const auto processContext = gl::ProcessContext{testGl, [](auto, auto) {}};
    gl::processResourcesSync(appController.glManager().resourceManager(), processContext);

    REQUIRE(!appController.glManager().resourceManager().needsProcessing());
    CHECK(window.canReloadEntityDefinitions());
  }
}

TEST_CASE(
  "Source displacement inspector applies distance alpha and guards stale grid "
  "coordinates",
  "[source]")
{
  auto env = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{env.dir()};
  auto fixture = AppControllerFixture{};
  auto& appController = fixture.appController();
  const auto solid = [](const size_t id, const size_t power, const bool withAlphas) {
    const auto planes = std::array<std::string, 6>{
      "(0 0 0) (0 64 0) (0 0 64)",
      "(64 0 0) (64 0 64) (64 64 0)",
      "(0 0 0) (0 0 64) (64 0 0)",
      "(0 64 0) (64 64 0) (0 64 64)",
      "(0 0 0) (64 0 0) (0 64 0)",
      "(0 0 64) (0 64 64) (64 0 64)"};
    auto text = "solid { id " + std::to_string(id) + "\n";
    for (size_t face = 0; face < planes.size(); ++face)
    {
      text += "side { id " + std::to_string(id + face + 1) + " plane \"" + planes[face]
        + "\" material \"synthetic/missing\" uaxis \"[1 0 0 0] 1\" vaxis \"[0 -1 0 0] 1\" rotation 0\n";
      if (face == 5)
      {
        const auto size = (size_t{1} << power) + 1;
        text += "dispinfo { power " + std::to_string(power)
                + " startposition \"[0 0 64]\" normals {\n";
        for (size_t row = 0; row < size; ++row)
        {
          text += "row" + std::to_string(row) + " \"";
          for (size_t column = 0; column < size; ++column)
          {
            text += "0 0 1 ";
          }
          text += "\"\n";
        }
        text += "} distances {\n";
        for (size_t row = 0; row < size; ++row)
        {
          text += "row" + std::to_string(row) + " \"";
          for (size_t column = 0; column < size; ++column)
          {
            text += (row == size / 2 && column == size / 2) ? "16 " : "0 ";
          }
          text += "\"\n";
        }
        text += "}\n";
        if (withAlphas)
        {
          text += "alphas {\n";
          for (size_t row = 0; row < size; ++row)
          {
            text += "row" + std::to_string(row) + " \"";
            for (size_t column = 0; column < size; ++column)
            {
              text += "128.00 ";
            }
            text += "\"\n";
          }
          text += "}\n";
        }
        text += "}\n";
      }
      text += "}\n";
    }
    return text + "}\n";
  };
  env.createFile(
    "displacement.vmf",
    "world { id 1 classname worldspawn\n" + solid(2, 6, true) + solid(20, 2, true)
      + solid(40, 2, false) + "}\n");
  auto loaded = MapDocument::loadDocument(
    appController.environmentConfig(),
    mdl::QuakeGameInfo,
    mdl::MapFormat::SourceVMF,
    vm::bbox3d{8192.0},
    env.dir() / "displacement.vmf",
    appController.taskManager(),
    appController.glManager().resourceManager());
  REQUIRE(loaded.is_success());
  auto document = std::move(loaded) | kdl::value();
  auto editor = FaceAttribsEditor{appController, *document};
  auto& map = document->map();
  const auto nodes = map.worldNode().defaultLayer()->children();
  REQUIRE(nodes.size() == 3);
  auto* large = dynamic_cast<mdl::BrushNode*>(nodes[0]);
  auto* small = dynamic_cast<mdl::BrushNode*>(nodes[1]);
  auto* sparse = dynamic_cast<mdl::BrushNode*>(nodes[2]);
  REQUIRE(large);
  REQUIRE(small);
  REQUIRE(sparse);
  const auto largeFace = large->brush().findFace(vm::vec3d{0, 0, 1});
  const auto smallFace = small->brush().findFace(vm::vec3d{0, 0, 1});
  const auto sparseFace = sparse->brush().findFace(vm::vec3d{0, 0, 1});
  REQUIRE(largeFace);
  REQUIRE(smallFace);
  REQUIRE(sparseFace);
  auto* row = editor.findChild<QSpinBox*>("sourceDisplacementRow");
  auto* column = editor.findChild<QSpinBox*>("sourceDisplacementColumn");
  auto* distance = editor.findChild<QDoubleSpinBox*>("sourceDisplacementDistance");
  auto* apply = editor.findChild<QPushButton*>("sourceDisplacementApply");
  REQUIRE(row);
  REQUIRE(column);
  REQUIRE(distance);
  REQUIRE(apply);
  mdl::selectBrushFaces(map, {mdl::BrushFaceHandle{*large, *largeFace}});
  QApplication::processEvents();
  REQUIRE(row->maximum() == 64);
  row->setValue(64);
  mdl::deselectAll(map);
  mdl::selectBrushFaces(map, {mdl::BrushFaceHandle{*small, *smallFace}});
  // No processEvents: the selection has changed but the queued refresh has not.
  CHECK_NOTHROW(row->setValue(63));
  CHECK(row->maximum() == 64);
  QApplication::processEvents();
  CHECK(row->maximum() == 4);
  CHECK(row->value() == 4);
  row->setValue(2);
  column->setValue(2);
  REQUIRE(distance->value() == 16);
  REQUIRE(apply->isEnabled());
  const auto count = map.modificationCount();
  distance->setValue(24);
  apply->click();
  CHECK(map.modificationCount() == count + 1);
  CHECK(small->brush().face(*smallFace).sourceDisplacement()->distance(2, 2) == 24);
  map.undoCommand();
  QApplication::processEvents();
  CHECK(distance->value() == 16);
  CHECK(small->brush().face(*smallFace).sourceDisplacement()->distance(2, 2) == 16);
  // The actual distance controls above act before the absent-alpha-controls RED.
  auto* alpha = editor.findChild<QDoubleSpinBox*>("sourceDisplacementAlpha");
  auto* applyAlpha = editor.findChild<QPushButton*>("sourceDisplacementAlphaApply");
  REQUIRE(alpha);
  REQUIRE(applyAlpha);
  CHECK(alpha->value() == 128);
  REQUIRE(applyAlpha->isEnabled());
  const auto geometry = small->brush().face(*smallFace).sourceDisplacement()->positions();
  const auto alphaCount = map.modificationCount();
  alpha->setValue(17.5);
  applyAlpha->click();
  CHECK(map.modificationCount() == alphaCount + 1);
  CHECK(small->brush().face(*smallFace).sourceDisplacement()->alpha(2, 2) == 17.5);
  CHECK(small->brush().face(*smallFace).sourceDisplacement()->positions() == geometry);
  map.undoCommand();
  QApplication::processEvents();
  CHECK(alpha->value() == 128);
  map.redoCommand();
  QApplication::processEvents();
  CHECK(alpha->value() == 17.5);
  const auto repeatedCount = map.modificationCount();
  applyAlpha->click();
  CHECK(map.modificationCount() == repeatedCount);
  mdl::deselectAll(map);
  mdl::selectBrushFaces(map, {mdl::BrushFaceHandle{*sparse, *sparseFace}});
  QApplication::processEvents();
  CHECK_FALSE(sparse->brush().face(*sparseFace).sourceDisplacement()->canEditAlphas());
  CHECK_FALSE(alpha->isEnabled());
  CHECK_FALSE(applyAlpha->isEnabled());
  REQUIRE(distance->isEnabled());
  REQUIRE(apply->isEnabled());
  distance->setValue(24);
  apply->click();
  CHECK(sparse->brush().face(*sparseFace).sourceDisplacement()->distance(2, 2) == 24);
}

TEST_CASE("Source material settings group game mounts without changing the catalog")
{
  const auto source = GENERATE(false, true);
  auto fixture = AppControllerFixture{};
  auto& app = fixture.appController();
  auto document = MapDocument::createDocument(
                    app.environmentConfig(),
                    mdl::QuakeGameInfo,
                    mdl::MapFormat::Valve,
                    vm::bbox3d{8192},
                    app.taskManager(),
                    app.glManager().resourceManager())
                  | kdl::value();
  auto env = fs::TestEnvironment{};
  if (source)
  {
    env.createFile("source.vmf", "world { id 1 classname worldspawn }\n");
    const auto loaded = document->load(
      app.environmentConfig(),
      mdl::QuakeGameInfo,
      mdl::MapFormat::SourceVMF,
      vm::bbox3d{8192},
      env.dir() / "source.vmf");
    if (loaded.is_error())
    {
      INFO(std::get<Error>(loaded.error()).msg);
    }
    REQUIRE(loaded.is_success());
  }
  auto& map = document->map();
  map.setIsCommandCollationEnabled(false);
  auto collections = std::vector<gl::MaterialCollection>{};
  for (const auto* path :
       {"materials/brick", "materials/brick/nested", "materials/tools"})
  {
    auto materials = std::vector<gl::Material>{};
    materials.emplace_back(
      std::string{path} + "/control", gl::createTextureResource(gl::Texture{1, 1}));
    collections.emplace_back(path, std::move(materials));
  }
  map.materialManager().setMaterialCollections(std::move(collections));
  REQUIRE(map.materialManager().materials().size() == 3);
  mdl::setEnabledMaterialCollections(map, {"materials/brick"});
  const auto before = source ? map.vmfScene()->serialize(map.worldNode()) : std::string{};
  auto editor = MaterialCollectionEditor{*document};
  auto* available = editor.findChild<QListWidget*>("MaterialCollections_Available");
  auto* enabled = editor.findChild<QListWidget*>("MaterialCollections_Enabled");
  REQUIRE(available);
  REQUIRE(enabled);
  REQUIRE(enabled->count() == 1);
  CHECK(available->count() == (source ? 1 : 2));
  if (source)
  {
    CHECK(enabled->item(0)->text().toStdString() == mdl::QuakeGameInfo.gameConfig.name);
    CHECK(enabled->item(0)->toolTip().contains("Material root:"));
    CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
  }
  CHECK(
    mdl::enabledMaterialCollections(map)
    == std::vector<std::filesystem::path>{"materials/brick"});
  available->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
  available->itemDoubleClicked(available->item(0));
  CHECK(mdl::enabledMaterialCollections(map).size() == (source ? 3 : 2));
  CHECK(enabled->count() == (source ? 1 : 2));
  CHECK(map.materialManager().materials().size() == 3);
  map.undoCommand();
  CHECK(
    mdl::enabledMaterialCollections(map)
    == std::vector<std::filesystem::path>{"materials/brick"});
  if (source)
  {
    CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
  }
  map.redoCommand();
  enabled->setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
  enabled->itemDoubleClicked(enabled->item(0));
  CHECK(mdl::enabledMaterialCollections(map).size() == (source ? 0 : 1));
  CHECK(map.materialManager().materials().size() == 3);
  map.undoCommand();
  CHECK(mdl::enabledMaterialCollections(map).size() == (source ? 3 : 2));
  CHECK(map.materialManager().collections().size() == 3);
}

TEST_CASE("Source alpha preview GPU controls opt-in", "[.][source][render]")
{
  auto fixture = AppControllerFixture{};
  auto format = QSurfaceFormat{};
  format.setVersion(2, 1);
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
  auto surface = QOffscreenSurface{};
  surface.setFormat(format);
  surface.create();
  REQUIRE(surface.isValid());
  auto context = QOpenGLContext{};
  context.setFormat(surface.format());
  REQUIRE(context.create());
  REQUIRE(context.makeCurrent(&surface));
  auto* functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_2_1>(&context);
  REQUIRE(functions);
  REQUIRE(functions->initializeOpenGLFunctions());
  auto gl = GlQt{*functions};
  auto target = QOpenGLFramebufferObject{8, 8};
  REQUIRE(target.isValid());
  REQUIRE(target.bind());
  gl.viewport(0, 0, 8, 8);
  const auto positions = std::array<float, 12>{-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0};
  gl.enableClientState(GL_VERTEX_ARRAY);
  gl.vertexPointer(3, GL_FLOAT, 0, positions.data());
  auto env = fs::TestEnvironment{};
  env.createFile("alpha-preview.vertsh", R"(#version 120
varying vec4 modelCoordinates;
varying vec3 modelNormal;
varying vec4 faceColor;
varying vec3 viewVector;
varying vec4 worldCoordinates;
varying float sourceBlendWeight;
void main() {
  gl_Position = gl_Vertex;
  gl_TexCoord[0] = vec4(0.5, 0.5, 0.0, 1.0);
  modelCoordinates = worldCoordinates = vec4(0.0, 0.0, 0.0, 1.0);
  modelNormal = viewVector = vec3(0.0, 0.0, 1.0);
  faceColor = vec4(1.0);
  sourceBlendWeight = 0.0;
}
)");
  auto sourceRoot = std::filesystem::path{__FILE__};
  for (size_t i = 0; i < 5; ++i)
  {
    sourceRoot = sourceRoot.parent_path();
  }
  const auto shaderDir = sourceRoot / "app/TrenchBroom/resources/shader";
  REQUIRE(std::filesystem::is_regular_file(shaderDir / "Face.fragsh"));
  auto manager = gl::ShaderManager{[&](const auto& path) {
    const auto name = path.filename();
    return name == "alpha-preview.vertsh" ? env.dir() / name : shaderDir / name;
  }};
  struct Scenario
  {
    img::ImageAlphaDomain domain;
    uint8_t alpha;
    float fade;
  };
  const auto scenarios = std::array<Scenario, 6>{
    {{img::ImageAlphaDomain::Opaque, 51, 1.0f},
     {img::ImageAlphaDomain::Opaque, 51, 0.4f},
     {img::ImageAlphaDomain::Binary, 51, 0.4f},
     {img::ImageAlphaDomain::Binary, 204, 0.4f},
     {img::ImageAlphaDomain::Graduated, 51, 1.0f},
     {img::ImageAlphaDomain::Graduated, 204, 0.4f}}};
  for (auto config :
       {gl::Shaders::FaceShader,
        gl::Shaders::PatchShader,
        gl::Shaders::EntityModelShader,
        gl::Shaders::MaterialBrowserShader,
        gl::Shaders::UvViewShader})
  {
    const auto face = config.name == gl::Shaders::FaceShader.name
                      || config.name == gl::Shaders::PatchShader.name;
    const auto uv = config.name == gl::Shaders::UvViewShader.name;
    config.vertexShaders = {"alpha-preview.vertsh"};
    const auto loaded = manager.loadProgram(gl, config);
    if (loaded.is_error())
    {
      FAIL(std::get<Error>(loaded.error()).msg);
    }
    REQUIRE(loaded.is_success());
    auto shader = gl::ActiveShader{gl, manager, config};
    shader.set("Brightness", 1.0f);
    shader.set("Material", 0);
    if (face || uv)
    {
      shader.set("ApplyMaterial", true);
    }
    for (const auto* extension : {".vmt", ".VMT", ".png"})
    {
      for (const auto scenario : scenarios)
      {
        CAPTURE(config.name, extension, scenario.alpha, scenario.fade);
        auto pixels = gl::TextureBuffer{4};
        const auto rgba = std::array<uint8_t, 4>{255, 0, 0, scenario.alpha};
        std::copy(rgba.begin(), rgba.end(), pixels.data());
        auto texture = gl::Texture{
          1,
          1,
          Color{RgbaF{1, 0, 0, 1}},
          GL_RGBA,
          gl::NoEmbeddedDefaults{},
          std::move(pixels)};
        texture.setAlphaDomain(scenario.domain);
        auto resource = gl::createTextureResource(std::move(texture));
        resource->uploadSync(gl);
        auto material = gl::Material{"authored alpha control", resource};
        material.setRelativePath(std::string{"control"} + extension);
        const auto graduated = scenario.domain == img::ImageAlphaDomain::Graduated;
        const auto editorFade = face && !graduated ? scenario.fade : 1.0f;
        if (face)
        {
          shader.set("Alpha", editorFade);
        }
        gl::setAlphaFuncUniforms(shader, &material);
        gl.clearColor(0, 0, 1, 1);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.enable(GL_BLEND);
        gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        gl.depthMask(editorFade < 1.0f ? GL_FALSE : GL_TRUE);
        material.activate(gl, GL_NEAREST, GL_NEAREST);
        CHECK(functions->glIsEnabled(GL_BLEND) == GL_TRUE);
        auto depth = GLboolean{};
        gl.getBooleanv(GL_DEPTH_WRITEMASK, &depth);
        CHECK(depth == (graduated || editorFade < 1.0f ? GL_FALSE : GL_TRUE));
        if (
          scenario.domain == img::ImageAlphaDomain::Opaque
          && std::string_view{extension} != ".png")
        {
          // Acted red arm: leaking the reserved texture alpha really changes pixels.
          shader.set("IgnoreTextureAlpha", false);
          gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
          auto rawPixel = std::array<uint8_t, 4>{};
          functions->glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rawPixel.data());
          const auto rawRed = static_cast<int>(std::lround(scenario.alpha * editorFade));
          CHECK(std::abs(int(rawPixel[0]) - rawRed) <= 2);
          gl.clear(GL_COLOR_BUFFER_BIT);
          gl::setAlphaFuncUniforms(shader, &material);
        }
        gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
        auto pixel = std::array<uint8_t, 4>{};
        functions->glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        const auto discard =
          scenario.domain == img::ImageAlphaDomain::Binary && scenario.alpha < 128;
        const auto sourceMask = std::string_view{extension} != ".png" && !graduated;
        const auto opacity = discard      ? 0.0f
                             : sourceMask ? editorFade
                                          : scenario.alpha / 255.0f * editorFade;
        const auto red = static_cast<int>(std::lround(255.0f * opacity));
        CHECK(std::abs(int(pixel[0]) - red) <= 2);
        CHECK(std::abs(int(pixel[2]) - (255 - red)) <= 2);
        material.deactivate(gl);
        resource->dropSync(gl);
      }
    }
    struct CullState
    {
      bool enabled;
      GLenum mode;
      GLenum frontFace;
    };
    gl.pushAttrib(GL_POLYGON_BIT);
    for (const auto state :
         {CullState{true, GL_BACK, GL_CW},
          CullState{false, GL_FRONT, GL_CW},
          CullState{true, GL_FRONT, GL_CCW},
          CullState{true, GL_FRONT_AND_BACK, GL_CW}})
    {
      for (const auto noCull : {false, true})
      {
        for (const auto overrideFront : {false, true})
        {
          for (const auto graduated : {false, true})
          {
            CAPTURE(
              config.name,
              state.enabled,
              state.mode,
              state.frontFace,
              noCull,
              overrideFront,
              graduated);
            auto pixels = gl::TextureBuffer{4};
            const auto rgba = std::array<uint8_t, 4>{
              255, 0, 0, static_cast<uint8_t>(graduated ? 128 : 255)};
            std::copy(rgba.begin(), rgba.end(), pixels.data());
            auto texture = gl::Texture{
              1,
              1,
              Color{RgbaF{1, 0, 0, 1}},
              GL_RGBA,
              gl::SourceEmbeddedDefaults{noCull},
              std::move(pixels)};
            texture.setAlphaDomain(
              graduated ? img::ImageAlphaDomain::Graduated
                        : img::ImageAlphaDomain::Opaque);
            auto resource = gl::createTextureResource(std::move(texture));
            resource->uploadSync(gl);
            auto material = gl::Material{"authored culling control", resource};
            material.setRelativePath("control.vmt");
            if (overrideFront)
            {
              material.setCulling(gl::MaterialCulling::Front);
            }
            if (face)
            {
              shader.set("Alpha", 1.0f);
            }
            gl::setAlphaFuncUniforms(shader, &material);
            if (state.enabled)
            {
              gl.enable(GL_CULL_FACE);
            }
            else
            {
              gl.disable(GL_CULL_FACE);
            }
            gl.cullFace(state.mode);
            functions->glFrontFace(state.frontFace);
            gl.enable(GL_BLEND);
            gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gl.depthMask(GL_TRUE);
            gl.clearColor(0, 0, 1, 1);
            gl.clear(GL_COLOR_BUFFER_BIT);
            material.activate(gl, GL_NEAREST, GL_NEAREST);
            const auto activeCull = state.enabled && (overrideFront || !noCull);
            const auto activeMode = overrideFront ? GLenum{GL_FRONT} : state.mode;
            CHECK(
              functions->glIsEnabled(GL_CULL_FACE) == (activeCull ? GL_TRUE : GL_FALSE));
            auto mode = GLint{};
            functions->glGetIntegerv(GL_CULL_FACE_MODE, &mode);
            CHECK(mode == static_cast<GLint>(activeMode));
            auto depth = GLboolean{};
            gl.getBooleanv(GL_DEPTH_WRITEMASK, &depth);
            CHECK(depth == (graduated ? GL_FALSE : GL_TRUE));
            gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
            auto pixel = std::array<uint8_t, 4>{};
            functions->glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            const auto front = state.frontFace == GL_CCW;
            const auto visible =
              !activeCull
              || (activeMode != GL_FRONT_AND_BACK && !(activeMode == GL_FRONT && front) && !(activeMode == GL_BACK && !front));
            const auto red = visible ? (graduated ? 128 : 255) : 0;
            CHECK(std::abs(int(pixel[0]) - red) <= 2);
            CHECK(std::abs(int(pixel[2]) - (255 - red)) <= 2);
            material.deactivate(gl);
            CHECK(
              functions->glIsEnabled(GL_CULL_FACE)
              == (state.enabled ? GL_TRUE : GL_FALSE));
            functions->glGetIntegerv(GL_CULL_FACE_MODE, &mode);
            CHECK(mode == static_cast<GLint>(state.mode));
            functions->glGetIntegerv(GL_FRONT_FACE, &mode);
            CHECK(mode == static_cast<GLint>(state.frontFace));
            CHECK(functions->glIsEnabled(GL_BLEND) == GL_TRUE);
            gl.getBooleanv(GL_DEPTH_WRITEMASK, &depth);
            CHECK(depth == GL_TRUE);
            resource->dropSync(gl);
          }
        }
      }
    }
    gl.popAttrib();

    struct CutoutScenario
    {
      float reference;
      uint8_t alpha;
      float fade;
      bool alphaTest;
      bool overrideReference;
    };
    auto actedDefaultCutoff = false;
    gl.pushAttrib(GL_POLYGON_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (const auto scenario :
         {CutoutScenario{0.25f, 102, 1.0f, true, false},
          CutoutScenario{0.75f, 153, 1.0f, true, false},
          CutoutScenario{0.75f, 204, 0.4f, true, false},
          CutoutScenario{0.25f, 102, 0.4f, true, false},
          CutoutScenario{1.0f, 255, 1.0f, true, false},
          CutoutScenario{1.0f, 254, 1.0f, true, false},
          CutoutScenario{0.75f, 153, 1.0f, true, true},
          CutoutScenario{0.75f, 51, 1.0f, false, false},
          CutoutScenario{0.75f, 51, 0.4f, false, false},
          CutoutScenario{0.75f, 0, 1.0f, false, false}})
    {
      CAPTURE(
        config.name,
        scenario.reference,
        scenario.alpha,
        scenario.fade,
        scenario.alphaTest,
        scenario.overrideReference);
      auto pixels = gl::TextureBuffer{4};
      const auto rgba = std::array<uint8_t, 4>{255, 0, 0, scenario.alpha};
      std::copy(rgba.begin(), rgba.end(), pixels.data());
      auto texture = gl::Texture{
        1,
        1,
        Color{RgbaF{1, 0, 0, 1}},
        GL_RGBA,
        gl::SourceEmbeddedDefaults{true, scenario.reference},
        std::move(pixels)};
      texture.setAlphaDomain(
        scenario.alphaTest ? img::ImageAlphaDomain::Binary
                           : img::ImageAlphaDomain::Opaque);
      auto resource = gl::createTextureResource(std::move(texture));
      resource->uploadSync(gl);
      auto material = gl::Material{"authored cutoff control", resource};
      material.setRelativePath("control.vmt");
      if (scenario.overrideReference)
      {
        material.setAlphaFunc(gl::MaterialAlphaFunc::Compare::GreaterEqual, 0.25f);
      }
      const auto cutoff = material.effectiveAlphaFunc();
      CHECK(cutoff.has_value() == scenario.alphaTest);
      if (cutoff)
      {
        CHECK(
          cutoff->threshold == (scenario.overrideReference ? 0.25f : scenario.reference));
      }
      const auto editorFade = face ? scenario.fade : 1.0f;
      if (face)
      {
        shader.set("Alpha", editorFade);
      }
      gl::setAlphaFuncUniforms(shader, &material);
      gl.enable(GL_CULL_FACE);
      gl.cullFace(GL_FRONT_AND_BACK);
      gl.enable(GL_BLEND);
      gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      gl.depthMask(editorFade < 1.0f ? GL_FALSE : GL_TRUE);
      gl.clearColor(0, 0, 1, 1);
      gl.clear(GL_COLOR_BUFFER_BIT);
      material.activate(gl, GL_NEAREST, GL_NEAREST);
      CHECK(functions->glIsEnabled(GL_CULL_FACE) == GL_FALSE);
      CHECK(functions->glIsEnabled(GL_BLEND) == GL_TRUE);
      auto depth = GLboolean{};
      gl.getBooleanv(GL_DEPTH_WRITEMASK, &depth);
      CHECK(depth == (editorFade < 1.0f ? GL_FALSE : GL_TRUE));
      if (
        cutoff && cutoff->threshold > 0.5f && scenario.alpha >= 128
        && scenario.alpha < 192)
      {
        // Acted negative arm: an ignored authored high cutoff visibly leaks this pixel.
        shader.set("AlphaFuncThreshold", 0.5f);
        gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
        auto leaked = std::array<uint8_t, 4>{};
        functions->glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, leaked.data());
        CHECK(
          std::abs(int(leaked[0]) - static_cast<int>(std::lround(255.0f * editorFade)))
          <= 2);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl::setAlphaFuncUniforms(shader, &material);
        actedDefaultCutoff = true;
      }
      gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
      auto pixel = std::array<uint8_t, 4>{};
      functions->glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
      const auto kept = !cutoff || scenario.alpha / 255.0f >= cutoff->threshold;
      const auto red = kept ? static_cast<int>(std::lround(255.0f * editorFade)) : 0;
      CHECK(std::abs(int(pixel[0]) - red) <= 2);
      CHECK(std::abs(int(pixel[2]) - (255 - red)) <= 2);
      material.deactivate(gl);
      CHECK(functions->glIsEnabled(GL_CULL_FACE) == GL_TRUE);
      auto mode = GLint{};
      functions->glGetIntegerv(GL_CULL_FACE_MODE, &mode);
      CHECK(mode == GL_FRONT_AND_BACK);
      CHECK(functions->glIsEnabled(GL_BLEND) == GL_TRUE);
      gl.getBooleanv(GL_DEPTH_WRITEMASK, &depth);
      CHECK(depth == (editorFade < 1.0f ? GL_FALSE : GL_TRUE));
      resource->dropSync(gl);
    }
    CHECK(actedDefaultCutoff);
    gl.popAttrib();
  }
  {
    gl.pushAttrib(GL_ALL_ATTRIB_BITS);
    const auto image = [](const std::array<uint8_t, 4>& rgba) {
      auto pixels = gl::TextureBuffer{4};
      std::copy(rgba.begin(), rgba.end(), pixels.data());
      auto texture = gl::Texture{
        1,
        1,
        Color{RgbaF{1, 1, 1, 1}},
        GL_RGBA,
        gl::SourceEmbeddedDefaults{},
        std::move(pixels)};
      texture.setAlphaDomain(img::ImageAlphaDomain::Opaque);
      return texture;
    };
    auto texture = image({255, 0, 0, 255});
    texture.setSourceBlendTexture(image({0, 255, 0, 17}));
    auto resource = gl::createTextureResource(std::move(texture));
    resource->uploadSync(gl);
    REQUIRE(resource->get());
    REQUIRE(resource->get()->sourceBlendTexture());
    REQUIRE(resource->get()->sourceBlendTexture()->isReady());
    auto material = gl::Material{"terrain layers", resource};
    material.setRelativePath("terrain.vmt");
    auto plainResource = gl::createTextureResource(image({255, 0, 0, 255}));
    plainResource->uploadSync(gl);
    auto plain = gl::Material{"ordinary following terrain", plainResource};
    plain.setRelativePath("plain.vmt");
    // Unlike the five legacy fragment controls, link the REAL production vertex stage.
    auto config = gl::Shaders::FaceShader;
    config.name += "-source-terrain-real-vertex";
    const auto loaded = manager.loadProgram(gl, config);
    INFO((loaded.is_error() ? std::get<Error>(loaded.error()).msg : "success"));
    REQUIRE(loaded.is_success());
    auto shader = gl::ActiveShader{gl, manager, config};
    shader.set("Brightness", 1.0f);
    shader.set("Material", 0);
    shader.set("ApplyMaterial", true);
    shader.set("Alpha", 1.0f);
    gl::setAlphaFuncUniforms(shader, &material);
    gl.activeTexture(GL_TEXTURE0);
    gl.disable(GL_CULL_FACE);
    gl.disable(GL_BLEND);
    functions->glMatrixMode(GL_PROJECTION);
    functions->glLoadIdentity();
    functions->glMatrixMode(GL_MODELVIEW);
    functions->glLoadIdentity();
    const auto readPixel = [&] {
      auto pixel = std::array<uint8_t, 4>{};
      functions->glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
      return pixel;
    };
    // Old primary rendering actually acts before the new Face-only uniform path.
    gl.clearColor(0, 0, 1, 1);
    gl.clear(GL_COLOR_BUFFER_BIT);
    material.activate(gl, GL_NEAREST, GL_NEAREST);
    gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
    const auto primary = readPixel();
    CHECK(primary[0] >= 253);
    CHECK(primary[1] <= 2);
    CHECK(primary[2] <= 2);
    material.deactivate(gl);
    auto sentinel = GLuint{};
    functions->glGenTextures(1, &sentinel);
    gl.activeTexture(GL_TEXTURE1);
    gl.bindTexture(GL_TEXTURE_2D, sentinel);
    gl.activeTexture(GL_TEXTURE0 + 2);
    auto blend = gl::SourceBlendMaterialRenderFunc{shader, true, GL_NEAREST, GL_NEAREST};
    auto weights = std::array<float, 4>{};
    gl::VertexAttributeTypes::Uv11::setup(
      gl, shader.program(), 0, sizeof(float), reinterpret_cast<size_t>(weights.data()));
    const auto draw =
      [&](const gl::Material& subject, const float weight, const float fade) {
        weights.fill(weight);
        shader.set("Alpha", fade);
        gl::setAlphaFuncUniforms(shader, &subject);
        REQUIRE_NOTHROW(blend.before(gl, &subject));
        if (fade < 1.0f)
        {
          gl.enable(GL_BLEND);
          gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        gl.clearColor(0, 0, 1, 1);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.drawArrays(GL_TRIANGLE_STRIP, 0, 4);
        const auto pixel = readPixel();
        blend.after(gl, &subject);
        auto active = GLint{};
        gl.getIntegerv(GL_ACTIVE_TEXTURE, &active);
        CHECK(active == GL_TEXTURE0 + 2);
        gl.activeTexture(GL_TEXTURE1);
        auto binding = GLint{};
        gl.getIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        CHECK(binding == static_cast<GLint>(sentinel));
        gl.activeTexture(GL_TEXTURE0 + 2);
        return pixel;
      };
    for (const auto weight : {0.0f, 0.5f, 1.0f})
    {
      const auto pixel = draw(material, weight, 1.0f);
      CHECK(std::abs(int(pixel[0]) - int(std::lround(255 * (1 - weight)))) <= 2);
      CHECK(std::abs(int(pixel[1]) - int(std::lround(255 * weight))) <= 2);
      CHECK(pixel[2] <= 2);
      CHECK(pixel[3] >= 253); // Child alpha17 is not terrain weight or Source opacity.
    }
    const auto faded = draw(material, 0.5f, 0.5f);
    CHECK(std::abs(int(faded[0]) - 64) <= 2);
    CHECK(std::abs(int(faded[1]) - 64) <= 2);
    CHECK(std::abs(int(faded[2]) - 128) <= 2);
    const auto following = draw(plain, 1.0f, 1.0f);
    CHECK(following[0] >= 253);
    CHECK(following[1] <= 2);
    CHECK(following[2] <= 2);
    gl::VertexAttributeTypes::Uv11::cleanup(gl, shader.program(), 0);
    auto client = GLint{};
    gl.getIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &client);
    CHECK(client == GL_TEXTURE0);
    gl.activeTexture(GL_TEXTURE1);
    gl.bindTexture(GL_TEXTURE_2D, 0);
    functions->glDeleteTextures(1, &sentinel);
    gl.activeTexture(GL_TEXTURE0);
    resource->dropSync(gl);
    plainResource->dropSync(gl);
    gl.popAttrib();
  }
  CHECK(gl.getError() == GL_NO_ERROR);
}

} // namespace tb::ui