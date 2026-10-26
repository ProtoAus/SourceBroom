/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "SourceDisplacementFixture.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/BrushRendererBrushCache.h"
#include "mdl/EditorContext.h"
#include "mdl/EntityNode.h"
#include "mdl/GroupNode.h"
#include "mdl/Hit.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_Geometry.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/NodeContents.h"
#include "mdl/PickResult.h"
#include "mdl/SourceEditorMetadata.h"
#include "mdl/VmfScene.h"

#include "vm/mat.h"
#include "vm/mat_ext.h"

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

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

TEST_CASE("VmfScene renderer vertices keep outward clockwise faces", "[vmf]")
{
  for (const auto ramp : {false, true})
  {
    const auto source = fixture(ramp);
    auto scene = VmfScene::read(source, vm::bbox3d{4096});
    auto cache = BrushRendererBrushCache{};
    cache.validateVertexCache(brush(scene.world()));
    const auto& vertices = cache.cachedVertices();
    REQUIRE(vertices.size() == 24);
    REQUIRE(cache.cachedFacesSortedByMaterial().size() == 6);
    auto triangles = size_t{0};
    for (const auto& face : cache.cachedFacesSortedByMaterial())
    {
      const auto first = face.indexOfFirstVertexRelativeToBrush;
      const auto& a = vertices.at(first).attr;
      const auto& normal = vertices.at(first).rest.attr;
      const auto delta = vm::vec3d{normal} - face.face->normal();
      CHECK(vm::dot(delta, delta) < 1e-10);
      for (size_t i = 1; i + 1 < face.vertexCount; ++i)
      {
        const auto& b = vertices.at(first + i).attr;
        const auto& c = vertices.at(first + i + 1).attr;
        CHECK(vm::dot(vm::cross(b - a, c - a), normal) < 0.0f);
        ++triangles;
      }
    }
    CHECK(triangles == 12);
    CHECK(scene.serialize(scene.world()) == source);
  }
}

