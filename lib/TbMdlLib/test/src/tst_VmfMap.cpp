/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SourceDisplacementFixture.h"
#include "base/NotifierConnection.h"
#include "fs/TestEnvironment.h"
#include "mdl/AddRemoveNodesCommand.h"
#include "mdl/Brush.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/BrushRendererBrushCache.h"
#include "mdl/CatchConfig.h"
#include "mdl/CommandProcessor.h"
#include "mdl/EditorContext.h"
#include "mdl/EntityDefinition.h"
#include "mdl/EntityNode.h"
#include "mdl/ExportOptions.h"
#include "mdl/GameConfig.h"
#include "mdl/GroupNode.h"
#include "mdl/Hit.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_Brushes.h"
#include "mdl/Map_CopyPaste.h"
#include "mdl/Map_Entities.h"
#include "mdl/Map_Geometry.h"
#include "mdl/Map_Groups.h"
#include "mdl/Map_NodeVisibility.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Picking.h"
#include "mdl/Map_Selection.h"
#include "mdl/Node.h"
#include "mdl/NodeContents.h"
#include "mdl/PasteType.h"
#include "mdl/PickResult.h"
#include "mdl/SelectionChange.h"
#include "mdl/SourceEditorMetadata.h"
#include "mdl/SourceTransformPolicy.h"
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
#include <limits>
#include <set>
#include <sstream>
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

std::string nativeGroupedRamp()
{
  auto source = nativeRamp(false);
  const auto end = source.find("}\r\neditor { ");
  REQUIRE(end != std::string::npos);
  source.insert(end, "editor { groupid 11 color \"9 8 7\" note first note second }\r\n");
  source.insert(
    source.find("editor { \"color\""),
    "group { id 10 note 007 note 0008 editor { color \"1 2 3\" } }\r\n"
    "group { id 11 editor { groupid 10 } }\r\n");
  return source;
}

std::string nativeDisplacementRoom(const std::string& attachment = {})
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
  original.insert(field, test::displacementBlock() + attachment);
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

std::string nativePolygonRoom(const std::string& polygon)
{
  auto source = nativeDisplacementRoom(polygon);
  const auto displacement = test::displacementBlock();
  source.erase(source.find(displacement), displacement.size());
  return source;
}

std::string nativePolygonEntity(const std::string& polygon)
{
  auto source = nativePolygonRoom(polygon);
  const auto solid = nativeFragment(source, "2");
  source.erase(source.find(solid), solid.size());
  const auto document = VmfDocument::parse(solid);
  auto edits = std::vector<VmfValueEdit>{};
  for (size_t i = 0; i < document.entries().size(); ++i)
  {
    const auto& entry = document.entries()[i];
    if (entry.value && document.tokenContents(entry.key) == "id")
    {
      const auto id = std::stoull(std::string{document.tokenContents(*entry.value)});
      edits.push_back({i, '"' + std::to_string(id == 2 ? 11 : id + 9) + '"'});
    }
  }
  return source + R"(entity { id 10 classname func_detail origin "0.00 0 0" )" + solid
         + document.serialize(edits) + " }";
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

TEST_CASE("VmfMap polygon caches history save and atomic refusal", "[vmf]")
{
  const auto invalid = GENERATE(false, true);
  const auto indexed = GENERATE(false, true);
  auto source = nativeRamp(false);
  // Independently authored ramp polygon: explicit indices vs physical row order.
  const auto polygon = indexed
    ? std::string{
        "point_data { \"numpts\" \"4\" \"point\" \"2 64 64 64\" "
        "\"point\" \"0 0.00 0 32\" \"point\" \"3 64 0 64\" "
        "\"point\" \"1 0 64 32\" }"}
    : std::string{
        "vertices_plus { \"v\" \"64 64 64\" \"v\" \"64 0 64\" "
        "\"v\" \"0.00 0 32\" \"v\" \"0 64 32\" }"};
  const auto document = VmfDocument::parse(source);
  auto append = std::vector<VmfStructuralEdit>{};
  for (size_t i = 0; i < document.entries().size(); ++i)
  {
    const auto& entry = document.entries()[i];
    if (
      entry.value && document.tokenContents(entry.key) == "id"
      && document.tokenContents(*entry.value) == "8")
    {
      append.push_back({VmfStructuralEdit::Type::Append, *entry.parent, polygon});
    }
  }
  REQUIRE(append.size() == 1);
  source = document.serialize({}, append);
  if (invalid)
  {
    const auto row = indexed ? std::string{"1 0 64 32"} : std::string{"0 64 32"};
    source.replace(source.find(row), row.size(), indexed ? "1 1 64 32" : "1 64 32");
  }
  auto env = fs::TestEnvironment{};
  const auto input = env.dir() / "point-data.vmf";
  const auto output = env.dir() / "saved.vmf";
  writeBinary(input, source);
  auto fixture = MapFixture{};
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto& map = fixture.load(input, config);
  auto& node = ramp(map);
  selectNodes(map, {&node});
  const auto beforeCount = map.modificationCount();
  if (invalid)
  {
    // An ordinary point entity is a second, otherwise valid acted batch subject.
    selectNodes(map, {map.worldNode().defaultLayer()->children().back()});
    const auto beforeHistory = map.undoCommandName();
    CHECK_FALSE(translateSelection(map, {16, 8, 4}));
    CHECK(map.modificationCount() == beforeCount);
    CHECK(map.undoCommandName() == beforeHistory);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }
  else
  {
    REQUIRE(translateSelection(map, {16, 8, 4}));
    CHECK(map.modificationCount() == beforeCount + 1);
    const auto saved = map.vmfScene()->serialize(map.worldNode());
    CHECK(saved.find("16 8 36") != std::string::npos);
    CHECK(saved.find("80 72 68") != std::string::npos);
    REQUIRE(map.saveAs(output).is_success());
    CHECK(readBinary(output) == saved);
    auto reopened = MapFixture{};
    auto& loaded = reopened.load(output, config);
    CHECK(ramp(loaded).brush().bounds() == node.brush().bounds());
    selectNodes(loaded, {&ramp(loaded)});
    REQUIRE(translateSelection(loaded, {1, 2, 3}));
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
    CHECK_FALSE(flipSelection(map, {0, 0, 0}, vm::axis::x));
    CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
  }
  CHECK(readBinary(input) == source);
}

TEST_CASE("VmfMap Hammer++ root settings acted atomic controls", "[vmf]")
{
  const auto fault = GENERATE(-1, 0, 1, 2, 3);
  auto settings = std::string{R"(palette_plus { color0 "0 128 255" }
colorcorrection_plus { name0 "" weight0 "+0.2500" }
bgimages_plus { })"};
  if (fault == 0)
  {
    settings += "unknown_plus {}";
  }
  if (fault == 1)
  {
    settings += "light_plus { future_field 0 }";
  }
  if (fault == 2)
  {
    settings += R"(light_plus { texlight "synthetic/light" })";
  }
  if (fault == 3)
  {
    settings += "palette_plus 0";
  }
  const auto source = nativeRamp(false) + settings;
  auto env = fs::TestEnvironment{};
  const auto input = env.dir() / "settings.vmf";
  writeBinary(input, source);
  auto fixture = MapFixture{};
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto& map = fixture.load(input, config);
  auto& node = ramp(map);
  const auto oldBrush = node.brush();
  auto* second = map.worldNode().defaultLayer()->children().back();
  REQUIRE(second != &node);
  REQUIRE(dynamic_cast<EntityNode*>(second));
  const auto oldSecond = static_cast<EntityNode*>(second)->entity();
  const auto nativeId = map.vmfScene()->nativeId(node);
  selectNodes(map, {&node, second});
  const auto selection = map.selection().nodes;
  const auto history = map.undoCommandName();
  const auto count = map.modificationCount();
  REQUIRE(map.vmfScene()->serialize(map.worldNode()) == source);
  if (fault < 0)
  {
    REQUIRE(translateSelection(map, {16, 8, 4}));
    CHECK(node.brush() != oldBrush);
    CHECK(static_cast<EntityNode*>(second)->entity() != oldSecond);
    CHECK(map.modificationCount() == count + 1);
    const auto saved = map.vmfScene()->serialize(map.worldNode());
    CHECK(saved.ends_with(settings));
    REQUIRE(map.saveAs(env.dir() / "saved.vmf").is_success());
    auto reopened = MapFixture{};
    auto& loaded = reopened.load(env.dir() / "saved.vmf", config);
    selectNodes(loaded, {&ramp(loaded)});
    REQUIRE(translateSelection(loaded, {1, 2, 3}));
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
  }
  else
  {
    CHECK_FALSE(translateSelection(map, {16, 8, 4}));
    CHECK(node.brush() == oldBrush);
    CHECK(static_cast<EntityNode*>(second)->entity() == oldSecond);
    CHECK(map.modificationCount() == count);
    CHECK(map.undoCommandName() == history);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }
  CHECK(map.selection().nodes == selection);
  CHECK(map.vmfScene()->nativeId(node) == nativeId);
  CHECK(readBinary(input) == source);
}

TEST_CASE("VmfMap point_data installed history opt-in", "[.][vmf][installed]")
{
  const auto* input = std::getenv("SOURCEBROOM_NATIVE_EDIT_INPUT");
  const auto* requestedEntry = std::getenv("SOURCEBROOM_NATIVE_EDIT_ENTRY");
  const auto* variant = std::getenv("SOURCEBROOM_NATIVE_EDIT_VARIANT");
  REQUIRE(input);
  REQUIRE(requestedEntry);
  const auto bounds = vm::bbox3d{VmfScene::PreviewWorldExtent};
  const auto original = readBinary(input);
  auto identityScene = VmfScene::read(original, bounds);
  auto nativeId = std::string{};
  const auto identity = [&](const auto& self, const Node& node) -> void {
    if (node.lineNumber() == std::stoull(requestedEntry) + 1)
    {
      REQUIRE(dynamic_cast<const BrushNode*>(&node));
      nativeId = identityScene.nativeId(node);
    }
    for (const auto* child : node.children())
    {
      self(self, *child);
    }
  };
  identity(identity, identityScene.world());
  REQUIRE_FALSE(nativeId.empty());
  auto inputs = std::vector<std::filesystem::path>{input};
  if (variant)
  {
    inputs.emplace_back(variant);
  }
  auto env = fs::TestEnvironment{};
  for (size_t i = 0; i < inputs.size(); ++i)
  {
    const auto bytes = readBinary(inputs[i]);
    auto fixture = MapFixture{};
    const auto config =
      MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = bounds};
    auto& map = fixture.load(inputs[i], config);
    auto* selected = static_cast<BrushNode*>(nullptr);
    const auto find = [&](const auto& self, Node& node) -> void {
      if (
        const auto* brush = dynamic_cast<const BrushNode*>(&node);
        brush && map.vmfScene()->nativeId(*brush) == nativeId)
      {
        REQUIRE_FALSE(selected);
        selected = dynamic_cast<BrushNode*>(&node);
      }
      for (auto* child : node.children())
      {
        self(self, *child);
      }
    };
    find(find, map.worldNode());
    INFO("Input variant index " << i << "; native solid " << nativeId);
    REQUIRE(selected);
    auto diagnostic = selected->brush();
    REQUIRE(
      diagnostic.transform(bounds, vm::translation_matrix(vm::vec3d{16, 8, 4}), true)
        .is_success());
    auto rootSchema = std::string{};
    if (i != 0)
    {
      const auto& document = map.vmfScene()->document();
      for (const auto root : document.roots())
      {
        const auto name =
          std::string{document.tokenContents(document.entries()[root].key)};
        if (name.ends_with("_plus"))
        {
          rootSchema += name + '\n';
          for (const auto field : document.entries()[root].children)
          {
            const auto& entry = document.entries()[field];
            rootSchema += "  " + std::string{document.tokenContents(entry.key)};
            if (entry.value)
            {
              auto scalarInput =
                std::istringstream{std::string{document.tokenContents(*entry.value)}};
              auto n = double{};
              auto count = size_t{};
              while (scalarInput >> n)
              {
                ++count;
              }
              rootSchema +=
                " scalar numeric-components=" + std::to_string(count)
                + (document.tokenContents(*entry.value).empty() ? " empty" : " nonempty");
            }
            else
            {
              rootSchema += " BLOCK";
            }
            rootSchema += '\n';
          }
        }
      }
    }
    INFO("Root schema:" << '\n' << rootSchema);
    REQUIRE_NOTHROW(
      map.vmfScene()->validateContents(*selected, NodeContents{diagnostic}, bounds));
    const auto oldBounds = selected->brush().bounds();
    selectNodes(map, {selected});
    const auto count = map.modificationCount();
    REQUIRE(translateSelection(map, {16, 8, 4}));
    CHECK(selected->brush().bounds().min == oldBounds.min + vm::vec3d{16, 8, 4});
    CHECK(map.modificationCount() == count + 1);
    const auto saved = map.vmfScene()->serialize(map.worldNode());
    REQUIRE(saved != bytes);
    const auto output = env.dir() / ("saved-" + std::to_string(i) + ".vmf");
    REQUIRE(map.saveAs(output).is_success());
    CHECK(readBinary(output) == saved);
    auto reopened = MapFixture{};
    auto& loaded = reopened.load(output, config);
    CHECK(loaded.vmfScene()->serialize(loaded.worldNode()) == saved);
    auto* reopenedSubject = static_cast<BrushNode*>(nullptr);
    const auto findReopened = [&](const auto& self, Node& node) -> void {
      if (
        auto* candidate = dynamic_cast<BrushNode*>(&node);
        candidate && loaded.vmfScene()->nativeId(*candidate) == nativeId)
      {
        REQUIRE_FALSE(reopenedSubject);
        reopenedSubject = candidate;
      }
      for (auto* child : node.children())
      {
        self(self, *child);
      }
    };
    findReopened(findReopened, loaded.worldNode());
    REQUIRE(reopenedSubject);
    const auto reopenedBefore = reopenedSubject->brush();
    selectNodes(loaded, {reopenedSubject});
    REQUIRE(translateSelection(loaded, {1, 2, 3}));
    REQUIRE(reopenedSubject->brush() != reopenedBefore);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
    CHECK(readBinary(inputs[i]) == bytes);
  }
}

TEST_CASE("VmfMap coupled polygon displacement history and atomic refusal", "[vmf]")
{
  const auto indexed = GENERATE(false, true);
  const auto polygon =
    indexed
      ? std::
          string{R"(point_data { numpts 4 point "2 64.00 64 64" point "0 0.00 0 64" point "3 64 0 64" point "1 0 64 64" })"}
      : std::string{
          R"(vertices_plus { v "64.00 64 64" v "64 0 64" v "0.00 0 64" v "0 64 64" })"};
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  SECTION("proper similarity saves and continues with an acted reopened edit")
  {
    const auto operation = GENERATE(0, 1, 2);
    const auto source = nativeDisplacementRoom(polygon);
    auto env = fs::TestEnvironment{};
    const auto input = env.dir() / "coupled.vmf";
    const auto output = env.dir() / "saved.vmf";
    writeBinary(input, source);
    auto fixture = MapFixture{};
    auto& map = fixture.load(input, config);
    auto& node = ramp(map);
    auto* second =
      dynamic_cast<EntityNode*>(map.worldNode().defaultLayer()->children().back());
    REQUIRE(second);
    const auto before = node.brush();
    const auto oldEntity = second->entity();
    const auto nativeId = map.vmfScene()->nativeId(node);
    selectNodes(map, {&node, second});
    const auto selection = map.selection().nodes;
    const auto count = map.modificationCount();
    const auto transform = operation == 0 ? vm::translation_matrix(vm::vec3d{16, 8, 4})
                           : operation == 1
                             ? vm::rotation_matrix(vm::vec3d{1, 0, 0}, 0.37)
                             : vm::scaling_matrix(vm::vec3d{2, 2, 2});
    if (operation == 0)
    {
      REQUIRE(translateSelection(map, {16, 8, 4}));
    }
    else if (operation == 1)
    {
      REQUIRE(rotateSelection(map, {0, 0, 0}, {1, 0, 0}, 0.37));
    }
    else
    {
      REQUIRE(scaleSelection(map, {0, 0, 0}, {2, 2, 2}));
    }
    REQUIRE(node.brush() != before);
    REQUIRE(second->entity() != oldEntity);
    const auto& oldMesh =
      *before.face(*before.findFace(vm::vec3d{0, 0, 1})).sourceDisplacement();
    auto meshes = size_t{0};
    for (const auto& face : node.brush().faces())
    {
      if (const auto& mesh = face.sourceDisplacement())
      {
        ++meshes;
        REQUIRE(mesh->baseTransformFrom(oldMesh));
        CHECK(*mesh->baseTransformFrom(oldMesh) == transform);
        for (size_t i = 0; i < oldMesh.positions().size(); ++i)
        {
          CHECK(
            vm::is_equal(
              mesh->positions()[i], transform * oldMesh.positions()[i], 0.000001));
        }
      }
    }
    CHECK(meshes == 1);
    CHECK(map.modificationCount() == count + 1);
    CHECK(map.modified());
    const auto saved = map.vmfScene()->serialize(map.worldNode());
    REQUIRE(saved != source);
    const auto ids = [](const std::string& bytes) {
      const auto document = VmfDocument::parse(bytes);
      auto result = std::vector<std::string>{};
      for (const auto& entry : document.entries())
      {
        if (entry.value && document.tokenContents(entry.key) == "id")
        {
          result.emplace_back(document.tokenContents(*entry.value));
        }
      }
      return result;
    };
    CHECK(ids(saved) == ids(source));
    REQUIRE(map.saveAs(output).is_success());
    CHECK_FALSE(map.modified());
    CHECK(readBinary(output) == saved);
    auto reopened = MapFixture{};
    auto& loaded = reopened.load(output, config);
    auto& next = ramp(loaded);
    CHECK(loaded.vmfScene()->nativeId(next) == nativeId);
    const auto nextBefore = next.brush();
    selectNodes(loaded, {&next});
    REQUIRE(translateSelection(loaded, {1, 2, 3}));
    REQUIRE(next.brush() != nextBefore);
    CHECK(loaded.vmfScene()->serialize(loaded.worldNode()) != saved);
    const auto history = map.undoCommandName();
    const auto afterCount = map.modificationCount();
    const auto dirty = map.modified();
    auto shear = vm::mat4x4d::identity();
    shear[1][0] = 0.25;
    CHECK_FALSE(scaleSelection(map, {0, 0, 0}, {2, 1, 1}));
    CHECK_FALSE(flipSelection(map, {0, 0, 0}, vm::axis::x));
    CHECK_FALSE(transformSelection(map, "Coupled shear refusal", shear));
    CHECK_FALSE(transformSelection(
      map,
      "Unclassified coupled rotation",
      vm::rotation_matrix(vm::vec3d{0, 0, 1}, 0.37)));
    CHECK(map.modificationCount() == afterCount);
    CHECK(map.modified() == dirty);
    CHECK(map.undoCommandName() == history);
    CHECK(map.selection().nodes == selection);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
    CHECK(node.brush() == before);
    CHECK(second->entity() == oldEntity);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
    CHECK(map.selection().nodes == selection);
    CHECK(map.vmfScene()->nativeId(node) == nativeId);
    CHECK(readBinary(input) == source);
  }
  SECTION("invalid batch preserves an independent entity and all history state")
  {
    const auto fault = GENERATE(0, 1, 2);
    auto source = nativeDisplacementRoom(polygon);
    if (fault == 0)
    {
      const auto cacheStart = source.find(polygon);
      REQUIRE(cacheStart != std::string::npos);
      source.replace(source.find("0 64 64", cacheStart), 7, "1 64 64");
    }
    if (fault == 1)
    {
      source += "future_root { value opaque }";
    }
    if (fault == 2)
    {
      source += R"(entity { id 20 classname info_overlay sides "8" })";
    }
    auto env = fs::TestEnvironment{};
    const auto input = env.dir() / "refusal.vmf";
    writeBinary(input, source);
    auto fixture = MapFixture{};
    auto& map = fixture.load(input, config);
    auto* subject =
      dynamic_cast<BrushNode*>(map.worldNode().defaultLayer()->children().front());
    REQUIRE(subject);
    auto& node = *subject;
    auto* second = static_cast<EntityNode*>(nullptr);
    for (auto* child : map.worldNode().defaultLayer()->children())
    {
      if (
        auto* entity = dynamic_cast<EntityNode*>(child);
        entity && map.vmfScene()->nativeId(*entity) == "9")
      {
        second = entity;
      }
    }
    REQUIRE(second);
    const auto before = node.brush();
    const auto oldEntity = second->entity();
    const auto nativeId = map.vmfScene()->nativeId(node);
    selectNodes(map, {second, &node});
    const auto selection = map.selection().nodes;
    const auto count = map.modificationCount();
    const auto history = map.undoCommandName();
    const auto dirty = map.modified();
    CHECK_FALSE(translateSelection(map, {16, 8, 4}));
    CHECK(node.brush() == before);
    CHECK(second->entity() == oldEntity);
    CHECK(map.modificationCount() == count);
    CHECK(map.modified() == dirty);
    CHECK(map.undoCommandName() == history);
    CHECK(map.selection().nodes == selection);
    CHECK(map.vmfScene()->nativeId(node) == nativeId);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
    CHECK(readBinary(input) == source);
  }
}

