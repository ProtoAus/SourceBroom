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
#include "mdl/NodeContents.h"
#include "mdl/SourceEditorMetadata.h"
#include "mdl/VmfScene.h"

#include "vm/mat.h"

#include <algorithm>
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

TEST_CASE("VmfScene read-only compatibility preview", "[vmf]")
{
  const auto bounds = vm::bbox3d{4096};
  auto source = fixture();
  auto expectedOmissions = size_t{0};
  auto strictRefuses = false;
  auto unlocated = false;
  SECTION("valid control is explicitly immutable too") {}
  SECTION("duplicate IDs are preserved, never repaired")
  {
    const auto at = source.find("\"id\" \"9\"");
    REQUIRE(at != std::string::npos);
    source.replace(at, std::string{"\"id\" \"9\""}.size(), "\"id\" \"0002\"");
    strictRefuses = true;
  }
  SECTION("invalid UV omits its entire solid, not just the failed side")
  {
    const auto at = source.find("[1 0 0 0.1] 0.25");
    REQUIRE(at != std::string::npos);
    source.replace(at, std::string{"[1 0 0 0.1] 0.25"}.size(), "[1 0 0 0.1] 0");
    expectedOmissions = 1;
    strictRefuses = true;
  }
  SECTION("incomplete solid has no proxy hull")
  {
    const auto first = source.find("side {");
    const auto last = source.find('}', first);
    REQUIRE(last != std::string::npos);
    source.erase(first, last - first + 1);
    expectedOmissions = 1;
    strictRefuses = true;
  }
  SECTION("an invalid first plane is explicitly unlocated")
  {
    const auto at = source.find("(0 0 0)");
    REQUIRE(at != std::string::npos);
    source.replace(at, 7, "(nan 0 0)");
    expectedOmissions = 1;
    strictRefuses = true;
    unlocated = true;
  }
  SECTION("an all-invalid brush entity does not become a fake point")
  {
    const auto oldWorld =
      std::string{"world { \"id\" \"1\" \"classname\" \"worldspawn\""};
    const auto at = source.find(oldWorld);
    REQUIRE(at != std::string::npos);
    source.replace(
      at,
      oldWorld.size(),
      oldWorld + " }\r\nentity { \"id\" \"10\" \"classname\" \"func_detail\"");
    const auto uv = source.find("[1 0 0 0.1] 0.25");
    REQUIRE(uv != std::string::npos);
    source.replace(uv, std::string{"[1 0 0 0.1] 0.25"}.size(), "[1 0 0 0.1] 0");
    expectedOmissions = 1;
    strictRefuses = true;
  }
  if (strictRefuses)
  {
    CHECK_THROWS_AS(VmfScene::read(source, bounds), std::invalid_argument);
  }
  else
  {
    const auto strict = VmfScene::read(source, bounds);
    CHECK_FALSE(strict.isReadOnlyPreview());
    CHECK(strict.serialize(strict.world()) == source);
  }
  auto scene = VmfScene::readReadOnlyPreview(source, bounds);
  CHECK(scene.isReadOnlyPreview());
  CHECK(scene.unprojectedSolidCount() == expectedOmissions);
  CHECK(scene.document().source() == source);
  CHECK(scene.serialize(scene.world()) == source);
  const auto& children = scene.world().defaultLayer()->children();
  REQUIRE(children.size() == (expectedOmissions ? 1u : 2u));
  if (expectedOmissions)
  {
    CHECK(std::ranges::none_of(children, [](const auto* node) {
      return dynamic_cast<const BrushNode*>(node) != nullptr;
    }));
    const auto omitted = std::ranges::find_if(
      scene.diagnostics(), [](const auto& d) { return d.unprojectedSolid; });
    REQUIRE(omitted != scene.diagnostics().end());
    CHECK(omitted->planeSample.has_value() == !unlocated);
  }
  auto* point = dynamic_cast<EntityNode*>(children.back());
  REQUIRE(point);
  CHECK(point->entity().classname() == "info_target");
  const auto binding = point->lineNumber();
  CHECK_THROWS_AS(scene.validateDuplication({point}), std::invalid_argument);
  CHECK_THROWS_AS(
    scene.validateAdditions(*scene.world().defaultLayer(), {point}, true),
    std::invalid_argument);
  CHECK_THROWS_AS(scene.registerAdditions({point}, true), std::invalid_argument);
  CHECK_THROWS_AS(scene.validateRemoval({point}), std::invalid_argument);
  CHECK_THROWS_AS(
    scene.validateReparent(*scene.world().defaultLayer(), {point}),
    std::invalid_argument);
  CHECK_THROWS_AS(
    scene.validateContents(*point, NodeContents{point->entity()}, bounds),
    std::invalid_argument);
  CHECK(point->lineNumber() == binding);
  CHECK(scene.serialize(scene.world()) == source);
  auto changed = point->entity();
  changed.addOrUpdateProperty("targetname", "preview_must_not_save");
  point->setEntity(std::move(changed));
  CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
}

