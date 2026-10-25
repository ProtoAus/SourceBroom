/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SourceDisplacementFixture.h"
#include "fs/TestEnvironment.h"
#include "mdl/AddRemoveNodesCommand.h"
#include "mdl/Brush.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/CatchConfig.h"
#include "mdl/EditorContext.h"
#include "mdl/EntityDefinition.h"
#include "mdl/EntityNode.h"
#include "mdl/ExportOptions.h"
#include "mdl/GameConfig.h"
#include "mdl/Hit.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_Brushes.h"
#include "mdl/Map_CopyPaste.h"
#include "mdl/Map_Entities.h"
#include "mdl/Map_Geometry.h"
#include "mdl/Map_NodeVisibility.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Picking.h"
#include "mdl/Map_Selection.h"
#include "mdl/Node.h"
#include "mdl/NodeContents.h"
#include "mdl/PasteType.h"
#include "mdl/PickResult.h"
#include "mdl/SwapNodeContentsCommand.h"
#include "mdl/UndoableCommand.h"
#include "mdl/VmfDocument.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"

#include "vm/mat.h"
#include "vm/mat_ext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::mdl
{
namespace
{
// Newly authored native ramp, not a user map or a Quake MAP conversion. Repeated
// outputs and opaque bytes make an accidental scene-only re-export observable.
std::string nativeRamp(const bool fractional, const bool globalExtension = false)
{
  const auto planes = std::array<std::string, 6>{
    "(0 0 0) (0 64 0) (0 0 64)",
    "(64 0 0) (64 0 64) (64 64 0)",
    "(0 0 0) (0 0 64) (64 0 0)",
    "(0 64 0) (64 64 0) (0 64 64)",
    "(0 0 0) (64 0 0) (0 64 0)",
    fractional ? "(0 0 32.0001) (0 64 32.0001) (64 0 64.0001)"
               : "(0 0 32) (0 64 32) (64 0 64)"};
  auto bytes = std::string{
    "\xEF\xBB\xBF// authored Map integration ramp; retain CRLF and decimals\r\n"
    "versioninfo { \"formatversion\" \"100\" }\r\n"
    "world { \"id\" \"1\" \"classname\" \"worldspawn\"\r\n"
    "solid { \"id\" \"2\"\r\n"};
  for (size_t i = 0; i < planes.size(); ++i)
  {
    bytes += "side { \"id\" \"" + std::to_string(i + 3) + "\" \"plane\" \""
      + planes[i]
      + "\" \"material\" \"synthetic/map_ramp\""
        " \"uaxis\" \"[1 0 0 0.1250] 0.2500\""
        " \"vaxis\" \"[0 -1 0 -0.5000] 0.2500\""
        " \"rotation\" \"0\" \"lightmapscale\" \"16\""
        " \"smoothing_groups\" \"0\" }\r\n";
  }
  bytes +=
    "}\r\neditor { \"color\" \"0 128 255\" } }\r\n"
    "entity { \"id\" \"9\" \"classname\" \"info_target\""
    " \"targetname\" \"untouched_marker\" \"origin\" \"8.000 16.00 24.0\"\r\n"
    "connections { \"OnUser1\" \"marker,Enable,,0,-1\""
    " \"OnUser1\" \"marker,Disable,,1,-1\" } }\r\n";
  if (globalExtension)
  {
    bytes += "sourcebroom_test_extension { \"opaque\" \"untouched bytes\" }\r\n";
  }
  return bytes;
}

std::string nativeDisplacementRoom()
{
  auto original = nativeRamp(false);
  const auto plane = std::string{"(0 0 32) (0 64 32) (64 0 64)"};
  const auto topPlane = original.find(plane);
  REQUIRE(topPlane != std::string::npos);
  original.replace(topPlane, plane.size(), "(0 0 64) (0 64 64) (64 0 64)");
  const auto topSide = original.find("\"id\" \"8\"");
  REQUIRE(topSide != std::string::npos);
  const auto field = original.find("\"smoothing_groups\" \"0\"", topSide);
  REQUIRE(field != std::string::npos);
  original.insert(field, test::displacementBlock());
  return original;
}

// TestEnvironment owns cleanup, but its text-mode createFile/loadFile translate
// CRLF on Windows. Preservation assertions must exercise binary disk I/O.
void writeBinary(const std::filesystem::path& path, const std::string_view bytes)
{
  auto stream = std::ofstream{path, std::ios::binary | std::ios::trunc};
  REQUIRE(stream.is_open());
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  stream.close();
  REQUIRE_FALSE(stream.fail());
}

std::string readBinary(const std::filesystem::path& path)
{
  auto stream = std::ifstream{path, std::ios::binary};
  REQUIRE(stream.is_open());
  auto bytes = std::string{std::istreambuf_iterator<char>{stream}, {}};
  REQUIRE_FALSE(stream.bad());
  return bytes;
}

BrushNode& ramp(Map& map)
{
  auto* layer = map.worldNode().defaultLayer();
  REQUIRE(layer->childCount() == 2u);
  for (auto* child : layer->children())
  {
    if (auto* brush = dynamic_cast<BrushNode*>(child))
    {
      return *brush;
    }
  }
  throw std::logic_error{"Native ramp brush is absent"};
}

using BindingPosition = std::array<size_t, 4>;
void collectFilePositions(const Node& node, std::vector<BindingPosition>& positions)
{
  positions.push_back(
    {0,
     node.lineNumber(),
     node.lineCount(),
     node.parent() ? node.parent()->lineNumber() : 0});
  if (const auto* brush = dynamic_cast<const BrushNode*>(&node))
  {
    for (const auto& face : brush->brush().faces())
    {
      positions.push_back({1, face.lineNumber(), 0, node.lineNumber()});
    }
  }
  for (const auto* child : node.children())
  {
    collectFilePositions(*child, positions);
  }
}

std::vector<BindingPosition> filePositions(const Map& map)
{
  auto positions = std::vector<BindingPosition>{};
  collectFilePositions(map.worldNode(), positions);
  // Undo may reinsert siblings in a different display order. Native identity,
  // parentage and binding positions, not display traversal order, are the contract.
  std::ranges::sort(positions);
  return positions;
}

// Compare native state rather than material-reference identity across independent
// MapFixtures, whose resource managers may own different fallback materials.
void checkReopenedBrush(
  const Brush& actual, const Brush& expected, const bool bindings = true)
{
  REQUIRE(actual.faceCount() == expected.faceCount());
  CHECK(actual.vertexCount() == expected.vertexCount());
  for (size_t axis = 0; axis < 3; ++axis)
  {
    CHECK(
      actual.bounds().min[axis]
      == Catch::Approx(expected.bounds().min[axis]).margin(1e-9));
    CHECK(
      actual.bounds().max[axis]
      == Catch::Approx(expected.bounds().max[axis]).margin(1e-9));
  }
  for (size_t i = 0; i < expected.faceCount(); ++i)
  {
    const auto& face = actual.face(i);
    const auto& before = expected.face(i);
    CHECK(face.points() == before.points());
    CHECK(face.materialName() == before.materialName());
    CHECK(face.surfaceAttributes() == before.surfaceAttributes());
    CHECK(face.uvAttributes() == before.uvAttributes());
    CHECK(face.takeUvCoordSystemSnapshot() == before.takeUvCoordSystemSnapshot());
    if (bindings)
    {
      CHECK(face.lineNumber() == before.lineNumber());
    }
    CHECK(face.preservesPlanePrecision() == before.preservesPlanePrecision());
  }
}

Node& nativeNode(Map& map, const std::string& id)
{
  auto* found = static_cast<Node*>(nullptr);
  const auto visit = [&](auto&& self, Node& node) -> void {
    if (
      node.lineNumber() && !dynamic_cast<LayerNode*>(&node)
      && map.vmfScene()->nativeId(node) == id)
    {
      found = &node;
    }
    for (auto* child : node.children())
    {
      self(self, *child);
    }
  };
  visit(visit, map.worldNode());
  REQUIRE(found);
  return *found;
}

std::string nativeFragment(const std::string& source, const std::string& id)
{
  const auto document = VmfDocument::parse(source);
  for (size_t i = 0; i < document.entries().size(); ++i)
  {
    const auto& entry = document.entries()[i];
    if (entry.isBlock() && document.tokenContents(entry.key) != "side")
    {
      for (const auto child : entry.children)
      {
        const auto& property = document.entries()[child];
        if (
          property.value && document.tokenContents(property.key) == "id"
          && document.tokenContents(*property.value) == id)
        {
          return source.substr(entry.key.offset, entry.end - entry.key.offset);
        }
      }
    }
  }
  throw std::logic_error{"Native fragment is absent: " + id};
}

std::vector<vm::vec3d> nativeCubePoints()
{
  return {
    {-32, -32, -32},
    {32, -32, -32},
    {-32, 32, -32},
    {32, 32, -32},
    {-32, -32, 32},
    {32, -32, 32},
    {-32, 32, 32},
    {32, 32, 32}};
}

void checkOnlyPlaneTokensChanged(const std::string& original, const std::string& saved)
{
  const auto before = VmfDocument::parse(original);
  const auto after = VmfDocument::parse(saved);
  REQUIRE(before.entries().size() == after.entries().size());
  auto edits = std::vector<VmfValueEdit>{};
  for (size_t i = 0; i < before.entries().size(); ++i)
  {
    const auto& entry = before.entries()[i];
    if (entry.value && before.tokenContents(entry.key) == "plane")
    {
      REQUIRE(after.entries()[i].value);
      edits.push_back({i, std::string{after.tokenText(*after.entries()[i].value)}});
    }
  }
  REQUIRE(edits.size() == 6u);
  CHECK(before.serialize(edits) == saved);
}
} // namespace

TEST_CASE("VmfMap read-only compatibility safety", "[vmf]")
{
  // A false modification flag must not grant access to map data.
  class Probe : public UndoableCommand
  {
    int& m_calls;

  public:
    explicit Probe(int& calls)
      : UndoableCommand{"read-only probe", false}
      , m_calls{calls}
    {
    }
    bool doPerformDo(Map& map) override
    {
      ++m_calls;
      auto entity = map.worldNode().entity();
      entity.addOrUpdateProperty("readonly_probe", "must_not_act");
      map.worldNode().setEntity(std::move(entity));
      return true;
    }
    bool doPerformUndo(Map&) override
    {
      ++m_calls;
      return true;
    }
  };
  const auto duplicateIds = GENERATE(false, true);
  auto original = nativeRamp(false);
  if (duplicateIds)
  {
    const auto at = original.find("\"id\" \"9\"");
    REQUIRE(at != std::string::npos);
    original.replace(at, std::string{"\"id\" \"9\""}.size(), "\"id\" \"0002\"");
  }
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "read-only source.vmf";
  writeBinary(path, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path,
    MapFixtureConfig{
      .mapFormat = MapFormat::SourceVMF,
      .worldBounds = vm::bbox3d{4096},
      .readOnlyVmfPreview = true,
    });
  REQUIRE(map.isReadOnlyVmfPreview());
  REQUIRE(map.vmfScene());
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  CHECK_FALSE(map.modified());
  auto& node = ramp(map);
  const auto before = node.brush();
  const auto positions = filePositions(map);
  selectNodes(map, {&node});
  REQUIRE(node.selected());
  REQUIRE(map.canUndoCommand());
  map.undoCommand();
  CHECK_FALSE(node.selected());
  map.redoCommand();
  REQUIRE(node.selected());
  CHECK_FALSE(map.modified());
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  const auto history = *map.undoCommandName();
  const auto count = map.modificationCount();
  auto calls = 0;
  Probe direct{calls};
  CHECK_FALSE(direct.isModification());
  CHECK_FALSE(direct.performDo(map));
  CHECK_FALSE(direct.performUndo(map));
  CHECK_FALSE(map.executeAndStore(std::make_unique<Probe>(calls)));
  CHECK(calls == 0);
  CHECK_FALSE(map.worldNode().entity().hasProperty("readonly_probe"));
  CHECK_FALSE(setEntityProperty(map, "targetname", "not_allowed"));
  CHECK_FALSE(transformSelection(
    map, "Preview transform", vm::translation_matrix(vm::vec3d{16, 8, 4})));
  hideNodes(map, {&node});
  CHECK(node.visibilityState() == VisibilityState::Inherited);
  checkReopenedBrush(node.brush(), before);
  CHECK(filePositions(map) == positions);
  CHECK(map.modificationCount() == count);
  REQUIRE(map.undoCommandName());
  CHECK(*map.undoCommandName() == history);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);

  const auto copy = env.dir() / "unchanged native copy.vmf";
  CHECK(map.save().is_error());
  CHECK(map.saveAs(copy).is_error());
  CHECK(map.saveTo(copy).is_error());
  CHECK_FALSE(std::filesystem::exists(copy));
  CHECK(
    map.exportAs(MapExportOptions{env.dir() / "partial.map", false, {}, {}}).is_error());
  CHECK(
    map
      .exportAs(
        ObjExportOptions{env.dir() / "partial.obj", ObjMtlPathMode::RelativeToExportPath})
      .is_error());
  CHECK_FALSE(std::filesystem::exists(env.dir() / "partial.map"));
  CHECK_FALSE(std::filesystem::exists(env.dir() / "partial.obj"));
  CHECK(map.saveOriginalVmfCopy(path).is_error());
  REQUIRE(map.saveOriginalVmfCopy(copy).is_success());
  CHECK(readBinary(copy) == original);
  CHECK(readBinary(path) == original);
  CHECK(map.path() == path);
  CHECK(map.vmfScene()->document().source() == original);
  CHECK(map.modificationCount() == count);
  CHECK_FALSE(map.modified());
  CHECK(map.saveOriginalVmfCopy(copy).is_error());
  CHECK(readBinary(copy) == original);
  auto reloaded = map.reload() | kdl::value();
  REQUIRE(reloaded->isReadOnlyVmfPreview());
  CHECK(reloaded->vmfScene()->serialize(reloaded->worldNode()) == original);
  CHECK(reloaded->save().is_error());

  // A low-level caller bypassing command admission still cannot save a delta.
  auto changed = map.worldNode().entity();
  changed.addOrUpdateProperty("bypass_probe", "must_not_save");
  map.worldNode().setEntity(std::move(changed));
  const auto forbidden = env.dir() / "must not exist.vmf";
  CHECK(map.saveOriginalVmfCopy(forbidden).is_error());
  CHECK_FALSE(std::filesystem::exists(forbidden));
  CHECK(readBinary(path) == original);

  auto controlFixture = MapFixture{};
  auto& control = controlFixture.create();
  Probe acted{calls};
  REQUIRE(acted.performDo(control));
  CHECK(calls == 1);
  CHECK(control.worldNode().entity().hasProperty("readonly_probe"));
  REQUIRE(acted.performUndo(control));
  CHECK(calls == 2);
}