TEST_CASE("VmfMap polygon cache ordinary ownership lifecycle", "[vmf]")
{
  const auto schema = GENERATE(0, 1, 2);
  const auto moved = GENERATE(false, true);
  const auto polygon =
    schema == 0 ? std::string{}
    : schema == 1
      ? std::
          string{R"(point_data { numpts 4 point "2 64.00 64 64" point "0 0.00 0 64" point "3 64 0 64" point "1 0 64 64" })"}
      : std::string{
          R"(vertices_plus { v "64.00 64 64" v "64 0 64" v "0.00 0 64" v "0 64 64" })"};
  auto source = nativeDisplacementRoom(polygon);
  const auto displacement = test::displacementBlock();
  source.erase(source.find(displacement), displacement.size());
  auto env = fs::TestEnvironment{};
  const auto input = env.dir() / "ownership.vmf";
  const auto output = env.dir() / "saved.vmf";
  writeBinary(input, source);
  auto fixture = MapFixture{};
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto& map = fixture.load(input, config);
  auto& original = ramp(map);
  deselectAll(map);
  selectNodes(map, {&original});
  if (moved)
  {
    REQUIRE(translateSelection(map, {16, 8, 4}));
  }
  const auto beforeOwnership = map.vmfScene()->serialize(map.worldNode());
  SECTION("duplicate clone move repeated duplicate delete save reopen and history")
  {
    const auto count = map.worldNode().defaultLayer()->childCount();
    duplicateSelectedNodes(map);
    REQUIRE(map.worldNode().defaultLayer()->childCount() == count + 1);
    REQUIRE(map.selection().brushes.size() == 1);
    auto* clone = map.selection().brushes.front();
    REQUIRE(clone != &original);
    const auto cloneId = map.vmfScene()->nativeId(*clone);
    CHECK(cloneId != map.vmfScene()->nativeId(original));
    REQUIRE(translateSelection(map, {8, 4, 2}));
    duplicateSelectedNodes(map);
    REQUIRE(map.worldNode().defaultLayer()->childCount() == count + 2);
    REQUIRE(map.selection().brushes.front() != clone);
    CHECK(map.vmfScene()->nativeId(*map.selection().brushes.front()) != cloneId);
    REQUIRE(removeSelectedNodes(map));
    CHECK(map.worldNode().defaultLayer()->childCount() == count + 1);
    const auto saved = map.vmfScene()->serialize(map.worldNode());
    REQUIRE(saved != beforeOwnership);
    REQUIRE(map.saveAs(output).is_success());
    REQUIRE(map.saveAs(env.dir() / "again.vmf").is_success());
    CHECK(readBinary(output) == readBinary(env.dir() / "again.vmf"));
    if (schema != 0)
    {
      CHECK(
        nativeFragment(saved, "2").find(moved ? "80 72 68" : "64.00 64 64")
        != std::string::npos);
    }
    auto reopened = MapFixture{};
    auto& loaded = reopened.load(output, config);
    auto& subject = nativeNode(loaded, cloneId);
    deselectAll(loaded);
    selectNodes(loaded, {&subject});
    duplicateSelectedNodes(loaded);
    REQUIRE(loaded.worldNode().defaultLayer()->childCount() == count + 2);
    REQUIRE(removeSelectedNodes(loaded));
    CHECK(loaded.vmfScene()->serialize(loaded.worldNode()) == saved);
    deselectAll(map);
    selectNodes(map, {clone});
    REQUIRE(removeSelectedNodes(map));
    CHECK(map.vmfScene()->serialize(map.worldNode()) == beforeOwnership);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == beforeOwnership);
  }
  SECTION("delete original and undo redo preserve exact source ownership")
  {
    REQUIRE(removeSelectedNodes(map));
    CHECK(map.worldNode().defaultLayer()->childCount() == 1);
    const auto deleted = map.vmfScene()->serialize(map.worldNode());
    REQUIRE(deleted != beforeOwnership);
    REQUIRE(map.saveAs(output).is_success());
    auto reopened = MapFixture{};
    auto& loaded = reopened.load(output, config);
    CHECK(loaded.worldNode().defaultLayer()->childCount() == 1);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == beforeOwnership);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == deleted);
  }
  while (map.canUndoCommand())
  {
    map.undoCommand();
  }
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  CHECK(readBinary(input) == source);
}

TEST_CASE("VmfMap polygon cache closed brush entity ownership", "[vmf]")
{
  const auto indexed = GENERATE(false, true);
  const auto reduced = GENERATE(false, true);
  const auto polygon =
    indexed
      ? std::
          string{R"(point_data { numpts 4 point "2 64.00 64 64" point "0 0.00 0 64" point "3 64 0 64" point "1 0 64 64" })"}
      : std::string{
          R"(vertices_plus { v "64.00 64 64" v "64 0 64" v "0.00 0 64" v "0 64 64" })"};
  const auto source = nativePolygonEntity(polygon);
  auto env = fs::TestEnvironment{};
  const auto input = env.dir() / "owner.vmf";
  const auto output = env.dir() / "saved.vmf";
  writeBinary(input, source);
  auto fixture = MapFixture{};
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto& map = fixture.load(input, config);
  auto& owner = nativeNode(map, "10");
  REQUIRE(owner.childCount() == 2);
  deselectAll(map);
  selectNodes(map, {&owner});
  REQUIRE(translateSelection(map, {16, 8, 4}));
  if (reduced)
  {
    deselectAll(map);
    selectNodes(map, {owner.children().back()});
    REQUIRE(removeSelectedNodes(map));
    REQUIRE(owner.childCount() == 1);
  }
  const auto beforeOwnership = map.vmfScene()->serialize(map.worldNode());
  // A selected child duplicates into a partial copy of its ordinary owner.
  deselectAll(map);
  selectNodes(map, {owner.children().front()});
  duplicateSelectedNodes(map);
  REQUIRE(owner.childCount() == (reduced ? 1u : 2u));
  REQUIRE(map.worldNode().defaultLayer()->childCount() == 3);
  REQUIRE(map.selection().nodes.front()->parent() != &owner);
  REQUIRE(removeSelectedNodes(map));
  CHECK(map.vmfScene()->serialize(map.worldNode()) == beforeOwnership);
  auto fresh = std::make_unique<BrushNode>(
    static_cast<BrushNode*>(owner.children().front())->brush());
  for (const auto& face : fresh->brush().faces())
  {
    face.setFilePosition(0, 0);
  }
  REQUIRE_NOTHROW(map.vmfScene()->validateAdditions(owner, {fresh.get()}, false));
  const auto added = addNodes(map, {{&owner, {fresh.release()}}});
  REQUIRE(added.size() == 1);
  REQUIRE(owner.childCount() == (reduced ? 2u : 3u));
  const auto freshId = map.vmfScene()->nativeId(*added.front());
  const auto withFresh = map.vmfScene()->serialize(map.worldNode());
  CHECK(nativeFragment(withFresh, freshId).find("point_data") == std::string::npos);
  CHECK(nativeFragment(withFresh, freshId).find("vertices_plus") == std::string::npos);
  const auto withFreshDocument = VmfDocument::parse(withFresh);
  CHECK(
    std::count_if(
      withFreshDocument.entries().begin(),
      withFreshDocument.entries().end(),
      [&](const auto& entry) {
        const auto name = withFreshDocument.tokenContents(entry.key);
        return entry.isBlock() && (name == "point_data" || name == "vertices_plus");
      })
    == (reduced ? 1 : 2));
  const auto freshOutput = env.dir() / "fresh-parent.vmf";
  REQUIRE(map.saveAs(freshOutput).is_success());
  auto freshReopened = MapFixture{};
  auto& freshLoaded = freshReopened.load(freshOutput, config);
  auto& freshSubject = nativeNode(freshLoaded, freshId);
  selectNodes(freshLoaded, {&freshSubject});
  REQUIRE(translateSelection(freshLoaded, {1, 2, 3}));
  const auto continuedFresh = freshLoaded.vmfScene()->serialize(freshLoaded.worldNode());
  REQUIRE(removeSelectedNodes(freshLoaded));
  const auto removedFresh = freshLoaded.vmfScene()->serialize(freshLoaded.worldNode());
  CHECK(nativeFragment(removedFresh, "2") == nativeFragment(beforeOwnership, "2"));
  freshLoaded.undoCommand();
  CHECK(freshLoaded.vmfScene()->serialize(freshLoaded.worldNode()) == continuedFresh);
  freshLoaded.undoCommand();
  CHECK(freshLoaded.vmfScene()->serialize(freshLoaded.worldNode()) == withFresh);
  freshLoaded.redoCommand();
  freshLoaded.redoCommand();
  CHECK(freshLoaded.vmfScene()->serialize(freshLoaded.worldNode()) == removedFresh);
  deselectAll(map);
  selectNodes(map, added);
  REQUIRE(translateSelection(map, {1, 2, 3}));
  const auto movedFresh = map.vmfScene()->serialize(map.worldNode());
  REQUIRE(movedFresh != withFresh);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == withFresh);
  map.redoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == movedFresh);
  REQUIRE(removeSelectedNodes(map));
  CHECK(map.vmfScene()->serialize(map.worldNode()) == beforeOwnership);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == movedFresh);
  map.redoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == beforeOwnership);
  deselectAll(map);
  selectNodes(map, {&owner});
  duplicateSelectedNodes(map);
  REQUIRE(map.worldNode().defaultLayer()->childCount() == 3);
  auto* clone = map.selection().nodes.front();
  if (dynamic_cast<BrushNode*>(clone))
  {
    clone = clone->parent();
  }
  REQUIRE(dynamic_cast<EntityNode*>(clone));
  REQUIRE(clone != &owner);
  REQUIRE(clone->childCount() == owner.childCount());
  const auto cloneId = map.vmfScene()->nativeId(*clone);
  REQUIRE(translateSelection(map, {8, 4, 2}));
  duplicateSelectedNodes(map);
  REQUIRE(map.worldNode().defaultLayer()->childCount() == 4);
  REQUIRE(removeSelectedNodes(map));
  const auto saved = map.vmfScene()->serialize(map.worldNode());
  REQUIRE(map.saveAs(output).is_success());
  REQUIRE(map.saveAs(env.dir() / "again.vmf").is_success());
  CHECK(readBinary(output) == readBinary(env.dir() / "again.vmf"));
  auto reopened = MapFixture{};
  auto& loaded = reopened.load(output, config);
  auto& next = nativeNode(loaded, cloneId);
  deselectAll(loaded);
  selectNodes(loaded, {&next});
  REQUIRE(translateSelection(loaded, {1, 2, 3}));
  duplicateSelectedNodes(loaded);
  REQUIRE(loaded.worldNode().defaultLayer()->childCount() == 4);
  REQUIRE(removeSelectedNodes(loaded));
  deselectAll(loaded);
  selectNodes(loaded, {&next});
  REQUIRE(removeSelectedNodes(loaded));
  deselectAll(map);
  selectNodes(map, {clone});
  REQUIRE(removeSelectedNodes(map));
  CHECK(map.vmfScene()->serialize(map.worldNode()) == beforeOwnership);
  deselectAll(map);
  selectNodes(map, {&owner});
  REQUIRE(removeSelectedNodes(map));
  REQUIRE(map.worldNode().defaultLayer()->childCount() == 1);
  const auto deleted = map.vmfScene()->serialize(map.worldNode());
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == beforeOwnership);
  map.redoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == deleted);
  while (map.canUndoCommand())
  {
    map.undoCommand();
  }
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  CHECK(readBinary(input) == source);
}

TEST_CASE("VmfMap polygon cache closed owner invalid batch is atomic", "[vmf]")
{
  const auto indexed = GENERATE(false, true);
  const auto fault = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
  auto polygon =
    indexed
      ? std::
          string{R"(point_data { numpts 4 point "2 64 64 64" point "0 0 0 64" point "3 64 0 64" point "1 0 64 64" })"}
      : std::string{
          R"(vertices_plus { v "64 64 64" v "64 0 64" v "0 0 64" v "0 64 64" })"};
  if (fault == 2)
  {
    polygon += test::displacementBlock();
  }
  if (fault == 4)
  {
    polygon.replace(polygon.find("0 64 64"), 7, "1 64 64");
  }
  auto source = nativePolygonEntity(polygon);
  if (fault == 0)
  {
    source.insert(source.rfind('}'), "future_owner_payload {} ");
  }
  if (fault == 1)
  {
    const auto at = source.find("smoothing_groups", source.find("entity { id 10"));
    source.insert(at, "future_side_payload {} ");
  }
  if (fault == 3)
  {
    source.replace(source.find("func_detail"), 11, "func_instance");
  }
  const auto cacheName =
    indexed ? std::string{"point_data"} : std::string{"vertices_plus"};
  if (fault == 5)
  {
    source.insert(source.rfind('}'), cacheName + " invalid ");
  }
  if (fault == 6)
  {
    source.insert(source.rfind('}'), "editor { " + polygon + " } ");
  }
  if (fault == 7)
  {
    const auto at = source.find("side {", source.find("entity { id 10"));
    source.insert(at, cacheName + " invalid ");
  }
  auto env = fs::TestEnvironment{};
  const auto input = env.dir() / "invalid-owner.vmf";
  writeBinary(input, source);
  auto fixture = MapFixture{};
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto& map = fixture.load(input, config);
  auto& owner = nativeNode(map, "10");
  auto& marker = nativeNode(map, "9");
  deselectAll(map);
  selectNodes(map, {&marker, &owner});
  const auto selection = map.selection().nodes;
  const auto positions = filePositions(map);
  const auto count = map.modificationCount();
  const auto history = map.undoCommandName();
  const auto dirty = map.modified();
  duplicateSelectedNodes(map);
  CHECK(map.worldNode().defaultLayer()->childCount() == 2);
  CHECK_FALSE(removeSelectedNodes(map));
  auto fresh = std::make_unique<BrushNode>(
    static_cast<BrushNode*>(owner.children().front())->brush());
  for (const auto& face : fresh->brush().faces())
  {
    face.setFilePosition(0, 0);
  }
  CHECK(addNodes(map, {{&owner, {fresh.release()}}}).empty());
  CHECK(map.selection().nodes == selection);
  CHECK(filePositions(map) == positions);
  CHECK(map.modificationCount() == count);
  CHECK(map.undoCommandName() == history);
  CHECK(map.modified() == dirty);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  deselectAll(map);
  selectNodes(map, {&marker});
  duplicateSelectedNodes(map);
  REQUIRE(map.selection().nodes.front() != &marker);
  CHECK(map.vmfScene()->nativeId(*map.selection().nodes.front()) == "12");
  CHECK(readBinary(input) == source);
}

TEST_CASE("VmfMap polygon ownership retains matched plain reference policy", "[vmf]")
{
  const auto schema = GENERATE(0, 1, 2);
  const auto root = GENERATE(false, true);
  const auto polygon =
    schema == 0 ? std::string{}
    : schema == 1
      ? std::
          string{R"(point_data { numpts 4 point "2 64 64 64" point "0 0 0 64" point "3 64 0 64" point "1 0 64 64" })"}
      : std::string{
          R"(vertices_plus { v "64 64 64" v "64 0 64" v "0 0 64" v "0 64 64" })"};
  auto source = nativePolygonRoom(polygon);
  source +=
    root ? "future_root {}" : R"(entity { id 20 classname info_overlay sides "8" })";
  auto env = fs::TestEnvironment{};
  const auto input = env.dir() / "references.vmf";
  writeBinary(input, source);
  auto fixture = MapFixture{};
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto& map = fixture.load(input, config);
  auto& original = nativeNode(map, "2");
  deselectAll(map);
  selectNodes(map, {&original});
  const auto count = map.worldNode().defaultLayer()->childCount();
  duplicateSelectedNodes(map);
  REQUIRE(map.worldNode().defaultLayer()->childCount() == count + 1);
  REQUIRE(removeSelectedNodes(map));
  deselectAll(map);
  selectNodes(map, {&original});
  const auto selection = map.selection().nodes;
  const auto history = map.undoCommandName();
  const auto changes = map.modificationCount();
  CHECK_FALSE(removeSelectedNodes(map));
  CHECK(map.selection().nodes == selection);
  CHECK(map.undoCommandName() == history);
  CHECK(map.modificationCount() == changes);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
}

TEST_CASE("VmfMap plain clipped entity ownership remains supported", "[vmf]")
{
  const auto source = nativePolygonEntity("");
  auto env = fs::TestEnvironment{};
  const auto input = env.dir() / "plain-clipped.vmf";
  writeBinary(input, source);
  auto fixture = MapFixture{};
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto& map = fixture.load(input, config);
  auto& owner = nativeNode(map, "10");
  auto& child = static_cast<BrushNode&>(nativeNode(map, "2"));
  const auto before = child.brush();
  REQUIRE(clipBrushes(map, {&child}, {{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}}));
  REQUIRE(child.brush() != before);
  const auto clipped = map.vmfScene()->serialize(map.worldNode());
  selectNodes(map, {&owner});
  duplicateSelectedNodes(map);
  REQUIRE(map.worldNode().defaultLayer()->childCount() == 3);
  REQUIRE(removeSelectedNodes(map));
  CHECK(map.vmfScene()->serialize(map.worldNode()) == clipped);
  deselectAll(map);
  selectNodes(map, {&child});
  REQUIRE(removeSelectedNodes(map));
  REQUIRE(owner.childCount() == 1);
  const auto reduced = map.vmfScene()->serialize(map.worldNode());
  auto fresh = std::make_unique<BrushNode>(
    static_cast<BrushNode*>(owner.children().front())->brush());
  for (const auto& face : fresh->brush().faces())
  {
    face.setFilePosition(0, 0);
  }
  const auto added = addNodes(map, {{&owner, {fresh.release()}}});
  REQUIRE(added.size() == 1);
  deselectAll(map);
  selectNodes(map, added);
  REQUIRE(removeSelectedNodes(map));
  CHECK(map.vmfScene()->serialize(map.worldNode()) == reduced);
  deselectAll(map);
  selectNodes(map, {&owner});
  duplicateSelectedNodes(map);
  REQUIRE(map.worldNode().defaultLayer()->childCount() == 3);
  REQUIRE(removeSelectedNodes(map));
  CHECK(map.vmfScene()->serialize(map.worldNode()) == reduced);
  const auto output = env.dir() / "saved.vmf";
  REQUIRE(map.saveAs(output).is_success());
  auto reopened = MapFixture{};
  auto& loaded = reopened.load(output, config);
  CHECK(loaded.vmfScene()->serialize(loaded.worldNode()) == reduced);
  while (map.canUndoCommand())
  {
    map.undoCommand();
  }
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
}