TEST_CASE("VmfScene native displacement mesh", "[vmf]")
{
  const auto bounds = vm::bbox3d{4096};
  const auto source = fixture(false, test::displacementBlock());
  auto scene = VmfScene::read(source, bounds);
  auto& node = brush(scene.world());
  const auto faceIndex = node.brush().findFace(vm::vec3d{0, 0, 1});
  REQUIRE(faceIndex);
  const auto& face = node.brush().face(*faceIndex);
  REQUIRE(face.sourceDisplacement());
  const auto& mesh = *face.sourceDisplacement();
  CHECK(mesh.positions()[1] == vm::vec3d{16, 0, 64});
  CHECK(mesh.positions()[5] == vm::vec3d{0, 16, 64});
  CHECK(mesh.positions()[12] == vm::vec3d{32, 32, 80});
  CHECK(mesh.normals()[12].z() > 0);
  CHECK(mesh.alphas()[4] == 255);
  CHECK(node.physicalBounds().max.z() == 80);
  CHECK(scene.serialize(scene.world()) == source);

  SECTION("renderer submits the native grid rather than a planar polygon fan")
  {
    auto cache = BrushRendererBrushCache{};
    cache.validateVertexCache(node);
    const auto& faces = cache.cachedFacesSortedByMaterial();
    const auto found = std::find_if(faces.begin(), faces.end(), [](const auto& f) {
      return bool(f.face->sourceDisplacement());
    });
    REQUIRE(found != faces.end());
    CHECK(found->vertexCount == 25);
    CHECK(found->indexCount == 96);
    const auto first = found->indexOfFirstVertexRelativeToBrush;
    CHECK(cache.cachedVertices()[first + 12].attr == vm::vec3f{32, 32, 80});
    CHECK(
      cache.cachedVertices()[first + 12].rest.rest.attr
      == face.uvCoords(vm::vec3d{32, 32, 64}));
    auto indices = std::vector<GLuint>(found->indexCount);
    found->writeIndices(indices, first + 200);
    for (size_t i = 0; i < indices.size(); ++i)
    {
      CHECK(indices[i] == first + 200 + mesh.indices()[i]);
    }
    CHECK(cache.cachedEdges().size() > 50);
    const auto sideIndex = node.brush().findFace(vm::vec3d{1, 0, 0});
    REQUIRE(sideIndex);
    const auto* neighbor = &node.brush().face(*sideIndex);
    const auto highlighted = std::count_if(
      cache.cachedEdges().begin(), cache.cachedEdges().end(), [&](const auto& edge) {
        const auto a = cache.cachedVertices()[edge.vertexIndex1RelativeToBrush].attr;
        const auto b = cache.cachedVertices()[edge.vertexIndex2RelativeToBrush].attr;
        return edge.face1 == neighbor && edge.face2 == neighbor && a.z() == 64
               && b.z() == 64;
      });
    CHECK(highlighted == 1);
  }
  SECTION("pick hits the raised surface outside the original hull")
  {
    auto hits = PickResult{};
    node.pick(EditorContext{}, vm::ray3d{{32, 32, 128}, {0, 0, -1}}, hits);
    REQUIRE(hits.size() == 1);
    CHECK(hits.all()[0].distance() == Catch::Approx(48));
    CHECK(hits.all()[0].hitPoint() == vm::vec3d{32, 32, 80});
  }
  SECTION("distance and material edits preserve unrelated native bytes")
  {
    auto copy = node.brush();
    auto edited = *copy.face(*faceIndex).sourceDisplacement();
    edited.setDistance(2, 2, 24);
    copy.face(*faceIndex).setSourceDisplacement(std::move(edited));
    node.setBrush(std::move(copy));
    CHECK(mesh.positions()[12].z() == 80);
    CHECK(node.physicalBounds().max.z() == 88);
    auto expected = source;
    const auto token = expected.find("+0  16  0");
    REQUIRE(token != std::string::npos);
    expected.replace(token, 9, "+0  24  0");
    const auto saved = scene.serialize(scene.world());
    CHECK(saved == expected);
    auto reopened = VmfScene::read(saved, bounds);
    CHECK(
      brush(reopened.world())
        .brush()
        .face(*faceIndex)
        .sourceDisplacement()
        ->distance(2, 2)
      == 24);
    auto materialEdit = node.brush();
    materialEdit.face(*faceIndex).setMaterialName("synthetic/changed");
    node.setBrush(std::move(materialEdit));
    CHECK(scene.serialize(scene.world()).find("synthetic/changed") != std::string::npos);
  }
  SECTION("whole base translation preserves the native grid block")
  {
    const auto delta = vm::vec3d{16.125, 8.25, 4.5};
    const auto lockUv = GENERATE(false, true);
    const auto base = mesh.basePositions()[12];
    const auto expectedUv = face.uvCoords(lockUv ? base : base + delta);
    auto moved = node.brush();
    REQUIRE(moved.transform(bounds, vm::translation_matrix(delta), lockUv).is_success());
    const auto movedFace = moved.findFace(vm::vec3d{0, 0, 1});
    REQUIRE(movedFace);
    CHECK(
      vm::is_equal(moved.face(*movedFace).uvCoords(base + delta), expectedUv, 0.0001f));
    node.setBrush(std::move(moved));
    CHECK(node.physicalBounds().max == vm::vec3d{80.125, 72.25, 84.5});
    const auto saved = scene.serialize(scene.world());
    auto expectedBlock = test::displacementBlock();
    const auto start = expectedBlock.find("[0 0 64]");
    REQUIRE(start != std::string::npos);
    expectedBlock.replace(start, std::string{"[0 0 64]"}.size(), "[16.125 8.25 68.5]");
    CHECK(saved.find(expectedBlock) != std::string::npos);
    auto reopened = VmfScene::read(saved, bounds);
    const auto index = brush(reopened.world()).brush().findFace(vm::vec3d{0, 0, 1});
    REQUIRE(index);
    CHECK(
      brush(reopened.world()).brush().face(*index).sourceDisplacement()->positions()[12]
      == vm::vec3d{48.125, 40.25, 84.5});
    CHECK(brush(reopened.world()).physicalBounds().max == node.physicalBounds().max);
  }
  SECTION("mesh-only and base-only movement refuse even below arithmetic tolerance")
  {
    const auto meshOnly = GENERATE(false, true);
    const auto delta = GENERATE(vm::vec3d{16, 8, 4}, vm::vec3d{0.00000001, 0, 0});
    auto changed = node.brush();
    if (meshOnly)
    {
      changed.face(*faceIndex).setSourceDisplacement(mesh.translated(delta));
    }
    else
    {
      REQUIRE(
        changed.transform(bounds, vm::translation_matrix(delta), false).is_success());
      const auto movedFace = changed.findFace(vm::vec3d{0, 0, 1});
      REQUIRE(movedFace);
      changed.face(*movedFace).setSourceDisplacement(mesh);
    }
    node.setBrush(std::move(changed));
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
  SECTION("rotation and uniform scale update submitted cache and retained topology")
  {
    const auto scale = GENERATE(1.0, 2.0, 0.5);
    const auto rotation = scale == 1;
    const auto transform = rotation ? vm::rotation_matrix(vm::vec3d{1, 0, 0}, 0.37)
                                    : vm::scaling_matrix(vm::vec3d{scale, scale, scale});
    auto changed = node.brush();
    REQUIRE(changed
              .transform(
                bounds,
                transform,
                true,
                rotation ? SourceTransformPolicy::ProperRotation
                         : SourceTransformPolicy::PositiveUniformScale)
              .is_success());
    CHECK_NOTHROW(scene.validateContents(node, NodeContents{changed}, bounds));
    node.setBrush(std::move(changed));
    auto cache = BrushRendererBrushCache{};
    cache.validateVertexCache(node);
    const auto& faces = cache.cachedFacesSortedByMaterial();
    const auto found = std::find_if(faces.begin(), faces.end(), [](const auto& f) {
      return bool(f.face->sourceDisplacement());
    });
    REQUIRE(found != faces.end());
    CHECK(found->vertexCount == 25);
    CHECK(found->indexCount == 96);
    const auto first = found->indexOfFirstVertexRelativeToBrush;
    for (size_t i = 0; i < mesh.positions().size(); ++i)
    {
      const auto& vertex = cache.cachedVertices()[first + i];
      CHECK(
        vm::is_equal(vertex.attr, vm::vec3f{transform * mesh.positions()[i]}, 0.0001f));
      CHECK(
        vm::is_equal(
          vertex.rest.attr,
          vm::vec3f{(vm::slice<3, 3>(transform, 0, 0) / scale) * mesh.normals()[i]},
          0.0001f));
      CHECK(
        vertex.rest.rest.attr
        == found->face->uvCoords(transform * mesh.basePositions()[i]));
    }
    auto indices = std::vector<GLuint>(found->indexCount);
    found->writeIndices(indices, first + 200);
    for (size_t i = 0; i < indices.size(); ++i)
    {
      CHECK(indices[i] == first + 200 + mesh.indices()[i]);
    }
    const auto saved = scene.serialize(scene.world());
    CHECK(saved != source);
    auto reopened = VmfScene::read(saved, bounds);
    auto meshes = size_t{0};
    for (const auto& f : brush(reopened.world()).brush().faces())
    {
      if (const auto& reopenedMesh = f.sourceDisplacement())
      {
        ++meshes;
        CHECK(
          vm::is_equal(
            reopenedMesh->positions()[12], transform * mesh.positions()[12], 0.000001));
        CHECK(reopenedMesh->alphas() == mesh.alphas());
      }
    }
    CHECK(meshes == 1);
  }
  SECTION("mesh-only or base-only rotations refuse even below arithmetic tolerance")
  {
    const auto meshOnly = GENERATE(false, true);
    const auto angle = GENERATE(0.37, 0.00000001);
    const auto transform = vm::rotation_matrix(vm::vec3d{0, 0, 1}, angle);
    auto changed = node.brush();
    if (meshOnly)
    {
      changed.face(*faceIndex).setSourceDisplacement(mesh.rigidlyTransformed(transform));
    }
    else
    {
      REQUIRE(
        changed.transform(bounds, transform, false, SourceTransformPolicy::ProperRotation)
          .is_success());
      const auto binding = face.lineNumber();
      for (size_t i = 0; i < changed.faceCount(); ++i)
      {
        if (changed.face(i).lineNumber() == binding)
        {
          changed.face(i).setSourceDisplacement(mesh);
        }
      }
    }
    CHECK_THROWS_AS(
      scene.validateContents(node, NodeContents{changed}, bounds), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == source);
    node.setBrush(std::move(changed));
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
  SECTION("malformed meshes refuse strictly and are counted as preview omissions")
  {
    auto invalid = source;
    const auto token = invalid.find("+0  16  0");
    REQUIRE(token != std::string::npos);
    invalid.replace(token, 9, "+0  nan  0");
    CHECK_THROWS_AS(VmfScene::read(invalid, bounds), std::invalid_argument);
    auto preview = VmfScene::readReadOnlyPreview(invalid, bounds);
    CHECK(preview.unprojectedSolidCount() == 1);
    CHECK(preview.serialize(preview.world()) == invalid);
  }
}

TEST_CASE("VmfScene every displacement agrees with whole base movement", "[vmf]")
{
  const auto scenario = GENERATE(0, 1, 2);
  const auto motion = GENERATE(0, 1, 2);
  const auto rotation = motion == 1;
  const auto scaling = motion == 2;
  const auto bounds = vm::bbox3d{4096};
  auto source = fixture(false, test::displacementBlock());
  auto bottom = test::displacementBlock();
  const auto start = bottom.find("[0 0 64]");
  REQUIRE(start != std::string::npos);
  bottom.replace(start, std::string{"[0 0 64]"}.size(), "[0 0 0]");
  const auto owner = std::string{"side { \"id\" \"7\""};
  const auto where = source.find(owner);
  REQUIRE(where != std::string::npos);
  source.insert(where + owner.size(), "\n" + bottom + "\n");
  auto scene = VmfScene::read(source, bounds);
  auto& node = brush(scene.world());
  auto displaced = std::vector<size_t>{};
  for (size_t i = 0; i < node.brush().faceCount(); ++i)
  {
    if (node.brush().face(i).sourceDisplacement())
    {
      displaced.push_back(node.brush().face(i).lineNumber());
    }
  }
  REQUIRE(displaced.size() == 2);
  CHECK(scene.serialize(scene.world()) == source);
  const auto findFace = [](const Brush& b, const size_t binding) {
    for (size_t i = 0; i < b.faceCount(); ++i)
    {
      if (b.face(i).lineNumber() == binding)
      {
        return i;
      }
    }
    throw std::logic_error{"Native face binding disappeared"};
  };
  auto changed = node.brush();
  if (scenario != 1)
  {
    const auto transform = rotation  ? vm::rotation_matrix(vm::vec3d{0, 0, 1}, 0.37)
                           : scaling ? vm::scaling_matrix(vm::vec3d{2, 2, 2})
                                     : vm::translation_matrix(vm::vec3d{16, 8, 4});
    REQUIRE(changed
              .transform(
                bounds,
                transform,
                false,
                rotation  ? SourceTransformPolicy::ProperRotation
                : scaling ? SourceTransformPolicy::PositiveUniformScale
                          : SourceTransformPolicy::TranslationOnly)
              .is_success());
  }
  if (scenario == 0)
  {
    CHECK_NOTHROW(scene.validateContents(node, NodeContents{changed}, bounds));
    node.setBrush(std::move(changed));
    const auto saved = scene.serialize(scene.world());
    CHECK(saved != source);
    auto reopened = VmfScene::read(saved, bounds);
    const auto& reopenedBrush = brush(reopened.world()).brush();
    for (const auto binding : displaced)
    {
      CHECK(
        vm::is_equal(
          reopenedBrush.face(findFace(reopenedBrush, binding))
            .sourceDisplacement()
            ->positions()[12],
          node.brush()
            .face(findFace(node.brush(), binding))
            .sourceDisplacement()
            ->positions()[12],
          0.000001));
    }
  }
  else
  {
    const auto later = findFace(changed, displaced.back());
    const auto& mesh = *changed.face(later).sourceDisplacement();
    changed.face(later).setSourceDisplacement(
      rotation
        ? mesh.rigidlyTransformed(vm::rotation_matrix(vm::vec3d{0, 0, 1}, 0.00000001))
      : scaling ? mesh.uniformlyScaled(
                    vm::scaling_matrix(vm::vec3d{1.00000001, 1.00000001, 1.00000001}))
                : mesh.translated({0.00000001, 0, 0}));
    CHECK_THROWS_AS(
      scene.validateContents(node, NodeContents{changed}, bounds), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == source);
    node.setBrush(std::move(changed));
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
}

TEST_CASE(
  "VmfScene explicit native rotation retains opaque and reference guards", "[vmf]")
{
  const auto reference = GENERATE(false, true);
  const auto attachment =
    test::displacementBlock()
    + (reference ? " sides \"8\" " : " future_attachment { key opaque } ");
  const auto source = fixture(false, attachment);
  const auto bounds = vm::bbox3d{4096};
  auto scene = VmfScene::read(source, bounds);
  auto& node = brush(scene.world());
  CHECK(scene.serialize(scene.world()) == source);
  auto changed = node.brush();
  const auto scale = GENERATE(false, true);
  REQUIRE(changed
            .transform(
              bounds,
              scale ? vm::scaling_matrix(vm::vec3d{2, 2, 2})
                    : vm::rotation_matrix(vm::vec3d{1, 0, 0}, 0.37),
              false,
              scale ? SourceTransformPolicy::PositiveUniformScale
                    : SourceTransformPolicy::ProperRotation)
            .is_success());
  CHECK_THROWS_AS(
    scene.validateContents(node, NodeContents{changed}, bounds), std::invalid_argument);
  CHECK(scene.serialize(scene.world()) == source);
  node.setBrush(std::move(changed));
  CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
}

TEST_CASE("VmfScene only parsed displacement sides relax opaque guards", "[vmf]")
{
  const auto misplaced =
    GENERATE(std::string{"none"}, std::string{"solid"}, std::string{"nested side"});
  auto source = fixture();
  if (misplaced != "none")
  {
    const auto owner = misplaced == "solid" ? std::string{"solid { \"id\" \"2\""}
                                            : std::string{"side { \"id\" \"3\""};
    const auto at = source.find(owner);
    REQUIRE(at != std::string::npos);
    const auto attachment = misplaced == "solid"
                              ? test::displacementBlock()
                              : "side { id 99 " + test::displacementBlock() + " }";
    source.insert(at + owner.size(), "\n" + attachment + "\n");
  }
  auto scene = VmfScene::read(source, vm::bbox3d{4096});
  auto& node = brush(scene.world());
  CHECK(scene.serialize(scene.world()) == source);
  for (const auto& face : node.brush().faces())
  {
    REQUIRE_FALSE(face.sourceDisplacement());
  }
  auto moved = node.brush();
  REQUIRE(
    moved.transform(vm::bbox3d{4096}, vm::translation_matrix(vm::vec3d{16, 8, 4}), false)
      .is_success());
  node.setBrush(std::move(moved));
  if (misplaced == "none")
  {
    CHECK(scene.serialize(scene.world()) != source);
  }
  else
  {
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
}

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
      fixture(true, "sourcebroom_opaque_geometry { key \"preserved\" }", true);
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
      fixture(false, "sourcebroom_opaque_geometry { key \"preserved\" }") + metadataBytes,
      bounds);
    CHECK(SourceEditorMetadata::read(opaque.document()).cameras[0].navigable());
    CHECK(opaque.serialize(opaque.world()) == opaque.document().source());
    translate(brush(opaque.world()));
    CHECK_THROWS_AS(opaque.serialize(opaque.world()), std::invalid_argument);
  }
  SECTION("valid empty native groups project without blocking unrelated transforms")
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
    CHECK(scene.world().defaultLayer()->childCount() == 3);
    translate(brush(scene.world()));
    const auto changed = scene.serialize(scene.world());
    CHECK(changed != source);
    auto reopened = VmfScene::read(changed, bounds);
    CHECK(reopened.serialize(reopened.world()) == changed);
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
TEST_CASE("VmfScene ordinary clip admission and surgical side deltas", "[vmf]")
{
  const auto bounds = vm::bbox3d{4096};
  auto original = fixture(
    false,
    "// retained side metadata\r\n"
    "\"lightmapscale\" \"00032\" \"smoothing_groups\" \"0017\" "
    "\"smoothing_groups\" \"4\" editor { \"color\" \"2 4 6\" } ");
  const auto rotation = original.rfind("\"rotation\" \"0\"");
  REQUIRE(rotation != std::string::npos);
  original.replace(
    rotation, std::string{"\"rotation\" \"0\""}.size(), "\"rotation\" \"12.500\"");
  auto scene = VmfScene::read(original, bounds);
  auto& node = brush(scene.world());
  const auto solidBinding = node.lineNumber();
  auto candidate = node.brush();
  auto cut = BrushFace::create(
    {32, 0, 0},
    {32, 0, 64},
    {32, 64, 0},
    "synthetic/cut",
    UvAttributes{},
    SurfaceAttributes{},
    MapFormat::SourceVMF);
  REQUIRE(cut.is_success());
  REQUIRE(candidate.clip(bounds, std::move(cut).value()).is_success());
  auto nodes =
    std::vector<std::pair<Node*, NodeContents>>{{&node, NodeContents{candidate}}};

  SECTION("registered survivors retain exact native subtrees and stable cut identity")
  {
    CHECK_THROWS_AS(
      scene.validateContents(node, nodes.front().second, bounds), std::invalid_argument);
    const auto before = node.brush();
    scene.registerClips(nodes, bounds);
    node.setBrush(std::get<Brush>(nodes.front().second.get()));
    CHECK(node.lineNumber() == solidBinding);
    CHECK(scene.nativeId(node) == "2");
    CHECK(node.brush().bounds().max.x() == 32);
    const auto saved = scene.serialize(scene.world());
    CHECK(saved == scene.serialize(scene.world()));
    CHECK(saved.find("\"id\" \"4\"") == std::string::npos);
    CHECK(saved.find("side\n{\n\"id\" \"9\"") != std::string::npos);
    for (const auto& face : node.brush().faces())
    {
      if (face.lineNumber() <= scene.document().entries().size())
      {
        CHECK(
          saved.find(scene.document().serializeEntry(face.lineNumber() - 1))
          != std::string::npos);
      }
    }
    CHECK_NOTHROW(VmfScene::read(saved, bounds));
    node.setBrush(before);
    CHECK(scene.serialize(scene.world()) == original);
    node.setBrush(std::get<Brush>(nodes.front().second.get()));
    CHECK(scene.serialize(scene.world()) == saved);
    auto aliased = node.brush();
    aliased.faces()[1].setFilePosition(aliased.faces()[0].lineNumber(), 1);
    node.setBrush(std::move(aliased));
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }

  SECTION("later invalid candidate is atomic including the side allocator")
  {
    // Repeating an owner in the same batch must fail after staging the first.
    nodes.push_back(nodes.front());
    CHECK_THROWS_AS(scene.registerClips(nodes, bounds), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == original);
    CHECK(
      std::ranges::any_of(
        std::get<Brush>(nodes.front().second.get()).faces(),
        [](const auto& face) { return face.lineNumber() == 0; }));
    nodes.pop_back();
    scene.registerClips(nodes, bounds);
    node.setBrush(std::get<Brush>(nodes.front().second.get()));
    CHECK(
      scene.serialize(scene.world()).find("side\n{\n\"id\" \"9\"") != std::string::npos);
  }

  SECTION("unregistered direct-save topology refuses")
  {
    node.setBrush(candidate);
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
}

TEST_CASE("VmfScene clip keeps arbitrary length reserved side namespace", "[vmf]")
{
  auto source = fixture();
  const auto huge = std::string(80, '9');
  source += "opaque { side { \"id\" \"000" + huge + "\" } }\n";
  // An unknown root intentionally prevents topology, but still reserves IDs.
  auto refused = VmfScene::read(source, vm::bbox3d{4096});
  auto& node = brush(refused.world());
  auto candidate = node.brush();
  auto cut = BrushFace::create(
    {32, 0, 0},
    {32, 0, 64},
    {32, 64, 0},
    "synthetic/cut",
    UvAttributes{},
    SurfaceAttributes{},
    MapFormat::SourceVMF);
  REQUIRE(cut.is_success());
  REQUIRE(candidate.clip(vm::bbox3d{4096}, std::move(cut).value()).is_success());
  auto nodes =
    std::vector<std::pair<Node*, NodeContents>>{{&node, NodeContents{candidate}}};
  CHECK_THROWS_AS(refused.registerClips(nodes, vm::bbox3d{4096}), std::invalid_argument);
  CHECK(refused.serialize(refused.world()) == source);

  // Recognized editor metadata is preserved and its opaque IDs reserve maxima.
  source = fixture(false, "editor { side { \"id\" \"000" + huge + "\" } }");
  auto scene = VmfScene::read(source, vm::bbox3d{4096});
  auto& ordinary = brush(scene.world());
  nodes = {{&ordinary, NodeContents{candidate}}};
  scene.registerClips(nodes, vm::bbox3d{4096});
  ordinary.setBrush(std::get<Brush>(nodes.front().second.get()));
  const auto saved = scene.serialize(scene.world());
  CHECK(saved.find("\"id\" \"1" + std::string(80, '0') + "\"") != std::string::npos);
  CHECK(scene.nativeId(ordinary) == "2");
}

TEST_CASE("VmfScene generated clip exact states and live owner guards", "[vmf]")
{
  const auto freshTarget = GENERATE(false, true);
  CAPTURE(freshTarget);
  const auto bounds = vm::bbox3d{4096};
  auto scene = VmfScene::read(
    fixture(
      false,
      "// retained template side\r\n\"lightmapscale\" \"00032\" "
      "\"smoothing_groups\" \"0017\" editor { \"color\" \"2 4 6\" } "),
    bounds);
  auto& original = brush(scene.world());
  auto owned = std::unique_ptr<Node>{original.clone(bounds)};
  auto* node = dynamic_cast<BrushNode*>(owned.get());
  REQUIRE(node);
  auto* layer = scene.world().defaultLayer();
  if (freshTarget)
  {
    node->setFilePosition(0, 0);
    for (const auto& face : node->brush().faces())
    {
      face.setFilePosition(0, 0);
    }
  }
  scene.validateAdditions(*layer, {node}, !freshTarget);
  scene.registerAdditions({node}, !freshTarget);
  layer->addChild(owned.release());
  const auto initialBrush = node->brush();
  const auto initial = scene.serialize(scene.world());
  const auto candidate = [&](BrushNode& target, const bool second) {
    auto result = target.brush();
    auto face = second ? BrushFace::create(
                           {0, 32, 0},
                           {64, 32, 0},
                           {0, 32, 64},
                           "synthetic/cut",
                           UvAttributes{},
                           SurfaceAttributes{},
                           MapFormat::SourceVMF)
                       : BrushFace::create(
                           {32, 0, 0},
                           {32, 0, 64},
                           {32, 64, 0},
                           "synthetic/cut",
                           UvAttributes{},
                           SurfaceAttributes{},
                           MapFormat::SourceVMF);
    REQUIRE(face.is_success());
    REQUIRE(result.clip(bounds, std::move(face).value()).is_success());
    return result;
  };
  auto batch = std::vector<std::pair<Node*, NodeContents>>{
    {node, NodeContents{candidate(*node, false)}}};

  SECTION(
    "diagonal clipping removes multiple template or fresh sides without reconstruction")
  {
    auto diagonal = node->brush();
    auto face = BrushFace::create(
      {32, 0, 0},
      {32, 0, 64},
      {0, 32, 0},
      "synthetic/diagonal",
      UvAttributes{},
      SurfaceAttributes{},
      MapFormat::SourceVMF);
    REQUIRE(face.is_success());
    REQUIRE(diagonal.clip(bounds, std::move(face).value()).is_success());
    REQUIRE(diagonal.faceCount() == 5);
    // The normalized diagonal-plane intersection has double-precision roundoff.
    CHECK(diagonal.bounds().max.x() == Catch::Approx{32.0}.epsilon(0).margin(1e-9));
    CHECK(diagonal.bounds().max.y() == Catch::Approx{32.0}.epsilon(0).margin(1e-9));
    batch = {{node, NodeContents{std::move(diagonal)}}};
    scene.registerClips(batch, bounds);
    node->setBrush(std::get<Brush>(batch.front().second.get()));
    const auto saved = scene.serialize(scene.world());
    CHECK(saved == scene.serialize(scene.world()));
    CHECK_NOTHROW(VmfScene::read(saved, bounds));
    node->setBrush(initialBrush);
    CHECK(scene.serialize(scene.world()) == initial);
    node->setBrush(std::get<Brush>(batch.front().second.get()));
    CHECK(scene.serialize(scene.world()) == saved);
  }

  SECTION("mixed fresh template entity batch stages nothing on a late invalid owner")
  {
    auto freshOwned = std::unique_ptr<Node>{original.clone(bounds)};
    auto* fresh = dynamic_cast<BrushNode*>(freshOwned.get());
    REQUIRE(fresh);
    fresh->setFilePosition(0, 0);
    for (const auto& face : fresh->brush().faces())
    {
      face.setFilePosition(0, 0);
    }
    scene.validateAdditions(*layer, {fresh}, false);
    scene.registerAdditions({fresh}, false);
    layer->addChild(freshOwned.release());
    auto entityOwned =
      std::make_unique<EntityNode>(Entity{{{"classname", "func_detail"}}});
    auto* entity = entityOwned.get();
    scene.validateAdditions(*layer, {entity}, false);
    scene.registerAdditions({entity}, false);
    layer->addChild(entityOwned.release());
    scene.validateReparent(*entity, {&original});
    layer->removeChild(&original);
    entity->addChild(&original);
    const auto before = scene.serialize(scene.world());
    batch.emplace_back(fresh, NodeContents{candidate(*fresh, false)});
    batch.emplace_back(&original, NodeContents{candidate(original, false)});
    auto foreign = std::unique_ptr<Node>{original.clone(bounds)};
    auto* foreignBrush = dynamic_cast<BrushNode*>(foreign.get());
    REQUIRE(foreignBrush);
    batch.emplace_back(foreignBrush, NodeContents{candidate(*foreignBrush, false)});
    // Detached foreign ancestry fails only after all three valid routes staged.
    CHECK_THROWS_AS(scene.registerClips(batch, bounds), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == before);
    for (const auto& [target, contents] : batch)
    {
      CHECK(
        std::ranges::any_of(
          std::get<Brush>(contents.get()).faces(),
          [](const auto& face) { return face.lineNumber() == 0; }));
    }
    batch.pop_back();
    scene.registerClips(batch, bounds);
    for (const auto& [target, contents] : batch)
    {
      dynamic_cast<BrushNode*>(target)->setBrush(std::get<Brush>(contents.get()));
    }
    // Initial template uses9..14; fresh uses15..20. A failed batch must not
    // consume21, the first cut identity of the subsequent valid batch.
    CHECK(
      scene.serialize(scene.world()).find("side\n{\n\"id\" \"21\"") != std::string::npos);
  }

  SECTION("live owner gates reject hidden point group layer and foreign world bypasses")
  {
    const auto kind = GENERATE(0, 1, 2, 3, 4);
    CAPTURE(kind);
    scene.registerClips(batch, bounds);
    node->setBrush(std::get<Brush>(batch.front().second.get()));
    const auto good = node->brush();
    const auto saved = scene.serialize(scene.world());
    if (kind == 0)
    {
      layer->setVisibilityState(VisibilityState::Hidden);
      CHECK_THROWS_AS(
        scene.validateClipContents(*node, NodeContents{initialBrush}, bounds),
        std::invalid_argument);
      CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
      layer->setVisibilityState(VisibilityState::Inherited);
    }
    else
    {
      layer->removeChild(node);
      if (kind == 1)
      {
        auto* point = dynamic_cast<EntityNode*>(layer->children().back());
        REQUIRE(point);
        point->addChild(node);
        CHECK_THROWS_AS(
          scene.validateClipContents(*node, NodeContents{initialBrush}, bounds),
          std::invalid_argument);
        CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
        point->removeChild(node);
      }
      if (kind == 2)
      {
        auto group = std::make_unique<GroupNode>(Group{"unsupported"});
        group->setFilePosition(99999, 1);
        layer->addChild(group.get());
        group->addChild(node);
        CHECK_THROWS_AS(
          scene.validateClipContents(*node, NodeContents{initialBrush}, bounds),
          std::invalid_argument);
        CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
        group->removeChild(node);
        layer->removeChild(group.get());
      }
      if (kind == 3)
      {
        auto custom = std::make_unique<LayerNode>(Layer{"unsupported"});
        scene.world().addChild(custom.get());
        custom->addChild(node);
        CHECK_THROWS_AS(
          scene.validateClipContents(*node, NodeContents{initialBrush}, bounds),
          std::invalid_argument);
        CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
        custom->removeChild(node);
        scene.world().removeChild(custom.get());
      }
      if (kind == 4)
      {
        auto foreign = VmfScene::read(fixture(), bounds);
        foreign.world().defaultLayer()->addChild(node);
        CHECK_THROWS_AS(
          scene.validateClipContents(*node, NodeContents{initialBrush}, bounds),
          std::invalid_argument);
        foreign.world().defaultLayer()->removeChild(node);
      }
      layer->addChild(node);
    }
    CHECK(scene.serialize(scene.world()) == saved);
    CHECK_NOTHROW(scene.validateClipContents(*node, NodeContents{good}, bounds));
  }

  SECTION("generic admission direct save and redo reject side and ancestry forgeries")
  {
    CHECK_THROWS_AS(
      scene.validateContents(*node, batch.front().second, bounds), std::invalid_argument);
    scene.registerClips(batch, bounds);
    node->setBrush(std::get<Brush>(batch.front().second.get()));
    const auto firstBrush = node->brush();
    if (!freshTarget)
    {
      const auto emitted = scene.serialize(scene.world());
      for (size_t i = 0; i < initialBrush.faceCount(); ++i)
      {
        if (std::ranges::any_of(firstBrush.faces(), [&](const auto& face) {
              return face.lineNumber() == initialBrush.face(i).lineNumber();
            }))
        {
          const auto sourceSide = original.brush().face(i).lineNumber() - 1;
          auto expected = scene.document().serializeEntry(sourceSide);
          auto sourceId = std::string{};
          for (const auto child : scene.document().entries()[sourceSide].children)
          {
            const auto& entry = scene.document().entries()[child];
            if (entry.value && scene.document().tokenContents(entry.key) == "id")
            {
              sourceId = scene.document().tokenContents(*entry.value);
            }
          }
          REQUIRE_FALSE(sourceId.empty());
          const auto oldId = "\"id\" \"" + sourceId + "\"";
          const auto at = expected.find(oldId);
          REQUIRE(at != std::string::npos);
          expected.replace(at, oldId.size(), "\"id\" \"" + std::to_string(i + 9) + "\"");
          CHECK(emitted.find(expected) != std::string::npos);
        }
      }
      CHECK(emitted.find("\"lightmapscale\" \"00032\"") != std::string::npos);
      CHECK(emitted.find("\"smoothing_groups\" \"0017\"") != std::string::npos);
    }
    auto second = std::vector<std::pair<Node*, NodeContents>>{
      {node, NodeContents{candidate(*node, true)}}};
    scene.registerClips(second, bounds);
    node->setBrush(std::get<Brush>(second.front().second.get()));
    const auto good = node->brush();
    const auto saved = scene.serialize(scene.world());
    auto retired = size_t{0};
    for (const auto& face : initialBrush.faces())
    {
      if (std::ranges::none_of(firstBrush.faces(), [&](const auto& current) {
            return current.lineNumber() == face.lineNumber();
          }))
      {
        retired = face.lineNumber();
      }
    }
    REQUIRE(retired != 0);
    const auto forgery = GENERATE(0, 1, 2, 3, 4);
    auto bad = good;
    if (forgery == 0)
      bad.faces()[0].setFilePosition(0, 0);
    if (forgery == 1)
      bad.faces()[0].setFilePosition(bad.faces()[1].lineNumber(), 1);
    if (forgery == 2)
      bad.faces()[0].setFilePosition(original.brush().face(0).lineNumber(), 1);
    if (forgery == 3)
      bad.faces()[0].setFilePosition(node->lineNumber(), 1);
    if (forgery == 4)
    {
      const auto cut = std::ranges::find_if(bad.faces(), [&](const auto& face) {
        return std::ranges::none_of(initialBrush.faces(), [&](const auto& old) {
          return old.lineNumber() == face.lineNumber();
        });
      });
      REQUIRE(cut != bad.faces().end());
      cut->setFilePosition(
        retired, 1); // Unique, owned, but never an admitted complete set.
    }
    CHECK_THROWS_AS(
      scene.validateClipContents(*node, NodeContents{bad}, bounds),
      std::invalid_argument);
    node->setBrush(std::move(bad));
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
    node->setBrush(good);
    node->setVisibilityState(VisibilityState::Hidden);
    CHECK_THROWS_AS(
      scene.validateClipContents(*node, NodeContents{firstBrush}, bounds),
      std::invalid_argument);
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
    node->setVisibilityState(VisibilityState::Inherited);
    layer->removeChild(node);
    CHECK_THROWS_AS(
      scene.validateClipContents(*node, NodeContents{firstBrush}, bounds),
      std::invalid_argument);
    layer->addChild(node);
    CHECK(scene.serialize(scene.world()) == saved);
    CHECK_NOTHROW(scene.validateClipContents(*node, NodeContents{initialBrush}, bounds));
    CHECK_NOTHROW(scene.validateClipContents(*node, NodeContents{firstBrush}, bounds));
    node->setBrush(initialBrush);
    CHECK(scene.serialize(scene.world()) == initial);
  }
}

TEST_CASE("VmfScene Both stages provenance siblings and keeper identities once", "[vmf]")
{
  const auto bounds = vm::bbox3d{4096};
  const auto original = fixture(
    false,
    "// retained literal side data\r\n\"lightmapscale\" \"00032\" "
    "\"smoothing_groups\" \"0017\" \"smoothing_groups\" \"4\" "
    "editor { \"color\" \"2 4 6\" }");
  auto scene = VmfScene::read(original, bounds);
  auto& node = brush(scene.world());
  const auto before = node.brush();
  const auto half = [&](const bool back) {
    auto result = before;
    auto face = BrushFace::create(
      {32, 0, 0},
      back ? vm::vec3d{32, 64, 0} : vm::vec3d{32, 0, 64},
      back ? vm::vec3d{32, 0, 64} : vm::vec3d{32, 64, 0},
      "synthetic/cut",
      UvAttributes{},
      SurfaceAttributes{},
      MapFormat::SourceVMF);
    REQUIRE(face.is_success());
    REQUIRE(result.clip(bounds, std::move(face).value()).is_success());
    return result;
  };
  auto sibling = std::unique_ptr<BrushNode>{static_cast<BrushNode*>(node.clone(bounds))};
  sibling->setBrush(half(true));
  const auto backBefore = sibling->brush();
  auto keepers =
    std::vector<std::pair<Node*, NodeContents>>{{&node, NodeContents{half(false)}}};
  auto siblings = std::vector<BrushNode*>{sibling.get()};

  const auto refuseFirst = GENERATE(false, true);
  CAPTURE(refuseFirst);
  if (refuseFirst)
  {
    auto other = std::unique_ptr<BrushNode>{static_cast<BrushNode*>(node.clone(bounds))};
    other->setBrush(half(true));
    keepers.push_back(keepers.front());
    siblings.push_back(other.get());
    CHECK_THROWS_AS(
      scene.registerBothClips({&node}, keepers, siblings, bounds), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == original);
    CHECK(sibling->lineNumber() == node.lineNumber());
    CHECK(
      std::ranges::any_of(
        std::get<Brush>(keepers.front().second.get()).faces(),
        [](const auto& face) { return !face.lineNumber(); }));
    keepers.pop_back();
    siblings.pop_back();
    auto bad = std::get<Brush>(keepers.front().second.get());
    auto& survivor = *std::ranges::find_if(
      bad.faces(), [](const auto& face) { return face.lineNumber() != 0; });
    survivor.setMaterialName("synthetic/forged");
    auto forged = std::vector<std::pair<Node*, NodeContents>>{{&node, NodeContents{bad}}};
    CHECK_THROWS_AS(
      scene.registerBothClips({&node}, forged, siblings, bounds), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == original);
  }
  scene.registerBothClips({&node}, keepers, siblings, bounds);
  CHECK(scene.nativeId(*sibling) == "10");
  const auto front = std::get<Brush>(keepers.front().second.get());
  const auto savedBack = sibling->brush();
  node.setBrush(front);
  auto* back = sibling.release();
  node.parent()->addChild(back);
  const auto saved = scene.serialize(scene.world());
  CHECK(saved.find("side\n{\n\"id\" \"9\"") != std::string::npos);
  auto objectIds = std::set<std::string>{};
  auto sideIds = std::set<std::string>{};
  const auto reparsed = VmfDocument::parse(saved);
  for (const auto& entry : reparsed.entries())
  {
    if (entry.value && reparsed.tokenContents(entry.key) == "id")
    {
      REQUIRE(entry.parent);
      const auto side =
        reparsed.tokenContents(reparsed.entries()[*entry.parent].key) == "side";
      auto& ids = side ? sideIds : objectIds;
      CHECK(ids.insert(std::string{reparsed.tokenContents(*entry.value)}).second);
    }
  }
  for (const auto& face : node.brush().faces())
  {
    if (face.lineNumber() <= scene.document().entries().size())
    {
      CHECK(
        saved.find(scene.document().serializeEntry(face.lineNumber() - 1))
        != std::string::npos);
    }
  }
  for (size_t i = 0; i < backBefore.faceCount(); ++i)
  {
    const auto& sourceFace = backBefore.faces()[i];
    if (sourceFace.lineNumber())
    {
      const auto source = sourceFace.lineNumber() - 1;
      auto idEntry = VmfEntryId{};
      for (const auto child : scene.document().entries()[source].children)
      {
        const auto& entry = scene.document().entries()[child];
        if (entry.value && scene.document().tokenContents(entry.key) == "id")
        {
          idEntry = child;
        }
      }
      // IDs are not exposed for faces; check the emitted side by its allocated
      // sequence, which is deterministic and independent of plane matching.
      const auto generatedId = std::to_string(10 + i);
      const auto edits = std::vector<VmfValueEdit>{{idEntry, "\"" + generatedId + "\""}};
      const auto expected = scene.document().serializeEntry(source, edits);
      CHECK(saved.find(expected) != std::string::npos);
    }
  }
  CHECK_NOTHROW(VmfScene::read(saved, bounds));
  auto* parent = back->parent();
  parent->removeChild(back);
  sibling.reset(back);
  node.setBrush(before);
  CHECK(scene.serialize(scene.world()) == original);
  CHECK_NOTHROW(scene.validateBothClipReplay({&node}, keepers, siblings, bounds));
  auto tampered = sibling->brush();
  tampered.faces().front().setFilePosition(node.brush().faces().front().lineNumber(), 1);
  sibling->setBrush(std::move(tampered));
  CHECK_THROWS_AS(
    scene.validateBothClipReplay({&node}, keepers, siblings, bounds),
    std::invalid_argument);
  sibling->setBrush(savedBack);
  CHECK_NOTHROW(scene.validateBothClipReplay({&node}, keepers, siblings, bounds));
  parent->addChild(sibling.release());
  node.setBrush(front);
  CHECK(scene.serialize(scene.world()) == saved);
}

TEST_CASE(
  "VmfScene Both native identity length budgets refuse before publication", "[vmf]")
{
  const auto objectBudget = GENERATE(false, true);
  const auto exhausted = GENERATE(false, true);
  CAPTURE(objectBudget, exhausted);
  const auto bounds = vm::bbox3d{4096};
  const auto huge = std::string(exhausted ? 4096 : 4095, '9');
  auto source = fixture();
  // Recognized editor payload reserves namespaces without becoming opaque geometry.
  const auto at = source.find("editor { \"color\"");
  REQUIRE(at != std::string::npos);
  source.insert(
    at + 9,
    objectBudget ? "solid { id \"" + huge + "\" } " : "side { id \"" + huge + "\" } ");
  auto scene = VmfScene::read(source, bounds);
  auto& node = brush(scene.world());
  const auto half = [&](const bool back) {
    auto result = node.brush();
    auto face = BrushFace::create(
      {32, 0, 0},
      back ? vm::vec3d{32, 64, 0} : vm::vec3d{32, 0, 64},
      back ? vm::vec3d{32, 0, 64} : vm::vec3d{32, 64, 0},
      "synthetic/cut",
      UvAttributes{},
      SurfaceAttributes{},
      MapFormat::SourceVMF);
    REQUIRE(face.is_success());
    REQUIRE(result.clip(bounds, std::move(face).value()).is_success());
    return result;
  };
  auto sibling = std::unique_ptr<BrushNode>{static_cast<BrushNode*>(node.clone(bounds))};
  sibling->setBrush(half(true));
  auto keepers =
    std::vector<std::pair<Node*, NodeContents>>{{&node, NodeContents{half(false)}}};
  const auto originalBinding = sibling->lineNumber();
  if (exhausted)
  {
    CHECK_THROWS_AS(
      scene.registerBothClips({&node}, keepers, {sibling.get()}, bounds),
      std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == source);
    CHECK(sibling->lineNumber() == originalBinding);
    CHECK(
      std::ranges::any_of(
        std::get<Brush>(keepers.front().second.get()).faces(),
        [](const auto& face) { return !face.lineNumber(); }));
    if (objectBudget)
    {
      // A cut side was staged before the sibling object ID overflow. Ordinary
      // clipping still gets side 9, proving no partial counter publication.
      scene.registerClips(keepers, bounds);
      node.setBrush(std::get<Brush>(keepers.front().second.get()));
      CHECK(
        scene.serialize(scene.world()).find("side\n{\n\"id\" \"9\"")
        != std::string::npos);
    }
  }
  else
  {
    CHECK_NOTHROW(scene.registerBothClips({&node}, keepers, {sibling.get()}, bounds));
    node.setBrush(std::get<Brush>(keepers.front().second.get()));
    node.parent()->addChild(sibling.release());
    const auto saved = scene.serialize(scene.world());
    CHECK(saved.find("\"id\" \"1" + std::string(4095, '0') + "\"") != std::string::npos);
    CHECK_NOTHROW(VmfScene::read(saved, bounds));
  }
}

TEST_CASE("VmfScene Both combined ID byte budget is atomic after staged halves", "[vmf]")
{
  const auto bounds = vm::bbox3d{4096};
  auto source = fixture();
  const auto at = source.find("editor { \"color\"");
  REQUIRE(at != std::string::npos);
  source.insert(at + 9, "side { id \"" + std::string(4095, '9') + "\" } ");
  auto scene = VmfScene::read(source, bounds);
  auto& node = brush(scene.world());
  auto owned = std::vector<std::unique_ptr<BrushNode>>{};
  auto roots = std::vector<Node*>{};
  // 4,086 generated sides at 4,096 ID bytes each leave room for one Both
  // output pair, but not two. This reaches the real production byte guard
  // without a private budget override or a million-object fixture.
  for (size_t i = 0; i < 681; ++i)
  {
    owned.emplace_back(static_cast<BrushNode*>(node.clone(bounds)));
    roots.push_back(owned.back().get());
  }
  scene.validateAdditions(*node.parent(), roots, true);
  scene.registerAdditions(roots, true);
  for (auto& root : owned)
  {
    node.parent()->addChild(root.release());
  }
  const auto before = scene.serialize(scene.world());
  auto* other = static_cast<BrushNode*>(roots.front());
  auto keepers = std::vector<std::pair<Node*, NodeContents>>{};
  auto siblings = std::vector<std::unique_ptr<BrushNode>>{};
  for (auto* target : {&node, other})
  {
    const auto half = [&](const bool back) {
      auto result = target->brush();
      auto cut = BrushFace::create(
        {32, 0, 0},
        back ? vm::vec3d{32, 64, 0} : vm::vec3d{32, 0, 64},
        back ? vm::vec3d{32, 0, 64} : vm::vec3d{32, 64, 0},
        "synthetic/cut",
        UvAttributes{},
        SurfaceAttributes{},
        MapFormat::SourceVMF);
      REQUIRE(cut.is_success());
      REQUIRE(result.clip(bounds, std::move(cut).value()).is_success());
      return result;
    };
    keepers.emplace_back(target, NodeContents{half(false)});
    auto sibling =
      std::unique_ptr<BrushNode>{static_cast<BrushNode*>(target->clone(bounds))};
    sibling->setBrush(half(true));
    siblings.push_back(std::move(sibling));
  }
  CHECK_THROWS_AS(
    scene.registerBothClips(
      {&node, other}, keepers, {siblings[0].get(), siblings[1].get()}, bounds),
    std::invalid_argument);
  CHECK(scene.serialize(scene.world()) == before);
  CHECK(siblings[0]->lineNumber() == node.lineNumber());
  CHECK(
    std::ranges::any_of(
      std::get<Brush>(keepers.front().second.get()).faces(),
      [](const auto& face) { return !face.lineNumber(); }));
  keepers.pop_back();
  CHECK_NOTHROW(scene.registerClips(keepers, bounds));
  node.setBrush(std::get<Brush>(keepers.front().second.get()));
  const auto expectedId = "1" + std::string(4091, '0') + "4086";
  CHECK(
    scene.serialize(scene.world()).find("\"id\" \"" + expectedId + "\"")
    != std::string::npos);
}

TEST_CASE("VmfScene native group admission registered detached container", "[vmf]")
{
  const auto generated = GENERATE(false, true);
  const auto source = generated ? std::string{"world { id 1 classname worldspawn }"}
                                : std::string{
                                    "world { id 1 classname worldspawn group { id 10 "
                                    "editor { color \"1 2 3\" } } }"};
  auto scene = VmfScene::read(source, vm::bbox3d{4096});
  auto* layer = scene.world().defaultLayer();
  auto owner = std::unique_ptr<GroupNode>{};
  if (generated)
  {
    owner = std::make_unique<GroupNode>(Group{"staged"});
    scene.registerGroupChanges(owner.get(), {{owner.get(), layer}});
    layer->addChild(owner.release());
  }
  auto* group = dynamic_cast<GroupNode*>(layer->children().front());
  REQUIRE(group);
  CHECK_THROWS_AS(scene.validateRegisteredDetachedGroup(*group), std::invalid_argument);
  layer->removeChild(group);
  owner.reset(group);
  const auto before = scene.serialize(scene.world());
  const auto binding = group->lineNumber();
  const auto link = group->linkId();
  const auto nativeId = scene.nativeId(*group);
  REQUIRE_NOTHROW(scene.validateRegisteredDetachedGroup(*group));
  // Detached admission must not weaken the strictly live closure API.
  CHECK_THROWS_AS(scene.validateGroupClosure({group}), std::invalid_argument);
  for (const auto state : {VisibilityState::Hidden, VisibilityState::Shown})
  {
    group->setVisibilityState(state);
    CHECK_THROWS_AS(scene.validateRegisteredDetachedGroup(*group), std::invalid_argument);
  }
  group->setVisibilityState(VisibilityState::Inherited);
  group->setLinkId("changed-parked-link");
  CHECK_THROWS_AS(scene.validateRegisteredDetachedGroup(*group), std::invalid_argument);
  group->setLinkId(link);
  for (const auto wrong : {size_t{0}, binding + 1})
  {
    group->setFilePosition(wrong, 1);
    CHECK_THROWS_AS(scene.validateRegisteredDetachedGroup(*group), std::invalid_argument);
  }
  group->setFilePosition(binding, 1);
  auto foreign = GroupNode{Group{"foreign"}};
  CHECK_THROWS_AS(scene.validateRegisteredDetachedGroup(foreign), std::invalid_argument);
  foreign.setFilePosition(binding, 1);
  foreign.setLinkId(link);
  CHECK_THROWS_AS(scene.validateRegisteredDetachedGroup(foreign), std::invalid_argument);
  REQUIRE_NOTHROW(scene.validateRegisteredDetachedGroup(*group));
  CHECK(scene.nativeId(*group) == nativeId);
  CHECK(group->lineNumber() == binding);
  CHECK(group->parent() == nullptr);
  CHECK(scene.serialize(scene.world()) == before);
  CHECK(scene.document().serialize() == source);
  auto control = std::make_unique<GroupNode>(Group{"control"});
  scene.registerGroupChanges(control.get(), {{control.get(), layer}});
  CHECK(scene.nativeId(*control) == (generated ? "3" : "11"));
  auto* other = control.get();
  const auto otherLink = other->linkId();
  layer->addChild(control.release());
  other->setLinkId(link);
  CHECK_THROWS_AS(scene.validateRegisteredDetachedGroup(*group), std::invalid_argument);
  other->setLinkId(otherLink);
  REQUIRE_NOTHROW(scene.validateRegisteredDetachedGroup(*group));
  layer->addChild(owner.release());
}

TEST_CASE(
  "VmfScene native group admission unsupported and preview detached containers", "[vmf]")
{
  const auto source = std::string{
    "world { id 1 classname worldspawn group { id 10 editor { visgroupshown 0 } } group "
    "{ id 30 } }"};
  auto scene = VmfScene::read(source, vm::bbox3d{4096});
  const auto metadata = SourceEditorMetadata::read(scene.document());
  REQUIRE(metadata.groups.size() == 2);
  auto unprojected = GroupNode{Group{"foreign"}};
  unprojected.setFilePosition(metadata.groups.front().entry + 1, 1);
  CHECK_THROWS_AS(
    scene.validateRegisteredDetachedGroup(unprojected), std::invalid_argument);
  CHECK(scene.serialize(scene.world()) == source);
  auto preview = VmfScene::readReadOnlyPreview(source, vm::bbox3d{4096});
  auto* parent = preview.world().defaultLayer();
  REQUIRE(parent->childCount() == 1);
  auto* group = dynamic_cast<GroupNode*>(parent->children().front());
  REQUIRE(group);
  parent->removeChild(group);
  auto owner = std::unique_ptr<GroupNode>{group};
  CHECK_THROWS_AS(preview.validateRegisteredDetachedGroup(*group), std::invalid_argument);
  parent->addChild(owner.release());
  CHECK(preview.serialize(preview.world()) == source);
}

TEST_CASE(
  "NativeCordon scene geometry guards and native group namespace remain independent",
  "[vmf]")
{
  const auto bounds = vm::bbox3d{131072.0};
  const auto legacy = GENERATE(false, true);
  const auto controller =
    legacy ? std::string{"cordon { mins \"(0 0 0)\" maxs \"(64 64 64)\" }"}
           : std::string{
               "cordons { cordon { box { mins \"(0 0 0)\" maxs \"(64 64 64)\" } } }"};
  const auto source = fixture() + controller;
  auto scene = VmfScene::read(source, bounds);
  auto control = VmfScene::read(source, bounds);
  auto& node = brush(scene.world());
  const auto binding = node.lineNumber();
  auto* layer = scene.world().defaultLayer();
  const auto siblings = layer->children();
  node.select();
  auto request = NativeCordonRequest{scene.cordons()};
  request.records[0].boxes[0].maxs = {80, 90, 100};
  request.records[0].active = true;
  auto change = scene.prepareCordonChange(request);
  REQUIRE(change);
  scene.applyCordonChange(*change, NativeCordonDirection::Forward);
  auto invalid = NativeCordonRequest{scene.cordons()};
  invalid.records[0].boxes[0].maxs[0] = -1;
  CHECK_THROWS_AS(scene.prepareCordonChange(invalid), std::invalid_argument);
  CHECK(node.selected());
  CHECK(node.lineNumber() == binding);
  CHECK(layer->children() == siblings);
  CHECK(node.brush().bounds() == vm::bbox3d{{0, 0, 0}, {64, 64, 64}});
  CHECK(node.visibilityState() == VisibilityState::Inherited);
  if (legacy)
  {
    CHECK_THROWS_AS(scene.validateRemoval({&node}), std::invalid_argument);
    CHECK_THROWS_AS(
      control.validateRemoval({&brush(control.world())}), std::invalid_argument);
  }
  else
  {
    CHECK_NOTHROW(scene.validateRemoval({&node}));
    auto group = std::make_unique<GroupNode>(Group{"new"});
    auto matched = std::make_unique<GroupNode>(Group{"control"});
    scene.registerGroupChanges(group.get(), {{group.get(), layer}});
    control.registerGroupChanges(
      matched.get(), {{matched.get(), control.world().defaultLayer()}});
    CHECK(scene.nativeId(*group) == control.nativeId(*matched));
    layer->addChild(group.release());
    control.world().defaultLayer()->addChild(matched.release());
  }
  scene.applyCordonChange(*change, NativeCordonDirection::Backward);
  CHECK(scene.serialize(scene.world()) == control.serialize(control.world()));
}

TEST_CASE(
  "NativeCordon scene preserves hidden opaque and saved metadata authority", "[vmf]")
{
  const auto bounds = vm::bbox3d{131072.0};
  const auto source = fixture(false, "future_side { data retained }", true)
    + "visgroups { visgroup { name \"preserved\" visgroupid 100 color \"1 2 3\" } }\r\n"
      "future { output first output second }\r\n"
      "cordons { active 0 cordon { name \"saved\" box { mins \"(0 0 0)\" maxs \"(72 83 94)\" } } }";
  auto scene = VmfScene::read(source, bounds);
  auto& node = brush(scene.world());
  REQUIRE(node.visibilityState() == VisibilityState::Hidden);
  CHECK_THROWS_AS(scene.validateRemoval({&node}), std::invalid_argument);
  auto request = NativeCordonRequest{scene.cordons()};
  request.records[0].boxes[0].maxs = {172, 183, 194};
  request.records[0].name = "live";
  auto change = scene.prepareCordonChange(request);
  REQUIRE(change);
  scene.applyCordonChange(*change, NativeCordonDirection::Forward);
  auto expected = source;
  expected.replace(
    expected.find("(72 83 94)"), std::string{"(72 83 94)"}.size(), "(172 183 194)");
  expected.replace(
    expected.find("\"saved\""), std::string{"\"saved\""}.size(), "\"live\"");
  CHECK(scene.serialize(scene.world()) == expected);
  CHECK(node.visibilityState() == VisibilityState::Hidden);
  CHECK_THROWS_AS(scene.validateRemoval({&node}), std::invalid_argument);
  const auto metadata = SourceEditorMetadata::read(scene.document());
  REQUIRE(metadata.cordons.size() == 1);
  CHECK(metadata.cordons[0].name == "saved");
  CHECK(scene.cordons().records[0].name == "live");
  CHECK(scene.document().source() == source);
  scene.applyCordonChange(*change, NativeCordonDirection::Backward);
  CHECK(scene.serialize(scene.world()) == source);
}

} // namespace tb::mdl