TEST_CASE("VmfMap displacement rotation undo and continued saving", "[vmf]")
{
  const auto lockUv = GENERATE(false, true);
  const auto angle = GENERATE(vm::constants<double>::half_pi(), 0.37);
  auto env = fs::TestEnvironment{};
  const auto originalPath = env.dir() / "displacement.vmf";
  const auto outputPath = env.dir() / "rotated displacement.vmf";
  const auto original = nativeDisplacementRoom();
  writeBinary(originalPath, original);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(originalPath, config);
  map.editorContext().setAlignmentLock(lockUv);
  auto& node = ramp(map);
  const auto before = node.brush();
  const auto oldFace = before.findFace(vm::vec3d{0, 0, 1});
  REQUIRE(oldFace);
  const auto& oldMesh = *before.face(*oldFace).sourceDisplacement();
  REQUIRE(oldMesh.positions()[12] == vm::vec3d{32, 32, 80});
  const auto center = vm::vec3d{8, 16, 24};
  const auto axis = vm::vec3d{1, 0, 0};
  const auto transform = vm::translation_matrix(center) * vm::rotation_matrix(axis, angle)
                         * vm::translation_matrix(-center);
  auto control = before;
  control.face(*oldFace).setSourceDisplacement(std::nullopt);
  REQUIRE(control.transform(map.worldBounds(), transform, lockUv).is_success());
  selectNodes(map, {&node});
  const auto count = map.modificationCount();
  REQUIRE(rotateSelection(map, center, axis, angle));
  CHECK(map.modificationCount() == count + 1);
  const auto meshFace = [](const Brush& brush) {
    for (size_t i = 0; i < brush.faceCount(); ++i)
    {
      if (brush.face(i).sourceDisplacement())
      {
        return i;
      }
    }
    throw std::logic_error{"Rotated mesh disappeared"};
  };
  const auto faceIndex = meshFace(node.brush());
  const auto& mesh = *node.brush().face(faceIndex).sourceDisplacement();
  for (size_t i = 0; i < oldMesh.positions().size(); ++i)
  {
    CHECK(
      vm::is_equal(mesh.positions()[i], transform * oldMesh.positions()[i], 0.000001));
    CHECK(
      vm::is_equal(
        mesh.basePositions()[i], transform * oldMesh.basePositions()[i], 0.000001));
  }
  CHECK(mesh.indices() == oldMesh.indices());
  CHECK(mesh.alphas() == oldMesh.alphas());
  const auto base = mesh.basePositions()[12];
  CHECK(
    vm::is_equal(
      node.brush().face(faceIndex).uvCoords(base),
      control.face(faceIndex).uvCoords(base),
      0.0001f));
  auto hits = PickResult{};
  pick(
    map,
    vm::ray3d{
      transform * vm::vec3d{32, 32, 128},
      vm::slice<3, 3>(transform, 0, 0) * vm::vec3d{0, 0, -1}},
    hits);
  REQUIRE_FALSE(hits.empty());
  CHECK(hits.all()[0].distance() == Catch::Approx(48).margin(0.000001));
  CHECK(
    vm::is_equal(
      hits.all()[0].hitPoint(), transform * oldMesh.positions()[12], 0.000001));
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  map.redoCommand();
  const auto delta = vm::vec3d{16, 8, 4};
  REQUIRE(translateSelection(map, delta));
  deselectAll(map);
  selectBrushFaces(map, {BrushFaceHandle{node, faceIndex}});
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 24));
  const auto expected = transform * vm::vec3d{32, 32, 88} + delta;
  CHECK(
    vm::is_equal(
      node.brush().face(faceIndex).sourceDisplacement()->positions()[12],
      expected,
      0.000001));
  REQUIRE(map.saveAs(outputPath).is_success());
  const auto saved = readBinary(outputPath);
  CHECK(saved != original);
  CHECK(nativeFragment(saved, "9") == nativeFragment(original, "9"));
  CHECK(readBinary(originalPath) == original);
  auto reopened = MapFixture{};
  auto& loaded = reopened.load(outputPath, config);
  CHECK(
    vm::is_equal(
      ramp(loaded)
        .brush()
        .face(meshFace(ramp(loaded).brush()))
        .sourceDisplacement()
        ->positions()[12],
      expected,
      0.000001));
  deselectAll(map);
  selectNodes(map, {&node});
  const auto beforeCount = map.modificationCount();
  const auto beforeHistory = map.undoCommandName();
  CHECK_FALSE(rotateSelection(map, center, {2, 0, 0}, 0.37));
  CHECK_FALSE(scaleSelection(map, {0, 0, 0}, {1.0000000000001, 1, 1}));
  CHECK_FALSE(scaleSelection(map, {0, 0, 0}, {2, 1, 1}));
  CHECK_FALSE(flipSelection(map, {0, 0, 0}, vm::axis::x));
  CHECK(map.modificationCount() == beforeCount);
  CHECK(map.undoCommandName() == beforeHistory);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
  deselectAll(map);
  selectBrushFaces(map, {BrushFaceHandle{node, faceIndex}});
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 32));
  REQUIRE(map.save().is_success());
  CHECK(readBinary(outputPath) != saved);
  map.undoCommand();
  REQUIRE(map.save().is_success());
  CHECK(readBinary(outputPath) == saved);
  CHECK(readBinary(originalPath) == original);
}