TEST_CASE("VmfMap polygon cache installed ownership opt-in", "[.][vmf][installed]")
{
  const auto* input = std::getenv("SOURCEBROOM_NATIVE_EDIT_INPUT");
  const auto* variant = std::getenv("SOURCEBROOM_NATIVE_EDIT_VARIANT");
  const auto* entry = std::getenv("SOURCEBROOM_NATIVE_EDIT_ENTRY");
  REQUIRE(input);
  REQUIRE(variant);
  REQUIRE(entry);
  const auto operation = GENERATE(0, 1);
  const auto bounds = vm::bbox3d{VmfScene::PreviewWorldExtent};
  auto identity = VmfScene::read(readBinary(input), bounds);
  auto nativeId = std::string{};
  const auto findEntry = [&](const auto& self, const Node& node) -> void {
    if (node.lineNumber() == std::stoull(entry) + 1)
    {
      const auto* brush = dynamic_cast<const BrushNode*>(&node);
      REQUIRE(brush);
      for (const auto& face : brush->brush().faces())
      {
        REQUIRE_FALSE(face.sourceDisplacement());
      }
      nativeId = identity.nativeId(node);
    }
    for (const auto* child : node.children())
    {
      self(self, *child);
    }
  };
  findEntry(findEntry, identity.world());
  REQUIRE_FALSE(nativeId.empty());
  INFO("Native ordinary subject " << nativeId);
  auto env = fs::TestEnvironment{};
  const auto inputs = std::array<std::filesystem::path, 2>{input, variant};
  for (size_t i = 0; i < inputs.size(); ++i)
  {
    INFO("Variant index " << i << "; operation " << operation);
    const auto bytes = readBinary(inputs[i]);
    auto fixture = MapFixture{};
    const auto config =
      MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = bounds};
    auto& map = fixture.load(inputs[i], config);
    const auto find =
      [&](const auto& self, Node& node, VmfScene& scene, const std::string& id) -> Node* {
      if (dynamic_cast<BrushNode*>(&node) && scene.nativeId(node) == id)
      {
        return &node;
      }
      for (auto* child : node.children())
      {
        if (auto* result = self(self, *child, scene, id))
        {
          return result;
        }
      }
      return nullptr;
    };
    auto* subject =
      dynamic_cast<BrushNode*>(find(find, map.worldNode(), *map.vmfScene(), nativeId));
    REQUIRE(subject);
    auto caches = size_t{0};
    for (const auto& face : subject->brush().faces())
    {
      REQUIRE_FALSE(face.sourceDisplacement());
      for (const auto child :
           map.vmfScene()->document().entries()[face.lineNumber() - 1].children)
      {
        const auto name = map.vmfScene()->document().tokenContents(
          map.vmfScene()->document().entries()[child].key);
        if (name == "point_data" || name == "vertices_plus")
        {
          ++caches;
        }
      }
    }
    REQUIRE(caches > 0);
    deselectAll(map);
    selectNodes(map, {subject});
    if (operation == 0)
    {
      REQUIRE(translateSelection(map, {16, 8, 4}));
      const auto parentCount = subject->parent()->childCount();
      duplicateSelectedNodes(map);
      REQUIRE(subject->parent()->childCount() == parentCount + 1);
      auto* clone = map.selection().brushes.front();
      REQUIRE(clone != subject);
      const auto cloneId = map.vmfScene()->nativeId(*clone);
      REQUIRE(translateSelection(map, {8, 4, 2}));
      duplicateSelectedNodes(map);
      REQUIRE(subject->parent()->childCount() == parentCount + 2);
      REQUIRE(removeSelectedNodes(map));
      const auto saved = map.vmfScene()->serialize(map.worldNode());
      const auto output = env.dir() / ("clone-" + std::to_string(i) + ".vmf");
      REQUIRE(map.saveAs(output).is_success());
      REQUIRE(
        map.saveAs(env.dir() / ("again-" + std::to_string(i) + ".vmf")).is_success());
      CHECK(readBinary(output) == saved);
      auto reopened = MapFixture{};
      auto& loaded = reopened.load(output, config);
      auto* next = find(find, loaded.worldNode(), *loaded.vmfScene(), cloneId);
      REQUIRE(next);
      deselectAll(loaded);
      selectNodes(loaded, {next});
      duplicateSelectedNodes(loaded);
      REQUIRE(loaded.selection().nodes.front() != next);
      REQUIRE(removeSelectedNodes(loaded));
      CHECK(loaded.vmfScene()->serialize(loaded.worldNode()) == saved);
      deselectAll(map);
      selectNodes(map, {clone});
      REQUIRE(removeSelectedNodes(map));
      const auto deleted = map.vmfScene()->serialize(map.worldNode());
      map.undoCommand();
      CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
      map.redoCommand();
      CHECK(map.vmfScene()->serialize(map.worldNode()) == deleted);
    }
    else
    {
      REQUIRE(removeSelectedNodes(map));
      const auto deleted = map.vmfScene()->serialize(map.worldNode());
      const auto output = env.dir() / ("deleted-" + std::to_string(i) + ".vmf");
      REQUIRE(map.saveAs(output).is_success());
      auto reopened = MapFixture{};
      auto& loaded = reopened.load(output, config);
      CHECK(loaded.vmfScene()->serialize(loaded.worldNode()) == deleted);
      map.undoCommand();
      CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
      map.redoCommand();
      CHECK(map.vmfScene()->serialize(map.worldNode()) == deleted);
    }
    while (map.canUndoCommand())
    {
      map.undoCommand();
    }
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
    CHECK(readBinary(inputs[i]) == bytes);
  }
}

TEST_CASE("VmfMap coupled polygon displacement installed opt-in", "[.][vmf][installed]")
{
  const auto* input = std::getenv("SOURCEBROOM_NATIVE_EDIT_INPUT");
  const auto* variant = std::getenv("SOURCEBROOM_NATIVE_EDIT_VARIANT");
  REQUIRE(input);
  REQUIRE(variant);
  const auto bounds = vm::bbox3d{VmfScene::PreviewWorldExtent};
  const auto original = readBinary(input);
  auto discovery = VmfScene::read(original, bounds);
  auto nativeId = std::string{};
  const auto coupled = [](const VmfScene& scene, const BrushNode& node) {
    auto parsed = false;
    auto polygon = false;
    const auto& document = scene.document();
    for (const auto& face : node.brush().faces())
    {
      parsed = parsed || bool(face.sourceDisplacement());
      for (const auto child : document.entries()[face.lineNumber() - 1].children)
      {
        const auto name = document.tokenContents(document.entries()[child].key);
        polygon = polygon || name == "point_data" || name == "vertices_plus";
      }
    }
    return parsed && polygon;
  };
  const auto discover = [&](const auto& self, const Node& node) -> void {
    if (nativeId.empty())
    {
      if (
        const auto* brush = dynamic_cast<const BrushNode*>(&node);
        brush && coupled(discovery, *brush))
      {
        nativeId = discovery.nativeId(node);
      }
      for (const auto* child : node.children())
      {
        self(self, *child);
      }
    }
  };
  discover(discover, discovery.world());
  INFO("Discovered coupled native solid " << nativeId);
  REQUIRE_FALSE(nativeId.empty());
  const auto inputs = std::array<std::filesystem::path, 2>{input, variant};
  auto env = fs::TestEnvironment{};
  for (size_t i = 0; i < inputs.size(); ++i)
  {
    INFO("Input variant index " << i);
    const auto bytes = readBinary(inputs[i]);
    auto fixture = MapFixture{};
    const auto config =
      MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = bounds};
    auto& map = fixture.load(inputs[i], config);
    const auto find = [&](const auto& self, Node& node, VmfScene& scene) -> BrushNode* {
      if (
        auto* brush = dynamic_cast<BrushNode*>(&node);
        brush && scene.nativeId(node) == nativeId)
      {
        return brush;
      }
      for (auto* child : node.children())
      {
        if (auto* found = self(self, *child, scene))
        {
          return found;
        }
      }
      return nullptr;
    };
    auto* selected = find(find, map.worldNode(), *map.vmfScene());
    REQUIRE(selected);
    REQUIRE(coupled(*map.vmfScene(), *selected));
    const auto before = selected->brush();
    auto candidate = before;
    REQUIRE(candidate
              .transform(
                bounds,
                vm::translation_matrix(vm::vec3d{16, 8, 4}),
                true,
                SourceTransformPolicy::TranslationOnly)
              .is_success());
    REQUIRE(candidate != before);
    CHECK_NOTHROW(
      map.vmfScene()->validateContents(*selected, NodeContents{candidate}, bounds));
    selectNodes(map, {selected});
    const auto selection = map.selection().nodes;
    const auto count = map.modificationCount();
    REQUIRE(translateSelection(map, {16, 8, 4}));
    REQUIRE(selected->brush() != before);
    CHECK(map.modificationCount() == count + 1);
    CHECK(map.selection().nodes == selection);
    CHECK(map.vmfScene()->nativeId(*selected) == nativeId);
    const auto saved = map.vmfScene()->serialize(map.worldNode());
    REQUIRE(saved != bytes);
    const auto output = env.dir() / ("coupled-" + std::to_string(i) + ".vmf");
    REQUIRE(map.saveAs(output).is_success());
    CHECK(readBinary(output) == saved);
    auto reopened = MapFixture{};
    auto& loaded = reopened.load(output, config);
    CHECK(loaded.vmfScene()->serialize(loaded.worldNode()) == saved);
    auto* next = find(find, loaded.worldNode(), *loaded.vmfScene());
    REQUIRE(next);
    REQUIRE(coupled(*loaded.vmfScene(), *next));
    const auto reopenedBefore = next->brush();
    selectNodes(loaded, {next});
    REQUIRE(translateSelection(loaded, {1, 2, 3}));
    REQUIRE(next->brush() != reopenedBefore);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
    CHECK(map.selection().nodes == selection);
    CHECK(readBinary(inputs[i]) == bytes);
  }
}

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
  CHECK(node.physicalBounds().contains(mesh.bounds()));
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

TEST_CASE(
  "VmfMap displacement uniform scaling through both APIs undo and continued saving",
  "[vmf]")
{
  const auto lockUv = GENERATE(false, true);
  const auto factor = GENERATE(2.0, 0.5);
  const auto boundingBoxApi = GENERATE(false, true);
  auto env = fs::TestEnvironment{};
  const auto originalPath = env.dir() / "displacement.vmf";
  const auto outputPath = env.dir() / "rotated displacement.vmf";
  auto original = nativeDisplacementRoom();
  const auto elevation = original.find("elevation 0");
  REQUIRE(elevation != std::string::npos);
  original.replace(
    elevation,
    std::string{"elevation 0"}.size(),
    "elevation 2 offsets { row2 \"0 0 0 0 0 0 1 2 3 0 0 0 0 0 0\" } "
    "offset_normals { row2 \"0 0 0 0 0 0 4 5 6 0 0 0 0 0 0\" }");
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
  REQUIRE(oldMesh.positions()[12] == vm::vec3d{33, 34, 85});
  const auto center = vm::vec3d{8, 16, 24};
  const auto transform = vm::translation_matrix(center)
                         * vm::scaling_matrix(vm::vec3d{factor, factor, factor})
                         * vm::translation_matrix(-center);
  auto control = before;
  control.face(*oldFace).setSourceDisplacement(std::nullopt);
  REQUIRE(control.transform(map.worldBounds(), transform, lockUv).is_success());
  selectNodes(map, {&node});
  const auto count = map.modificationCount();
  if (boundingBoxApi)
  {
    const auto box = node.brush().bounds();
    REQUIRE(
      scaleSelection(map, box, vm::bbox3d{transform * box.min, transform * box.max}));
  }
  else
  {
    REQUIRE(scaleSelection(map, center, {factor, factor, factor}));
  }
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
  CHECK(node.physicalBounds().contains(mesh.bounds()));
  CHECK(mesh.indices() == oldMesh.indices());
  CHECK(mesh.alphas() == oldMesh.alphas());
  const auto base = mesh.basePositions()[12];
  CHECK(
    vm::is_equal(
      node.brush().face(faceIndex).uvCoords(base),
      control.face(faceIndex).uvCoords(base),
      0.0001f));
  auto hits = PickResult{};
  pick(map, vm::ray3d{transform * vm::vec3d{33, 34, 128}, vm::vec3d{0, 0, -1}}, hits);
  REQUIRE_FALSE(hits.empty());
  CHECK(hits.all()[0].distance() == Catch::Approx(43 * factor).margin(0.000001));
  CHECK(
    vm::is_equal(
      hits.all()[0].hitPoint(), transform * oldMesh.positions()[12], 0.000001));
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  map.redoCommand();
  const auto rotation = vm::rotation_matrix(vm::vec3d{1, 0, 0}, 0.37);
  REQUIRE(rotateSelection(map, {0, 0, 0}, {1, 0, 0}, 0.37));
  const auto delta = vm::vec3d{16, 8, 4};
  REQUIRE(translateSelection(map, delta));
  deselectAll(map);
  const auto continuedFace = meshFace(node.brush());
  REQUIRE(node.brush().face(continuedFace).sourceDisplacement());
  CHECK(
    node.brush().face(continuedFace).lineNumber() == before.face(*oldFace).lineNumber());
  selectBrushFaces(map, {BrushFaceHandle{node, continuedFace}});
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 24 * factor));
  const auto expected = rotation * (transform * vm::vec3d{33, 34, 93}) + delta;
  CHECK(
    vm::is_equal(
      node.brush().face(continuedFace).sourceDisplacement()->positions()[12],
      expected,
      0.000001));
  CAPTURE(lockUv, factor, boundingBoxApi);
  const auto saveResult = map.saveAs(outputPath);
  auto saveError = std::string{};
  saveResult | kdl::if_error([&](const auto& error) { saveError = error.msg; })
    | kdl::ignore();
  INFO(saveError);
  REQUIRE(saveResult.is_success());
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
  CHECK_FALSE(scaleSelection(map, {0, 0, 0}, {-1, -1, -1}));
  CHECK_FALSE(scaleSelection(map, {0, 0, 0}, {0, 0, 0}));
  CHECK_FALSE(
    scaleSelection(map, {0, 0, 0}, {std::numeric_limits<double>::quiet_NaN(), 1, 1}));
  CHECK_FALSE(transformSelection(
    map, "Unclassified uniform scale", vm::scaling_matrix(vm::vec3d{2, 2, 2})));
  const auto box = node.brush().bounds();
  CHECK_FALSE(scaleSelection(
    map,
    box,
    vm::bbox3d{box.min, box.min + box.size() * vm::vec3d{1.0000000000001, 1, 1}}));
  CHECK_FALSE(flipSelection(map, {0, 0, 0}, vm::axis::x));
  CHECK(map.modificationCount() == beforeCount);
  CHECK(map.undoCommandName() == beforeHistory);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
  deselectAll(map);
  selectBrushFaces(map, {BrushFaceHandle{node, continuedFace}});
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 32 * factor));
  REQUIRE(map.save().is_success());
  CHECK(readBinary(outputPath) != saved);
  map.undoCommand();
  REQUIRE(map.save().is_success());
  CHECK(readBinary(outputPath) == saved);
  CHECK(readBinary(originalPath) == original);
  deselectAll(map);
  selectNodes(map, {&node});
  // Continue scaling an already obliquely rotated, moved and height-edited frame.
  if (boundingBoxApi)
  {
    const auto continuedBox = node.brush().bounds();
    REQUIRE(scaleSelection(
      map, continuedBox, vm::bbox3d{continuedBox.min * 0.5, continuedBox.max * 0.5}));
  }
  else
  {
    REQUIRE(scaleSelection(map, {0, 0, 0}, {0.5, 0.5, 0.5}));
  }
  CHECK(
    vm::is_equal(
      node.brush().face(meshFace(node.brush())).sourceDisplacement()->positions()[12],
      expected * 0.5,
      0.000001));
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == saved);
}

