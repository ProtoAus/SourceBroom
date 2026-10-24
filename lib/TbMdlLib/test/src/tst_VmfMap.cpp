/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "fs/TestEnvironment.h"
#include "mdl/Brush.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/CatchConfig.h"
#include "mdl/EditorContext.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_CopyPaste.h"
#include "mdl/Map_Entities.h"
#include "mdl/Map_Geometry.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/Node.h"
#include "mdl/VmfDocument.h"
#include "mdl/WorldNode.h"

#include "vm/mat.h"

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
void checkReopenedBrush(const Brush& actual, const Brush& expected)
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
    CHECK(face.lineNumber() == before.lineNumber());
    CHECK(face.preservesPlanePrecision() == before.preservesPlanePrecision());
  }
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
    REQUIRE(transformSelection(
      map, "Move with unknown root", vm::translation_matrix(vm::vec3d{16, 0, 0})));
    REQUIRE(map.modified());
    CHECK(map.save().is_error());
    CHECK(readBinary(path) == original);
    CHECK(map.modified());
  }

  SECTION("unsafe topology duplicate and deletion saves refuse without losing bytes")
  {
    const auto operation =
      GENERATE(std::string{"topology"}, std::string{"duplicate"}, std::string{"delete"});
    CAPTURE(operation);
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
    if (operation == "topology")
    {
      REQUIRE(addVertex(map, vm::vec3d{32, 32, 128}));
      REQUIRE(ramp(map).brush().faceCount() != before.faceCount());
    }
    else if (operation == "duplicate")
    {
      duplicateSelectedNodes(map);
      REQUIRE(map.worldNode().defaultLayer()->childCount() == 3u);
    }
    else
    {
      removeSelectedNodes(map);
      REQUIRE(map.worldNode().defaultLayer()->childCount() == 1u);
    }
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