TEST_CASE("VmfScene", "[vmf]")
{
  const auto bounds = vm::bbox3d{4096};
  SECTION("finite Source preview envelope accepts distant closed solids without repair")
  {
    const auto previewBounds =
      vm::bbox3d{-VmfScene::PreviewWorldExtent, VmfScene::PreviewWorldExtent};
    auto scene = VmfScene::read(fixture(), bounds);
    auto moved = brush(scene.world()).brush();
    REQUIRE(
      moved
        .transform(
          previewBounds, vm::translation_matrix(vm::vec3d{100000, -90000, 80000}), true)
        .is_success());
    brush(scene.world()).setBrush(std::move(moved));
    const auto native = scene.serialize(scene.world());
    CHECK_THROWS_AS(
      VmfScene::read(native, vm::bbox3d{-65536, 65536}), std::invalid_argument);
    auto reopened = VmfScene::read(native, previewBounds);
    CHECK(reopened.serialize(reopened.world()) == native);
    const auto& b = brush(reopened.world()).brush();
    CHECK(b.faceCount() == 6);
    CHECK(b.vertexCount() == 8);
    CHECK(b.bounds() == vm::bbox3d{{100000, -90000, 80000}, {100064, -89936, 80064}});

    auto open = fixture();
    const auto first = open.find("side {");
    const auto last = open.find('}', first);
    REQUIRE(first != std::string::npos);
    REQUIRE(last != std::string::npos);
    open.erase(first, last - first + 1);
    CHECK_THROWS_AS(VmfScene::read(open, previewBounds), std::invalid_argument);
    CHECK_THROWS_AS(
      VmfScene::read(open, vm::bbox3d{-1048576, 1048576}), std::invalid_argument);
  }
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
  SECTION("connections on unsupported owners retain opaque brush guards")
  {
    for (const auto& owner : {"side", "solid", "world", "other"})
    {
      INFO(owner);
      const auto attachment =
        std::string{"connections { future_geometry { \"origin\" \"0 0 0\" } } "};
      auto source = fixture();
      if (std::string{owner} == "side")
        source = fixture(false, attachment);
      else if (std::string{owner} == "solid")
        source.insert(source.find("solid {") + 7, attachment);
      else if (std::string{owner} == "world")
        source.insert(source.find("world {") + 7, attachment);
      else
        source = fixture(false, "future { " + attachment + "} ");
      auto scene = VmfScene::read(source, bounds);
      CHECK(scene.serialize(scene.world()) == source);
      translate(brush(scene.world()));
      CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
    }
  }
  SECTION("only supported flat entity outputs permit affected transforms")
  {
    for (const auto& contents :
         {"\"OnUser1\" \"!self,Use,,0.000,-1\"",
          "future { \"origin\" \"0 0 0\" }",
          "\"OnUser1\" \"malformed raw attachment\""})
    {
      INFO(contents);
      auto source = fixture();
      source.insert(source.rfind('}'), std::string{" connections { "} + contents + " } ");
      const auto supported = std::string{contents}.find("!self") != std::string::npos;
      // Origin edits must use the same conservative attachment check.
      {
        auto scene = VmfScene::read(source, bounds);
        CHECK(scene.serialize(scene.world()) == source);
        auto* node =
          dynamic_cast<EntityNode*>(scene.world().defaultLayer()->children()[1]);
        auto changed = node->entity();
        changed.addOrUpdateProperty("origin", "9 17 25");
        node->setEntity(changed);
        if (supported)
        {
          const auto saved = scene.serialize(scene.world());
          auto reopened = VmfScene::read(saved, bounds);
          CHECK(reopened.serialize(reopened.world()) == saved);
          CHECK(saved.find(contents) != std::string::npos);
        }
        else
          CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
      }
      // Unsupported output attachments may refer to the moved brush.
      {
        auto scene = VmfScene::read(source, bounds);
        CHECK(scene.serialize(scene.world()) == source);
        translate(brush(scene.world()));
        if (supported)
          CHECK_NOTHROW(scene.serialize(scene.world()));
        else
          CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
      }
    }
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
  SECTION(
    "unregistered additions refuse but plain native deletion preserves unrelated bytes")
  {
    auto scene = VmfScene::read(fixture(), bounds);
    auto* layer = scene.world().defaultLayer();
    auto* original = layer->children()[0];
    auto copy = std::unique_ptr<Node>{original->clone(bounds)};
    layer->addChild(copy.get());
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
    layer->removeChild(copy.get());
    const auto& entry = scene.document().entries()[original->lineNumber() - 1];
    auto expected = scene.document().source();
    expected.erase(entry.key.offset, entry.end - entry.key.offset);
    layer->removeChild(original);
    auto owned = std::unique_ptr<Node>{original};
    const auto deleted = scene.serialize(scene.world());
    CHECK(deleted == expected);
    auto reopened = VmfScene::read(deleted, bounds);
    CHECK(reopened.world().defaultLayer()->childCount() == layer->childCount());
    CHECK(reopened.serialize(reopened.world()) == deleted);
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
  SECTION("read-only editor metadata survives acted brush edits without relaxing guards")
  {
    const auto metadataBytes = std::string{
      "visgroups { visgroup { \"visgroupid\" \"10\" \"name\" \"Synthetic\" } }\r\n"
      "cameras { \"activecamera\" \"0\" camera { \"position\" \"[8 16 24]\" "
      "\"look\" \"[32 32 32]\" } }\r\n"
      "cordons { \"active\" \"1\" cordon { \"name\" \"Synthetic\" \"active\" \"1\" "
      "box { \"mins\" \"-128 -128 -128\" \"maxs\" \"128 128 128\" } } }\r\n"};
    for (const auto hidden : {false, true})
    {
      CAPTURE(hidden);
      const auto source = fixture(false, {}, hidden) + metadataBytes;
      auto scene = VmfScene::read(source, bounds);
      const auto snapshot = SourceEditorMetadata::read(scene.document());
      REQUIRE(snapshot.diagnostics.empty());
      REQUIRE(snapshot.memberships.size() == 3);
      CHECK(snapshot.memberships[1].hiddenByContainer == hidden);
      CHECK(scene.serialize(scene.world()) == source);
      translate(brush(scene.world()));
      const auto saved = scene.serialize(scene.world());
      CHECK(saved != source);
      CHECK(saved.substr(saved.size() - metadataBytes.size()) == metadataBytes);
      auto reopened = VmfScene::read(saved, bounds);
      const auto acted = SourceEditorMetadata::read(reopened.document());
      CHECK(acted.diagnostics.empty());
      CHECK(acted.memberships[1].hiddenByContainer == hidden);
      CHECK(acted.activeCamera == snapshot.activeCamera);
      CHECK(acted.cameras[0].eye == snapshot.cameras[0].eye);
      CHECK(acted.cordons[0].boxes == snapshot.cordons[0].boxes);
      CHECK(
        brush(reopened.world()).brush().bounds() == vm::bbox3d{{16, 8, 4}, {80, 72, 68}});
      brush(reopened.world())
        .setVisibilityState(
          hidden ? VisibilityState::Inherited : VisibilityState::Hidden);
      CHECK_THROWS_AS(reopened.serialize(reopened.world()), std::invalid_argument);
    }
    // Interpreting metadata does not make geometry-bearing opaque blocks editable.
    auto opaque = VmfScene::read(
      fixture(false, "dispinfo { \"power\" \"2\" }") + metadataBytes, bounds);
    CHECK(SourceEditorMetadata::read(opaque.document()).cameras[0].navigable());
    CHECK(opaque.serialize(opaque.world()) == opaque.document().source());
    translate(brush(opaque.world()));
    CHECK_THROWS_AS(opaque.serialize(opaque.world()), std::invalid_argument);
  }
  SECTION("existing native groups remain read-only with conservative transform refusal")
  {
    auto source = fixture();
    const auto editor = source.find("editor { \"color\"");
    REQUIRE(editor != std::string::npos);
    source.insert(editor, "group { \"id\" \"20\" editor { \"color\" \"1 2 3\" } } ");
    auto scene = VmfScene::read(source, bounds);
    const auto snapshot = SourceEditorMetadata::read(scene.document());
    REQUIRE(snapshot.groups.size() == 1);
    CHECK(snapshot.groups[0].nativeId == "20");
    CHECK(snapshot.diagnostics.empty());
    CHECK(scene.serialize(scene.world()) == source);
    // Group references are not scene parent bindings. Do not invent a GroupNode
    // projection that would bypass native hierarchy/unknown-attachment guards.
    translate(brush(scene.world()));
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
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