TEST_CASE(
  "VmfMap native scale overflow and cancellation refuse before history changes", "[vmf]")
{
  const auto boundingBoxApi = GENERATE(false, true);
  const auto overflow = GENERATE(false, true);
  const auto factor = overflow ? 2.0 : 0.5;
  const auto center = overflow ? vm::vec3d{} : vm::vec3d{0, 0, 2};
  const auto transform = vm::translation_matrix(center)
                         * vm::scaling_matrix(vm::vec3d{factor, factor, factor})
                         * vm::translation_matrix(-center);
  auto original = nativeDisplacementRoom();
  const auto elevation = original.find("elevation 0");
  REQUIRE(elevation != std::string::npos);
  original.replace(elevation, std::string{"elevation 0"}.size(), "elevation 1e308");
  for (const auto row : {"0.00 +0  0  0 0", "0.00 +0  16  0 0"})
  {
    while (original.find(row) != std::string::npos)
    {
      original.replace(
        original.find(row),
        std::string{row}.size(),
        "-1e308 -1e308 -1e308 -1e308 -1e308");
    }
  }
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "finite cancelled displacement.vmf";
  writeBinary(path, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto& node = ramp(map);
  selectNodes(map, {&node});
  const auto count = map.modificationCount();
  const auto history = map.undoCommandName();
  if (boundingBoxApi)
  {
    const auto box = node.brush().bounds();
    CHECK_FALSE(
      scaleSelection(map, box, vm::bbox3d{transform * box.min, transform * box.max}));
  }
  else
  {
    CHECK_FALSE(scaleSelection(map, center, {factor, factor, factor}));
  }
  CHECK(map.modificationCount() == count);
  CHECK(map.undoCommandName() == history);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  CHECK(readBinary(path) == original);
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

TEST_CASE("VmfMap explicitly initializes absent alpha rows with exact history", "[vmf]")
{
  auto original = nativeDisplacementRoom();
  const auto parsed = VmfDocument::parse(original);
  const auto alpha =
    std::find_if(parsed.entries().begin(), parsed.entries().end(), [&](const auto& e) {
      return e.isBlock() && parsed.tokenContents(e.key) == "alphas";
    });
  REQUIRE(alpha != parsed.entries().end());
  original.erase(alpha->key.offset, alpha->end - alpha->key.offset);
  auto env = fs::TestEnvironment{};
  const auto originalPath = env.dir() / "missing alpha original.vmf";
  writeBinary(originalPath, original);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(originalPath, config);
  auto& node = ramp(map);
  const auto face = node.brush().findFace(vm::vec3d{0, 0, 1});
  REQUIRE(face);
  selectBrushFaces(map, {BrushFaceHandle{node, *face}});
  const auto before = *node.brush().face(*face).sourceDisplacement();
  REQUIRE(before.canInitializeAlphas());
  REQUIRE_FALSE(before.canEditAlphas());
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 24));
  CHECK(map.vmfScene()->serialize(map.worldNode()).find("24") != std::string::npos);
  map.undoCommand();
  REQUIRE(map.vmfScene()->serialize(map.worldNode()) == original);
  CHECK_FALSE(setSourceDisplacementAlpha(map, 2, 2, 17.5));
  CHECK_THROWS_AS(map.vmfScene()->validateDuplication({&node}), std::invalid_argument);

  SECTION("initialization scalar history save reopen and translation preserve data")
  {
    const auto count = map.modificationCount();
    const auto initialized = initializeSourceDisplacementAlphas(map);
    if (!initialized)
    {
      CHECK(map.modificationCount() == count);
      CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
      CHECK(*node.brush().face(*face).sourceDisplacement() == before);
    }
    REQUIRE(initialized);
    CHECK(map.modificationCount() == count + 1);
    CHECK(node.brush().face(*face).sourceDisplacement()->canEditAlphas());
    CHECK(
      node.brush().face(*face).sourceDisplacement()->alphas()
      == std::vector<double>(25, 0));
    CHECK_FALSE(initializeSourceDisplacementAlphas(map));
    REQUIRE(setSourceDisplacementAlpha(map, 2, 2, 17.5));
    CHECK(
      node.brush().face(*face).sourceDisplacement()->positions() == before.positions());
    const auto authored = map.vmfScene()->serialize(map.worldNode());
    map.undoCommand();
    CHECK(node.brush().face(*face).sourceDisplacement()->alpha(2, 2) == 0);
    CHECK(node.brush().face(*face).sourceDisplacement()->canEditAlphas());
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    CHECK(node.brush().face(*face).sourceDisplacement()->canInitializeAlphas());
    map.redoCommand();
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == authored);
    const auto savePath = env.dir() / "initialized alpha SaveAs.vmf";
    REQUIRE(map.saveAs(savePath).is_success());
    CHECK(readBinary(originalPath) == original);
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(savePath, config);
    auto& reopenedNode = ramp(reopened);
    const auto reopenedFace = reopenedNode.brush().findFace(vm::vec3d{0, 0, 1});
    REQUIRE(reopenedFace);
    CHECK(
      reopenedNode.brush().face(*reopenedFace).sourceDisplacement()->alpha(2, 2) == 17.5);
    CHECK_FALSE(reopenedNode.brush()
                  .face(*reopenedFace)
                  .sourceDisplacement()
                  ->canInitializeAlphas());
    deselectAll(map);
    selectNodes(map, {&node});
    const auto cloneCount = map.modificationCount();
    duplicateSelectedNodes(map);
    REQUIRE(map.selection().brushes.size() == 1);
    CHECK(map.selection().brushes.front() == &node);
    CHECK(map.modificationCount() == cloneCount);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == authored);
    CHECK_THROWS_AS(map.vmfScene()->validateDuplication({&node}), std::invalid_argument);
    const auto delta = vm::vec3d{96, 0, 0};
    REQUIRE(
      transformSelection(map, "Move initialized alpha", vm::translation_matrix(delta)));
    const auto moved = map.vmfScene()->serialize(map.worldNode());
    CHECK(moved != authored);
    CHECK(
      node.brush().face(*face).sourceDisplacement()->positions()[12]
      == before.positions()[12] + delta);
    REQUIRE(map.saveAs(savePath).is_success());
    auto movedFixture = MapFixture{};
    auto& movedMap = movedFixture.load(savePath, config);
    auto& movedNode = ramp(movedMap);
    const auto movedFace = movedNode.brush().findFace(vm::vec3d{0, 0, 1});
    REQUIRE(movedFace);
    CHECK(movedNode.brush().face(*movedFace).sourceDisplacement()->alpha(2, 2) == 17.5);
    CHECK(
      movedNode.brush().face(*movedFace).sourceDisplacement()->positions()[12]
      == before.positions()[12] + delta);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == authored);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == moved);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == authored);
    CHECK(readBinary(originalPath) == original);

    const auto plainPath = env.dir() / "ordinary clone control.vmf";
    writeBinary(plainPath, nativeRamp(false));
    auto plainFixture = MapFixture{};
    auto& plain = plainFixture.load(plainPath, config);
    auto& plainNode = ramp(plain);
    selectNodes(plain, {&plainNode});
    duplicateSelectedNodes(plain);
    REQUIRE(plain.selection().brushes.size() == 1);
    CHECK(plain.selection().brushes.front() != &plainNode);
  }
  SECTION("explicit row creation is an undo boundary with normal collation enabled")
  {
    map.setIsCommandCollationEnabled(true);
    REQUIRE(setSourceDisplacementDistance(map, 2, 2, 24));
    const auto distanceOnly = map.vmfScene()->serialize(map.worldNode());
    REQUIRE(initializeSourceDisplacementAlphas(map));
    REQUIRE(setSourceDisplacementAlpha(map, 2, 2, 17.5));
    REQUIRE(setSourceDisplacementAlpha(map, 2, 2, 23.25));
    const auto authored = map.vmfScene()->serialize(map.worldNode());
    map.undoCommand();
    CHECK(node.brush().face(*face).sourceDisplacement()->alpha(2, 2) == 0);
    CHECK(node.brush().face(*face).sourceDisplacement()->canEditAlphas());
    map.undoCommand();
    CHECK(node.brush().face(*face).sourceDisplacement()->canInitializeAlphas());
    CHECK(map.vmfScene()->serialize(map.worldNode()) == distanceOnly);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    map.redoCommand();
    map.redoCommand();
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == authored);
    CHECK(node.brush().face(*face).sourceDisplacement()->alpha(2, 2) == 23.25);
  }
  SECTION("no selection and a plain side do not create data")
  {
    deselectAll(map);
    const auto count = map.modificationCount();
    CHECK_FALSE(initializeSourceDisplacementAlphas(map));
    selectBrushFaces(
      map, {BrushFaceHandle{node, (*face + 1) % node.brush().faceCount()}});
    CHECK_FALSE(initializeSourceDisplacementAlphas(map));
    CHECK(map.modificationCount() == count);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  }
  SECTION("explicit read-only preview cannot initialize even a valid absent table")
  {
    auto previewConfig = config;
    previewConfig.readOnlyVmfPreview = true;
    auto previewFixture = MapFixture{};
    auto& preview = previewFixture.load(originalPath, previewConfig);
    auto& previewNode = ramp(preview);
    const auto previewFace = previewNode.brush().findFace(vm::vec3d{0, 0, 1});
    REQUIRE(previewFace);
    selectBrushFaces(preview, {BrushFaceHandle{previewNode, *previewFace}});
    const auto count = preview.modificationCount();
    CHECK_FALSE(initializeSourceDisplacementAlphas(preview));
    CHECK(preview.modificationCount() == count);
    CHECK(preview.vmfScene()->serialize(preview.worldNode()) == original);
  }
}

TEST_CASE(
  "VmfMap displacement alpha command preserves geometry and exact history", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto originalPath = env.dir() / "alpha original.vmf";
  const auto original = nativeDisplacementRoom();
  writeBinary(originalPath, original);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(originalPath, config);
  auto& node = ramp(map);
  const auto index = node.brush().findFace(vm::vec3d{0, 0, 1});
  REQUIRE(index);
  REQUIRE(node.brush().face(*index).sourceDisplacement());
  selectBrushFaces(map, {BrushFaceHandle{node, *index}});
  const auto before = *node.brush().face(*index).sourceDisplacement();
  const auto positions = filePositions(map);
  const auto bounds = node.physicalBounds();
  REQUIRE(before.canEditAlphas());
  CHECK(before.alpha(2, 2) == 128);
  // Actual pre-existing distance command controls the alpha publish gate.
  REQUIRE(setSourceDisplacementDistance(map, 2, 2, 24));
  CHECK(node.physicalBounds().max.z() == 88);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  const auto count = map.modificationCount();
  SECTION("no-op and invalid input never add history or mutate source")
  {
    CHECK(setSourceDisplacementAlpha(map, 2, 2, 128));
    CHECK_FALSE(setSourceDisplacementAlpha(map, 5, 2, 17));
    CHECK_FALSE(setSourceDisplacementAlpha(map, 2, 5, 17));
    CHECK_FALSE(setSourceDisplacementAlpha(map, 2, 2, -1));
    CHECK_FALSE(setSourceDisplacementAlpha(map, 2, 2, 256));
    CHECK(map.modificationCount() == count);
    CHECK(*node.brush().face(*index).sourceDisplacement() == before);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  }
  SECTION("requires exactly one selected native displacement face")
  {
    deselectAll(map);
    CHECK_FALSE(setSourceDisplacementAlpha(map, 2, 2, 17));
    const auto ordinary = (*index + 1) % node.brush().faceCount();
    selectBrushFaces(map, {BrushFaceHandle{node, ordinary}});
    CHECK_FALSE(setSourceDisplacementAlpha(map, 2, 2, 17));
    deselectAll(map);
    selectBrushFaces(
      map, {BrushFaceHandle{node, ordinary}, BrushFaceHandle{node, *index}});
    CHECK_FALSE(setSourceDisplacementAlpha(map, 2, 2, 17));
    CHECK(map.modificationCount() == count);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  }
  SECTION("fractional scalar save reopen undo redo and subsequent movement")
  {
    const auto weights = [&] {
      auto& cache = node.brushRendererBrushCache();
      cache.validateVertexCache(node);
      const auto& faces = cache.cachedFacesSortedByMaterial();
      const auto found = std::find_if(faces.begin(), faces.end(), [](const auto& face) {
        return bool(face.face->sourceDisplacement());
      });
      REQUIRE(found != faces.end());
      for (const auto& face : faces)
      {
        if (!face.face->sourceDisplacement())
        {
          CHECK(
            gl::getVertexComponent<3>(
              cache.cachedVertices()[face.indexOfFirstVertexRelativeToBrush])[0]
            == 0);
        }
      }
      const auto first = found->indexOfFirstVertexRelativeToBrush;
      return std::array<float, 3>{
        gl::getVertexComponent<3>(cache.cachedVertices()[first])[0],
        gl::getVertexComponent<3>(cache.cachedVertices()[first + 12])[0],
        gl::getVertexComponent<3>(cache.cachedVertices()[first + 4])[0]};
    };
    // Populate the real node cache before a geometry-identical scalar command.
    CHECK(weights() == std::array<float, 3>{0, static_cast<float>(128.0 / 255.0), 1});
    REQUIRE(setSourceDisplacementAlpha(map, 2, 2, 17.5));
    CHECK(weights() == std::array<float, 3>{0, static_cast<float>(17.5 / 255.0), 1});
    CHECK(map.modificationCount() == count + 1);
    REQUIRE(map.undoCommandName());
    CHECK(*map.undoCommandName() == "Change Displacement Alpha");
    const auto after = node.brush().face(*index).sourceDisplacement();
    CHECK(after->alpha(2, 2) == 17.5);
    CHECK(after->positions() == before.positions());
    CHECK(after->basePositions() == before.basePositions());
    CHECK(after->normals() == before.normals());
    CHECK(after->indices() == before.indices());
    CHECK(node.physicalBounds() == bounds);
    CHECK(filePositions(map) == positions);
    auto hits = PickResult{};
    pick(map, vm::ray3d{{32, 32, 128}, {0, 0, -1}}, hits);
    REQUIRE_FALSE(hits.empty());
    CHECK(hits.all()[0].distance() == Catch::Approx(48));
    const auto outputPath = env.dir() / "alpha saved.vmf";
    REQUIRE(map.saveAs(outputPath).is_success());
    auto expected = original;
    const auto row = std::string{"row2 \"0 64 128 192 255\""};
    const auto offset = expected.find(row);
    REQUIRE(offset != std::string::npos);
    expected.replace(offset, row.size(), "row2 \"0 64 17.5 192 255\"");
    CHECK(readBinary(outputPath) == expected);
    CHECK(readBinary(originalPath) == original);
    auto reopened = MapFixture{};
    auto& loaded = reopened.load(outputPath, config);
    const auto reopenedMesh = ramp(loaded).brush().face(*index).sourceDisplacement();
    REQUIRE(reopenedMesh);
    CHECK(reopenedMesh->alpha(2, 2) == 17.5);
    CHECK(reopenedMesh->positions() == before.positions());
    REQUIRE(loaded.save().is_success());
    CHECK(readBinary(outputPath) == expected);
    const auto savedCount = map.modificationCount();
    CHECK(setSourceDisplacementAlpha(map, 2, 2, 17.5));
    CHECK(map.modificationCount() == savedCount);
    map.undoCommand();
    CHECK(*node.brush().face(*index).sourceDisplacement() == before);
    CHECK(weights() == std::array<float, 3>{0, static_cast<float>(128.0 / 255.0), 1});
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    map.redoCommand();
    CHECK(weights() == std::array<float, 3>{0, static_cast<float>(17.5 / 255.0), 1});
    CHECK(map.vmfScene()->serialize(map.worldNode()) == expected);
    deselectAll(map);
    selectNodes(map, {&node});
    REQUIRE(translateSelection(map, {16, 8, 4}));
    CHECK(node.brush().face(*index).sourceDisplacement()->alpha(2, 2) == 17.5);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == expected);
  }
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

  SECTION("plain native affine routes preserve planes UVs bindings and exact history")
  {
    const auto operation = GENERATE(0, 1, 2);
    const auto lockUv = GENERATE(false, true);
    auto env = fs::TestEnvironment{};
    const auto path = env.dir() / "plain affine original.vmf";
    const auto original = nativeRamp(true);
    writeBinary(path, original);
    auto fixture = MapFixture{};
    auto& map = fixture.load(path, config);
    auto& node = ramp(map);
    const auto before = node.brush();
    const auto positions = filePositions(map);
    auto matrix = vm::scaling_matrix(vm::vec3d{2, 1.5, 0.5});
    if (operation == 1)
    {
      matrix = vm::mat4x4d::identity();
      matrix[1][0] = 0.25;
    }
    else if (operation == 2)
    {
      matrix = vm::scaling_matrix(vm::vec3d{-1, 1, 1});
    }
    selectNodes(map, {&node});
    map.editorContext().setAlignmentLock(lockUv);
    if (operation == 0)
    {
      REQUIRE(scaleSelection(map, {0, 0, 0}, {2, 1.5, 0.5}));
    }
    else if (operation == 1)
    {
      REQUIRE(transformSelection(map, "Plain native shear", matrix));
    }
    else
    {
      REQUIRE(flipSelection(map, {0, 0, 0}, vm::axis::x));
    }
    const auto changed = node.brush();
    CHECK(changed != before);
    CHECK(changed.faceCount() == before.faceCount());
    CHECK(filePositions(map) == positions);
    for (const auto& face : before.faces())
    {
      const auto found =
        std::ranges::find(changed.faces(), face.lineNumber(), &BrushFace::lineNumber);
      REQUIRE(found != changed.faces().end());
      const auto oldAxes = face.takeUvCoordSystemSnapshot();
      const auto newAxes = found->takeUvCoordSystemSnapshot();
      REQUIRE(oldAxes);
      REQUIRE(newAxes);
      const auto& oldUv = face.uvAttributes();
      const auto& newUv = found->uvAttributes();
      for (const auto& point : face.points())
      {
        const auto transformed = matrix * point;
        CHECK(
          vm::dot(found->boundary().normal, transformed)
          == Catch::Approx(found->boundary().distance).margin(0.000001));
        if (lockUv)
        {
          CHECK(
            vm::dot(oldAxes->uAxis, point) / oldUv.scale.x() + oldUv.offset.x()
            == Catch::Approx(
                 vm::dot(newAxes->uAxis, transformed) / newUv.scale.x()
                 + newUv.offset.x())
                 .margin(0.0001));
          CHECK(
            vm::dot(oldAxes->vAxis, point) / oldUv.scale.y() + oldUv.offset.y()
            == Catch::Approx(
                 vm::dot(newAxes->vAxis, transformed) / newUv.scale.y()
                 + newUv.offset.y())
                 .margin(0.0001));
        }
      }
    }
    const auto savedPath = env.dir() / "plain affine saved.vmf";
    REQUIRE(map.saveAs(savedPath).is_success());
    const auto saved = readBinary(savedPath);
    CHECK(saved != original);
    CHECK(readBinary(path) == original);
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(savedPath, config);
    checkReopenedBrush(ramp(reopened).brush(), changed);
    CHECK(filePositions(reopened) == positions);
    REQUIRE(reopened.save().is_success());
    CHECK(readBinary(savedPath) == saved);
    map.undoCommand();
    CHECK(node.brush() == before);
    const auto undone = env.dir() / "plain affine undone.vmf";
    REQUIRE(map.saveTo(undone).is_success());
    CHECK(readBinary(undone) == original);
    map.redoCommand();
    CHECK(node.brush() == changed);
    const auto redone = env.dir() / "plain affine redone.vmf";
    REQUIRE(map.saveTo(redone).is_success());
    CHECK(readBinary(redone) == saved);
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
    REQUIRE(map.worldNode().defaultLayer()->childCount() == 3);
    REQUIRE(dynamic_cast<GroupNode*>(&nativeNode(map, groupId)));
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
    CHECK(reopened.worldNode().defaultLayer()->childCount() == 4);
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
TEST_CASE("VmfMap in-place ordinary clip history and admission", "[vmf]")
{
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "ordinary clip.vmf";
  const auto original = nativeRamp(false);
  writeBinary(path, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto& node = ramp(map);
  selectNodes(map, {&node});
  const auto binding = node.lineNumber();
  const auto selection = map.selection().nodes;
  const auto plane = std::array<vm::vec3d, 3>{{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}};

  SECTION("cut material and UV attributes match the nearest-face preview")
  {
    const auto source = std::ranges::find_if(node.brush().faces(), [](const auto& face) {
      return face.boundary().normal == vm::vec3d{1, 0, 0};
    });
    REQUIRE(source != node.brush().faces().end());
    const auto material = source->materialName();
    const auto uv = source->uvAttributes();
    REQUIRE(clipBrushes(map, {&node}, plane));
    CHECK(node.brush().bounds().max.x() == 32);
    const auto cut = std::ranges::find_if(node.brush().faces(), [&](const auto& face) {
      return face.lineNumber() > map.vmfScene()->document().entries().size();
    });
    REQUIRE(cut != node.brush().faces().end());
    CHECK(cut->materialName() == material);
    CHECK(cut->uvAttributes() == uv);
  }

  SECTION("repeated clips save reopen move undo and redo deterministically")
  {
    REQUIRE(clipBrushes(map, {&node}, plane));
    CHECK(node.lineNumber() == binding);
    CHECK(map.selection().nodes == selection);
    CHECK(node.brush().bounds().max.x() == 32);
    const auto first = map.vmfScene()->serialize(map.worldNode());
    REQUIRE(clipBrushes(map, {&node}, {{{0, 32, 0}, {64, 32, 0}, {0, 32, 64}}}));
    CHECK(node.brush().bounds().max.y() == 32);
    const auto second = map.vmfScene()->serialize(map.worldNode());
    CHECK(first != second);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == first);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    map.redoCommand();
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == second);
    const auto saved = env.dir() / "clipped.vmf";
    REQUIRE(map.saveAs(saved).is_success());
    CHECK(readBinary(saved) == second);
    REQUIRE(map.save().is_success());
    CHECK(readBinary(saved) == second);
    auto recovered = MapFixture{};
    auto& reopened = recovered.load(saved, config);
    CHECK(ramp(reopened).brush().bounds() == node.brush().bounds());
    REQUIRE(translateSelection(map, {8, 0, 0}));
    auto painted = node.brush();
    for (auto& face : painted.faces())
    {
      face.setMaterialName("synthetic/after_clip");
    }
    REQUIRE(map.executeAndStore(
      std::make_unique<SwapNodeContentsCommand>(
        "Paint clipped brush",
        std::vector<std::pair<Node*, NodeContents>>{
          {&node, NodeContents{std::move(painted)}}})));
    REQUIRE(map.save().is_success());
    CHECK(readBinary(saved).find("synthetic/after_clip") != std::string::npos);
    CHECK_NOTHROW(
      VmfScene::read(map.vmfScene()->serialize(map.worldNode()), map.worldBounds()));
    CHECK(readBinary(path) == original);
  }

  SECTION(
    "outside tangent degenerate and later-invalid batches consume no IDs or history")
  {
    const auto count = map.modificationCount();
    const auto positions = filePositions(map);
    const auto undo = map.canUndoCommand();
    const auto redo = map.canRedoCommand();
    CHECK_FALSE(clipBrushes(map, {&node}, {{{128, 0, 0}, {128, 0, 64}, {128, 64, 0}}}));
    CHECK_FALSE(clipBrushes(map, {&node}, {{{64, 0, 0}, {64, 0, 64}, {64, 64, 0}}}));
    CHECK_FALSE(clipBrushes(map, {&node}, {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}}));
    CHECK_FALSE(clipBrushes(map, {&node, &node}, plane));
    CHECK(map.modificationCount() == count);
    CHECK(map.canUndoCommand() == undo);
    CHECK(map.canRedoCommand() == redo);
    CHECK(map.selection().nodes == selection);
    CHECK(filePositions(map) == positions);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    REQUIRE(clipBrushes(map, {&node}, plane));
    CHECK(
      map.vmfScene()->serialize(map.worldNode()).find("side\n{\n\"id\" \"9\"")
      != std::string::npos);
  }

  SECTION("a later hidden template refuses the entire command batch")
  {
    duplicateSelectedNodes(map);
    auto* duplicate = map.selection().brushes.front();
    REQUIRE(duplicate != &node);
    const auto before = map.vmfScene()->serialize(map.worldNode());
    const auto count = map.modificationCount();
    const auto positions = filePositions(map);
    const auto selected = map.selection().nodes;
    const auto undo = map.canUndoCommand();
    const auto redo = map.canRedoCommand();
    duplicate->setVisibilityState(VisibilityState::Hidden);
    CHECK_FALSE(clipBrushes(map, {&node, duplicate}, plane));
    duplicate->setVisibilityState(VisibilityState::Inherited);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
    CHECK(map.modificationCount() == count);
    CHECK(filePositions(map) == positions);
    CHECK(map.selection().nodes == selected);
    CHECK(map.canUndoCommand() == undo);
    CHECK(map.canRedoCommand() == redo);
    REQUIRE(clipBrushes(map, {&node}, plane));
    CHECK(
      map.vmfScene()->serialize(map.worldNode()).find("side\n{\n\"id\" \"15\"")
      != std::string::npos);
  }

  SECTION("generic content swap cannot acquire a new cut side")
  {
    auto candidate = node.brush();
    auto face = BrushFace::create(
      plane[0],
      plane[1],
      plane[2],
      "cut",
      UvAttributes{},
      SurfaceAttributes{},
      MapFormat::SourceVMF);
    REQUIRE(face.is_success());
    REQUIRE(candidate.clip(map.worldBounds(), std::move(face).value()).is_success());
    const auto count = map.modificationCount();
    CHECK_FALSE(map.executeAndStore(
      std::make_unique<SwapNodeContentsCommand>(
        "Unregistered clip",
        std::vector<std::pair<Node*, NodeContents>>{
          {&node, NodeContents{std::move(candidate)}}})));
    CHECK(map.modificationCount() == count);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    REQUIRE(clipBrushes(map, {&node}, plane));
  }
}

