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
#include <QLabel>
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
#include "gl/PerspectiveCamera.h"
#include "gl/Resource.h"
#include "gl/ResourceManager.h"
#include "gl/ShaderManager.h"
#include "gl/Shaders.h"
#include "gl/TestGl.h"
#include "gl/TestUtils.h"
#include "gl/Texture.h"
#include "gl/VboManager.h"
#include "gl/VertexAttributeType.h"
#include "mdl/BrushBuilder.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushFaceHandle.h"
#include "mdl/BrushNode.h"
#include "mdl/BrushRendererBrushCache.h"
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
#include "mdl/Map_Brushes.h"
#include "mdl/Map_Entities.h"
#include "mdl/Map_Groups.h"
#include "mdl/Map_NativeCameras.h"
#include "mdl/Map_NativeVisgroups.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/SourceEditorMetadata.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"
#include "render/BrushRendererArrays.h"
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
#include "ui/NativeCameraEditor.h"
#include "ui/NativeVisgroupEditor.h"
#include "ui/QPathUtils.h"
#include "ui/SourceBuild.h"
#include "ui/SourceBuildDialog.h"
#include "ui/SourceToolsDialog.h"
#include "ui/SwitchableMapViewContainer.h"
#include "ui/TabBook.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

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

TEST_CASE("NativeCamera UI actual MapWindow live authoring and history")
{
  auto env = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{env.dir()};
  auto fixture = AppControllerFixture{};
  auto& app = fixture.appController();
  const auto source = std::string{
    "world { id 1 classname worldspawn }\r\n"
    "cameras { activecamera 0 camera { position \"[1 2 3]\" look \"[1 3 3]\" } }\r\n"};
  const auto path = env.dir() / "camera-live.vmf";
  {
    auto stream = std::ofstream{path, std::ios::binary};
    stream << source;
    REQUIRE(stream);
  }
  auto loaded = MapDocument::loadDocument(
    app.environmentConfig(),
    mdl::QuakeGameInfo,
    mdl::MapFormat::SourceVMF,
    vm::bbox3d{8192},
    path,
    app.taskManager(),
    app.glManager().resourceManager());
  REQUIRE(loaded.is_success());
  auto window = MapWindow{app, std::move(loaded) | kdl::value()};
  auto& map = window.document().map();
  REQUIRE(
    mdl::SourceEditorMetadata::read(map.vmfScene()->document()).cameras.size() == 1);
  REQUIRE(mdl::setEntityProperty(map, "camera_ui_control", "acted"));
  CHECK(
    map.vmfScene()->serialize(map.worldNode()).find("camera_ui_control")
    != std::string::npos);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  auto* views = window.findChild<SwitchableMapViewContainer*>();
  REQUIRE(views);
  const auto viewPositionBefore = views->perspectiveCamera().position();
  const auto viewDirectionBefore = views->perspectiveCamera().direction();
  auto* panel = window.findChild<QWidget*>("nativeCameraEditor");
  REQUIRE(panel);
  auto* eye = panel->findChild<QLineEdit*>("nativeCameraEye");
  auto* apply = panel->findChild<QPushButton*>("nativeCameraApply");
  REQUIRE(eye);
  REQUIRE(apply);
  REQUIRE(apply->isEnabled());
  eye->setText("10 20 30");
  eye->setModified(true);
  apply->click();
  CHECK(map.vmfScene()->cameras().records[0].eye == vm::vec3d{10, 20, 30});
  CHECK(map.modified());
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  map.redoCommand();
  CHECK(map.vmfScene()->cameras().records[0].eye == vm::vec3d{10, 20, 30});
  auto* records = panel->findChild<QComboBox*>("nativeCameraRecords");
  auto* active = panel->findChild<QComboBox*>("nativeCameraActive");
  auto* activeApply = panel->findChild<QPushButton*>("nativeCameraApplyActive");
  auto* add = panel->findChild<QPushButton*>("nativeCameraAdd");
  auto* remove = panel->findChild<QPushButton*>("nativeCameraDelete");
  auto* clear = panel->findChild<QPushButton*>("nativeCameraRemove");
  REQUIRE(records);
  REQUIRE(active);
  REQUIRE(activeApply);
  REQUIRE(add);
  REQUIRE(remove);
  REQUIRE(clear);
  active->setCurrentIndex(0);
  activeApply->click();
  CHECK(map.vmfScene()->cameras().active.kind == mdl::NativeCameraActiveKind::Absent);
  active->setCurrentIndex(1);
  activeApply->click();
  CHECK(map.vmfScene()->cameras().active.kind == mdl::NativeCameraActiveKind::None);
  add->click();
  REQUIRE(records->count() == 2);
  const auto retained = map.vmfScene()->cameras().records[1].id;
  active->setCurrentIndex(3);
  activeApply->click();
  CHECK(map.vmfScene()->cameras().active.camera == retained);
  records->setCurrentIndex(0);
  remove->click();
  REQUIRE(records->count() == 1);
  CHECK(map.vmfScene()->cameras().active.camera == retained);
  CHECK(map.vmfScene()->cameras().records[0].id == retained);
  clear->click();
  CHECK_FALSE(map.vmfScene()->cameras().present);
  map.undoCommand();
  CHECK(map.vmfScene()->cameras().present);
  REQUIRE(map.vmfScene()->cameras().records.size() == 1);
  CHECK(map.vmfScene()->cameras().records[0].id == retained);
  CHECK(views->perspectiveCamera().position() == viewPositionBefore);
  CHECK(views->perspectiveCamera().direction() == viewDirectionBefore);
  auto original = std::ifstream{path, std::ios::binary};
  CHECK(
    std::string{
      std::istreambuf_iterator<char>{original}, std::istreambuf_iterator<char>{}}
    == source);
  auto* capture = panel->findChild<QPushButton*>("nativeCameraCapture");
  REQUIRE(capture); // Acting native-camera/history controls precede missing workflow.
  REQUIRE(capture->isEnabled());
  auto* perspectiveView = static_cast<MapViewBase*>(nullptr);
  auto* orthographicView = static_cast<MapViewBase*>(nullptr);
  for (auto* view : window.findChildren<MapViewBase*>())
  {
    if (view->camera().perspectiveProjection())
    {
      perspectiveView = view;
    }
    else
    {
      orthographicView = view;
    }
  }
  REQUIRE(perspectiveView);
  auto& camera = perspectiveView->camera();
  camera.moveTo({11, 22, 33});
  camera.setDirection({0, 1, 0}, {0, 0, 1});
  qApp->focusChanged(
    nullptr, perspectiveView); // Exercise the owning window's focus route.
  const auto bytesBeforeCapture = map.vmfScene()->serialize(map.worldNode());
  const auto countBeforeCapture = map.modificationCount();
  const auto redoBeforeCapture = map.redoCommandName();
  auto* look = panel->findChild<QLineEdit*>("nativeCameraLook");
  REQUIRE(look);
  capture->click();
  CHECK(eye->text() == "11 22 33");
  CHECK(look->text() == "11 86 33"); // Absolute target64 units along the active view.
  CHECK(eye->isModified());
  CHECK(look->isModified());
  CHECK(map.modificationCount() == countBeforeCapture);
  CHECK(map.redoCommandName() == redoBeforeCapture);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == bytesBeforeCapture);
  CHECK(camera.position() == vm::vec3f{11, 22, 33});
  CHECK(camera.direction() == vm::vec3f{0, 1, 0});
  apply->click(); // Draft capture is not itself a native write; this explicit action is.
  CHECK(map.vmfScene()->cameras().records[0].eye == vm::vec3d{11, 22, 33});
  CHECK(map.vmfScene()->cameras().records[0].look == vm::vec3d{11, 86, 33});
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == bytesBeforeCapture);
  eye->setText("77 88 99");
  eye->setModified(true);
  REQUIRE(QMetaObject::invokeMethod(apply, "pressed"));
  capture->click(); // Reading a new draft cancels an older pressed write intent.
  REQUIRE(QMetaObject::invokeMethod(apply, "clicked"));
  CHECK(map.modificationCount() == countBeforeCapture);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == bytesBeforeCapture);
  REQUIRE(orthographicView);
  qApp->focusChanged(nullptr, orthographicView);
  eye->setText("8 8 8");
  capture->click();
  CHECK(eye->text() == "8 8 8"); // Do not guess another pane when2D is active.
  CHECK(map.modificationCount() == countBeforeCapture);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == bytesBeforeCapture);
  qApp->focusChanged(nullptr, perspectiveView);
  {
    auto standalone = NativeCameraEditor{window.document()};
    auto* disabled = standalone.findChild<QPushButton*>("nativeCameraCapture");
    REQUIRE(disabled);
    CHECK_FALSE(disabled->isEnabled());
  }
  {
    auto invalid = NativeCameraEditor{
      window.document(), nullptr, [] {
        return std::optional<NativeCameraDraft>{NativeCameraDraft{{1, 2, 3}, {1, 2, 3}}};
      }};
    auto* invalidEye = invalid.findChild<QLineEdit*>("nativeCameraEye");
    auto* invalidCapture = invalid.findChild<QPushButton*>("nativeCameraCapture");
    REQUIRE(invalidEye);
    REQUIRE(invalidCapture);
    const auto unchangedDraft = invalidEye->text();
    invalidCapture->click();
    CHECK(invalidEye->text() == unchangedDraft);
    CHECK(map.modificationCount() == countBeforeCapture);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytesBeforeCapture);
  }
}