TEST_CASE("VmfMap untyped or overflowing native vectors refuse rotation", "[vmf]")
{
  const auto overflow = GENERATE(false, true);
  const auto row = overflow ? std::string{"1.7e308 -1.7e308 0 0 0 0 0 0 0 0 0 0 0 0 0"}
                            : std::string{"unbound"};
  auto original = nativeDisplacementRoom();
  const auto owner = std::string{"dispinfo {"};
  const auto at = original.find(owner);
  REQUIRE(at != std::string::npos);
  original.insert(at + owner.size(), " offset_normals { row0 \"" + row + "\" } ");
  auto env = fs::TestEnvironment{};
  const auto originalPath = env.dir() / "untyped displacement.vmf";
  writeBinary(originalPath, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    originalPath,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto& node = ramp(map);
  selectNodes(map, {&node});
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  REQUIRE(translateSelection(map, {16, 8, 4}));
  const auto translated = map.vmfScene()->serialize(map.worldNode());
  CHECK(translated != original);
  CHECK(translated.find(row) != std::string::npos);
  const auto count = map.modificationCount();
  const auto history = map.undoCommandName();
  REQUIRE_FALSE(
    rotateSelection(map, {0, 0, 0}, {0, 0, 1}, vm::constants<double>::quarter_pi()));
  CHECK(map.modificationCount() == count);
  CHECK(map.undoCommandName() == history);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == translated);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  CHECK(readBinary(originalPath) == original);
}

TEST_CASE("VmfMap displacement translation undo and continued saving", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto originalPath = env.dir() / "displacement.vmf";
  const auto outputPath = env.dir() / "moved displacement.vmf";
  const auto original = nativeDisplacementRoom();
  writeBinary(originalPath, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    originalPath,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto& node = ramp(map);
  selectNodes(map, {&node});
  const auto count = map.modificationCount();
  REQUIRE(translateSelection(map, {16, 8, 4}));
  CHECK(map.modificationCount() == count + 1);
  CHECK(node.brush().bounds().min == vm::vec3d{16, 8, 4});
  CHECK(node.physicalBounds().max == vm::vec3d{80, 72, 84});
  auto hits = PickResult{};
  pick(map, vm::ray3d{{48, 40, 128}, {0, 0, -1}}, hits);
  REQUIRE_FALSE(hits.empty());
  CHECK(hits.all()[0].distance() == Catch::Approx(44));
  CHECK(hits.all()[0].hitPoint() == vm::vec3d{48, 40, 84});
  map.undoCommand();
  CHECK(node.physicalBounds().max.z() == 80);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  map.redoCommand();
  CHECK(node.physicalBounds().max.z() == 84);

  const auto before = map.vmfScene()->serialize(map.worldNode());
  const auto beforeCount = map.modificationCount();
  const auto beforeHistory = map.undoCommandName();
  CHECK_FALSE(transformSelection(
    map, "Unclassified native rotation", vm::rotation_matrix(vm::vec3d{0, 0, 1}, 1.0)));
  CHECK_FALSE(scaleSelection(map, {0, 0, 0}, {2, 1, 1}));
  CHECK_FALSE(flipSelection(map, {0, 0, 0}, vm::axis::x));
  auto shear = vm::mat4x4d::identity();
  shear[1][0] = 0.25;
  CHECK_FALSE(transformSelection(map, "Unsupported displacement shear", shear));
  CHECK_FALSE(translateSelection(map, {0, 0, 10000}));
  CHECK(map.modificationCount() == beforeCount);
  CHECK(map.undoCommandName() == beforeHistory);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == before);

  const auto face = node.brush().findFace(vm::vec3d{0, 0, 1});
  REQUIRE(face);
  deselectAll(map);
  selectBrushFaces(map, {BrushFaceHandle{node, *face}});
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 24));
  CHECK(node.physicalBounds().max.z() == 92);
  REQUIRE(map.saveAs(outputPath).is_success());
  const auto saved = readBinary(outputPath);
  CHECK(saved.find("[16 8 68]") != std::string::npos);
  CHECK(saved.find("+0  24  0") != std::string::npos);
  CHECK(saved.find("untouched_marker") != std::string::npos);
  CHECK(readBinary(originalPath) == original);
  auto reopened = MapFixture{};
  auto& loaded = reopened.load(
    outputPath,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  CHECK(ramp(loaded).physicalBounds().max == vm::vec3d{80, 72, 92});
  const auto reopenedFace = ramp(loaded).brush().findFace(vm::vec3d{0, 0, 1});
  REQUIRE(reopenedFace);
  CHECK(
    ramp(loaded).brush().face(*reopenedFace).sourceDisplacement()->positions()[12]
    == vm::vec3d{48, 40, 92});
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 32));
  REQUIRE(map.save().is_success());
  CHECK(readBinary(outputPath).find("+0  32  0") != std::string::npos);
  map.undoCommand();
  REQUIRE(map.save().is_success());
  CHECK(readBinary(outputPath) == saved);
  CHECK(readBinary(originalPath) == original);
}