TEST_CASE(
  "VmfMap ordinary clip and Both unsupported attachments refuse atomically", "[vmf]")
{
  const auto both = GENERATE(false, true);
  const auto clip = both ? clipBrushesBoth : clipBrushes;
  CAPTURE(both);
  const auto kind = GENERATE(
    std::string{"opaque side"},
    std::string{"opaque solid"},
    std::string{"reference"},
    std::string{"instance"},
    std::string{"group"},
    std::string{"opaque group"},
    std::string{"unknown root"},
    std::string{"displacement"},
    std::string{"hidden"},
    std::string{"brush entity"},
    std::string{"generated"},
    std::string{"duplicate"},
    std::string{"generated reference"},
    std::string{"template reference"});
  CAPTURE(kind);
  auto original = kind == "displacement" ? nativeDisplacementRoom() : nativeRamp(false);
  if (kind == "opaque side")
  {
    original.insert(original.find("\"smoothing_groups\""), "foreign { key value } ");
  }
  if (kind == "opaque solid")
  {
    original.insert(original.find("solid {") + 7, "foreign { key value } ");
  }
  if (
    kind == "reference" || kind == "generated reference" || kind == "template reference")
  {
    original +=
      "entity { \"id\" \"100\" \"classname\" \"info_overlay\" \"sides\" \"3\" }";
  }
  if (kind == "instance")
  {
    original += "entity { id 100 classname func_instance file synthetic.vmf }";
  }
  if (kind == "unknown root")
  {
    original += "future_geometry_reference { sides 3 }";
  }
  if (kind == "group")
  {
    original.insert(
      original.find("solid {"), "group { id 100 editor { color \"1 2 3\" } } ");
  }
  if (kind == "opaque group")
  {
    original.insert(
      original.find("solid {"),
      "group { id 100 foreign { sides 3 } editor { color \"1 2 3\" } } ");
  }
  if (kind == "hidden")
  {
    original.insert(original.find("solid {"), "hidden { ");
    original.insert(original.find("editor { \"color\""), "} ");
  }
  if (kind == "brush entity")
  {
    original.insert(
      original.find("solid {"),
      "} entity { \"id\" \"100\" \"classname\" \"func_detail\" ");
  }
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "clip refusal.vmf";
  writeBinary(path, original);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  // Reference/instance arms add siblings; resolve the authoritative solid ID.
  auto* target = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  REQUIRE(target);
  selectNodes(map, {target});
  if (kind == "duplicate" || kind == "template reference")
  {
    duplicateSelectedNodes(map);
    target = map.selection().brushes.front();
  }
  if (kind == "generated" || kind == "generated reference")
  {
    REQUIRE(createBrush(
      map,
      {{0, 0, 0},
       {64, 0, 0},
       {0, 64, 0},
       {64, 64, 0},
       {0, 0, 64},
       {64, 0, 64},
       {0, 64, 64},
       {64, 64, 64}}));
    target = map.selection().brushes.front();
  }
  const auto before = map.vmfScene()->serialize(map.worldNode());
  const auto count = map.modificationCount();
  const auto positions = filePositions(map);
  const auto selection = map.selection().nodes;
  if (
    kind == "brush entity" || kind == "generated" || kind == "duplicate"
    || kind == "group")
  {
    REQUIRE(clip(map, {target}, {{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}}));
    CHECK(target->brush().bounds().max.x() == 32);
    CHECK_NOTHROW(
      VmfScene::read(map.vmfScene()->serialize(map.worldNode()), map.worldBounds()));
    if (kind == "group")
    {
      CHECK(target->brush().bounds().max.x() == 32);
      auto& empty = nativeNode(map, "100");
      REQUIRE(dynamic_cast<GroupNode*>(&empty));
      CHECK(empty.childCount() == 0);
      const auto bytes = map.vmfScene()->serialize(map.worldNode());
      CHECK(nativeFragment(bytes, "100") == nativeFragment(original, "100"));
      const auto saved = env.dir() / "valid empty group clip.vmf";
      REQUIRE(map.saveAs(saved).is_success());
      auto reopenFixture = MapFixture{};
      auto& reopened = reopenFixture.load(saved, config);
      CHECK(nativeNode(reopened, "100").childCount() == 0);
      CHECK(
        reopened.worldNode().defaultLayer()->childCount()
        == map.worldNode().defaultLayer()->childCount());
      map.undoCommand();
      map.redoCommand();
      CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
    }
    map.undoCommand();
  }
  else
  {
    CHECK_FALSE(clip(map, {target}, {{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}}));
  }
  CHECK(map.modificationCount() == count);
  CHECK(filePositions(map) == positions);
  CHECK(map.selection().nodes == selection);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
  const auto controlPath = env.dir() / "plain clip control.vmf";
  writeBinary(controlPath, nativeRamp(false));
  auto controlFixture = MapFixture{};
  auto& control = controlFixture.load(controlPath, config);
  REQUIRE(clip(control, {&ramp(control)}, {{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}}));
  CHECK(
    dynamic_cast<BrushNode&>(nativeNode(control, "2")).brush().bounds().max.x() == 32);
}

TEST_CASE(
  "VmfMap ordinary clip generated and entity ownership before first save", "[vmf]")
{
  const auto route = GENERATE(
    std::string{"fresh"},
    std::string{"template"},
    std::string{"entity"},
    std::string{"wrapped original"},
    std::string{"wrapped fresh"},
    std::string{"duplicated child"},
    std::string{"clipped duplicate"});
  CAPTURE(route);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "ownership input.vmf";
  auto original = nativeRamp(false);
  if (route == "entity" || route == "duplicated child")
  {
    const auto solid = nativeFragment(original, "2");
    original.erase(original.find(solid), solid.size());
    auto sibling = solid;
    for (int id = 8; id >= 2; --id)
    {
      const auto token = "\"id\" \"" + std::to_string(id) + "\"";
      const auto at = sibling.find(token);
      REQUIRE(at != std::string::npos);
      sibling.replace(at, token.size(), "\"id\" \"" + std::to_string(id + 20) + "\"");
    }
    original += "entity { \"id\" \"100\" classname func_detail " + solid + sibling
      + "connections { OnUser1 \"marker,Enable,,0,-1\" OnUser1 \"marker,Disable,,1,-1\" } "
        "editor { color \"001 002 003\" } }";
  }
  writeBinary(path, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto* node = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  REQUIRE(node);
  selectNodes(map, {node});
  if (route == "fresh" || route == "wrapped fresh")
  {
    deselectAll(map);
    REQUIRE(createBrush(map, nativeCubePoints()));
    node = map.selection().brushes.front();
  }
  if (route == "wrapped original" || route == "wrapped fresh")
  {
    const auto definition = EntityDefinition{.name = "func_detail"};
    REQUIRE(createBrushEntity(map, definition));
  }
  const auto clipX = [&](BrushNode* target) {
    const auto x = target->brush().bounds().center().x();
    return clipBrushes(map, {target}, {{{x, 0, 0}, {x, 0, 64}, {x, 64, 0}}});
  };
  if (route == "clipped duplicate")
  {
    REQUIRE(clipX(node));
  }
  if (route == "template" || route == "duplicated child" || route == "clipped duplicate")
  {
    duplicateSelectedNodes(map);
    REQUIRE(map.selection().brushes.size() == 1);
    auto* duplicate = map.selection().brushes.front();
    REQUIRE(duplicate != node);
    node = duplicate;
  }
  const auto initial = map.vmfScene()->serialize(map.worldNode());
  const auto id = map.vmfScene()->nativeId(*node);
  const auto binding = node->lineNumber();
  const auto selected = map.selection().nodes;
  const auto oldBounds = node->brush().bounds();
  auto oldSides = std::set<size_t>{};
  for (const auto& face : node->brush().faces())
  {
    oldSides.insert(face.lineNumber());
  }
  REQUIRE(
    clipX(node)); // No save/reload has occurred since creation/duplication/wrapping.
  CHECK(node->lineNumber() == binding);
  CHECK(map.vmfScene()->nativeId(*node) == id);
  CHECK(map.selection().nodes == selected);
  CHECK(node->brush().bounds().max.x() == oldBounds.center().x());
  auto newSides = size_t{0};
  for (const auto& face : node->brush().faces())
  {
    newSides += oldSides.contains(face.lineNumber()) ? 0 : 1;
  }
  CHECK(newSides == 1);
  const auto first = map.vmfScene()->serialize(map.worldNode());
  CHECK(first == map.vmfScene()->serialize(map.worldNode()));
  CHECK(nativeFragment(first, "9") == nativeFragment(original, "9"));
  if (route == "template" || route == "clipped duplicate")
  {
    CHECK(nativeFragment(first, "2") == nativeFragment(initial, "2"));
    CHECK(nativeFragment(first, id).find("0.2500") != std::string::npos);
  }
  if (route == "entity" || route == "duplicated child")
  {
    CHECK(nativeFragment(first, "22") == nativeFragment(original, "22"));
    CHECK(
      first.find("OnUser1 \"marker,Enable,,0,-1\" OnUser1 \"marker,Disable,,1,-1\"")
      != std::string::npos);
    CHECK(first.find("color \"001 002 003\"") != std::string::npos);
  }
  const auto y = node->brush().bounds().center().y();
  REQUIRE(clipBrushes(map, {node}, {{{0, y, 0}, {64, y, 0}, {0, y, 64}}}));
  const auto second = map.vmfScene()->serialize(map.worldNode());
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == first);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == initial);
  if (route != "entity")
  {
    map.undoCommand(); // Creation, wrapping or duplication remains a separate command.
    CHECK(map.vmfScene()->serialize(map.worldNode()) != initial);
    if (route == "wrapped fresh")
    {
      map.undoCommand();
      CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
      map.redoCommand();
    }
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == initial);
  }
  map.redoCommand();
  map.redoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == second);
  const auto saved = env.dir() / "ownership saved.vmf";
  REQUIRE(map.saveAs(saved).is_success());
  REQUIRE(map.save().is_success());
  CHECK(readBinary(saved) == second);
  auto recovered = MapFixture{};
  auto& reopened = recovered.load(saved, config);
  auto& reopenedNode = dynamic_cast<BrushNode&>(nativeNode(reopened, id));
  checkReopenedBrush(reopenedNode.brush(), node->brush(), false);
  const auto reopenedX = reopenedNode.brush().bounds().center().x();
  REQUIRE(clipBrushes(
    reopened,
    {&reopenedNode},
    {{{reopenedX, 0, 0}, {reopenedX, 0, 64}, {reopenedX, 64, 0}}}));
  REQUIRE(translateSelection(map, {4, 0, 0}));
  auto painted = node->brush();
  for (auto& face : painted.faces())
  {
    face.setMaterialName("synthetic/ownership_paint");
  }
  REQUIRE(map.executeAndStore(
    std::make_unique<SwapNodeContentsCommand>(
      "Paint owned clip",
      std::vector<std::pair<Node*, NodeContents>>{
        {node, NodeContents{std::move(painted)}}})));
  REQUIRE(map.save().is_success());
  CHECK(readBinary(saved).find("synthetic/ownership_paint") != std::string::npos);
  CHECK(readBinary(path) == original);
}

TEST_CASE("VmfMap clip redo live owner refusal preserves retry history", "[vmf]")
{
  const auto refuseRedo = GENERATE(false, true);
  const auto wrapFresh = GENERATE(false, true);
  CAPTURE(refuseRedo, wrapFresh);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "redo owner input.vmf";
  writeBinary(path, nativeRamp(false));
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto* node = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  REQUIRE(node);
  selectNodes(map, {node});
  if (wrapFresh)
  {
    deselectAll(map);
    REQUIRE(createBrush(map, nativeCubePoints()));
    node = map.selection().brushes.front();
    REQUIRE(createBrushEntity(map, EntityDefinition{.name = "func_detail"}));
  }
  else
  {
    duplicateSelectedNodes(map);
    node = map.selection().brushes.front();
  }
  const auto before = map.vmfScene()->serialize(map.worldNode());
  const auto beforeBounds = node->brush().bounds();
  const auto x = beforeBounds.center().x();
  REQUIRE(clipBrushes(map, {node}, {{{x, 0, 0}, {x, 0, 64}, {x, 64, 0}}}));
  const auto after = map.vmfScene()->serialize(map.worldNode());
  const auto afterPositions = filePositions(map);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
  REQUIRE(map.canRedoCommand());
  const auto redoName = map.redoCommandName();
  const auto selected = map.selection().nodes;
  const auto count = map.modificationCount();
  const auto positions = filePositions(map);
  const auto undoName = map.undoCommandName();
  auto* owner = node->parent();
  REQUIRE(owner);
  if (refuseRedo)
  {
    owner->setVisibilityState(VisibilityState::Hidden);
  }
  map.redoCommand();
  if (refuseRedo)
  {
    CHECK(node->brush().bounds() == beforeBounds);
    CHECK(map.modificationCount() == count);
    CHECK(filePositions(map) == positions);
    CHECK(map.undoCommandName() == undoName);
    REQUIRE(map.canRedoCommand());
    CHECK(map.redoCommandName() == redoName);
    CHECK(map.selection().nodes == selected);
    map.redoCommand();
    REQUIRE(map.canRedoCommand());
    CHECK(node->brush().bounds() == beforeBounds);
    CHECK(map.modificationCount() == count);
    CHECK(filePositions(map) == positions);
    CHECK(map.undoCommandName() == undoName);
    CHECK(map.redoCommandName() == redoName);
    owner->setVisibilityState(VisibilityState::Inherited);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == after);
    CHECK(filePositions(map) == afterPositions);
    CHECK(map.modificationCount() == count + 1);
    CHECK(node->brush().bounds().max.x() == x);
    CHECK_NOTHROW(
      VmfScene::read(map.vmfScene()->serialize(map.worldNode()), map.worldBounds()));
  }
  else
  {
    CHECK(map.vmfScene()->serialize(map.worldNode()) == after);
    CHECK(node->brush().bounds().max.x() == x);
  }
}

TEST_CASE("VmfMap Both native halves history ownership and preserving save", "[vmf]")
{
  const auto route = GENERATE(
    std::string{"loaded"},
    std::string{"fresh"},
    std::string{"template"},
    std::string{"entity"},
    std::string{"wrapped fresh"},
    std::string{"duplicated child"},
    std::string{"clipped duplicate"});
  CAPTURE(route);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "both input.vmf";
  auto original = nativeRamp(false);
  if (route == "entity" || route == "duplicated child")
  {
    const auto solid = nativeFragment(original, "2");
    original.erase(original.find(solid), solid.size());
    original += "entity { id 100 classname func_detail " + solid
      + "connections { OnUser1 \"marker,Enable,,0,-1\" OnUser1 \"marker,Disable,,1,-1\" } "
        "editor { color \"001 002 003\" } }";
  }
  writeBinary(path, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto* node = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  REQUIRE(node);
  selectNodes(map, {node});
  if (route == "fresh" || route == "wrapped fresh")
  {
    deselectAll(map);
    REQUIRE(createBrush(map, nativeCubePoints()));
    node = map.selection().brushes.front();
  }
  if (route == "wrapped fresh")
  {
    REQUIRE(createBrushEntity(map, EntityDefinition{.name = "func_detail"}));
  }
  if (route == "clipped duplicate")
  {
    REQUIRE(clipBrushes(map, {node}, {{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}}));
  }
  if (route == "template" || route == "duplicated child" || route == "clipped duplicate")
  {
    duplicateSelectedNodes(map);
    node = map.selection().brushes.front();
  }
  selectNodes(map, {node});
  auto* parent = node->parent();
  const auto children = parent->childCount();
  const auto id = map.vmfScene()->nativeId(*node);
  const auto binding = node->lineNumber();
  const auto before = node->brush();
  const auto beforeBytes = map.vmfScene()->serialize(map.worldNode());
  const auto beforeSelection = map.selection().nodes;
  const auto beforePositions = filePositions(map);
  const auto count = map.modificationCount();
  const auto x = before.bounds().center().x();
  REQUIRE(clipBrushesBoth(map, {node}, {{{x, 0, 0}, {x, 0, 64}, {x, 64, 0}}}));
  CHECK(map.modificationCount() == count + 1);
  CHECK(parent->childCount() == children + 1);
  REQUIRE(map.selection().brushes.size() == 2);
  auto* back = map.selection().brushes[0] == node ? map.selection().brushes[1]
                                                  : map.selection().brushes[0];
  CHECK(back->parent() == parent);
  CHECK(node->lineNumber() == binding);
  CHECK(map.vmfScene()->nativeId(*node) == id);
  CHECK(back->lineNumber() != binding);
  const auto backId = map.vmfScene()->nativeId(*back);
  CHECK(backId != id);
  CHECK(node->brush().closed());
  CHECK(back->brush().closed());
  CHECK(node->brush().fullySpecified());
  CHECK(back->brush().fullySpecified());
  auto frontBounds = before.bounds();
  auto backBounds = before.bounds();
  frontBounds.max[0] = x;
  backBounds.min[0] = x;
  if (route != "fresh" && route != "wrapped fresh")
  {
    frontBounds.max[2] = 32 + x / 2;
  }
  for (size_t axis = 0; axis < 3; ++axis)
  {
    CHECK(
      node->brush().bounds().min[axis]
      == Catch::Approx(frontBounds.min[axis]).margin(1e-9));
    CHECK(
      node->brush().bounds().max[axis]
      == Catch::Approx(frontBounds.max[axis]).margin(1e-9));
    CHECK(
      back->brush().bounds().min[axis]
      == Catch::Approx(backBounds.min[axis]).margin(1e-9));
    CHECK(
      back->brush().bounds().max[axis]
      == Catch::Approx(backBounds.max[axis]).margin(1e-9));
  }
  auto bindings = std::set<size_t>{};
  for (const auto* half : {node, back})
  {
    auto cutCount = size_t{0};
    const auto normal = half == node ? vm::vec3d{1, 0, 0} : vm::vec3d{-1, 0, 0};
    const auto nearest =
      std::ranges::min_element(before.faces(), {}, [&](const auto& face) {
        return vm::squared_length(face.boundary().normal - normal);
      });
    for (const auto& face : half->brush().faces())
    {
      CHECK(bindings.insert(face.lineNumber()).second);
      if (face.boundary().normal == normal)
      {
        ++cutCount;
        CHECK(face.materialName() == nearest->materialName());
        CHECK(face.uvAttributes() == nearest->uvAttributes());
        const auto uv = face.takeUvCoordSystemSnapshot();
        REQUIRE(uv);
        CHECK(uv->uAxis == (half == node ? vm::vec3d{0, 1, 0} : vm::vec3d{0, -1, 0}));
        CHECK(uv->vAxis == vm::vec3d{0, 0, -1});
      }
      else if (half == node)
      {
        CHECK(std::ranges::any_of(before.faces(), [&](const auto& source) {
          return source.lineNumber() == face.lineNumber();
        }));
      }
      else
      {
        CHECK(std::ranges::none_of(before.faces(), [&](const auto& source) {
          return source.lineNumber() == face.lineNumber();
        }));
      }
    }
    CHECK(cutCount == 1);
  }
  const auto front = node->brush();
  const auto backState = back->brush();
  const auto after = map.vmfScene()->serialize(map.worldNode());
  const auto afterPositions = filePositions(map);
  CHECK(after == map.vmfScene()->serialize(map.worldNode()));
  CHECK(nativeFragment(after, "9") == nativeFragment(original, "9"));
  if (route == "entity" || route == "duplicated child")
  {
    CHECK(after.find("color \"001 002 003\"") != std::string::npos);
    CHECK(after.find("OnUser1 \"marker,Disable,,1,-1\"") != std::string::npos);
  }
  for (int replay = 0; replay < 2; ++replay)
  {
    map.undoCommand();
    CHECK(map.modificationCount() == count);
    CHECK(parent->childCount() == children);
    CHECK(back->parent() == nullptr);
    CHECK(map.selection().nodes == beforeSelection);
    CHECK(filePositions(map) == beforePositions);
    checkReopenedBrush(node->brush(), before);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == beforeBytes);
    const auto parked = env.dir() / ("parked" + std::to_string(replay) + ".vmf");
    REQUIRE(map.saveAs(parked).is_success());
    CHECK(readBinary(parked) == beforeBytes);
    auto parkedScene = VmfScene::read(readBinary(parked), map.worldBounds());
    CHECK(parkedScene.serialize(parkedScene.world()) == beforeBytes);
    map.redoCommand();
    CHECK(map.modificationCount() == count + 1);
    CHECK(back->parent() == parent);
    CHECK(
      std::ranges::find(map.selection().brushes, back) != map.selection().brushes.end());
    checkReopenedBrush(node->brush(), front);
    checkReopenedBrush(back->brush(), backState);
    CHECK(filePositions(map) == afterPositions);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == after);
  }
  const auto saved = env.dir() / "both saved.vmf";
  REQUIRE(map.saveAs(saved).is_success());
  REQUIRE(map.save().is_success());
  CHECK(readBinary(saved) == after);
  auto recovered = MapFixture{};
  auto& reopened = recovered.load(saved, config);
  checkReopenedBrush(
    dynamic_cast<BrushNode&>(nativeNode(reopened, id)).brush(), front, false);
  checkReopenedBrush(
    dynamic_cast<BrushNode&>(nativeNode(reopened, backId)).brush(), backState, false);
  REQUIRE(translateSelection(map, {4, 0, 0}));
  CHECK_NOTHROW(
    VmfScene::read(map.vmfScene()->serialize(map.worldNode()), map.worldBounds()));
  map.undoCommand();
  const auto y = front.bounds().center().y();
  REQUIRE(clipBrushesBoth(map, {node, back}, {{{0, y, 0}, {64, y, 0}, {0, y, 64}}}));
  CHECK(map.selection().brushes.size() == 4);
  CHECK_NOTHROW(
    VmfScene::read(map.vmfScene()->serialize(map.worldNode()), map.worldBounds()));
  CHECK(readBinary(path) == original);
}

