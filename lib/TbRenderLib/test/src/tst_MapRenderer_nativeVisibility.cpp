/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "TestPreferenceStore.h"
#include "base/PreferenceManager.h"
#include "mdl/BrushBuilder.h"
#include "mdl/BrushNode.h"
#include "mdl/EntityNode.h"
#include "mdl/GroupNode.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/Map_Nodes.h"
#include "mdl/WorldNode.h"
#include "render/EntityModelRenderer.h"
#include "render/EntityRenderer.h"
#include "render/MapRenderer.h"

#include "kd/result.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::render
{
struct NativeVisibilityRendererTest
{
  static int desired(mdl::Node& node)
  {
    return MapRenderer::determineDesiredRenderers(node);
  }
  static int tracked(MapRenderer& renderer, mdl::Node& node)
  {
    return renderer.m_trackedNodes.at(&node);
  }
  static void changed(MapRenderer& renderer, const std::vector<mdl::Node*>& nodes)
  {
    renderer.nodeVisibilityDidChange(nodes);
  }
  static bool visible(const EntityRenderer& renderer, const mdl::EntityNode& node)
  {
    return renderer.visible(node);
  }
  static bool visible(const EntityModelRenderer& renderer, const mdl::EntityNode& node)
  {
    return renderer.visible(node);
  }
};
TEST_CASE("MapRenderer native visibility selected showHidden and closure", "[visgroups]")
{
  PreferenceManager::createInstance(std::make_unique<TestPreferenceStore>(), true);
  struct Cleanup
  {
    ~Cleanup() { PreferenceManager::destroyInstance(); }
  } cleanup;
  auto fixture = mdl::MapFixture{};
  auto& map = fixture.create();
  auto* owner = new mdl::EntityNode{mdl::Entity{{{"classname", "func_detail"}}}};
  auto builder = mdl::BrushBuilder{map.worldNode().mapFormat(), map.worldBounds()};
  auto* brush =
    new mdl::BrushNode{builder.createCube(32, "synthetic/grid") | kdl::value()};
  owner->addChild(brush);
  auto* control = new mdl::EntityNode{mdl::Entity{{{"classname", "info_target"}}}};
  mdl::addNodes(map, {{map.worldNode().defaultLayer(), {owner, control}}});
  brush->select();
  control->select();
  auto renderer = MapRenderer{map};
  auto entities =
    EntityRenderer{map.logger(), map.entityModelManager(), map.editorContext()};
  auto models =
    EntityModelRenderer{map.logger(), map.entityModelManager(), map.editorContext()};
  entities.setShowHiddenEntities(true);
  models.setShowHiddenEntities(true);
  CHECK(NativeVisibilityRendererTest::desired(*brush) != 0);
  CHECK(NativeVisibilityRendererTest::visible(entities, *control));
  CHECK(NativeVisibilityRendererTest::visible(models, *control));
  owner->setNativeVisibility({false, true, false});
  NativeVisibilityRendererTest::changed(renderer, {owner});
  CHECK(NativeVisibilityRendererTest::desired(*brush) == 0);
  CHECK(NativeVisibilityRendererTest::tracked(renderer, *brush) == 0);
  CHECK(NativeVisibilityRendererTest::tracked(renderer, *control) != 0);
  CHECK_FALSE(NativeVisibilityRendererTest::visible(entities, *owner));
  CHECK_FALSE(NativeVisibilityRendererTest::visible(models, *owner));
  owner->setNativeVisibility({});
  brush->setNativeVisibility({false, false, true});
  NativeVisibilityRendererTest::changed(renderer, {brush});
  CHECK(NativeVisibilityRendererTest::tracked(renderer, *brush) == 0);
  CHECK(NativeVisibilityRendererTest::tracked(renderer, *owner) != 0);
  brush->setNativeVisibility({});
  NativeVisibilityRendererTest::changed(renderer, {brush});
  CHECK(NativeVisibilityRendererTest::tracked(renderer, *brush) != 0);
  control->setVisibilityState(mdl::VisibilityState::Hidden);
  CHECK(NativeVisibilityRendererTest::visible(entities, *control));
  CHECK(NativeVisibilityRendererTest::visible(models, *control));
  control->setNativeVisibility({true, false, false});
  CHECK_FALSE(NativeVisibilityRendererTest::visible(entities, *control));
  CHECK_FALSE(NativeVisibilityRendererTest::visible(models, *control));
}
} // namespace tb::render