TEST_CASE("NativeCamera UI freezes field intent and rejects stale row releases")
{
  auto env = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{env.dir()};
  auto fixture = AppControllerFixture{};
  auto& app = fixture.appController();
  const auto source = std::string{
    "world { id 1 classname worldspawn }\n"
    "cameras { activecamera 0 camera { position \"[1 2 3]\" look \"[1 3 3]\" }"
    " camera { position \"[4 5 6]\" look \"[4 6 6]\" } }\n"};
  const auto path = env.dir() / "camera-stale.vmf";
  {
    auto stream = std::ofstream{path, std::ios::binary};
    stream << source;
    REQUIRE(stream);
  }
  auto loaded = MapDocument::loadDocument(
    app.environmentConfig(),
    mdl::QuakeGameInfo,
    mdl::MapFormat::SourceVMF,
    vm::bbox3d{8192},
    path,
    app.taskManager(),
    app.glManager().resourceManager());
  REQUIRE(loaded.is_success());
  auto window = MapWindow{app, std::move(loaded) | kdl::value()};
  auto& map = window.document().map();
  auto* panel = window.findChild<QWidget*>("nativeCameraEditor");
  REQUIRE(panel);
  auto* records = panel->findChild<QComboBox*>("nativeCameraRecords");
  auto* eye = panel->findChild<QLineEdit*>("nativeCameraEye");
  auto* apply = panel->findChild<QPushButton*>("nativeCameraApply");
  auto* add = panel->findChild<QPushButton*>("nativeCameraAdd");
  auto* status = panel->findChild<QLabel*>("nativeCameraStatus");
  REQUIRE(records);
  REQUIRE(eye);
  REQUIRE(apply);
  REQUIRE(add);
  REQUIRE(status);
  const auto press = [](QPushButton* button) {
    REQUIRE(QMetaObject::invokeMethod(button, "pressed", Qt::DirectConnection));
  };
  const auto release = [](QPushButton* button) {
    REQUIRE(
      QMetaObject::invokeMethod(
        button, "clicked", Qt::DirectConnection, Q_ARG(bool, false)));
  };
  eye->setText("50 60 70");
  eye->setModified(true);
  press(apply);
  eye->setText("80 90 100");
  eye->setModified(true);
  release(apply);
  CHECK(map.vmfScene()->cameras().records[0].eye == vm::vec3d{50, 60, 70});
  eye->setText("11 12 13");
  eye->setModified(true);
  press(apply);
  const auto count = map.modificationCount();
  records->setCurrentIndex(1);
  release(apply);
  CHECK(map.modificationCount() == count);
  CHECK(status->text().contains("Stale"));
  CHECK(map.vmfScene()->cameras().records[1].eye == vm::vec3d{4, 5, 6});
  records->setCurrentIndex(0);
  eye->setText("14 15 16");
  eye->setModified(true);
  press(apply);
  auto changed = mdl::NativeCameraRequest{mdl::nativeCameras(map)};
  changed.records[1].look = {7, 8, 9};
  REQUIRE(
    mdl::applyNativeCameras(map, changed).status
    == mdl::NativeCameraApplyStatus::Changed);
  const auto after = map.modificationCount();
  release(apply);
  CHECK(map.modificationCount() == after);
  CHECK(status->text().contains("Stale"));
  CHECK(map.vmfScene()->cameras().records[0].eye == vm::vec3d{50, 60, 70});
  eye->setText("20 30 40");
  eye->setModified(true);
  press(apply);
  press(add);
  release(apply);
  CHECK(map.modificationCount() == after);
  release(add);
  CHECK(map.vmfScene()->cameras().records.size() == 3);
  CHECK(map.modificationCount() == after + 1);
  const auto bytes = map.vmfScene()->serialize(map.worldNode());
  const auto unchanged = map.modificationCount();
  eye->setText("invalid");
  eye->setModified(true);
  apply->click();
  CHECK(map.modificationCount() == unchanged);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
}
TEST_CASE("NativeCamera UI readonly unsupported and nonnative modes deny authoring")
{
  const auto kind = GENERATE(0, 1, 2, 3);
  auto env = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{env.dir()};
  auto fixture = AppControllerFixture{};
  auto& app = fixture.appController();
  auto source = std::string{
    "world { id 1 classname worldspawn }\n"
    "cameras { activecamera 0 camera { position \"[1 2 3]\" look \"[1 3 3]\" } }\n"};
  if (kind == 1)
  {
    source = "versioninfo { formatversion 999 }\n" + source;
  }
  if (kind == 2)
  {
    source += "cameras {}\n";
  }
  if (kind == 3)
  {
    source = "{\n\"classname\" \"worldspawn\"\n}\n";
  }
  const auto path = env.dir() / (kind == 3 ? "other.map" : "restricted.vmf");
  {
    auto stream = std::ofstream{path, std::ios::binary};
    stream << source;
    REQUIRE(stream);
  }
  auto loaded = MapDocument::loadDocument(
    app.environmentConfig(),
    mdl::QuakeGameInfo,
    kind == 3 ? mdl::MapFormat::Standard : mdl::MapFormat::SourceVMF,
    vm::bbox3d{8192},
    path,
    app.taskManager(),
    app.glManager().resourceManager(),
    kind == 0);
  REQUIRE(loaded.is_success());
  auto window = MapWindow{app, std::move(loaded) | kdl::value()};
  auto& map = window.document().map();
  auto* panel = window.findChild<QWidget*>("nativeCameraEditor");
  REQUIRE(panel);
  if (kind == 3)
  {
    CHECK(panel->isHidden());
  }
  for (const auto* name :
       {"nativeCameraCapture",
        "nativeCameraAdd",
        "nativeCameraApply",
        "nativeCameraDelete",
        "nativeCameraApplyActive",
        "nativeCameraRemove"})
  {
    auto* button = panel->findChild<QPushButton*>(name);
    REQUIRE(button);
    CHECK_FALSE(button->isEnabled());
    button->click();
  }
  CHECK_FALSE(map.modified());
  CHECK_FALSE(map.undoCommandName());
  if (kind != 3)
  {
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
    auto* inspect = window.findAction("Menu/Run/Source Tools...");
    REQUIRE(inspect);
    CHECK(inspect->isEnabled());
  }
}