TEST_CASE("VmfMap Both no-op mixed and refusal allocator controls", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "both controls.vmf";
  const auto original = nativeRamp(false);
  writeBinary(path, original);
  auto fixture = MapFixture{};
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto& map = fixture.load(path, config);
  auto* node = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  REQUIRE(node);
  selectNodes(map, {node});
  const auto plane = std::array<vm::vec3d, 3>{{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}};
  SECTION(
    "all no-op tangent invalid plane duplicate and nonfinite leave all state unchanged")
  {
    const auto count = map.modificationCount();
    const auto positions = filePositions(map);
    const auto selected = map.selection().nodes;
    const auto undo = map.canUndoCommand();
    const auto redo = map.canRedoCommand();
    CHECK_FALSE(
      clipBrushesBoth(map, {node}, {{{128, 0, 0}, {128, 0, 64}, {128, 64, 0}}}));
    CHECK_FALSE(clipBrushesBoth(map, {node}, {{{64, 0, 0}, {64, 0, 64}, {64, 64, 0}}}));
    CHECK_FALSE(clipBrushesBoth(map, {node}, {{{0, 0, 0}, {0, 0, 64}, {0, 64, 0}}}));
    CHECK_FALSE(clipBrushesBoth(map, {node}, {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}}));
    auto invalid = plane;
    invalid[0][0] = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(clipBrushesBoth(map, {node}, invalid));
    CHECK_FALSE(clipBrushesBoth(map, {node, node}, plane));
    CHECK(map.modificationCount() == count);
    CHECK(map.canUndoCommand() == undo);
    CHECK(map.canRedoCommand() == redo);
    CHECK(filePositions(map) == positions);
    CHECK(map.selection().nodes == selected);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    REQUIRE(clipBrushesBoth(map, {node}, plane));
    CHECK(map.vmfScene()->nativeId(*map.selection().brushes.back()) == "10");
    CHECK(
      map.vmfScene()->serialize(map.worldNode()).find("side\n{\n\"id\" \"9\"")
      != std::string::npos);
  }
  SECTION("mixed no-op survives selected and is validated even when it does not split")
  {
    duplicateSelectedNodes(map);
    auto* untouched = map.selection().brushes.front();
    REQUIRE(translateSelection(map, {128, 0, 0}));
    selectNodes(map, {node, untouched});
    const auto before = map.vmfScene()->serialize(map.worldNode());
    const auto untouchedState = untouched->brush();
    const auto count = map.modificationCount();
    const auto positions = filePositions(map);
    untouched->setVisibilityState(VisibilityState::Hidden);
    CHECK_FALSE(clipBrushesBoth(map, {node, untouched}, plane));
    untouched->setVisibilityState(VisibilityState::Inherited);
    CHECK(map.modificationCount() == count);
    CHECK(filePositions(map) == positions);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
    REQUIRE(clipBrushesBoth(map, {node, untouched}, plane));
    CHECK(map.selection().brushes.size() == 3);
    CHECK(untouched->selected());
    checkReopenedBrush(untouched->brush(), untouchedState);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
    CHECK(untouched->selected());
  }
}

TEST_CASE(
  "VmfMap Both replay refuses changed live owner or parked payload without retry",
  "[vmf]")
{
  const auto changed =
    GENERATE(std::string{"owner"}, std::string{"parked"}, std::string{"live"});
  CAPTURE(changed);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "both replay.vmf";
  writeBinary(path, nativeRamp(false));
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto* node = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  REQUIRE(node);
  selectNodes(map, {node});
  REQUIRE(clipBrushesBoth(map, {node}, {{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}}));
  auto* back = map.selection().brushes.back();
  REQUIRE(back != node);
  map.undoCommand();
  const auto count = map.modificationCount();
  const auto selected = map.selection().nodes;
  if (changed == "owner")
  {
    node->parent()->setVisibilityState(VisibilityState::Hidden);
  }
  else
  {
    auto* target = changed == "parked" ? back : node;
    auto payload = target->brush();
    payload.faces().front().setMaterialName("synthetic/forged");
    target->setBrush(std::move(payload));
  }
  const auto positions = filePositions(map);
  map.redoCommand();
  CHECK(map.modificationCount() == count);
  CHECK(filePositions(map) == positions);
  CHECK(map.selection().nodes == selected);
  CHECK_FALSE(map.canRedoCommand());
  CHECK(node->brush().bounds().max.x() == 64);
}

TEST_CASE("VmfMap Both multi-parent batch admission and ownership", "[vmf]")
{
  const auto kind = GENERATE(
    std::string{"valid"},
    std::string{"hidden owner"},
    std::string{"unsupported owner"},
    std::string{"opaque"},
    std::string{"displacement"});
  CAPTURE(kind);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "both parents.vmf";
  auto original = nativeRamp(false);
  auto second =
    nativeFragment(kind == "displacement" ? nativeDisplacementRoom() : original, "2");
  for (int id = 8; id >= 2; --id)
  {
    const auto token = "\"id\" \"" + std::to_string(id) + "\"";
    const auto at = second.find(token);
    REQUIRE(at != std::string::npos);
    second.replace(at, token.size(), "\"id\" \"" + std::to_string(id + 20) + "\"");
  }
  if (kind == "opaque")
  {
    second.insert(second.find("\"smoothing_groups\""), "future { key value } ");
  }
  original += "entity { id 100 classname func_detail " + second + " }";
  writeBinary(path, original);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto* first = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  auto* later = dynamic_cast<BrushNode*>(&nativeNode(map, "22"));
  REQUIRE(first);
  REQUIRE(later);
  auto* owner = dynamic_cast<EntityNode*>(later->parent());
  REQUIRE(owner);
  selectNodes(map, {first, later});
  const auto beforePositions = filePositions(map);
  const auto selected = map.selection().nodes;
  const auto count = map.modificationCount();
  const auto entity = owner->entity();
  if (kind == "hidden owner")
  {
    owner->setVisibilityState(VisibilityState::Hidden);
  }
  if (kind == "unsupported owner")
  {
    auto changed = entity;
    changed.setClassname("func_instance");
    owner->setEntity(std::move(changed));
  }
  const auto plane = std::array<vm::vec3d, 3>{{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}};
  if (kind != "valid")
  {
    CHECK_FALSE(clipBrushesBoth(map, {first, later}, plane));
    owner->setEntity(entity);
    owner->setVisibilityState(VisibilityState::Inherited);
    CHECK(map.modificationCount() == count);
    CHECK(filePositions(map) == beforePositions);
    CHECK(map.selection().nodes == selected);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    REQUIRE(clipBrushesBoth(map, {first}, plane));
    CHECK(
      map.vmfScene()->serialize(map.worldNode()).find("side\n{\n\"id\" \"29\"")
      != std::string::npos);
    CHECK(map.vmfScene()->nativeId(*map.selection().brushes.back()) == "101");
  }
  else
  {
    REQUIRE(clipBrushesBoth(map, {first, later}, plane));
    REQUIRE(map.selection().brushes.size() == 4);
    const auto halves = map.selection().nodes;
    CHECK(owner->childCount() == 2);
    CHECK(map.worldNode().defaultLayer()->childCount() == 4);
    const auto after = map.vmfScene()->serialize(map.worldNode());
    const auto afterPositions = filePositions(map);
    map.undoCommand();
    CHECK(owner->childCount() == 1);
    CHECK(map.worldNode().defaultLayer()->childCount() == 3);
    CHECK(map.selection().nodes == selected);
    CHECK(filePositions(map) == beforePositions);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    map.redoCommand();
    CHECK(map.selection().nodes == halves);
    CHECK(filePositions(map) == afterPositions);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == after);
    CHECK_NOTHROW(VmfScene::read(after, map.worldBounds()));
    // Destruction of attached command history must leave tree-owned halves alive.
    map.commandProcessor().clear();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == after);
  }
}

TEST_CASE("VmfMap native group authoring loaded projection and property control", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "nested native groups.vmf";
  auto original = nativeRamp(false);
  const auto solidEnd = original.find("}\r\neditor { ");
  REQUIRE(solidEnd != std::string::npos);
  original.insert(solidEnd, "editor { \"groupid\" \"11\" }\r\n");
  const auto worldEditor = original.find("editor { \"color\"");
  REQUIRE(worldEditor != std::string::npos);
  original.insert(
    worldEditor,
    "group { \"id\" \"10\" editor { \"color\" \"1 2 3\" } }\r\n"
    "group { \"id\" \"11\" editor { \"groupid\" \"10\" } }\r\n");
  writeBinary(path, original);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  const auto unchanged = env.dir() / "unchanged.vmf";
  REQUIRE(map.saveAs(unchanged).is_success());
  CHECK(readBinary(unchanged) == original);

  // This supported unrelated edit must ACT even before group projection exists.
  auto& marker = nativeNode(map, "9");
  selectNodes(map, {&marker});
  REQUIRE(setEntityProperty(map, "targetname", "acted_marker"));
  const auto changed = env.dir() / "changed.vmf";
  REQUIRE(map.saveAs(changed).is_success());
  auto expected = original;
  expected.replace(
    expected.find("untouched_marker"),
    std::string{"untouched_marker"}.size(),
    "acted_marker");
  CHECK(readBinary(changed) == expected);
  map.undoCommand();
  const auto undone = env.dir() / "undone.vmf";
  REQUIRE(map.saveAs(undone).is_success());
  CHECK(readBinary(undone) == original);

  auto groups = std::vector<GroupNode*>{};
  const auto visit = [&](auto&& self, Node& node) -> void {
    if (auto* group = dynamic_cast<GroupNode*>(&node))
    {
      groups.push_back(group);
    }
    for (auto* child : node.children())
    {
      self(self, *child);
    }
  };
  visit(visit, map.worldNode());
  REQUIRE(groups.size() == 2);
  CHECK(map.vmfScene()->nativeId(*groups[0]) == "10");
  CHECK(map.vmfScene()->nativeId(*groups[1]) == "11");
  CHECK(groups[1]->parent() == groups[0]);
  CHECK(nativeNode(map, "2").parent() == groups[1]);
  const auto originalOrder = map.worldNode().defaultLayer()->children();
  deselectAll(map);
  selectNodes(map, {&marker, groups[0]});
  REQUIRE(canGroupSelectedNodes(map));
  auto* outer = groupSelectedNodes(map, "ignored native name");
  REQUIRE(outer);
  CHECK(map.vmfScene()->nativeId(*outer) == "12");
  CHECK(outer->group().name() == "Group 12");
  CHECK(outer->children() == std::vector<Node*>{&marker, groups[0]});
  CHECK(map.selection().nodes == std::vector<Node*>{outer});
  const auto grouped = env.dir() / "grouped.vmf";
  REQUIRE(map.saveAs(grouped).is_success());
  const auto groupedBytes = readBinary(grouped);
  CHECK(groupedBytes != original);
  auto reopenedFixture = MapFixture{};
  auto& reopened = reopenedFixture.load(grouped, config);
  CHECK(nativeNode(reopened, "10").parent() == &nativeNode(reopened, "12"));
  CHECK(nativeNode(reopened, "9").parent() == &nativeNode(reopened, "12"));
  CHECK(
    nativeNode(reopened, "12").children()
    == std::vector<Node*>{&nativeNode(reopened, "10"), &nativeNode(reopened, "9")});
  CHECK(reopened.vmfScene()->serialize(reopened.worldNode()) == groupedBytes);
  REQUIRE(reopened.save().is_success());
  REQUIRE(reopened.save().is_success());
  CHECK(readBinary(grouped) == groupedBytes);
  map.undoCommand();
  CHECK(map.worldNode().defaultLayer()->children() == originalOrder);
  CHECK(map.selection().nodes == std::vector<Node*>{&marker, groups[0]});
  const auto groupUndone = env.dir() / "group undone.vmf";
  REQUIRE(map.saveAs(groupUndone).is_success());
  CHECK(readBinary(groupUndone) == original);
  map.redoCommand();
  CHECK(map.selection().nodes == std::vector<Node*>{outer});
  REQUIRE(canUngroupSelectedNodes(map));
  ungroupSelectedNodes(map);
  CHECK(groups[0]->parent() == map.worldNode().defaultLayer());
  CHECK(groups[1]->parent() == groups[0]);
  const auto dissolved = env.dir() / "dissolved.vmf";
  REQUIRE(map.saveAs(dissolved).is_success());
  CHECK(readBinary(dissolved) == original);
  map.undoCommand();
  const auto restored = env.dir() / "restored.vmf";
  REQUIRE(map.saveAs(restored).is_success());
  CHECK(readBinary(restored) == groupedBytes);
}

TEST_CASE("VmfMap native group authoring grouped ordinary actions", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "grouped action.vmf";
  const auto original = nativeGroupedRamp();
  writeBinary(path, original);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto* outer = dynamic_cast<GroupNode*>(&nativeNode(map, "10"));
  auto* inner = dynamic_cast<GroupNode*>(&nativeNode(map, "11"));
  auto* target = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  REQUIRE(outer);
  REQUIRE(inner);
  REQUIRE(target);
  const auto saveReopen = [&]() {
    const auto bytes = map.vmfScene()->serialize(map.worldNode());
    const auto saved = env.dir() / "action saved.vmf";
    REQUIRE(map.saveAs(saved).is_success());
    CHECK(readBinary(saved) == bytes);
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(saved, config);
    CHECK(reopened.vmfScene()->serialize(reopened.worldNode()) == bytes);
    CHECK(readBinary(path) == original);
  };

  SECTION("loaded UNGROUP removes only immediate container and replays original bytes")
  {
    selectNodes(map, {outer});
    REQUIRE(canUngroupSelectedNodes(map));
    ungroupSelectedNodes(map);
    CHECK(inner->parent() == map.worldNode().defaultLayer());
    CHECK(target->parent() == inner);
    CHECK(map.selection().nodes == std::vector<Node*>{inner});
    const auto bytes = map.vmfScene()->serialize(map.worldNode());
    const auto parsed = VmfDocument::parse(bytes);
    const auto metadata = SourceEditorMetadata::read(parsed);
    CHECK(metadata.groups.size() == 1);
    saveReopen();
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    CHECK(map.selection().nodes == std::vector<Node*>{outer});
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
  }
  SECTION("whole group transform and duplicate remap nested identities")
  {
    selectNodes(map, {outer});
    REQUIRE(
      transformSelection(map, "Move group", vm::translation_matrix(vm::vec3d{64, 0, 0})));
    CHECK(target->brush().bounds().min.x() == 64);
    saveReopen();
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    map.redoCommand();
    duplicateSelectedNodes(map);
    REQUIRE(map.selection().groups.size() == 1);
    auto* copy = map.selection().groups.front();
    REQUIRE(copy != outer);
    CHECK(map.vmfScene()->nativeId(*copy) == "12");
    CHECK(copy->linkId() != outer->linkId());
    REQUIRE(copy->childCount() == 1);
    auto* nested = dynamic_cast<GroupNode*>(copy->children().front());
    REQUIRE(nested);
    CHECK(map.vmfScene()->nativeId(*nested) == "13");
    CHECK(nested->linkId() != inner->linkId());
    REQUIRE(nested->childCount() == 1);
    CHECK(map.vmfScene()->nativeId(*nested->children().front()) == "14");
    const auto bytes = map.vmfScene()->serialize(map.worldNode());
    saveReopen();
    map.undoCommand();
    map.redoCommand();
    CHECK(map.selection().groups.front() == copy);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
    REQUIRE(removeSelectedNodes(map));
    CHECK(outer->parent() == map.worldNode().defaultLayer());
    map.undoCommand();
    CHECK(map.selection().groups.front() == copy);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
  }
  SECTION("open close is transient and create inside group keeps lexical world owner")
  {
    openGroup(map, *outer);
    openGroup(map, *inner);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    REQUIRE(createBrush(map, nativeCubePoints()));
    auto* added = map.selection().brushes.front();
    CHECK(added->parent() == inner);
    const auto id = map.vmfScene()->nativeId(*added);
    saveReopen();
    const auto bytes = map.vmfScene()->serialize(map.worldNode());
    CHECK(nativeFragment(bytes, id).find("groupid") != std::string::npos);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    map.redoCommand();
    CHECK(map.selection().brushes.front() == added);
    closeGroup(map);
    closeGroup(map);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
  }
  SECTION("leaf duplicate delete and plain brush entity wrapping")
  {
    openGroup(map, *outer);
    openGroup(map, *inner);
    selectNodes(map, {target});
    duplicateSelectedNodes(map);
    REQUIRE(map.selection().brushes.size() == 1);
    auto* copy = map.selection().brushes.front();
    REQUIRE(copy != target);
    CHECK(copy->parent() == inner);
    saveReopen();
    REQUIRE(removeSelectedNodes(map));
    map.undoCommand();
    CHECK(copy->parent() == inner);
    const auto definition = EntityDefinition{.name = "func_detail"};
    auto* entity = createBrushEntity(map, definition);
    REQUIRE(entity);
    CHECK(entity->parent() == inner);
    CHECK(copy->parent() == entity);
    saveReopen();
    map.undoCommand();
    CHECK(copy->parent() == inner);
    saveReopen();
  }
  SECTION("Front Back and Both ACT under nested group")
  {
    const auto mode = GENERATE(0, 1, 2);
    const auto plane =
      mode == 1 ? std::array<vm::vec3d, 3>{{{32, 0, 0}, {32, 64, 0}, {32, 0, 64}}}
                : std::array<vm::vec3d, 3>{{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}};
    openGroup(map, *outer);
    openGroup(map, *inner);
    selectNodes(map, {target});
    REQUIRE((mode == 2 ? clipBrushesBoth : clipBrushes)(map, {target}, plane));
    CHECK(target->parent() == inner);
    if (mode == 1)
    {
      CHECK(target->brush().bounds().min.x() == 32);
    }
    else
    {
      CHECK(target->brush().bounds().max.x() == 32);
    }
    CHECK(inner->childCount() == (mode == 2 ? 2 : 1));
    const auto bytes = map.vmfScene()->serialize(map.worldNode());
    saveReopen();
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
  }
}