TEST_CASE("VmfMap displacement distance undo and safe save", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto originalPath = env.dir() / "displacement.vmf";
  const auto outputPath = env.dir() / "displacement edited.vmf";
  const auto original = nativeDisplacementRoom();
  writeBinary(originalPath, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    originalPath,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto& node = ramp(map);
  const auto index = node.brush().findFace(vm::vec3d{0, 0, 1});
  REQUIRE(index);
  REQUIRE(node.brush().face(*index).sourceDisplacement());
  selectBrushFaces(map, {BrushFaceHandle{node, *index}});
  const auto count = map.modificationCount();
  auto hits = PickResult{};
  pick(map, vm::ray3d{{32, 32, 128}, {0, 0, -1}}, hits);
  REQUIRE_FALSE(hits.empty());
  CHECK(hits.all()[0].distance() == Catch::Approx(48));
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 24));
  CHECK(map.modificationCount() == count + 1);
  CHECK(node.physicalBounds().max.z() == 88);
  REQUIRE(map.undoCommandName());
  CHECK(*map.undoCommandName() == "Change Displacement Distance");
  hits = PickResult{};
  pick(map, vm::ray3d{{32, 32, 128}, {0, 0, -1}}, hits);
  REQUIRE_FALSE(hits.empty());
  CHECK(hits.all()[0].distance() == Catch::Approx(40));
  const auto changedCount = map.modificationCount();
  CHECK_FALSE(setSourceDisplacementDistance(map, 5, 2, 10));
  CHECK_FALSE(setSourceDisplacementDistance(map, 2, 2, 10000));
  CHECK(map.modificationCount() == changedCount);
  CHECK(node.brush().face(*index).sourceDisplacement()->distance(2, 2) == 24);
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 24));
  CHECK(map.modificationCount() == changedCount);
  map.undoCommand();
  CHECK(node.brush().face(*index).sourceDisplacement()->distance(2, 2) == 16);
  CHECK(node.physicalBounds().max.z() == 80);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  map.redoCommand();
  CHECK(node.brush().face(*index).sourceDisplacement()->distance(2, 2) == 24);
  REQUIRE(map.saveAs(outputPath).is_success());
  auto expected = original;
  const auto sample = expected.find("+0  16  0");
  REQUIRE(sample != std::string::npos);
  expected.replace(sample, 9, "+0  24  0");
  CHECK(readBinary(outputPath) == expected);
  CHECK(readBinary(originalPath) == original);
  auto reopened = MapFixture{};
  auto& loaded = reopened.load(
    outputPath,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  CHECK(ramp(loaded).brush().face(*index).sourceDisplacement()->distance(2, 2) == 24);
  CHECK(ramp(loaded).physicalBounds().max.z() == 88);
}