TEST_CASE("Source tools direct saved action is safe for read-only and unsaved maps")
{
  auto env = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{env.dir()};
  auto fixture = AppControllerFixture{};
  auto& app = fixture.appController();
  SECTION("saved native snapshots work in editable and explicit read-only modes")
  {
    const auto preview = GENERATE(false, true);
    INFO("read-only snapshot preview: " << preview);
    const auto source = std::string{
      "world { id 1 classname worldspawn }\n"
      "cameras { activecamera 0 camera { position \"[40 50 60]\" look \"[40 60 60]\" } "
      "}\n"};
    const auto path = env.dir() / "source-tools.vmf";
    {
      auto stream = std::ofstream{path, std::ios::binary};
      stream << source;
      REQUIRE(stream);
    }
    auto loaded = MapDocument::loadDocument(
      app.environmentConfig(),
      mdl::QuakeGameInfo,
      mdl::MapFormat::SourceVMF,
      vm::bbox3d{8192},
      path,
      app.taskManager(),
      app.glManager().resourceManager(),
      preview);
    REQUIRE(loaded.is_success());
    auto window = MapWindow{app, std::move(loaded) | kdl::value()};
    auto& map = window.document().map();
    REQUIRE(map.isReadOnlyVmfPreview() == preview);
    if (!preview)
    {
      REQUIRE(mdl::setEntityProperty(map, "unsaved_tools_probe", "draft"));
    }
    const auto modifications = map.modificationCount();
    const auto before = map.vmfScene()->serialize(map.worldNode());
    if (preview)
    {
      for (const auto* actionPath : {"Menu/Run/Compile...", "Menu/Run/Launch..."})
      {
        auto* blocked = window.findAction(actionPath);
        REQUIRE(blocked);
        CHECK_FALSE(blocked->isEnabled());
      }
      window.showCompileDialog();
      CHECK_FALSE(window.findChild<SourceBuildDialog*>());
      CHECK_FALSE(window.findChild<SourceBuild*>());
    }
    auto legacy =
      SourceToolsDialog{nullptr, pathAsQString(path), SourceBspTarget::Generic};
    auto* oldEye = legacy.findChild<QPushButton*>("sourceMetadataGoCamera");
    REQUIRE(oldEye);
    REQUIRE(oldEye->isEnabled());
    auto* action = window.findAction("Menu/Run/Source Tools...");
    CHECK(action);
    if (action)
    {
      REQUIRE(action->isEnabled());
      action->trigger();
      QApplication::processEvents();
      auto* tools = window.findChild<SourceToolsDialog*>();
      REQUIRE(tools);
      CHECK(window.findChildren<SourceToolsDialog*>().size() == 1);
      action->trigger();
      QApplication::processEvents();
      CHECK(window.findChild<SourceToolsDialog*>() == tools);
      CHECK(window.findChildren<SourceToolsDialog*>().size() == 1);
      CHECK_FALSE(window.findChild<SourceBuildDialog*>());
      CHECK_FALSE(window.findChild<SourceBuild*>());
      auto* target = tools->findChild<QComboBox*>("sourcePackTarget");
      REQUIRE(target);
      CHECK(
        target->currentIndex() == static_cast<int>(loadSourceBuildSettings().bspTarget));
      auto* views = window.findChild<SwitchableMapViewContainer*>();
      REQUIRE(views);
      views->moveCameraToPosition(vm::vec3f{}, false, vm::vec3f{-1, 0, 0});
      auto* eye = tools->findChild<QPushButton*>("sourceMetadataGoCamera");
      auto* view = tools->findChild<QPushButton*>("sourceMetadataGoCameraView");
      REQUIRE(eye);
      REQUIRE(view);
      eye->click();
      QTest::qWait(MapViewBase::DefaultCameraAnimationDuration + 80);
      CHECK(
        vm::length(views->perspectiveCamera().position() - vm::vec3f{40, 50, 60})
        < 0.0001f);
      CHECK(views->perspectiveCamera().direction() == vm::vec3f{-1, 0, 0});
      view->click();
      QTest::qWait(MapViewBase::DefaultCameraAnimationDuration + 80);
      CHECK(
        vm::length(views->perspectiveCamera().direction() - vm::vec3f{0, 1, 0})
        < 0.00001f);
      CHECK(map.modificationCount() == modifications);
      CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
      auto original = std::ifstream{path, std::ios::binary};
      CHECK(
        std::string{
          std::istreambuf_iterator<char>{original}, std::istreambuf_iterator<char>{}}
        == source);
    }
  }
  SECTION("unsaved native and non-native documents have no saved-tools entry authority")
  {
    const auto format = GENERATE(mdl::MapFormat::SourceVMF, mdl::MapFormat::Standard);
    auto gameInfo = mdl::QuakeGameInfo;
    if (format == mdl::MapFormat::SourceVMF)
    {
      const auto starter = env.dir() / "starter.vmf";
      auto stream = std::ofstream{starter, std::ios::binary};
      stream << "world { id 1 classname worldspawn }";
      REQUIRE(stream);
      stream.close();
      gameInfo.gameConfig.fileFormats = {{"Source VMF", starter}};
    }
    auto created = MapDocument::createDocument(
      app.environmentConfig(),
      gameInfo,
      format,
      vm::bbox3d{8192},
      app.taskManager(),
      app.glManager().resourceManager());
    REQUIRE(created.is_success());
    auto window = MapWindow{app, std::move(created) | kdl::value()};
    CHECK_FALSE(window.document().map().persistent());
    auto* action = window.findAction("Menu/Run/Source Tools...");
    CHECK(action);
    if (action)
    {
      CHECK_FALSE(action->isEnabled());
      action->trigger();
      window.showSourceToolsDialog();
      QApplication::processEvents();
      CHECK_FALSE(window.findChild<SourceToolsDialog*>());
      CHECK_FALSE(window.findChild<SourceBuild*>());
    }
  }
  SECTION("saved non-native maps are refused by menu and controller")
  {
    const auto path = env.dir() / "other.map";
    {
      auto stream = std::ofstream{path, std::ios::binary};
      stream << "{\n\"classname\" \"worldspawn\"\n}\n";
      REQUIRE(stream);
    }
    auto loaded = MapDocument::loadDocument(
      app.environmentConfig(),
      mdl::QuakeGameInfo,
      mdl::MapFormat::Standard,
      vm::bbox3d{8192},
      path,
      app.taskManager(),
      app.glManager().resourceManager());
    REQUIRE(loaded.is_success());
    auto window = MapWindow{app, std::move(loaded) | kdl::value()};
    REQUIRE(window.document().map().persistent());
    auto* action = window.findAction("Menu/Run/Source Tools...");
    REQUIRE(action);
    CHECK_FALSE(action->isEnabled());
    action->trigger();
    window.showSourceToolsDialog();
    QApplication::processEvents();
    CHECK_FALSE(window.findChild<SourceToolsDialog*>());
    CHECK_FALSE(window.findChild<SourceBuild*>());
  }
}

