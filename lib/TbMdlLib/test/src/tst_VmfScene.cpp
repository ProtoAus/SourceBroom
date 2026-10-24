/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/EditorContext.h"
#include "mdl/EntityNode.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/Map_Geometry.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/VmfScene.h"

#include "vm/mat.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
std::string fixture(bool ramp = false, std::string attachment = {}, bool hidden = false)
{
  const auto planes = std::array<std::string, 6>{
    "(0 0 0) (0 64 0) (0 0 64)",
    "(64 0 0) (64 0 64) (64 64 0)",
    "(0 0 0) (0 0 64) (64 0 0)",
    "(0 64 0) (64 64 0) (0 64 64)",
    "(0 0 0) (64 0 0) (0 64 0)",
    ramp ? "(0 0 32) (0 64 32) (64 0 64)" : "(0 0 64) (0 64 64) (64 0 64)"};
  auto s = std::string{
    "// public synthetic fixture\r\nversioninfo { \"formatversion\" \"100\" }\r\n"
    "world { \"id\" \"1\" \"classname\" \"worldspawn\"\r\n"};
  if (hidden)
  {
    s += "hidden { ";
  }
  s += "solid { \"id\" \"2\"\r\n";
  for (size_t i = 0; i < planes.size(); ++i)
  {
    s += "side { \"id\" \"" + std::to_string(i + 3) + "\" \"plane\" \"" + planes[i]
      + "\" \"material\" \"synthetic/grid\" \"uaxis\" \"[1 0 0 0.1] 0.25\""
        " \"vaxis\" \"[0 -1 0 0] 0.25\" \"rotation\" \"0\" \"lightmapscale\" \"16\" ";
    if (i == 5)
    {
      s += attachment;
    }
    s += "}\r\n";
  }
  s += "}\r\n";
  if (hidden)
  {
    s += "}\r\n";
  }
  s +=
    "editor { \"color\" \"0 128 255\" } }\r\n"
    "entity { \"id\" \"9\" \"classname\" \"info_target\" \"origin\" \"8 16 24\""
    " \"targetname\" \"marker\" }\r\n";
  return s;
}
BrushNode& brush(WorldNode& world)
{
  return *dynamic_cast<BrushNode*>(world.defaultLayer()->children().at(0));
}
void translate(BrushNode& node)
{
  auto b = node.brush();
  REQUIRE(b.transform(vm::bbox3d{4096}, vm::translation_matrix(vm::vec3d{16, 8, 4}), true)
            .is_success());
  node.setBrush(std::move(b));
}
} // namespace