TEST_CASE("VmfMap", "[vmf]")
{
  const auto config = MapFixtureConfig{
    .mapFormat = MapFormat::SourceVMF,
    .worldBounds = vm::bbox3d{4096},
  };

  SECTION("native disk load no-op save and clipboard preserve bindings and bytes")
  {
    const auto fractional = GENERATE(false, true);
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "native ramp.vmf";
    const auto original = nativeRamp(fractional, true);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    REQUIRE(map.worldNode().mapFormat() == MapFormat::SourceVMF);
    CHECK(map.path() == path);
    CHECK(map.persistent());
    CHECK_FALSE(map.modified());
    const auto& brush = ramp(map).brush();
    CHECK(brush.faceCount() == 6u);
    CHECK(brush.vertexCount() == 8u);
    CHECK(brush.bounds().min == vm::vec3d{0, 0, 0});
    CHECK(brush.bounds().max.x() == 64.0);
    CHECK(brush.bounds().max.y() == 64.0);
    CHECK(
      brush.bounds().max.z() == Catch::Approx(fractional ? 64.0001 : 64.0).margin(1e-9));
    REQUIRE(map.save().is_success());
    CHECK(readBinary(path) == original);
    CHECK_FALSE(map.modified());

    selectNodes(map, {&ramp(map)});
    const auto positions = filePositions(map);
    const auto modificationCount = map.modificationCount();
    const auto clipboard = serializeSelectedNodes(map);
    CHECK_FALSE(clipboard.empty());
    CHECK(filePositions(map) == positions);
    CHECK(map.modificationCount() == modificationCount);
    CHECK_FALSE(map.modified());
    REQUIRE(map.save().is_success());
    CHECK(readBinary(path) == original);
    const auto copy = env.dir() / "clipboard control.vmf";
    REQUIRE(map.saveAs(copy).is_success());
    CHECK(readBinary(copy) == original);
    CHECK(readBinary(path) == original);
    CHECK(filePositions(map) == positions);
  }

  SECTION(
    "normal transform native Save As reopen undo and redo preserve exact invariants")
  {
    const auto fractional = GENERATE(false, true);
    const auto delta = GENERATE(vm::vec3d{16, 8, 4}, vm::vec3d{0.125, -0.5, 0.25});
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "original.vmf";
    const auto editedPath = env.dir() / "edited.vmf";
    const auto original = nativeRamp(fractional);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    REQUIRE(map.worldNode().mapFormat() == MapFormat::SourceVMF);
    auto& node = ramp(map);
    const auto before = node.brush();
    const auto positions = filePositions(map);
    selectNodes(map, {&node});
    map.editorContext().setAlignmentLock(false);
    const auto transform = vm::translation_matrix(delta);
    REQUIRE(transformSelection(map, "Translate native VMF ramp", transform));
    REQUIRE(map.modified());
    const auto moved = node.brush();
    REQUIRE(moved.faceCount() == before.faceCount());
    CHECK(moved.bounds().min == delta);
    CHECK(moved.bounds().max.x() == 64.0 + delta.x());
    CHECK(moved.bounds().max.y() == 64.0 + delta.y());
    CHECK(
      moved.bounds().max.z()
      == Catch::Approx((fractional ? 64.0001 : 64.0) + delta.z()).margin(1e-9));
    for (size_t i = 0; i < moved.faceCount(); ++i)
    {
      REQUIRE(moved.face(i).preservesPlanePrecision());
      for (size_t p = 0; p < before.face(i).points().size(); ++p)
      {
        CHECK(moved.face(i).points()[p] == transform * before.face(i).points()[p]);
      }
      CHECK(moved.face(i).uvAttributes() == before.face(i).uvAttributes());
      CHECK(
        moved.face(i).takeUvCoordSystemSnapshot()
        == before.face(i).takeUvCoordSystemSnapshot());
    }
    CHECK(filePositions(map) == positions);
    const auto clipboard = serializeSelectedNodes(map);
    CHECK_FALSE(clipboard.empty());
    CHECK(filePositions(map) == positions);
    REQUIRE(map.saveAs(editedPath).is_success());
    CHECK(map.path() == editedPath);
    CHECK_FALSE(map.modified());
    CHECK(readBinary(path) == original);
    const auto saved = readBinary(editedPath);
    REQUIRE(saved != original);
    checkOnlyPlaneTokensChanged(original, saved);

    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(editedPath, config);
    REQUIRE(reopened.worldNode().mapFormat() == MapFormat::SourceVMF);
    checkReopenedBrush(ramp(reopened).brush(), moved);
    CHECK(filePositions(reopened) == positions);
    REQUIRE(reopened.save().is_success());
    CHECK(readBinary(editedPath) == saved);

    map.undoCommand();
    CHECK(node.brush() == before);
    CHECK(filePositions(map) == positions);
    CHECK(map.modified()); // Disk still contains the saved transformed state.
    const auto undonePath = env.dir() / "undone.vmf";
    REQUIRE(map.saveTo(undonePath).is_success());
    CHECK(readBinary(undonePath) == original);
    CHECK(map.modified()); // saveTo must not change the save point or backing path.
    CHECK(map.path() == editedPath);
    map.redoCommand();
    CHECK(node.brush() == moved);
    CHECK(filePositions(map) == positions);
    CHECK_FALSE(map.modified());
    const auto redonePath = env.dir() / "redone.vmf";
    REQUIRE(map.saveTo(redonePath).is_success());
    CHECK(readBinary(redonePath) == saved);
    REQUIRE(map.save().is_success());
    CHECK(readBinary(editedPath) == saved);
    CHECK(readBinary(path) == original);
  }

  SECTION(
    "externally changed backing destination is refused without clearing dirty state")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "external change.vmf";
    const auto original = nativeRamp(true);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    selectNodes(map, {&ramp(map)});
    map.editorContext().setAlignmentLock(false);
    REQUIRE(transformSelection(
      map,
      "Translate before external change",
      vm::translation_matrix(vm::vec3d{16, 8, 4})));
    REQUIRE(map.modified());
    const auto moved = ramp(map).brush();
    const auto positions = filePositions(map);
    const auto modificationCount = map.modificationCount();
    const auto external = original + "// independently changed on disk\r\n";
    writeBinary(path, external);
    const auto files = env.directoryContents({});
    CHECK(map.save().is_error());
    CHECK(readBinary(path) == external);
    CHECK(map.path() == path);
    CHECK(map.modified());
    CHECK(map.modificationCount() == modificationCount);
    CHECK(ramp(map).brush() == moved);
    CHECK(filePositions(map) == positions);
    CHECK(env.directoryContents({}) == files);
    const auto recoveredPath = env.dir() / "recovered.vmf";
    REQUIRE(map.saveAs(recoveredPath).is_success());
    checkOnlyPlaneTokensChanged(original, readBinary(recoveredPath));
    CHECK(readBinary(path) == external);
    CHECK_FALSE(map.modified());
  }

  SECTION("default locked transforms retain texel mapping without preview dimensions")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "locked native ramp.vmf";
    const auto original = nativeRamp(true);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    REQUIRE(map.editorContext().alignmentLock());
    const auto before = ramp(map).brush();
    for (const auto& face : before.faces())
    {
      REQUIRE(face.material() == nullptr);
      REQUIRE(face.textureSize() == vm::vec2f{1, 1});
    }
    const auto delta = vm::vec3d{16, 0, 0};
    selectNodes(map, {&ramp(map)});
    REQUIRE(transformSelection(map, "Locked native move", vm::translation_matrix(delta)));
    for (const auto& face : ramp(map).brush().faces())
    {
      CHECK(face.uvAttributes().offset.x() == -63.875f);
    }
    const auto edited = env.dir() / "locked edited.vmf";
    REQUIRE(map.saveAs(edited).is_success());
    auto recovered = MapFixture{};
    auto& reopened = recovered.load(edited, config);
    for (const auto& face : before.faces())
    {
      const auto found = std::ranges::find(
        ramp(reopened).brush().faces(), face.lineNumber(), &BrushFace::lineNumber);
      REQUIRE(found != ramp(reopened).brush().faces().end());
      const auto oldAxes = face.takeUvCoordSystemSnapshot();
      const auto newAxes = found->takeUvCoordSystemSnapshot();
      REQUIRE(oldAxes);
      REQUIRE(newAxes);
      const auto& oldUv = face.uvAttributes();
      const auto& newUv = found->uvAttributes();
      const auto p = face.points()[0];
      const auto q = p + delta;
      CHECK(
        vm::dot(oldAxes->uAxis, p) / oldUv.scale.x() + oldUv.offset.x()
        == Catch::Approx(vm::dot(newAxes->uAxis, q) / newUv.scale.x() + newUv.offset.x())
             .margin(0.00001));
      CHECK(
        vm::dot(oldAxes->vAxis, p) / oldUv.scale.y() + oldUv.offset.y()
        == Catch::Approx(vm::dot(newAxes->vAxis, q) / newUv.scale.y() + newUv.offset.y())
             .margin(0.00001));
    }
    REQUIRE(map.canUndoCommand());
    map.undoCommand();
    const auto undone = env.dir() / "locked undone.vmf";
    REQUIRE(map.saveAs(undone).is_success());
    CHECK(readBinary(undone) == original);
    CHECK(readBinary(path) == original);
  }

  SECTION(
    "native new map resolves its VMF starter without retaining saved-file association")
  {
    auto env = fs::TestEnvironment{};
    const auto starter = env.dir() / "native starter.vmf";
    const auto original = nativeRamp(true);
    writeBinary(starter, original);
    auto nativeConfig = config;
    nativeConfig.mapFormat = MapFormat::SourceVMF;
    nativeConfig.gameInfo.gameConfig.fileFormats = {{"Source VMF", starter}};
    auto fixture = MapFixture{};
    auto& map = fixture.create(nativeConfig);
    REQUIRE(map.vmfScene());
    CHECK(map.worldNode().mapFormat() == MapFormat::SourceVMF);
    CHECK_FALSE(map.persistent());
    CHECK_FALSE(map.modified());
    CHECK(map.path() != starter);
    CHECK(map.worldNode().defaultLayer()->childCount() == 2);
    const auto saved = env.dir() / "new native map.vmf";
    REQUIRE(map.saveAs(saved).is_success());
    CHECK(readBinary(saved) == original);
    CHECK(readBinary(starter) == original);
    REQUIRE(createBrush(map, nativeCubePoints()));
    REQUIRE(map.save().is_success());
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(saved, config);
    CHECK(reopened.worldNode().defaultLayer()->childCount() == 3);
    CHECK(readBinary(starter) == original);
  }

  SECTION(
    "native counters reserve large leading-zero group and side IDs without narrowing or "
    "reuse")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "native ID control.vmf";
    auto original = nativeRamp(false);
    const auto groupId = std::string{"000900719925474099300000000000"};
    const auto at = original.find("editor { \"color\"");
    REQUIRE(at != std::string::npos);
    original.insert(
      at, "group { \"id\" \"" + groupId + "\" editor { color \"1 2 3\" } }\r\n");
    const auto oldSide = std::string{"\"id\" \"8\""};
    const auto sideAt = original.find(oldSide);
    REQUIRE(sideAt != std::string::npos);
    original.replace(sideAt, oldSide.size(), "\"id\" \"000800719925474099300000000000\"");
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    const auto definition = EntityDefinition{
      .name = "info_target",
      .pointEntityDefinition = PointEntityDefinition{.bounds = vm::bbox3d{8}}};
    auto* point = createPointEntity(map, definition, vm::vec3d{16, 16, 16});
    REQUIRE(point);
    CHECK(map.vmfScene()->nativeId(*point) == "900719925474099300000000001");
    map.undoCommand();
    REQUIRE(map.worldNode().defaultLayer()->childCount() == 2);
    REQUIRE(createBrush(map, nativeCubePoints()));
    auto* cube = map.selection().brushes.front();
    REQUIRE(map.vmfScene()->nativeId(*cube) == "900719925474099300000000002");
    const auto positions = filePositions(map);
    const auto saved = env.dir() / "large native IDs.vmf";
    REQUIRE(map.saveAs(saved).is_success());
    const auto bytes = readBinary(saved);
    CHECK(bytes.find("\"800719925474099300000000001\"") != std::string::npos);
    CHECK(nativeFragment(bytes, groupId) == nativeFragment(original, groupId));
    CHECK(filePositions(map) == positions);
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(saved, config);
    CHECK(reopened.worldNode().defaultLayer()->childCount() == 3);
    CHECK(dynamic_cast<BrushNode*>(&nativeNode(reopened, "900719925474099300000000002")));
    CHECK(readBinary(path) == original);
  }

  SECTION(
    "fresh native duplicates retain distinct side sets through transform and history")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "fresh duplicate control.vmf";
    writeBinary(path, nativeRamp(false));
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    REQUIRE(createBrush(map, nativeCubePoints()));
    REQUIRE(map.vmfScene()->nativeId(*map.selection().brushes.front()) == "10");
    const auto before = env.dir() / "before fresh duplicate.vmf";
    REQUIRE(map.saveAs(before).is_success());
    const auto beforeBytes = readBinary(before);
    duplicateSelectedNodes(map);
    REQUIRE(map.selection().brushes.size() == 1);
    auto* duplicate = map.selection().brushes.front();
    REQUIRE(map.vmfScene()->nativeId(*duplicate) == "11");
    REQUIRE(map.worldNode().defaultLayer()->childCount() == 4);
    map.editorContext().setAlignmentLock(false);
    REQUIRE(transformSelection(
      map, "Move fresh duplicate", vm::translation_matrix(vm::vec3d{64, 0, 0})));
    const auto data = duplicate->brush();
    const auto positions = filePositions(map);
    const auto saved = env.dir() / "fresh duplicate.vmf";
    REQUIRE(map.saveAs(saved).is_success());
    const auto bytes = readBinary(saved);
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(saved, config);
    checkReopenedBrush(
      dynamic_cast<BrushNode&>(nativeNode(reopened, "11")).brush(), data, false);
    CHECK(nativeFragment(bytes, "10") == nativeFragment(beforeBytes, "10"));
    map.undoCommand();
    map.undoCommand();
    const auto undone = env.dir() / "fresh duplicate undone.vmf";
    REQUIRE(map.saveAs(undone).is_success());
    CHECK(readBinary(undone) == beforeBytes);
    map.redoCommand();
    map.redoCommand();
    CHECK(filePositions(map) == positions);
    const auto redone = env.dir() / "fresh duplicate redone.vmf";
    REQUIRE(map.saveAs(redone).is_success());
    CHECK(readBinary(redone) == bytes);
  }

  SECTION(
    "native brush entity wrapping whole and partial duplication preserve ownership and "
    "metadata")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "native ownership control.vmf";
    const auto original = nativeRamp(true);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    auto* brush = &ramp(map);
    selectNodes(map, {brush});
    const auto definition = EntityDefinition{.name = "func_detail"};
    auto* owner = createBrushEntity(map, definition);
    REQUIRE(owner);
    REQUIRE(map.vmfScene()->nativeId(*owner) == "10");
    CHECK(brush->parent() == owner);
    CHECK(map.vmfScene()->nativeId(*brush) == "2");
    const auto single = env.dir() / "single native owner.vmf";
    REQUIRE(map.saveAs(single).is_success());
    const auto singleBytes = readBinary(single);
    CHECK(nativeFragment(singleBytes, "2") == nativeFragment(original, "2"));
    deselectAll(map);
    REQUIRE(createBrush(map, nativeCubePoints()));
    auto* fresh = map.selection().brushes.front();
    REQUIRE(fresh->parent() == map.worldNode().defaultLayer());
    deselectAll(map);
    REQUIRE(reparentNodes(map, {{owner, {fresh}}}));
    REQUIRE(fresh->parent() == owner);
    REQUIRE(owner->childCount() == 2);
    REQUIRE(map.selection().nodes.empty());
    selectNodes(map, {fresh});
    const auto noOpPositions = filePositions(map);
    const auto noOpUndo = map.undoCommandName();
    REQUIRE(reparentNodes(map, {{owner, {fresh}}}));
    CHECK(map.undoCommandName() == noOpUndo);
    CHECK(filePositions(map) == noOpPositions);
    deselectAll(map);
    selectNodes(map, owner->children());
    REQUIRE(map.selection().brushes.size() == 2);
    duplicateSelectedNodes(map);
    REQUIRE(map.selection().brushes.size() == 2);
    auto* whole = dynamic_cast<EntityNode*>(map.selection().brushes.front()->parent());
    REQUIRE(whole);
    REQUIRE(whole != owner);
    REQUIRE(whole->childCount() == 2);
    const auto wholeId = map.vmfScene()->nativeId(*whole);
    deselectAll(map);
    selectNodes(map, {brush});
    duplicateSelectedNodes(map);
    REQUIRE(map.selection().brushes.size() == 1);
    auto* partial = dynamic_cast<EntityNode*>(map.selection().brushes.front()->parent());
    REQUIRE(partial);
    REQUIRE(partial != owner);
    REQUIRE(partial->childCount() == 1);
    const auto partialId = map.vmfScene()->nativeId(*partial);
    const auto saved = env.dir() / "native ownership duplicates.vmf";
    REQUIRE(map.saveAs(saved).is_success());
    const auto bytes = readBinary(saved);
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(saved, config);
    CHECK(nativeNode(reopened, "10").childCount() == 2);
    CHECK(nativeNode(reopened, wholeId).childCount() == 2);
    CHECK(nativeNode(reopened, partialId).childCount() == 1);
    CHECK(nativeFragment(bytes, "9") == nativeFragment(original, "9"));
    CHECK(nativeFragment(bytes, "2") == nativeFragment(original, "2"));

    // Reopening makes the owner/solid original native subtrees. Editing a child
    // then deleting its ancestor must remove only that highest source span.
    auto deletionFixture = MapFixture{};
    auto& deletion = deletionFixture.load(single, config);
    selectNodes(deletion, {&nativeNode(deletion, "2")});
    REQUIRE(transformSelection(
      deletion,
      "Edit child before ancestor deletion",
      vm::translation_matrix(vm::vec3d{16, 0, 0})));
    // The selected last brush implicitly removes its now-empty native owner.
    REQUIRE(removeSelectedNodes(deletion));
    const auto deleted = env.dir() / "deleted native ancestor.vmf";
    REQUIRE(deletion.saveAs(deleted).is_success());
    auto expected = singleBytes;
    const auto span = nativeFragment(singleBytes, "10");
    expected.erase(expected.find(span), span.size());
    CHECK(readBinary(deleted) == expected);
    CHECK_NOTHROW(VmfScene::read(readBinary(deleted), deletion.worldBounds()));
    deletion.undoCommand();
    deletion.undoCommand();
    const auto restored = env.dir() / "native ancestor restored.vmf";
    REQUIRE(deletion.saveAs(restored).is_success());
    CHECK(readBinary(restored) == singleBytes);
    CHECK(readBinary(path) == original);
  }

  SECTION("fresh native brushes reject split or missing side identities before mutation")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "fresh topology control.vmf";
    const auto original = nativeRamp(false);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    const auto points = std::vector<vm::vec3d>{
      {-32, -32, -32},
      {32, -32, -32},
      {-32, 32, -32},
      {32, 32, -32},
      {-32, -32, 32},
      {32, -32, 32},
      {-32, 32, 32},
      {32, 32, 32}};
    REQUIRE(createBrush(map, points));
    auto* cube = map.selection().brushes.front();
    REQUIRE(cube->brush().faceCount() == 6);
    const auto positions = filePositions(map);
    const auto count = map.modificationCount();
    const auto selection = map.selection().nodes;
    const auto undo = map.canUndoCommand();
    const auto redo = map.canRedoCommand();
    CHECK_FALSE(addVertex(map, vm::vec3d{0, 0, 64}));
    auto aliased = cube->brush();
    aliased.faces()[1].setFilePosition(aliased.faces()[0].lineNumber(), 1);
    CHECK_FALSE(map.executeAndStore(
      std::make_unique<SwapNodeContentsCommand>(
        "Duplicate native side identity",
        std::vector<std::pair<Node*, NodeContents>>{
          {cube, NodeContents{std::move(aliased)}}})));
    CHECK(cube->brush().faceCount() == 6);
    CHECK(filePositions(map) == positions);
    CHECK(map.modificationCount() == count);
    CHECK(map.selection().nodes == selection);
    CHECK(map.canUndoCommand() == undo);
    CHECK(map.canRedoCommand() == redo);
    const auto savedPath = env.dir() / "fresh topology refused.vmf";
    REQUIRE(map.saveAs(savedPath).is_success());
    CHECK_NOTHROW(VmfScene::read(readBinary(savedPath), map.worldBounds()));
    REQUIRE(cube->brush().faceCount() == 6);
    map.editorContext().setAlignmentLock(false);
    REQUIRE(transformSelection(
      map, "Move fresh native control", vm::translation_matrix(vm::vec3d{64, 0, 0})));
    const auto moved = env.dir() / "fresh transform control.vmf";
    REQUIRE(map.saveAs(moved).is_success());
    auto reopened = VmfScene::read(readBinary(moved), map.worldBounds());
    CHECK(reopened.world().defaultLayer()->childCount() == 3);
    CHECK(readBinary(path) == original);
  }

  SECTION(
    "ambiguous repeated classnames preserve no edit loading and fail geometry "
    "classification closed")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "repeated classname control.vmf";
    auto original = nativeRamp(false);
    const auto classname = std::string{"\"classname\" \"info_target\""};
    const auto at = original.find(classname);
    REQUIRE(at != std::string::npos);
    original.insert(at + classname.size(), " " + classname);
    auto projection = std::optional<VmfScene>{};
    CHECK_NOTHROW(projection.emplace(VmfScene::read(original, vm::bbox3d{4096})));
    REQUIRE(projection);
    CHECK(projection->serialize(projection->world()) == original);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    REQUIRE(map.save().is_success());
    CHECK(readBinary(path) == original);
    auto& brush = ramp(map);
    selectNodes(map, {&brush});
    const auto positions = filePositions(map);
    const auto count = map.modificationCount();
    CHECK_FALSE(transformSelection(
      map, "Ambiguous reference guard", vm::translation_matrix(vm::vec3d{16, 0, 0})));
    CHECK_FALSE(removeSelectedNodes(map));
    CHECK(filePositions(map) == positions);
    CHECK(map.modificationCount() == count);
    CHECK(map.selection().nodes == std::vector<Node*>{&brush});
    REQUIRE(map.save().is_success());
    CHECK(readBinary(path) == original);
  }

  SECTION("native fresh brush point entity and outputs survive save reopen undo redo")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "authoring control.vmf";
    const auto original = nativeRamp(true);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    const auto originalPositions = filePositions(map);
    const auto entryCount = map.vmfScene()->document().entries().size();
    const auto points = std::vector<vm::vec3d>{
      {128.0001, 0, 0},
      {192.0001, 0, 0},
      {128.0001, 64, 0},
      {192.0001, 64, 0},
      {128.0001, 0, 64},
      {192.0001, 0, 64},
      {128.0001, 64, 64},
      {192.0001, 64, 64}};
    REQUIRE(createBrush(map, points));
    REQUIRE(map.selection().brushes.size() == 1);
    auto* cube = map.selection().brushes.front();
    REQUIRE(map.vmfScene()->nativeId(*cube) == "10");
    CHECK(cube->lineNumber() > entryCount);
    CHECK(cube->brush().bounds().min.x() == Catch::Approx(128.0001).margin(1e-8));
    for (const auto& face : cube->brush().faces())
    {
      CHECK(face.lineNumber() > entryCount);
    }
    const auto definition = EntityDefinition{
      .name = "info_target",
      .pointEntityDefinition = PointEntityDefinition{.bounds = vm::bbox3d{8}}};
    auto* point = createPointEntity(map, definition, vm::vec3d{128, 32, 16});
    REQUIRE(point);
    REQUIRE(map.vmfScene()->nativeId(*point) == "11");
    REQUIRE(setSourceEntityConnections(
      map,
      {{"OnUser1", "!self,Enable,,0,-1", std::nullopt},
       {"OnUser1", "!self,Disable,,1,-1", std::nullopt}}));
    REQUIRE(map.worldNode().defaultLayer()->childCount() == 4);
    const auto cubeData = cube->brush();
    const auto positions = filePositions(map);
    CHECK_FALSE(serializeSelectedNodes(map).empty());
    CHECK(filePositions(map) == positions);
    const auto savedPath = env.dir() / "authored.vmf";
    REQUIRE(map.saveAs(savedPath).is_success());
    const auto saved = readBinary(savedPath);
    CHECK(nativeFragment(saved, "9") == nativeFragment(original, "9"));
    CHECK(nativeFragment(saved, "2") == nativeFragment(original, "2"));
    CHECK(filePositions(map) == positions);
    REQUIRE(map.save().is_success());
    CHECK(readBinary(savedPath) == saved);
    CHECK(readBinary(path) == original);
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(savedPath, config);
    REQUIRE(reopened.worldNode().defaultLayer()->childCount() == 4);
    checkReopenedBrush(
      dynamic_cast<BrushNode&>(nativeNode(reopened, "10")).brush(), cubeData, false);
    const auto& entity = dynamic_cast<EntityNode&>(nativeNode(reopened, "11")).entity();
    REQUIRE(entity.property("origin"));
    CHECK(*entity.property("origin") == *point->entity().property("origin"));
    REQUIRE(entity.sourceConnections().size() == 2);
    CHECK(entity.sourceConnections()[0].value == "!self,Enable,,0,-1");
    CHECK(entity.sourceConnections()[1].value == "!self,Disable,,1,-1");
    for (size_t i = 0; i < 16 && map.canUndoCommand(); ++i)
    {
      map.undoCommand();
    }
    REQUIRE_FALSE(map.canUndoCommand());
    CHECK(filePositions(map) == originalPositions);
    const auto undone = env.dir() / "authoring undone.vmf";
    REQUIRE(map.saveAs(undone).is_success());
    CHECK(readBinary(undone) == original);
    REQUIRE(map.canRedoCommand());
    for (size_t i = 0; i < 16 && map.canRedoCommand(); ++i)
    {
      map.redoCommand();
    }
    REQUIRE_FALSE(map.canRedoCommand());
    CHECK(filePositions(map) == positions);
    const auto redone = env.dir() / "authoring redone.vmf";
    REQUIRE(map.saveAs(redone).is_success());
    CHECK(readBinary(redone) == saved);
    deselectAll(map);
    selectNodes(map, {&nativeNode(map, "10"), &nativeNode(map, "11")});
    REQUIRE(removeSelectedNodes(map));
    REQUIRE(map.worldNode().defaultLayer()->childCount() == 2);
    const auto deleted = env.dir() / "authoring deleted.vmf";
    REQUIRE(map.saveAs(deleted).is_success());
    CHECK(readBinary(deleted) == original);
    map.undoCommand();
    CHECK(filePositions(map) == positions);
    const auto restored = env.dir() / "authoring restored.vmf";
    REQUIRE(map.saveAs(restored).is_success());
    CHECK(readBinary(restored) == saved);
  }

  SECTION(
    "native duplicates use fresh IDs and retain template UV metadata and repeated "
    "outputs")
  {
    const auto kind = GENERATE(std::string{"brush"}, std::string{"entity"});
    CAPTURE(kind);
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "duplicate control.vmf";
    auto original = nativeRamp(true);
    if (kind == "entity")
    {
      original.insert(
        original.rfind('}'), " editor { /*keep editor*/ color \"1 2 3\" } ");
    }
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    const auto originalPositions = filePositions(map);
    auto& source = nativeNode(map, kind == "brush" ? "2" : "9");
    selectNodes(map, {&source});
    duplicateSelectedNodes(map);
    REQUIRE(map.worldNode().defaultLayer()->childCount() == 3);
    REQUIRE(map.selection().nodes.size() == 1);
    auto* duplicate = map.selection().nodes.front();
    CHECK(duplicate != &source);
    REQUIRE(map.vmfScene()->nativeId(*duplicate) == "10");
    if (kind == "brush")
    {
      map.editorContext().setAlignmentLock(false);
      REQUIRE(transformSelection(
        map, "Move native duplicate", vm::translation_matrix(vm::vec3d{16, 8, 4})));
    }
    else
    {
      auto rows = dynamic_cast<EntityNode*>(duplicate)->entity().sourceConnections();
      REQUIRE(rows.size() == 2);
      rows[0].value = "!self,Disable,,0.125,-1";
      REQUIRE(setSourceEntityConnections(map, rows));
    }
    const auto positions = filePositions(map);
    const auto savedPath = env.dir() / "duplicated.vmf";
    REQUIRE(map.saveAs(savedPath).is_success());
    const auto saved = readBinary(savedPath);
    CHECK(nativeFragment(saved, "2") == nativeFragment(original, "2"));
    CHECK(nativeFragment(saved, "9") == nativeFragment(original, "9"));
    CHECK(filePositions(map) == positions);
    CHECK(readBinary(path) == original);
    auto recovered = MapFixture{};
    auto& reopened = recovered.load(savedPath, config);
    REQUIRE(reopened.worldNode().defaultLayer()->childCount() == 3);
    if (kind == "brush")
    {
      checkReopenedBrush(
        dynamic_cast<BrushNode&>(nativeNode(reopened, "10")).brush(),
        dynamic_cast<BrushNode*>(duplicate)->brush(),
        false);
      CHECK(
        nativeFragment(saved, "10").find("[1 0 0 0.1250] 0.2500") != std::string::npos);
    }
    else
    {
      CHECK(saved.starts_with(original));
      const auto& rows = dynamic_cast<EntityNode&>(nativeNode(reopened, "10"))
                           .entity()
                           .sourceConnections();
      REQUIRE(rows.size() == 2);
      CHECK(rows[0].value == "!self,Disable,,0.125,-1");
      CHECK(rows[1].value == "marker,Disable,,1,-1");
      CHECK(
        nativeFragment(saved, "10").find("/*keep editor*/ color \"1 2 3\"")
        != std::string::npos);
    }
    map.undoCommand();
    map.undoCommand();
    CHECK(filePositions(map) == originalPositions);
    const auto undone = env.dir() / "duplicate undone.vmf";
    REQUIRE(map.saveAs(undone).is_success());
    CHECK(readBinary(undone) == original);
    map.redoCommand();
    map.redoCommand();
    CHECK(filePositions(map) == positions);
    const auto redone = env.dir() / "duplicate redone.vmf";
    REQUIRE(map.saveAs(redone).is_success());
    CHECK(readBinary(redone) == saved);
  }

  SECTION(
    "native original deletion removes only the native subtree and undo restores exact "
    "bytes")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "deletion control.vmf";
    const auto original = nativeRamp(false);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    const auto positions = filePositions(map);
    auto& brush = ramp(map);
    const auto document = VmfDocument::parse(original);
    const auto& entry = document.entries()[brush.lineNumber() - 1];
    auto expected = original;
    expected.erase(entry.key.offset, entry.end - entry.key.offset);
    selectNodes(map, {&brush});
    REQUIRE(removeSelectedNodes(map));
    REQUIRE(map.worldNode().defaultLayer()->childCount() == 1);
    const auto deleted = env.dir() / "deleted original.vmf";
    REQUIRE(map.saveAs(deleted).is_success());
    CHECK(readBinary(deleted) == expected);
    CHECK(readBinary(path) == original);
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(deleted, config);
    REQUIRE(reopened.worldNode().defaultLayer()->childCount() == 1);
    CHECK(nativeNode(reopened, "9").lineNumber() > 0);
    map.undoCommand();
    CHECK(filePositions(map) == positions);
    const auto undone = env.dir() / "deletion undone.vmf";
    REQUIRE(map.saveAs(undone).is_success());
    CHECK(readBinary(undone) == original);
    map.redoCommand();
    const auto redone = env.dir() / "deletion redone.vmf";
    REQUIRE(map.saveAs(redone).is_success());
    CHECK(readBinary(redone) == expected);
  }

  SECTION("authored room first-brush loop can retain an explicitly requested copy")
  {
    auto env = fs::TestEnvironment{};
    const auto* input = std::getenv("SOURCEBROOM_A4_INPUT");
    const auto* output = std::getenv("SOURCEBROOM_A4_OUTPUT");
    const auto original = input ? readBinary(input) : nativeRamp(true);
    const auto path = env.dir() / "room control.vmf";
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    auto* selected = static_cast<BrushNode*>(nullptr);
    for (auto* child : map.worldNode().defaultLayer()->children())
    {
      if (auto* brush = dynamic_cast<BrushNode*>(child))
      {
        selected = brush;
      }
    }
    REQUIRE(selected);
    const auto before = selected->brush().bounds();
    selectNodes(map, {selected});
    REQUIRE(transformSelection(
      map, "Native room ramp move", vm::translation_matrix(vm::vec3d{16, 0, 0})));
    const auto expectedBounds = before.translate(vm::vec3d{16, 0, 0});
    CHECK(vm::is_equal(selected->brush().bounds().min, expectedBounds.min, 1e-8));
    CHECK(vm::is_equal(selected->brush().bounds().max, expectedBounds.max, 1e-8));
    const auto editOutputs = std::getenv("SOURCEBROOM_A5_OUTPUTS") != nullptr;
    if (editOutputs)
    {
      auto* relay = static_cast<EntityNode*>(nullptr);
      for (auto* child : map.worldNode().defaultLayer()->children())
      {
        if (auto* entity = dynamic_cast<EntityNode*>(child))
        {
          if (entity->entity().classname() == "logic_relay")
          {
            relay = entity;
          }
        }
      }
      REQUIRE(relay);
      auto rows = relay->entity().sourceConnections();
      REQUIRE(rows.size() == 2);
      const auto originalSecond = rows[1];
      rows[0].value = "!self,Disable,,0.125,-1";
      deselectAll(map);
      selectNodes(map, {relay});
      REQUIRE(setSourceEntityConnections(map, rows));
      REQUIRE(setEntityProperty(map, "spawnflags", "0"));
      CHECK(relay->entity().sourceConnections()[1] == originalSecond);
    }
    if (std::getenv("SOURCEBROOM_A52_AUTHORING"))
    {
      deselectAll(map);
      map.setCurrentMaterialName("tools/toolsnodraw");
      REQUIRE(createBrush(map, nativeCubePoints()));
      REQUIRE(transformSelection(
        map, "Place authored cube", vm::translation_matrix(vm::vec3d{0, 0, 64})));
      const auto count = map.worldNode().defaultLayer()->childCount();
      duplicateSelectedNodes(map);
      REQUIRE(map.worldNode().defaultLayer()->childCount() == count + 1);
      REQUIRE(removeSelectedNodes(map));
      CHECK(map.worldNode().defaultLayer()->childCount() == count);
      const auto definition = EntityDefinition{
        .name = "info_target",
        .pointEntityDefinition = PointEntityDefinition{.bounds = vm::bbox3d{8}}};
      REQUIRE(createPointEntity(map, definition, vm::vec3d{128, 0, 64}));
    }
    const auto destination =
      output ? std::filesystem::path{output} : env.dir() / "room edited.vmf";
    REQUIRE_FALSE(std::filesystem::exists(destination));
    REQUIRE(map.saveAs(destination).is_success());
    auto recovered = MapFixture{};
    auto& reopened = recovered.load(destination, config);
    CHECK(reopened.worldNode().mapFormat() == MapFormat::SourceVMF);
    CHECK(
      reopened.worldNode().defaultLayer()->childCount()
      == map.worldNode().defaultLayer()->childCount());
    if (editOutputs)
    {
      auto checked = false;
      for (auto* child : reopened.worldNode().defaultLayer()->children())
      {
        if (const auto* entity = dynamic_cast<const EntityNode*>(child))
        {
          if (entity->entity().classname() == "logic_relay")
          {
            const auto& rows = entity->entity().sourceConnections();
            REQUIRE(rows.size() == 2);
            CHECK(rows[0].value == "!self,Disable,,0.125,-1");
            CHECK(rows[1].value == "!self,Enable,,1,-1");
            REQUIRE(entity->entity().property("spawnflags"));
            CHECK(*entity->entity().property("spawnflags") == "0");
            checked = true;
          }
        }
      }
      CHECK(checked);
    }
    CHECK(readBinary(path) == original);
    if (input)
    {
      CHECK(readBinary(input) == original);
    }
  }

  SECTION("unknown global extensions preserve bytes but block geometry edits")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "unknown extension.vmf";
    const auto original = nativeRamp(false, true);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    REQUIRE(map.save().is_success());
    selectNodes(map, {&ramp(map)});
    const auto positions = filePositions(map);
    const auto count = map.modificationCount();
    REQUIRE_FALSE(transformSelection(
      map, "Move with unknown root", vm::translation_matrix(vm::vec3d{16, 0, 0})));
    CHECK_FALSE(map.modified());
    CHECK(map.modificationCount() == count);
    CHECK(filePositions(map) == positions);
    REQUIRE(map.save().is_success());
    CHECK(readBinary(path) == original);
  }

  SECTION(
    "unsupported native actions refuse before changing objects selection or history")
  {
    const auto operation = GENERATE(
      std::string{"topology"},
      std::string{"opaque duplicate"},
      std::string{"opaque delete"},
      std::string{"node paste"},
      std::string{"csg"});
    CAPTURE(operation);
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "admission control.vmf";
    auto original = nativeRamp(false);
    if (operation.starts_with("opaque"))
    {
      const auto needle = std::string{"\"smoothing_groups\" \"0\""};
      original.insert(
        original.find(needle) + needle.size(),
        " sourcebroom_opaque_geometry { key \"preserved\" }");
    }
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    auto& brush = ramp(map);
    selectNodes(map, {&brush});
    const auto positions = filePositions(map);
    const auto count = map.modificationCount();
    const auto undo = map.canUndoCommand();
    const auto redo = map.canRedoCommand();
    if (operation == "topology")
    {
      CHECK_FALSE(addVertex(map, vm::vec3d{32, 32, 128}));
    }
    else if (operation == "opaque duplicate")
    {
      duplicateSelectedNodes(map);
    }
    else if (operation == "opaque delete")
    {
      CHECK_FALSE(removeSelectedNodes(map));
    }
    else if (operation == "node paste")
    {
      const auto clipboard = serializeSelectedNodes(map);
      REQUIRE_FALSE(clipboard.empty());
      CHECK(paste(map, clipboard) == PasteType::Failed);
    }
    else
    {
      CHECK_FALSE(csgHollow(map));
    }
    CHECK(map.worldNode().defaultLayer()->childCount() == 2);
    CHECK(map.selection().nodes == std::vector<Node*>{&brush});
    CHECK(filePositions(map) == positions);
    CHECK(map.modificationCount() == count);
    CHECK(map.canUndoCommand() == undo);
    CHECK(map.canRedoCommand() == redo);
    CHECK_FALSE(map.modified());
    REQUIRE(map.save().is_success());
    CHECK(readBinary(path) == original);
  }

  SECTION("unregistered low-level mutations remain unsavable and preserve destinations")
  {
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "guarded original.vmf";
    const auto destination = env.dir() / "existing destination.vmf";
    const auto original = nativeRamp(false);
    const auto sentinel = std::string{"// do not replace this destination\r\n"};
    writeBinary(path, original);
    writeBinary(destination, sentinel);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    const auto before = ramp(map).brush();
    const auto positions = filePositions(map);
    selectNodes(map, {&ramp(map)});
    auto* unregistered = ramp(map).cloneRecursively(map.worldBounds());
    REQUIRE(map.executeAndStore(
      AddRemoveNodesCommand::add({{map.worldNode().defaultLayer(), {unregistered}}})));
    REQUIRE(map.worldNode().defaultLayer()->childCount() == 3u);
    REQUIRE(map.modified());
    const auto modificationCount = map.modificationCount();
    const auto unsafePositions = filePositions(map);
    const auto files = env.directoryContents({});
    CHECK(map.save().is_error());
    CHECK(map.saveAs(destination).is_error());
    CHECK(readBinary(path) == original);
    CHECK(readBinary(destination) == sentinel);
    CHECK(map.path() == path);
    CHECK(map.modified());
    CHECK(map.modificationCount() == modificationCount);
    CHECK(filePositions(map) == unsafePositions);
    CHECK(env.directoryContents({}) == files);
    map.undoCommand();
    CHECK(ramp(map).brush() == before);
    CHECK(filePositions(map) == positions);
    CHECK_FALSE(map.modified());
    REQUIRE(map.save().is_success());
    CHECK(readBinary(path) == original);
    map.redoCommand();
    REQUIRE(map.modified());
    CHECK(map.save().is_error());
    CHECK(readBinary(path) == original);
    CHECK(readBinary(destination) == sentinel);
    CHECK(map.modified());
  }
}
} // namespace tb::mdl