TEST_CASE("Source camera view actual MapWindow moves eye and heading without map edits")
{
  auto env = fs::TestEnvironment{};
  auto settings = NativeGroupSettingsScope{env.dir()};
  auto fixture = AppControllerFixture{};
  auto& app = fixture.appController();
  const auto source = std::string{
    "world { id 1 classname worldspawn }\n"
    "cameras { activecamera 0 camera { position \"[8 16 24]\" look \"[32 32 32]\" } }\n"};
  {
    auto stream = std::ofstream{env.dir() / "camera-view.vmf", std::ios::binary};
    stream << source;
    REQUIRE(stream);
  }
  auto loaded = MapDocument::loadDocument(
    app.environmentConfig(),
    mdl::QuakeGameInfo,
    mdl::MapFormat::SourceVMF,
    vm::bbox3d{8192},
    env.dir() / "camera-view.vmf",
    app.taskManager(),
    app.glManager().resourceManager());
  REQUIRE(loaded.is_success());
  auto window = MapWindow{app, std::move(loaded) | kdl::value()};
  auto& map = window.document().map();
  auto* views = window.findChild<SwitchableMapViewContainer*>();
  REQUIRE(views);
  const auto& camera = views->perspectiveCamera();
  const auto oldDirection = camera.direction();
  const auto oldZoom = camera.zoom();
  const auto modifications = map.modificationCount();
  const auto before = map.vmfScene()->serialize(map.worldNode());
  views->moveCameraToPosition(vm::vec3f{8, 16, 24}, false);
  CHECK(camera.position() == vm::vec3f{8, 16, 24});
  CHECK(camera.direction() == oldDirection);
  auto step = 1.0f;
  for (const auto& direction : {-oldDirection, vm::vec3f{0, 0, 1}})
  {
    const auto position = vm::vec3f{32, 64, 96} * step;
    views->moveCameraToPosition(position, false, direction);
    CHECK(camera.position() == position);
    CHECK(vm::length(camera.direction() - direction) < 0.00001f);
    CHECK(std::isfinite(camera.up().x()));
    CHECK(std::isfinite(camera.up().y()));
    CHECK(std::isfinite(camera.up().z()));
    CHECK(std::abs(vm::length(camera.up()) - 1.0f) < 0.00001f);
    CHECK(camera.zoom() == oldZoom);
    step += 1.0f;
  }
  views->moveCameraToPosition(vm::vec3f{512, -512, 256}, true);
  const auto destination = vm::vec3f{128, 64, 32};
  const auto requestedHeading = vm::vec3f{-1, 0, 0};
  views->moveCameraToPosition(destination, true, requestedHeading);
  QTest::qWait(MapViewBase::DefaultCameraAnimationDuration + 80);
  CHECK(vm::length(camera.position() - destination) < 0.0001f);
  CHECK(vm::length(camera.direction() - requestedHeading) < 0.00001f);
  CHECK(camera.zoom() == oldZoom);
  const auto heading = camera.direction();
  views->moveCameraToPosition(vm::vec3f{24, 16, 8}, false);
  CHECK(camera.direction() == heading);
  CHECK(map.modificationCount() == modifications);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
  auto original = std::ifstream{env.dir() / "camera-view.vmf", std::ios::binary};
  CHECK(
    std::string{
      std::istreambuf_iterator<char>{original}, std::istreambuf_iterator<char>{}}
    == source);
}

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
  const auto solid = [](
                       const size_t id,
                       const size_t power,
                       const bool withAlphas,
                       const bool partial = false,
                       const size_t offsetRows = 0) {
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
          for (size_t row = 0; row < (partial ? size_t{1} : size); ++row)
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
        if (offsetRows != 0)
        {
          text += "offsets {\n";
          for (size_t row = 0; row < offsetRows; ++row)
          {
            text += "row" + std::to_string(row) + " \"";
            for (size_t column = 0; column < size; ++column)
            {
              text += "0.00 +0 -0 ";
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
    "world { id 1 classname worldspawn\n" + solid(2, 6, true)
      + solid(20, 2, true, false, 5) + solid(40, 2, false) + solid(60, 2, true, true, 1)
      + "}\n");
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
  REQUIRE(nodes.size() == 4);
  auto* large = dynamic_cast<mdl::BrushNode*>(nodes[0]);
  auto* small = dynamic_cast<mdl::BrushNode*>(nodes[1]);
  auto* sparse = dynamic_cast<mdl::BrushNode*>(nodes[2]);
  auto* partial = dynamic_cast<mdl::BrushNode*>(nodes[3]);
  REQUIRE(partial);
  REQUIRE(large);
  REQUIRE(small);
  REQUIRE(sparse);
  const auto largeFace = large->brush().findFace(vm::vec3d{0, 0, 1});
  const auto smallFace = small->brush().findFace(vm::vec3d{0, 0, 1});
  const auto sparseFace = sparse->brush().findFace(vm::vec3d{0, 0, 1});
  const auto partialFace = partial->brush().findFace(vm::vec3d{0, 0, 1});
  REQUIRE(partialFace);
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
  auto* initializeAlpha =
    editor.findChild<QPushButton*>("sourceDisplacementAlphaInitialize");
  REQUIRE(initializeAlpha);
  CHECK(initializeAlpha->isHidden());
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
  // Existing scalar controls ACT before the structural initialization boundary.
  REQUIRE_FALSE(initializeAlpha->isHidden());
  REQUIRE(initializeAlpha->isEnabled());
  const auto sparseGeometry =
    sparse->brush().face(*sparseFace).sourceDisplacement()->positions();
  const auto initializeCount = map.modificationCount();
  initializeAlpha->click();
  CHECK(map.modificationCount() == initializeCount + 1);
  REQUIRE(sparse->brush().face(*sparseFace).sourceDisplacement()->canEditAlphas());
  CHECK(initializeAlpha->isHidden());
  REQUIRE(applyAlpha->isEnabled());
  CHECK(alpha->value() == 0);
  alpha->setValue(17.5);
  applyAlpha->click();
  CHECK(sparse->brush().face(*sparseFace).sourceDisplacement()->alpha(2, 2) == 17.5);
  CHECK(
    sparse->brush().face(*sparseFace).sourceDisplacement()->positions()
    == sparseGeometry);
  map.undoCommand();
  QApplication::processEvents();
  CHECK(alpha->value() == 0);
  CHECK(sparse->brush().face(*sparseFace).sourceDisplacement()->canEditAlphas());
  map.undoCommand();
  QApplication::processEvents();
  CHECK(sparse->brush().face(*sparseFace).sourceDisplacement()->canInitializeAlphas());
  CHECK_FALSE(initializeAlpha->isHidden());
  CHECK_FALSE(applyAlpha->isEnabled());
  map.redoCommand();
  map.redoCommand();
  QApplication::processEvents();
  CHECK(alpha->value() == 17.5);
  mdl::deselectAll(map);
  mdl::selectBrushFaces(map, {mdl::BrushFaceHandle{*partial, *partialFace}});
  QApplication::processEvents();
  CHECK(initializeAlpha->isHidden());
  CHECK_FALSE(initializeAlpha->isEnabled());
  CHECK_FALSE(alpha->isEnabled());
  CHECK_FALSE(applyAlpha->isEnabled());
  CHECK(distance->isEnabled());
  const auto partialCount = map.modificationCount();
  initializeAlpha->click();
  CHECK(map.modificationCount() == partialCount);
  // Actual legacy distance/alpha/history controls act before the missing offset UI.
  auto* applyOffset = editor.findChild<QPushButton*>("sourceDisplacementOffsetApply");
  auto* initializeOffset =
    editor.findChild<QPushButton*>("sourceDisplacementOffsetInitialize");
  const auto offset = std::array{
    editor.findChild<QDoubleSpinBox*>("sourceDisplacementOffsetX"),
    editor.findChild<QDoubleSpinBox*>("sourceDisplacementOffsetY"),
    editor.findChild<QDoubleSpinBox*>("sourceDisplacementOffsetZ")};
  REQUIRE(applyOffset);
  REQUIRE(initializeOffset);
  for (auto* box : offset)
  {
    REQUIRE(box);
    CHECK_FALSE(box->isEnabled()); // Present partial rows are not silently filled.
  }
  CHECK_FALSE(applyOffset->isEnabled());
  CHECK(initializeOffset->isHidden());
  map.setIsCommandCollationEnabled(false);
  mdl::deselectAll(map);
  mdl::selectBrushFaces(map, {mdl::BrushFaceHandle{*small, *smallFace}});
  QApplication::processEvents();
  row->setValue(2);
  column->setValue(2);
  REQUIRE(applyOffset->isEnabled());
  CHECK(initializeOffset->isHidden());
  const auto beforeOffset = *small->brush().face(*smallFace).sourceDisplacement();
  const auto offsetCount = map.modificationCount();
  offset[0]->setValue(1.25);
  offset[1]->setValue(-2.5);
  offset[2]->setValue(3.75);
  distance->setValue(999); // These unrelated drafts must not be applied.
  alpha->setValue(7);
  applyOffset->click();
  CHECK(map.modificationCount() == offsetCount + 1);
  CHECK(
    small->brush().face(*smallFace).sourceDisplacement()->offset(2, 2)
    == vm::vec3d{1.25, -2.5, 3.75});
  CHECK(
    small->brush().face(*smallFace).sourceDisplacement()->positions()[12]
    == beforeOffset.positions()[12] + vm::vec3d{1.25, -2.5, 3.75});
  CHECK(
    small->brush().face(*smallFace).sourceDisplacement()->distance(2, 2)
    == beforeOffset.distance(2, 2));
  CHECK(
    small->brush().face(*smallFace).sourceDisplacement()->alphas()
    == beforeOffset.alphas());
  applyOffset->click();
  CHECK(map.modificationCount() == offsetCount + 1);
  map.undoCommand();
  QApplication::processEvents();
  CHECK(*small->brush().face(*smallFace).sourceDisplacement() == beforeOffset);
  offset[0]->setValue(4);
  offset[1]->setValue(5);
  offset[2]->setValue(6);
  REQUIRE(QMetaObject::invokeMethod(applyOffset, "pressed"));
  offset[0]->setValue(100); // Freeze field intent, not only the target row.
  REQUIRE(QMetaObject::invokeMethod(applyOffset, "clicked"));
  CHECK(
    small->brush().face(*smallFace).sourceDisplacement()->offset(2, 2)
    == vm::vec3d{4, 5, 6});
  map.undoCommand();
  QApplication::processEvents();
  const auto staleCount = map.modificationCount();
  offset[0]->setValue(1);
  REQUIRE(QMetaObject::invokeMethod(applyOffset, "pressed"));
  row->setValue(3);
  REQUIRE(QMetaObject::invokeMethod(applyOffset, "clicked"));
  CHECK(map.modificationCount() == staleCount);
  CHECK(*small->brush().face(*smallFace).sourceDisplacement() == beforeOffset);
  row->setValue(2);
  offset[0]->setValue(1);
  REQUIRE(QMetaObject::invokeMethod(applyOffset, "pressed"));
  REQUIRE(mdl::setSourceDisplacementAlpha(map, 2, 2, 23.5));
  const auto backendCount = map.modificationCount();
  REQUIRE(QMetaObject::invokeMethod(applyOffset, "clicked"));
  CHECK(map.modificationCount() == backendCount);
  CHECK(
    small->brush().face(*smallFace).sourceDisplacement()->offset(2, 2) == vm::vec3d{});
  map.undoCommand();
  QApplication::processEvents();
  offset[0]->setValue(1);
  REQUIRE(QMetaObject::invokeMethod(applyOffset, "pressed"));
  mdl::deselectAll(map);
  mdl::selectBrushFaces(map, {mdl::BrushFaceHandle{*sparse, *sparseFace}});
  // Synchronous notification denies the old action before queued widget refresh.
  CHECK_FALSE(applyOffset->isEnabled());
  REQUIRE(QMetaObject::invokeMethod(applyOffset, "clicked"));
  CHECK(map.modificationCount() == staleCount);
  QApplication::processEvents();
  REQUIRE_FALSE(initializeOffset->isHidden());
  REQUIRE(initializeOffset->isEnabled());
  CHECK_FALSE(applyOffset->isEnabled());
  const auto sparseBeforeOffset = *sparse->brush().face(*sparseFace).sourceDisplacement();
  REQUIRE(QMetaObject::invokeMethod(initializeOffset, "pressed"));
  REQUIRE(
    QMetaObject::invokeMethod(
      applyOffset, "clicked")); // Wrong button consumes no operation.
  REQUIRE(QMetaObject::invokeMethod(initializeOffset, "clicked"));
  CHECK(map.modificationCount() == staleCount);
  initializeOffset->click();
  CHECK(map.modificationCount() == staleCount + 1);
  REQUIRE(applyOffset->isEnabled());
  CHECK(initializeOffset->isHidden());
  CHECK(
    sparse->brush().face(*sparseFace).sourceDisplacement()->positions()
    == sparseBeforeOffset.positions());
  offset[2]->setValue(2);
  applyOffset->click();
  CHECK(
    sparse->brush().face(*sparseFace).sourceDisplacement()->positions()[12]
    == sparseBeforeOffset.positions()[12] + vm::vec3d{0, 0, 2});
  map.undoCommand();
  QApplication::processEvents();
  CHECK(sparse->brush().face(*sparseFace).sourceDisplacement()->canEditOffsets());
  CHECK(offset[2]->value() == 0);
  map.undoCommand();
  QApplication::processEvents();
  CHECK(sparse->brush().face(*sparseFace).sourceDisplacement()->canInitializeOffsets());
  CHECK_FALSE(applyOffset->isEnabled());
  map.redoCommand();
  map.redoCommand();
  QApplication::processEvents();
  CHECK(
    sparse->brush().face(*sparseFace).sourceDisplacement()->offset(2, 2)
    == vm::vec3d{0, 0, 2});
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
    {
      // Exercise the packed cache -> production holder -> real VBO path, not client
      // arrays.
      using Vertex = mdl::BrushRendererBrushCache::Vertex;
      const auto packed = std::array<Vertex, 4>{
        Vertex{
          vm::vec3f{-1, -1, 0},
          vm::vec3f{0, 0, 1},
          vm::vec2f{0.5f, 0.5f},
          vm::vec<float, 1>{1}},
        Vertex{
          vm::vec3f{1, -1, 0},
          vm::vec3f{0, 0, 1},
          vm::vec2f{0.5f, 0.5f},
          vm::vec<float, 1>{1}},
        Vertex{
          vm::vec3f{-1, 1, 0},
          vm::vec3f{0, 0, 1},
          vm::vec2f{0.5f, 0.5f},
          vm::vec<float, 1>{1}},
        Vertex{
          vm::vec3f{1, 1, 0},
          vm::vec3f{0, 0, 1},
          vm::vec2f{0.5f, 0.5f},
          vm::vec<float, 1>{1}}};
      auto vboManager = gl::VboManager{};
      {
        auto vertices = render::BrushVertexArray{};
        const auto [allocation, destination] =
          vertices.getPointerToInsertVerticesAt(packed.size());
        REQUIRE(allocation);
        CHECK(sizeof(*destination) == sizeof(packed.front()));
        std::memcpy(destination, packed.data(), packed.size() * sizeof(*destination));
        vertices.prepare(gl, vboManager);
        REQUIRE(vertices.prepared());
        REQUIRE(vertices.setup(gl, shader.program()));
        shader.set("Alpha", 1.0f);
        gl::setAlphaFuncUniforms(shader, &material);
        blend.before(gl, &material);
        gl.clearColor(0, 0, 1, 1);
        gl.clear(GL_COLOR_BUFFER_BIT);
        gl.drawArrays(GL_TRIANGLE_STRIP, static_cast<GLint>(allocation->pos), 4);
        for (const auto point :
             {vm::vec2i{2, 2}, vm::vec2i{5, 2}, vm::vec2i{2, 5}, vm::vec2i{5, 5}})
        {
          auto pixel = std::array<uint8_t, 4>{};
          functions->glReadPixels(
            point.x(), point.y(), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
          CHECK(pixel[0] <= 2);
          CHECK(pixel[1] >= 253);
          CHECK(pixel[2] <= 2);
        }
        blend.after(gl, &material);
        // Scalar opacity must act through this same REAL packed production upload.
        gl.pushAttrib(GL_POLYGON_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl.disable(GL_DEPTH_TEST);
        for (const auto alpha : {uint8_t{255}, uint8_t{128}})
        {
          for (const auto opacity : {1.0f, 0.25f, 0.0f})
          {
            CAPTURE(alpha, opacity);
            auto buffer = gl::TextureBuffer{4};
            const auto rgba = std::array<uint8_t, 4>{255, 0, 0, alpha};
            std::copy(rgba.begin(), rgba.end(), buffer.data());
            auto translucent = gl::Texture{
              1,
              1,
              Color{RgbaF{1, 0, 0, 1}},
              GL_RGBA,
              gl::SourceEmbeddedDefaults{true, 0.5f, opacity},
              std::move(buffer)};
            translucent.setAlphaDomain(img::ImageAlphaDomain::Graduated);
            auto pixels = gl::createTextureResource(std::move(translucent));
            pixels->uploadSync(gl);
            auto subject = gl::Material{"scalar opacity", pixels};
            subject.setRelativePath("scalar.vmt");
            shader.set("Alpha", 1.0f);
            gl::setAlphaFuncUniforms(shader, &subject);
            gl.enable(GL_BLEND);
            gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gl.depthMask(GL_TRUE);
            blend.before(gl, &subject);
            auto depth = GLboolean{};
            gl.getBooleanv(GL_DEPTH_WRITEMASK, &depth);
            CHECK(depth == GL_FALSE);
            gl.clearColor(0, 0, 1, 1);
            gl.clear(GL_COLOR_BUFFER_BIT);
            gl.drawArrays(GL_TRIANGLE_STRIP, static_cast<GLint>(allocation->pos), 4);
            const auto expected = static_cast<int>(std::lround(alpha * opacity));
            for (const auto point :
                 {vm::vec2i{2, 2}, vm::vec2i{5, 2}, vm::vec2i{2, 5}, vm::vec2i{5, 5}})
            {
              auto pixel = std::array<uint8_t, 4>{};
              functions->glReadPixels(
                point.x(), point.y(), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
              CHECK(std::abs(int(pixel[0]) - expected) <= 2);
              CHECK(pixel[1] <= 2);
              CHECK(std::abs(int(pixel[2]) - (255 - expected)) <= 2);
            }
            blend.after(gl, &subject);
            gl.getBooleanv(GL_DEPTH_WRITEMASK, &depth);
            CHECK(depth == GL_TRUE);
            CHECK(functions->glIsEnabled(GL_BLEND) == GL_TRUE);
            pixels->dropSync(gl);
          }
        }
        // The last scalar was zero: both null and ordinary following bindings reset it.
        for (const auto useNull : {true, false})
        {
          gl::setAlphaFuncUniforms(shader, useNull ? nullptr : &plain);
          blend.before(gl, &plain);
          gl.clearColor(0, 0, 1, 1);
          gl.clear(GL_COLOR_BUFFER_BIT);
          gl.drawArrays(GL_TRIANGLE_STRIP, static_cast<GLint>(allocation->pos), 4);
          const auto pixel = readPixel();
          CHECK(pixel[0] >= 253);
          CHECK(pixel[1] <= 2);
          CHECK(pixel[2] <= 2);
          blend.after(gl, &plain);
        }
        gl.popAttrib();
        vertices.cleanup(gl, shader.program());
        auto clientUnit = GLint{};
        gl.getIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &clientUnit);
        CHECK(clientUnit == GL_TEXTURE0);
      }
      vboManager.destroyPendingVbos(gl);
    }
    resource->dropSync(gl);
    plainResource->dropSync(gl);
    gl.popAttrib();
  }
  CHECK(gl.getError() == GL_NO_ERROR);
}

} // namespace tb::ui