TEST_CASE(
  "VmfMap native group authoring fresh template promotion and exact order", "[vmf]")
{
  const auto templateMap = GENERATE(false, true);
  auto env = fs::TestEnvironment{};
  const auto starter = env.dir() / "starter.vmf";
  writeBinary(
    starter, templateMap ? nativeRamp(false) : "world { id 1 classname worldspawn }\n");
  auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  config.gameInfo.gameConfig.fileFormats = {{"Source VMF", starter}};
  auto fixture = MapFixture{};
  auto& map = fixture.create(config);
  REQUIRE(createBrush(map, nativeCubePoints()));
  auto* brush = map.selection().brushes.front();
  auto* entity = createBrushEntity(map, EntityDefinition{.name = "func_detail"});
  REQUIRE(entity);
  REQUIRE(brush->parent() == entity);
  const auto definition = EntityDefinition{
    .name = "info_target",
    .pointEntityDefinition = PointEntityDefinition{.bounds = vm::bbox3d{8}}};
  auto* point = createPointEntity(map, definition, vm::vec3d{128, 0, 0});
  REQUIRE(point);
  auto* foreign = createPointEntity(map, definition, vm::vec3d{256, 0, 0});
  REQUIRE(foreign);
  // An unrelated sibling must not be detached merely to restore ordering.
  const auto order = map.worldNode().defaultLayer()->children();
  auto notifications = size_t{0};
  const auto witness =
    map.nodesWillBeRemovedNotifier.connect([&](const std::vector<Node*>& nodes) {
      CHECK(std::ranges::find(nodes, foreign) == nodes.end());
      ++notifications;
    });
  deselectAll(map);
  selectNodes(map, {brush, point});
  const auto selection = map.selection().nodes;
  const auto before = map.vmfScene()->serialize(map.worldNode());
  const auto count = map.modificationCount();
  auto* group = groupSelectedNodes(map, "not native metadata");
  REQUIRE(group);
  CHECK(group->children() == std::vector<Node*>{entity, point});
  CHECK(brush->parent() == entity);
  CHECK(map.modificationCount() == count + 1);
  const auto bytes = map.vmfScene()->serialize(map.worldNode());
  map.undoCommand();
  CHECK(map.modificationCount() == count);
  CHECK(map.worldNode().defaultLayer()->children() == order);
  CHECK(map.selection().nodes == selection);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
  map.redoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
  REQUIRE(notifications > 0);
  const auto saved = env.dir() / "fresh grouped.vmf";
  REQUIRE(map.saveAs(saved).is_success());
  auto reopenedFixture = MapFixture{};
  auto& reopened = reopenedFixture.load(saved, config);
  auto& reopenedEntity = nativeNode(reopened, map.vmfScene()->nativeId(*entity));
  CHECK(dynamic_cast<GroupNode*>(reopenedEntity.parent()));
  CHECK(
    nativeNode(reopened, map.vmfScene()->nativeId(*brush)).parent() == &reopenedEntity);
}

TEST_CASE(
  "VmfMap native group authoring unsupported components and allocator control", "[vmf]")
{
  const auto kind = GENERATE(
    std::string{"dangling"},
    std::string{"cycle"},
    std::string{"ambiguous"},
    std::string{"hidden"},
    std::string{"opaque"},
    std::string{"entity internal"},
    std::string{"duplicate group id"},
    std::string{"invalid group id"},
    std::string{"missing group id"},
    std::string{"repeated group id"},
    std::string{"dangling entity owner"});
  CAPTURE(kind);
  auto source = kind == "dangling entity owner" ? nativeRamp(false) : nativeGroupedRamp();
  if (kind == "dangling entity owner")
  {
    source.insert(
      source.find("solid {"),
      "} entity { id 12 classname func_detail editor { groupid 99 } ");
  }
  if (kind == "dangling")
  {
    source.replace(source.find("groupid 11"), 10, "groupid 99");
  }
  if (kind == "cycle")
  {
    source.replace(source.find("color \"1 2 3\""), 13, "groupid 11");
  }
  if (kind == "ambiguous")
  {
    source.insert(source.find("groupid 11"), "groupid 10 ");
  }
  if (kind == "hidden")
  {
    source.insert(source.find("solid {"), "hidden { ");
    source.insert(source.find("group { id 10"), "} ");
  }
  if (kind == "opaque")
  {
    source.insert(source.find("group { id 11") + 8, "foreign { sides 3 } ");
  }
  if (kind == "entity internal")
  {
    source.insert(source.find("solid {"), "} entity { id 12 classname func_detail ");
  }
  if (kind == "duplicate group id")
  {
    source.insert(source.find("group { id 11"), "group { id 10 } ");
  }
  if (kind == "invalid group id")
  {
    source.replace(source.find("id 10"), 5, "id nope");
  }
  if (kind == "missing group id")
  {
    source.erase(source.find("id 10"), 5);
  }
  if (kind == "repeated group id")
  {
    source.insert(source.find("id 10"), "id 10 ");
  }
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "unsupported group.vmf";
  writeBinary(path, source);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  CHECK_FALSE(map.vmfScene()->diagnostics().empty());
  auto& target = nativeNode(map, "2");
  auto& marker = nativeNode(map, "9");
  selectNodes(map, {&marker, &target}); // valid earlier candidate, invalid later closure
  if (kind == "hidden")
  {
    CHECK_FALSE(target.selected());
    REQUIRE(marker.selected());
    REQUIRE(map.selection().nodes == std::vector<Node*>{&marker});
    // Deliberately UNSAFE low-level precondition, not ordinary selectable behavior.
    // Keep the backend whole-closure refusal acted after the selection-veto proof.
    map.selectionWillChangeNotifier();
    target.select();
    auto changed = SelectionChange{};
    changed.selectedNodes = {&target};
    map.selectionDidChangeNotifier(changed);
    REQUIRE(target.selected());
    REQUIRE(map.selection().nodes == std::vector<Node*>{&marker, &target});
  }
  const auto selection = map.selection().nodes;
  const auto positions = filePositions(map);
  const auto count = map.modificationCount();
  CHECK_FALSE(canGroupSelectedNodes(map));
  CHECK_FALSE(groupSelectedNodes(map, ""));
  CHECK(map.selection().nodes == selection);
  CHECK(filePositions(map) == positions);
  CHECK(map.modificationCount() == count);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  if (kind == "dangling entity owner")
  {
    deselectAll(map);
    selectNodes(map, {&target});
    CHECK_FALSE(transformSelection(
      map, "Refuse unsupported owner", vm::translation_matrix(vm::vec3d{16, 0, 0})));
    auto* brush = dynamic_cast<BrushNode*>(&target);
    REQUIRE(brush);
    CHECK_FALSE(clipBrushes(map, {brush}, {{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}}));
    duplicateSelectedNodes(map);
    CHECK(map.selection().nodes == std::vector<Node*>{&target});
    CHECK_FALSE(removeSelectedNodes(map));
    CHECK(map.modificationCount() == count);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }
  deselectAll(map);
  selectNodes(map, {&marker});
  REQUIRE(setEntityProperty(map, "targetname", "unrelated_acted"));
  CHECK(map.vmfScene()->serialize(map.worldNode()) != source);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  const auto definition = EntityDefinition{
    .name = "info_target",
    .pointEntityDefinition = PointEntityDefinition{.bounds = vm::bbox3d{8}}};
  auto* control = createPointEntity(map, definition, vm::vec3d{128, 0, 0});
  REQUIRE(control);
  CHECK(
    map.vmfScene()->nativeId(*control)
    == (kind == "entity internal" || kind == "dangling entity owner" ? "13" : "12"));
}

TEST_CASE(
  "VmfMap native group authoring rename linked preview and inverse guards", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "group guards.vmf";
  const auto source = nativeGroupedRamp();
  writeBinary(path, source);
  const auto preview = GENERATE(false, true);
  const auto config = MapFixtureConfig{
    .mapFormat = MapFormat::SourceVMF,
    .worldBounds = vm::bbox3d{4096},
    .readOnlyVmfPreview = preview};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto* outer = dynamic_cast<GroupNode*>(&nativeNode(map, "10"));
  REQUIRE(outer);
  selectNodes(map, {outer});
  const auto count = map.modificationCount();
  renameSelectedGroups(map, "forbidden");
  CHECK(outer->group().name() == "Group 10");
  CHECK_FALSE(canCreateLinkedDuplicate(map));
  CHECK_FALSE(createLinkedDuplicate(map));
  CHECK_FALSE(canSeparateSelectedLinkedGroups(map));
  separateSelectedLinkedGroups(map);
  CHECK_FALSE(canExtractLinkedGroups(map));
  CHECK(extractLinkedGroups(map).empty());
  mergeSelectedGroupsWithGroup(map, outer);
  CHECK(map.modificationCount() == count);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  if (preview)
  {
    CHECK_FALSE(canGroupSelectedNodes(map));
    CHECK_FALSE(canUngroupSelectedNodes(map));
    CHECK_FALSE(groupSelectedNodes(map, ""));
    ungroupSelectedNodes(map);
    CHECK(map.modificationCount() == count);
  }
  else
  {
    auto* group = groupSelectedNodes(map, "");
    REQUIRE(group);
    const auto grouped = map.vmfScene()->serialize(map.worldNode());
    const auto selected = map.selection().nodes;
    const auto order = map.worldNode().defaultLayer()->children();
    group->setVisibilityState(VisibilityState::Hidden);
    map.undoCommand();
    CHECK(map.selection().nodes == selected);
    CHECK(map.worldNode().defaultLayer()->children() == order);
    CHECK(outer->parent() == group);
    group->setVisibilityState(VisibilityState::Inherited);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == grouped);
  }
}

TEST_CASE(
  "VmfMap native group authoring raw membership numeric identity and budget guards",
  "[vmf]")
{
  const auto size = GENERATE(5, 90, 4097);
  auto source = nativeGroupedRamp();
  const auto id = size == 5 ? std::string{"00010"} : std::string(size - 1, '9') + '8';
  source.replace(source.find("id 10"), 5, "id " + id);
  source.replace(source.find("groupid 10"), 10, "groupid " + id);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "numeric groups.vmf";
  writeBinary(path, source);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto* outer = dynamic_cast<GroupNode*>(&nativeNode(map, id));
  REQUIRE(outer);
  CHECK(outer->group().name() == "Group " + id);
  openGroup(map, *outer);
  auto* inner = dynamic_cast<GroupNode*>(&nativeNode(map, "11"));
  REQUIRE(inner);
  openGroup(map, *inner);
  auto& target = nativeNode(map, "2");
  selectNodes(map, {&target});
  const auto positions = filePositions(map);
  const auto count = map.modificationCount();
  if (size > 4096)
  {
    CHECK_FALSE(canGroupSelectedNodes(map));
    CHECK_FALSE(groupSelectedNodes(map, ""));
    CHECK(filePositions(map) == positions);
    CHECK(map.modificationCount() == count);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
    return;
  }
  auto* group = groupSelectedNodes(map, "");
  REQUIRE(group);
  const auto next = size == 5 ? std::string{"12"} : std::string(size - 1, '9') + '9';
  CHECK(map.vmfScene()->nativeId(*group) == next);
  auto expectedSolid = nativeFragment(source, "2");
  expectedSolid.replace(expectedSolid.find("groupid 11"), 10, "groupid \"" + next + "\"");
  const auto bytes = map.vmfScene()->serialize(map.worldNode());
  CHECK(nativeFragment(bytes, "2") == expectedSolid);
  CHECK(nativeFragment(bytes, id) == nativeFragment(source, id));
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  map.redoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
  ungroupSelectedNodes(map);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  const auto saved = env.dir() / "numeric saved.vmf";
  REQUIRE(map.saveAs(saved).is_success());
  CHECK(readBinary(saved) == source);
}

TEST_CASE(
  "VmfMap native group authoring foreign siblings and live closure guards", "[vmf]")
{
  auto source = nativeGroupedRamp();
  source.insert(
    source.find("\"untouched_marker\"") + std::string{"\"untouched_marker\""}.size(),
    " editor { groupid 999 note first note second } ");
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "foreign sibling.vmf";
  writeBinary(path, source);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto& foreign = nativeNode(map, "9");
  auto& outer = nativeNode(map, "10");
  const auto order = map.worldNode().defaultLayer()->children();
  const auto witness =
    map.nodesWillBeRemovedNotifier.connect([&](const std::vector<Node*>& nodes) {
      CHECK(std::ranges::find(nodes, &foreign) == nodes.end());
    });
  selectNodes(map, {&outer});
  auto* created = groupSelectedNodes(map, "");
  REQUIRE(created);
  const auto bytes = map.vmfScene()->serialize(map.worldNode());
  CHECK(nativeFragment(bytes, "9") == nativeFragment(source, "9"));
  map.undoCommand();
  CHECK(map.worldNode().defaultLayer()->children() == order);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  map.redoCommand();
  const auto selected = map.selection().nodes;
  const auto count = map.modificationCount();
  const auto mode = GENERATE(0, 1, 2);
  if (mode == 0)
  {
    foreign.parent()->removeChild(&foreign);
    created->addChild(&foreign);
  }
  else if (mode == 1)
  {
    created->setLinkId("foreign-link");
  }
  else
  {
    auto* nested = dynamic_cast<GroupNode*>(&nativeNode(map, "11"));
    REQUIRE(nested);
    created->setLinkId(nested->linkId());
  }
  CHECK_FALSE(canUngroupSelectedNodes(map));
  ungroupSelectedNodes(map);
  CHECK(map.selection().nodes == selected);
  CHECK(map.modificationCount() == count);
  map.undoCommand();
  CHECK(outer.parent() == created);
  CHECK(map.selection().nodes == selected);
  CHECK_THROWS_AS(map.vmfScene()->serialize(map.worldNode()), std::invalid_argument);
  if (mode == 0)
  {
    created->removeChild(&foreign);
    map.worldNode().defaultLayer()->insertChild(0, &foreign);
  }
}

TEST_CASE(
  "VmfMap native group authoring staged later duplicate budget refusal is atomic",
  "[vmf]")
{
  auto source = nativeGroupedRamp();
  const auto huge = std::string(4095, '9') + '8';
  source.replace(source.find("id 10"), 5, "id " + huge);
  source.replace(source.find("groupid 10"), 10, "groupid " + huge);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "staged group budget.vmf";
  writeBinary(path, source);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto& marker = nativeNode(map, "9");
  auto& inner = nativeNode(map, "11");
  selectNodes(map, {&marker, &inner}); // different physical parents, one whole batch
  const auto selection = map.selection().nodes;
  const auto positions = filePositions(map);
  const auto count = map.modificationCount();
  duplicateSelectedNodes(map); // first legal ID stages, a later ID grows to 4097 digits
  CHECK(map.selection().nodes == selection);
  CHECK(map.modificationCount() == count);
  CHECK(filePositions(map) == positions);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  auto* target = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  REQUIRE(target);
  REQUIRE(clipBrushes(map, {target}, {{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}}));
  const auto clipped = VmfDocument::parse(map.vmfScene()->serialize(map.worldNode()));
  CHECK(std::ranges::any_of(clipped.entries(), [&](const auto& row) {
    return row.value && row.parent && clipped.tokenContents(row.key) == "id"
           && clipped.tokenContents(*row.value) == "9"
           && clipped.tokenContents(clipped.entries()[*row.parent].key) == "side";
  }));
  map.undoCommand();
  deselectAll(map);
  selectNodes(map, {&marker});
  REQUIRE(canGroupSelectedNodes(map));
  auto* group = groupSelectedNodes(map, "");
  REQUIRE(group);
  CHECK(map.vmfScene()->nativeId(*group) == std::string(4096, '9'));
  const auto bytes = map.vmfScene()->serialize(map.worldNode());
  const auto saved = env.dir() / "budget control.vmf";
  REQUIRE(map.saveAs(saved).is_success());
  CHECK(readBinary(saved) == bytes);
}