TEST_CASE("VmfScene", "[vmf]")
{
  const auto bounds = vm::bbox3d{4096};
  SECTION("box bounds winding double axes and native bindings")
  {
    const auto source = fixture();
    auto scene = VmfScene::read(source, bounds);
    const auto& b = brush(scene.world());
    CHECK(b.brush().bounds() == vm::bbox3d{{0, 0, 0}, {64, 64, 64}});
    CHECK(b.brush().faceCount() == 6);
    CHECK(b.brush().vertexCount() == 8);
    CHECK(scene.nativeId(b) == "2");
    for (const auto& face : b.brush().faces())
    {
      CHECK(vm::dot(face.normal(), face.center() - vm::vec3d{32, 32, 32}) > 0);
      REQUIRE(face.takeUvCoordSystemSnapshot());
      CHECK(face.takeUvCoordSystemSnapshot()->uAxis == vm::vec3d{1, 0, 0});
      CHECK(face.uvAttributes().offset.x() == 0.1f);
    }
    CHECK(scene.serialize(scene.world()) == source);
    auto clone = std::unique_ptr<WorldNode>{
      static_cast<WorldNode*>(scene.world().cloneRecursively(bounds))};
    CHECK(scene.nativeId(brush(*clone)) == "2");
    CHECK(scene.serialize(*clone) == source);
    CHECK(
      *dynamic_cast<EntityNode*>(clone->defaultLayer()->children()[1])
         ->entity()
         .property("targetname")
      == "marker");
  }
  SECTION("ramp actual transform save reopen and undo")
  {
    const auto source = fixture(true);
    auto scene = VmfScene::read(source, bounds);
    auto undo = std::unique_ptr<WorldNode>{
      static_cast<WorldNode*>(scene.world().cloneRecursively(bounds))};
    translate(brush(scene.world()));
    const auto saved = scene.serialize(scene.world());
    CHECK(saved != source);
    CHECK(saved.find("\"lightmapscale\" \"16\"") != std::string::npos);
    CHECK(saved.find("// public synthetic fixture\r\n") == 0);
    CHECK(saved.find("\"uaxis\" \"[1 0 0 0.1] 0.25\"") == std::string::npos);
    auto reopened = VmfScene::read(saved, bounds);
    CHECK(
      brush(reopened.world()).brush().bounds() == vm::bbox3d{{16, 8, 4}, {80, 72, 68}});
    CHECK(scene.nativeId(brush(reopened.world())) == "2");
    CHECK(scene.serialize(*undo) == source);
  }
  SECTION("near-integer native ramp survives command transform save reopen and undo")
  {
    auto source = fixture(true);
    const auto plane = std::string{"(0 0 32) (0 64 32) (64 0 64)"};
    source.replace(
      source.find(plane), plane.size(), "(0 0 32.0001) (0 64 32.0001) (64 0 64.0001)");
    auto scene = VmfScene::read(source, bounds);
    auto& native = brush(scene.world());
    const auto topIndex =
      native.brush().findFace(vm::vec3d{-0.4472135954999579, 0, 0.8944271909999159});
    REQUIRE(topIndex);
    const auto top = *topIndex;
    const auto points = native.brush().face(top).points();
    CHECK(points[0].z() == 32.0001);
    CHECK(native.brush().bounds().max.z() == Catch::Approx(64.0001).margin(1e-9));
    CHECK(
      native.brush().findClosestVertexPosition({0, 0, 32.0001}).z()
      == Catch::Approx(32.0001).margin(1e-9));
    CHECK(scene.serialize(scene.world()) == source);

    auto mapFixture = MapFixture{};
    auto& map = mapFixture.create({.mapFormat = MapFormat::Valve, .worldBounds = bounds});
    auto* node = static_cast<BrushNode*>(native.clone(bounds));
    REQUIRE(node->brush().face(top).preservesPlanePrecision());
    addNodes(map, {{map.worldNode().defaultLayer(), {node}}});
    selectNodes(map, {node});
    map.editorContext().setAlignmentLock(false);
    const auto translation = vm::translation_matrix(vm::vec3d{16, 8, 4});
    REQUIRE(transformSelection(map, "Translate native ramp", translation));
    const auto& moved = node->brush().face(top);
    CHECK(moved.preservesPlanePrecision());
    for (size_t i = 0; i < points.size(); ++i)
    {
      CHECK(moved.points()[i] == translation * points[i]);
    }
    CHECK(node->brush().bounds().max.z() == Catch::Approx(68.0001).margin(1e-9));
    CHECK(
      node->brush().findClosestVertexPosition({16, 8, 36.0001}).z()
      == Catch::Approx(36.0001).margin(1e-9));
    // With alignment lock off, translation must not rewrite native UV tokens.
    for (size_t i = 0; i < native.brush().faceCount(); ++i)
    {
      CHECK(
        node->brush().face(i).uvAttributes() == native.brush().face(i).uvAttributes());
      CHECK(
        node->brush().face(i).takeUvCoordSystemSnapshot()
        == native.brush().face(i).takeUvCoordSystemSnapshot());
    }
    native.setBrush(node->brush());
    const auto saved = scene.serialize(scene.world());
    const auto originalDocument = VmfDocument::parse(source);
    const auto savedDocument = VmfDocument::parse(saved);
    REQUIRE(originalDocument.entries().size() == savedDocument.entries().size());
    auto planeEdits = std::vector<VmfValueEdit>{};
    for (size_t i = 0; i < originalDocument.entries().size(); ++i)
    {
      const auto& entry = originalDocument.entries()[i];
      if (entry.value && originalDocument.tokenContents(entry.key) == "plane")
      {
        planeEdits.push_back(
          {i, std::string{savedDocument.tokenText(*savedDocument.entries()[i].value)}});
      }
    }
    // Every byte outside the six plane tokens, including all original UV tokens,
    // must survive the normal command's native save unchanged.
    CHECK(originalDocument.serialize(planeEdits) == saved);
    auto reopened = VmfScene::read(saved, bounds);
    CHECK(brush(reopened.world()).brush().face(top).points() == moved.points());
    CHECK(
      brush(reopened.world()).brush().bounds().max.z()
      == Catch::Approx(68.0001).margin(1e-9));
    for (size_t i = 0; i < native.brush().faceCount(); ++i)
    {
      CHECK(
        brush(reopened.world()).brush().face(i).uvAttributes()
        == native.brush().face(i).uvAttributes());
      CHECK(
        brush(reopened.world()).brush().face(i).takeUvCoordSystemSnapshot()
        == native.brush().face(i).takeUvCoordSystemSnapshot());
    }
    // Vertex-derived plane updates also go through setPoints/correctPoints.
    auto regenerated = brush(reopened.world()).brush();
    REQUIRE(regenerated.face(top).updatePointsFromVertices().is_success());
    CHECK(regenerated.face(top).preservesPlanePrecision());
    for (const auto& p : regenerated.face(top).points())
    {
      CHECK(p.z() == Catch::Approx(0.5 * p.x() + 28.0001).margin(1e-9));
    }
    map.undoCommand();
    CHECK(node->brush().face(top).preservesPlanePrecision());
    CHECK(node->brush().face(top).points() == points);
    native.setBrush(node->brush());
    CHECK(scene.serialize(scene.world()) == source);
    map.redoCommand();
    CHECK(node->brush().face(top).preservesPlanePrecision());
    CHECK(node->brush().face(top).points()[0] == translation * points[0]);
  }
  SECTION("double UV axes and untouched decimal tokens")
  {
    auto source = fixture();
    const auto oldAxis = std::string{"[1 0 0 0.1] 0.25"};
    source.replace(
      source.find(oldAxis), oldAxis.size(), "[1 0.123456789012345 0 0.1] 0.25");
    auto scene = VmfScene::read(source, bounds);
    bool found = false;
    for (const auto& f : brush(scene.world()).brush().faces())
    {
      if (f.takeUvCoordSystemSnapshot()->uAxis.y() == 0.123456789012345)
      {
        found = true;
      }
    }
    CHECK(found);
    CHECK(scene.serialize(scene.world()) == source);
  }
  SECTION("entity surgical value edit")
  {
    const auto source = fixture();
    auto scene = VmfScene::read(source, bounds);
    auto* node = dynamic_cast<EntityNode*>(scene.world().defaultLayer()->children()[1]);
    auto e = node->entity();
    e.addOrUpdateProperty("targetname", "changed");
    node->setEntity(std::move(e));
    auto expected = source;
    expected.replace(expected.find("\"marker\""), 8, "\"changed\"");
    CHECK(scene.serialize(scene.world()) == expected);
  }
  SECTION("hidden and displacement bytes survive but transforms refuse")
  {
    const auto source =
      fixture(true, "dispinfo { \"power\" \"2\" normals { \"row0\" \"0 0 1\" } }", true);
    auto scene = VmfScene::read(source, bounds);
    CHECK(brush(scene.world()).visibilityState() == VisibilityState::Hidden);
    CHECK_FALSE(scene.diagnostics().empty());
    CHECK(scene.serialize(scene.world()) == source);
    translate(brush(scene.world()));
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
  SECTION("topology additions deletions and lost bindings refuse")
  {
    auto scene = VmfScene::read(fixture(), bounds);
    auto* node = scene.world().defaultLayer()->children()[0];
    node->setFilePosition(0, 0);
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
    node->setFilePosition(12345, 1);
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
  SECTION("real object additions and deletions refuse")
  {
    auto scene = VmfScene::read(fixture(), bounds);
    auto* layer = scene.world().defaultLayer();
    auto* original = layer->children()[0];
    auto copy = std::unique_ptr<Node>{original->clone(bounds)};
    layer->addChild(copy.get());
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
    layer->removeChild(copy.get());
    layer->removeChild(original);
    auto owned = std::unique_ptr<Node>{original};
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
  SECTION("face and object IDs have separate namespaces")
  {
    auto source = fixture();
    source.replace(source.find("\"id\" \"3\""), 8, "\"id\" \"1\"");
    source.replace(source.find("\"id\" \"4\""), 8, "\"id\" \"2\"");
    auto scene = VmfScene::read(source, bounds);
    CHECK(scene.serialize(scene.world()) == source);
    translate(brush(scene.world()));
    const auto edited = scene.serialize(scene.world());
    CHECK(edited != source);
    auto reopened = VmfScene::read(edited, bounds);
    CHECK(reopened.serialize(reopened.world()) == edited);

    auto duplicateFace = source;
    const auto secondSide = duplicateFace.find("side", duplicateFace.find("side") + 4);
    const auto secondId = duplicateFace.find("\"id\" \"2\"", secondSide);
    REQUIRE(secondId != std::string::npos);
    duplicateFace.replace(secondId, 8, "\"id\" \"001\"");
    CHECK_THROWS_AS(VmfScene::read(duplicateFace, bounds), std::invalid_argument);
    auto duplicateObject = source;
    duplicateObject.replace(duplicateObject.find("\"id\" \"9\""), 8, "\"id\" \"1\"");
    CHECK_THROWS_AS(VmfScene::read(duplicateObject, bounds), std::invalid_argument);
  }
  SECTION("unknown version and overlay references refuse edits")
  {
    auto source = fixture();
    source.replace(source.find("\"100\""), 5, "\"999\"");
    auto scene = VmfScene::read(source, bounds);
    CHECK(scene.serialize(scene.world()) == source);
    translate(brush(scene.world()));
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
    for (const auto* classname :
         {"info_overlay",
          "info_overlay_transition",
          "func_instance",
          "func_instance_parms"})
    {
      CAPTURE(classname);
      source = fixture();
      source.replace(source.find("info_target"), 11, classname);
      auto attached = VmfScene::read(source, bounds);
      CHECK(attached.serialize(attached.world()) == source);
      translate(brush(attached.world()));
      CHECK_THROWS_AS(attached.serialize(attached.world()), std::invalid_argument);
    }
  }
  SECTION("invalid plane UV and duplicate ID refuse without dropped faces")
  {
    for (const auto& pair : std::array<std::pair<std::string, std::string>, 3>{
           std::pair{"(0 0 0) (0 64 0) (0 0 64)", "(0 0 0) (0 0 0) (0 0 0)"},
           std::pair{"[1 0 0 0.1] 0.25", "[1 0 0 nan] 0.25"},
           std::pair{"\"id\" \"9\"", "\"id\" \"2\""}})
    {
      auto source = fixture();
      source.replace(source.find(pair.first), pair.first.size(), pair.second);
      CHECK_THROWS_AS(VmfScene::read(source, bounds), std::invalid_argument);
    }
  }
}
} // namespace tb::mdl
