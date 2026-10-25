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
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QMenu>
#include <QMimeData>
#include <QTimer>

#include "TestEnvironment.h"
#include "base/Result.h"
#include "gl/GlManager.h"
#include "gl/Resource.h"
#include "gl/ResourceManager.h"
#include "gl/TestGl.h"
#include "gl/TestUtils.h"
#include "mdl/BrushNode.h"
#include "mdl/EntityDefinition.h"
#include "mdl/EntityDefinitionManager.h"
#include "mdl/GameConfigFixture.h"
#include "mdl/Grid.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_Selection.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"
#include "ui/AppControllerFixture.h"
#include "ui/CatchConfig.h"
#include "ui/MapDocument.h"
#include "ui/MapDocumentFixture.h"
#include "ui/MapViewBase.h"
#include "ui/MapWindow.h"

#include <fstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::ui
{

// Simple resource type for testing resource processing
struct TestResource
{
  void upload(gl::Gl&) const {}
  void drop(gl::Gl&) const {}
};

TEST_CASE("MapWindow")
{
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

} // namespace tb::ui