TEST_CASE(
  "VmfMap native group authoring admission ambiguous candidate isolation", "[vmf]")
{
  const auto editor = GENERATE(
    std::string{"editor { groupid 10 groupid 10 }"},
    std::string{"editor { groupid 10 groupid 20 }"},
    std::string{"editor { groupid 00010 groupid 020 }"},
    std::string{"editor { groupid 10 } editor { groupid 20 }"},
    std::string{"editor {} editor { groupid 010 }"},
    std::string{"editor scalar editor { groupid 010 }"});
  const auto entityMember = GENERATE(false, true);
  CAPTURE(editor, entityMember);
  const auto implicates20 = editor.find("20") != std::string::npos;
  auto source = nativeGroupedRamp();
  const auto old =
    std::string{"editor { groupid 11 color \"9 8 7\" note first note second }"};
  if (!entityMember)
  {
    source.replace(source.find(old), old.size(), editor);
  }
  source.insert(source.find("group { id 11"), "group { id 20 } group { id 30 } ");
  source += "entity { id 21 classname info_target "
    + (entityMember ? editor : std::string{"editor { groupid 10 }"}) + " }\r\n"
    "entity { id 22 classname info_target editor { groupid 20 } }\r\n"
    "entity { id 23 classname info_target editor { groupid 11 } }\r\n"
    "entity { id 24 classname info_target editor { groupid 10 } }\r\n";
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "ambiguous candidates.vmf";
  writeBinary(path, source);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path, {.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  auto& marker = nativeNode(map, "9");
  selectNodes(map, {&marker});
  REQUIRE(setEntityProperty(map, "targetname", "admission_control_acted"));
  CHECK(map.vmfScene()->serialize(map.worldNode()) != source);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  auto ids = std::vector<std::string>{};
  const auto visit = [&](auto&& self, const Node& node) -> void {
    if (dynamic_cast<const GroupNode*>(&node))
    {
      ids.push_back(map.vmfScene()->nativeId(node));
    }
    for (const auto* child : node.children())
    {
      self(self, *child);
    }
  };
  visit(visit, map.worldNode());
  // Do not invoke old UNGROUP/delete on an incorrectly projected empty component.
  CHECK(ids == (implicates20 ? std::vector<std::string>{"30"}
                            : std::vector<std::string>{"20", "30"}));
  for (const auto& id : {"2", "21", "23", "24"})
  {
    deselectAll(map);
    selectNodes(map, {&nativeNode(map, id)});
    const auto selection = map.selection().nodes;
    const auto positions = filePositions(map);
    const auto count = map.modificationCount();
    const auto undo = map.canUndoCommand();
    const auto redo = map.canRedoCommand();
    CHECK_FALSE(canGroupSelectedNodes(map));
    CHECK_FALSE(groupSelectedNodes(map, ""));
    CHECK(map.selection().nodes == selection);
    CHECK(filePositions(map) == positions);
    CHECK(map.modificationCount() == count);
    CHECK(map.canUndoCommand() == undo);
    CHECK(map.canRedoCommand() == redo);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }
  deselectAll(map);
  selectNodes(map, {&nativeNode(map, "22")});
  CHECK(canGroupSelectedNodes(map) == !implicates20);
  deselectAll(map);
  selectNodes(map, {&marker});
  REQUIRE(canGroupSelectedNodes(map));
  auto* control = groupSelectedNodes(map, "");
  REQUIRE(control);
  CHECK(map.vmfScene()->nativeId(*control) == "31");
  CHECK(marker.parent() == control);
  CHECK(map.vmfScene()->serialize(map.worldNode()) != source);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
}

TEST_CASE(
  "VmfMap native group authoring admission malformed membership preflight", "[vmf]")
{
  const auto editor = GENERATE(
    std::string{"editor { groupid { raw 77 } }"},
    std::string{"editor { groupid 77 groupid { raw 77 } }"},
    std::string{"editor { visgroupid { raw 77 } }"},
    std::string{"editor { visgroupid 77 visgroupid { raw 77 } }"},
    std::string{"editor { visgroupshown { raw 1 } }"},
    std::string{"editor { visgroupshown 1 visgroupshown { raw 1 } }"},
    std::string{"editor { visgroupautoshown { raw 1 } }"},
    std::string{"editor { visgroupautoshown 1 visgroupautoshown { raw 1 } }"},
    std::string{"editor { visgroupshown yes }"},
    std::string{"editor { visgroupautoshown yes }"},
    std::string{"editor { visgroupshown 1 visgroupshown 1 }"},
    std::string{"editor { visgroupshown 0 }"},
    std::string{"editor { visgroupautoshown 0 }"},
    std::string{"editor { } editor { }"},
    std::string{"editor scalar"});
  CAPTURE(editor);
  auto source = nativeRamp(false);
  source.insert(source.find("}\r\neditor { "), editor + "\r\n");
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "block membership.vmf";
  writeBinary(path, source);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path, {.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  auto& marker = nativeNode(map, "9");
  selectNodes(map, {&marker});
  REQUIRE(setEntityProperty(map, "targetname", "shape_control_acted"));
  CHECK(map.vmfScene()->serialize(map.worldNode()) != source);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  deselectAll(map);
  selectNodes(map, {&nativeNode(map, "2")});
  const auto selection = map.selection().nodes;
  const auto positions = filePositions(map);
  const auto count = map.modificationCount();
  const auto undo = map.canUndoCommand();
  const auto redo = map.canRedoCommand();
  CHECK_FALSE(canGroupSelectedNodes(map));
  CHECK_FALSE(groupSelectedNodes(map, ""));
  CHECK_FALSE(map.vmfScene()->diagnostics().empty());
  CHECK(map.selection().nodes == selection);
  CHECK(filePositions(map) == positions);
  CHECK(map.modificationCount() == count);
  CHECK(map.canUndoCommand() == undo);
  CHECK(map.canRedoCommand() == redo);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  deselectAll(map);
  selectNodes(map, {&marker});
  REQUIRE(canGroupSelectedNodes(map));
  auto* control = groupSelectedNodes(map, "");
  REQUIRE(control);
  CHECK(map.vmfScene()->nativeId(*control) == "10");
  CHECK(marker.parent() == control);
  CHECK(map.vmfScene()->serialize(map.worldNode()) != source);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
}


TEST_CASE(
  "VmfMap native group authoring command live notification index lifetime", "[vmf]")
{
  const auto nested = GENERATE(false, true);
  const auto removeInsideContainer = GENERATE(false, true);
  CAPTURE(nested, removeInsideContainer);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "indexed groups.vmf";
  auto source = nativeGroupedRamp();
  if (nested)
  {
    source.insert(source.find("connections {"), "editor { groupid 11 } ");
  }
  writeBinary(path, source);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto* marker = &nativeNode(map, "9");
  auto* inner = dynamic_cast<GroupNode*>(&nativeNode(map, "11"));
  auto* outer = dynamic_cast<GroupNode*>(&nativeNode(map, "10"));
  REQUIRE(inner);
  REQUIRE(outer);
  selectNodes(map, {marker});
  REQUIRE(setEntityProperty(map, "targetname", "index_control"));
  CHECK(map.findNodes("index_control") == std::vector<Node*>{marker});
  map.undoCommand();
  REQUIRE(map.vmfScene()->serialize(map.worldNode()) == source);
  if (nested)
  {
    openGroup(map, *outer);
    openGroup(map, *inner);
  }
  auto connection = NotifierConnection{};
  auto detached = size_t{0};
  auto visits = size_t{0};
  auto notified = std::vector<Node*>{};
  connection += map.nodesWereAddedNotifier.connect([&](const std::vector<Node*>& roots) {
    const auto visit = [&](auto&& self, Node* node) -> void {
      if (node == marker)
      {
        ++visits;
      }
      for (auto* child : node->children())
      {
        self(self, child);
      }
    };
    notified.insert(notified.end(), roots.begin(), roots.end());
    for (auto* root : roots)
    {
      auto* top = root;
      while (top->parent())
      {
        top = top->parent();
      }
      if (top != &map.worldNode())
      {
        ++detached;
      }
      visit(visit, root);
    }
  });
  const auto checkIndex = [&](Node* expectedRoot) {
    CHECK(notified == std::vector<Node*>{expectedRoot});
    notified.clear();
    CHECK(map.findNodes("untouched_marker") == std::vector<Node*>{marker});
    CHECK(detached == 0);
    CHECK(visits == 1);
    visits = 0;
    detached = 0;
  };
  deselectAll(map);
  selectNodes(map, {marker});
  auto* created = groupSelectedNodes(map, "");
  REQUIRE(created);
  auto* destination = nested ? static_cast<Node*>(inner) : map.worldNode().defaultLayer();
  CHECK(created->parent() == destination);
  CHECK(created->children() == std::vector<Node*>{marker});
  checkIndex(created);
  map.undoCommand();
  checkIndex(marker);
  map.redoCommand();
  checkIndex(created);
  ungroupSelectedNodes(map);
  REQUIRE(marker->parent() == destination);
  checkIndex(marker);
  map.undoCommand();
  checkIndex(created);
  map.redoCommand();
  checkIndex(marker);
  if (removeInsideContainer)
  {
    map.undoCommand();
    checkIndex(created);
    REQUIRE(marker->parent() == created);
  }
  selectNodes(map, {marker});
  REQUIRE(removeSelectedNodes(map));
  // Raw Node* queries only: no cast/dereference of a possibly stale result.
  CHECK(map.findNodes("untouched_marker").empty());
  map.commandProcessor().clear();
  CHECK(map.findNodes("untouched_marker").empty());
}

TEST_CASE("VmfMap native group authoring command parked replay preflight", "[vmf]")
{
  const auto grouping = GENERATE(false, true);
  const auto state = GENERATE(
    std::string{"normal"},
    std::string{"hidden"},
    std::string{"link"},
    std::string{"locked"},
    std::string{"unlocked"});
  CAPTURE(grouping, state);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "parked groups.vmf";
  const auto source = nativeGroupedRamp();
  writeBinary(path, source);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto* marker = &nativeNode(map, "9");
  selectNodes(map, {marker});
  REQUIRE(setEntityProperty(map, "targetname", "park_control"));
  CHECK(map.findNodes("park_control") == std::vector<Node*>{marker});
  map.undoCommand();
  REQUIRE(map.vmfScene()->serialize(map.worldNode()) == source);
  GroupNode* parked = nullptr;
  if (grouping)
  {
    parked = groupSelectedNodes(map, "");
    REQUIRE(parked);
    map.undoCommand();
  }
  else
  {
    parked = dynamic_cast<GroupNode*>(&nativeNode(map, "10"));
    REQUIRE(parked);
    deselectAll(map);
    selectNodes(map, {parked});
    ungroupSelectedNodes(map);
  }
  REQUIRE(parked->parent() == nullptr);
  REQUIRE(parked->children().empty());
  const auto id = map.vmfScene()->nativeId(*parked);
  const auto link = parked->linkId();
  if (state == "hidden")
  {
    parked->setVisibilityState(VisibilityState::Hidden);
  }
  if (state == "link")
  {
    parked->setLinkId("changed-parked-link");
  }
  if (state == "locked")
  {
    parked->setLockState(LockState::Locked);
  }
  if (state == "unlocked")
  {
    parked->setLockState(LockState::Unlocked);
  }
  const auto before = map.vmfScene()->serialize(map.worldNode());
  const auto positions = filePositions(map);
  const auto order = map.worldNode().defaultLayer()->children();
  const auto selected = map.selection().nodes;
  const auto count = map.modificationCount();
  if (grouping)
  {
    map.redoCommand();
  }
  else
  {
    map.undoCommand();
  }
  if (state == "normal" || state == "unlocked")
  {
    REQUIRE(parked->parent() == map.worldNode().defaultLayer());
    CHECK(map.vmfScene()->nativeId(*parked) == id);
    CHECK(parked->linkId() == link);
    CHECK(
      parked->children()
      == std::vector<Node*>{grouping ? marker : &nativeNode(map, "11")});
    CHECK(map.selection().nodes == std::vector<Node*>{parked});
    if (grouping)
    {
      map.undoCommand();
    }
    else
    {
      map.redoCommand();
    }
    CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
    CHECK(map.selection().nodes == selected);
    CHECK(map.worldNode().defaultLayer()->children() == order);
  }
  else
  {
    // Failed replay may destroy command-owned parked nodes. Never use the
    // retained pointer after this point, except as a raw address comparison.
    CHECK(map.modificationCount() == count);
    CHECK(map.selection().nodes == selected);
    CHECK(map.worldNode().defaultLayer()->children() == order);
    CHECK(filePositions(map) == positions);
    CHECK_FALSE(map.canRedoCommand());
    // Safe old-runtime RED cleanup only via a node that is actually live.
    for (auto* child : map.worldNode().defaultLayer()->children())
    {
      if (child == parked)
      {
        child->setVisibilityState(VisibilityState::Inherited);
        child->setLockState(LockState::Inherited);
        dynamic_cast<GroupNode*>(child)->setLinkId(link);
      }
    }
    CHECK(map.vmfScene()->serialize(map.worldNode()) == before);
    deselectAll(map);
    selectNodes(map, {marker});
    auto* control = groupSelectedNodes(map, "");
    REQUIRE(control);
    CHECK(map.vmfScene()->nativeId(*control) == (grouping ? "13" : "12"));
  }
}

TEST_CASE("VmfMap native group authoring command opened context refusal", "[vmf]")
{
  const auto ancestor = GENERATE(false, true);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "opened groups.vmf";
  const auto source = nativeGroupedRamp();
  writeBinary(path, source);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path,
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto* outer = dynamic_cast<GroupNode*>(&nativeNode(map, "10"));
  auto* inner = dynamic_cast<GroupNode*>(&nativeNode(map, "11"));
  REQUIRE(outer);
  REQUIRE(inner);
  selectNodes(map, {&nativeNode(map, "9")});
  REQUIRE(setEntityProperty(map, "targetname", "context_control"));
  map.undoCommand();
  REQUIRE(map.vmfScene()->serialize(map.worldNode()) == source);
  openGroup(map, *outer);
  openGroup(map, *inner);
  deselectAll(map);
  selectNodes(map, {ancestor ? outer : inner});
  const auto positions = filePositions(map);
  const auto selected = map.selection().nodes;
  const auto count = map.modificationCount();
  const auto undo = map.canUndoCommand();
  const auto redo = map.canRedoCommand();
  // Stop the old unsafe branch before detaching any current editing context.
  REQUIRE_FALSE(canUngroupSelectedNodes(map));
  ungroupSelectedNodes(map);
  CHECK(map.editorContext().currentGroup() == inner);
  CHECK(inner->parent() == outer);
  CHECK(outer->parent() == map.worldNode().defaultLayer());
  CHECK(filePositions(map) == positions);
  CHECK(map.selection().nodes == selected);
  CHECK(map.modificationCount() == count);
  CHECK(map.canUndoCommand() == undo);
  CHECK(map.canRedoCommand() == redo);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  REQUIRE(createBrush(map, nativeCubePoints()));
  auto* brush = map.selection().brushes.front();
  CHECK(brush->parent() == inner);
  CHECK(map.vmfScene()->nativeId(*brush) == "12");
  auto* group = groupSelectedNodes(map, "");
  REQUIRE(group);
  CHECK(group->parent() == inner);
  CHECK(map.vmfScene()->nativeId(*group) == "13");
  REQUIRE(canUngroupSelectedNodes(map));
  ungroupSelectedNodes(map);
  CHECK(brush->parent() == inner);
  CHECK(map.editorContext().currentGroup() == inner);
}
TEST_CASE("VmfMap native group authoring isolation independent plain brush", "[vmf]")
{
  const auto opaque = GENERATE(false, true);
  const auto operation = GENERATE(
    std::string{"group"},
    std::string{"transform"},
    std::string{"front"},
    std::string{"back"},
    std::string{"both"});
  CAPTURE(opaque, operation);
  auto source = nativeRamp(false);
  const auto foreignId = opaque ? std::string{"100"} : std::string{"10"};
  // Keep the shared attachment fixture's opaque input unchanged; ordinary raw
  // group recognition must not accept its unknown geometry-reference child.
  source.insert(
    source.find("solid {"),
    opaque ? "group { id 100 foreign { sides 3 } editor { color \"1 2 3\" } } "
           : "group { id 10 } ");
  source.insert(
    source.find("\"untouched_marker\"") + std::string{"\"untouched_marker\""}.size(),
    " editor { groupid " + foreignId + " groupid " + foreignId + " } ");
  source += "entity { id 3 classname info_target targetname independent_control }\r\n";
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "independent brush isolation.vmf";
  writeBinary(path, source);
  const auto config =
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}};
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto& point = nativeNode(map, "3");
  auto& foreign = nativeNode(map, "9");
  auto* brush = dynamic_cast<BrushNode*>(&nativeNode(map, "2"));
  REQUIRE(brush);
  CHECK(brush->parent() == map.worldNode().defaultLayer());
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  selectNodes(map, {&point});
  REQUIRE(setEntityProperty(map, "targetname", "control_acted"));
  auto propertyBytes = source;
  propertyBytes.replace(
    propertyBytes.find("independent_control"),
    std::string{"independent_control"}.size(),
    "\"control_acted\"");
  CHECK(map.vmfScene()->serialize(map.worldNode()) == propertyBytes);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);

  const auto foreignBytes = [&](const std::string& bytes) {
    CHECK(nativeFragment(bytes, foreignId) == nativeFragment(source, foreignId));
    CHECK(nativeFragment(bytes, "9") == nativeFragment(source, "9"));
  };
  deselectAll(map);
  selectNodes(map, {&foreign});
  const auto positions = filePositions(map);
  const auto foreignSelection = map.selection().nodes;
  const auto count = map.modificationCount();
  const auto undo = map.canUndoCommand();
  const auto redo = map.canRedoCommand();
  CHECK_FALSE(canGroupSelectedNodes(map));
  CHECK_FALSE(groupSelectedNodes(map, ""));
  CHECK(filePositions(map) == positions);
  CHECK(map.selection().nodes == foreignSelection);
  CHECK(map.modificationCount() == count);
  CHECK(map.canUndoCommand() == undo);
  CHECK(map.canRedoCommand() == redo);
  CHECK(map.vmfScene()->serialize(map.worldNode()) == source);

  deselectAll(map);
  selectNodes(map, {brush});
  const auto brushSelection = map.selection().nodes;
  auto* created = static_cast<GroupNode*>(nullptr);
  auto acted = false;
  if (operation == "group")
  {
    CHECK(canGroupSelectedNodes(map) == !opaque);
    created = groupSelectedNodes(map, "");
    acted = created != nullptr;
  }
  else if (operation == "transform")
  {
    acted = transformSelection(
      map, "Move independent brush", vm::translation_matrix(vm::vec3d{16, 0, 0}));
  }
  else
  {
    const auto plane =
      operation == "back"
        ? std::array<vm::vec3d, 3>{{{32, 0, 0}, {32, 64, 0}, {32, 0, 64}}}
        : std::array<vm::vec3d, 3>{{{32, 0, 0}, {32, 0, 64}, {32, 64, 0}}};
    acted = (operation == "both" ? clipBrushesBoth : clipBrushes)(map, {brush}, plane);
  }
  if (opaque)
  {
    CHECK_FALSE(acted);
    CHECK(filePositions(map) == positions);
    CHECK(map.selection().nodes == brushSelection);
    CHECK(map.modificationCount() == count);
    CHECK(map.canUndoCommand() == undo);
    CHECK(map.canRedoCommand() == redo);
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }
  else
  {
    // RED stops safely here; never dereference a refused GROUP result.
    REQUIRE(acted);
    CHECK(map.modificationCount() == count + 1);
    if (operation == "group")
    {
      REQUIRE(created);
      CHECK(map.vmfScene()->nativeId(*created) == "11");
      CHECK(brush->parent() == created);
      CHECK(created->children() == std::vector<Node*>{brush});
    }
    else if (operation == "transform")
    {
      CHECK(brush->brush().bounds().min.x() == 16);
      CHECK(brush->brush().bounds().max.x() == 80);
    }
    else if (operation == "back")
    {
      CHECK(brush->brush().bounds().min.x() == 32);
    }
    else
    {
      CHECK(brush->brush().bounds().max.x() == 32);
    }
    const auto bytes = map.vmfScene()->serialize(map.worldNode());
    CHECK(bytes != source);
    foreignBytes(bytes);
    const auto saved = env.dir() / "acted isolation.vmf";
    REQUIRE(map.saveAs(saved).is_success());
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(saved, config);
    CHECK(reopened.vmfScene()->serialize(reopened.worldNode()) == bytes);
    auto* reopenedBrush = dynamic_cast<BrushNode*>(&nativeNode(reopened, "2"));
    REQUIRE(reopenedBrush);
    CHECK(reopenedBrush->brush().bounds() == brush->brush().bounds());
    if (operation == "group")
    {
      CHECK(reopenedBrush->parent() == &nativeNode(reopened, "11"));
      CHECK(dynamic_cast<GroupNode*>(reopenedBrush->parent()));
    }
    if (operation == "both")
    {
      auto* back = dynamic_cast<BrushNode*>(&nativeNode(reopened, "11"));
      REQUIRE(back);
      CHECK(back->parent() == reopenedBrush->parent());
      CHECK(back->brush().bounds().min.x() == 32);
    }
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
    CHECK(filePositions(map) == positions);
    CHECK(map.modificationCount() == count);
    CHECK(map.selection().nodes == brushSelection);
    map.redoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }
  // A legal next GROUP proves the rejected ambiguous closure consumed no ID.
  // BOTH intentionally retains its admitted sibling ID even after undo.
  if (opaque || operation != "group")
  {
    deselectAll(map);
    selectNodes(map, {opaque ? &point : static_cast<Node*>(brush)});
    auto* control = groupSelectedNodes(map, "");
    REQUIRE(control);
    CHECK(map.vmfScene()->nativeId(*control) == (opaque ? "101" : operation == "both" ? "12" : "11"));
    foreignBytes(map.vmfScene()->serialize(map.worldNode()));
    map.undoCommand();
    CHECK(map.vmfScene()->serialize(map.worldNode()) == source);
  }
  CHECK(readBinary(path) == source);
}

} // namespace tb::mdl
