/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "base/NotifierConnection.h"
#include "fs/TestEnvironment.h"
#include "mdl/CommandProcessor.h"
#include "mdl/EditorContext.h"
#include "mdl/EntityNode.h"
#include "mdl/GroupNode.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/Map_Entities.h"
#include "mdl/Map_Groups.h"
#include "mdl/Map_NativeCordons.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/NativeCordonCommand.h"
#include "mdl/Selection.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"

#include <fstream>
#include <iterator>
#include <limits>
#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::mdl
{
namespace
{
const auto config = MapFixtureConfig{.mapFormat = MapFormat::SourceVMF};
const auto original = std::string{
  "\xEF\xBB\xBF// synthetic history fixture\r\n"
  "versioninfo { \"formatversion\" \"100\" }\r\n"
  "world { \"id\" \"1\" \"classname\" \"worldspawn\" }\r\n"
  "cordons { \"active\" \"0\" cordon { \"name\" \"Loaded\" \"active\" \"1\" "
  "box { \"mins\" \"(-01.00 0 0)\" \"maxs\" \"(64 64 64)\" } } }\r\n"};
void write(const std::filesystem::path& path, const std::string& bytes)
{
  auto stream = std::ofstream{path, std::ios::binary};
  stream << bytes;
}
std::string read(const std::filesystem::path& path)
{
  auto stream = std::ifstream{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}
std::string serialize(const Map& map)
{
  return map.vmfScene()->serialize(map.worldNode());
}
} // namespace

TEST_CASE("NativeCordon history dispatch runtime control", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "original.vmf";
  write(path, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  CHECK(serialize(map) == original);

  selectNodes(map, {&map.worldNode()});
  REQUIRE(setEntityProperty(map, "message", "safe native control"));
  CHECK(map.modified());
  CHECK(serialize(map).find("safe native control") != std::string::npos);
  map.undoCommand();
  CHECK(serialize(map) == original);
  CHECK_FALSE(map.modified());

  auto request = NativeCordonRequest{nativeCordons(map)};
  request.records.front().name = "Live";
  request.active = true;
  request.records.front().boxes.front().maxs = vm::vec3d{128, 64, 64};
  const auto count = map.modificationCount();
  const auto result = applyNativeCordons(map, request);
  CHECK(result.status == NativeCordonApplyStatus::Changed);
  CHECK(map.modificationCount() == count + 1);
  CHECK(map.modified());
  CHECK(map.undoCommandName() != nullptr);
  if (map.undoCommandName())
  {
    CHECK(*map.undoCommandName() == "Edit Native Cordons");
  }
  CHECK(nativeCordons(map).records.front().name == std::optional<std::string>{"Live"});
  auto expected = original;
  expected.replace(expected.find("\"Loaded\""), 8, "\"Live\"");
  expected.replace(expected.find("\"active\" \"0\""), 12, "\"active\" \"1\"");
  expected.replace(expected.find("(64 64 64)"), 10, "(128 64 64)");
  CHECK(serialize(map) == expected);
  const auto saved = env.dir() / "edited.vmf";
  REQUIRE(map.saveAs(saved).is_success());
  CHECK(read(saved) == expected);
  CHECK(read(path) == original);
  const auto live = nativeCordons(map);
  const auto* document = &map.vmfScene()->document();
  auto reopenedFixture = MapFixture{};
  auto& reopened = reopenedFixture.load(saved, config);
  CHECK(nativeCordons(reopened).records.front().name == "Live");
  auto further = NativeCordonRequest{nativeCordons(reopened)};
  further.records.front().active = false;
  REQUIRE(
    applyNativeCordons(reopened, further).status == NativeCordonApplyStatus::Changed);
  reopened.undoCommand();
  CHECK(serialize(reopened) == expected);
  reopened.redoCommand();
  CHECK(nativeCordons(reopened).records.front().active == false);

  map.undoCommand();
  CHECK(map.modified()); // Save As moved the save point, not the retained source.
  CHECK(serialize(map) == original);
  const auto undone = env.dir() / "undone.vmf";
  REQUIRE(map.saveTo(undone).is_success());
  CHECK(read(undone) == original);
  map.redoCommand();
  CHECK_FALSE(map.modified());
  CHECK(serialize(map) == expected);
  CHECK(nativeCordons(map).records.front().id == live.records.front().id);
  CHECK(
    nativeCordons(map).records.front().boxes.front().id
    == live.records.front().boxes.front().id);
  CHECK(&map.vmfScene()->document() == document);
  REQUIRE(map.save().is_success());
  CHECK(read(saved) == expected);
}
namespace
{
NativeCordonRequest createRequest(const Map& map)
{
  auto request = NativeCordonRequest{nativeCordons(map)};
  request.form = NativeCordonForm::Modern;
  request.active = false;
  request.records.push_back({{}, "New", false, {{{}, {-8, -16, -32}, {8, 16, 32}}}});
  return request;
}
NativeCordonRequest removeRequest(const Map& map)
{
  auto request = NativeCordonRequest{nativeCordons(map)};
  request.form = NativeCordonForm::None;
  request.active.reset();
  request.records.clear();
  return request;
}
} // namespace

TEST_CASE("NativeCordon history noops refusals and queued tokens", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "source.vmf";
  write(path, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto foreignFixture = MapFixture{};
  auto& foreign = foreignFixture.load(path, config);
  const auto baseline = nativeCordons(map);
  auto request = NativeCordonRequest{baseline};
  CHECK(applyNativeCordons(map, request).status == NativeCordonApplyStatus::Unchanged);
  CHECK_FALSE(map.undoCommandName());
  CHECK_FALSE(map.modified());
  request.records.front().name = "First";
  REQUIRE(applyNativeCordons(map, request).status == NativeCordonApplyStatus::Changed);
  map.undoCommand();
  REQUIRE(map.redoCommandName());
  const auto count = map.modificationCount();
  const auto revision = nativeCordons(map).revision;
  auto assertRefused = [&](const NativeCordonRequest& input) {
    const auto result = applyNativeCordons(map, input);
    CHECK(result.status == NativeCordonApplyStatus::Refused);
    CHECK_FALSE(result.reason.empty());
    CHECK(serialize(map) == original);
    CHECK(map.modificationCount() == count);
    CHECK(nativeCordons(map).revision == revision);
    CHECK_FALSE(map.undoCommandName());
    CHECK(map.redoCommandName() != nullptr);
  };
  assertRefused(request); // Edit/undo has equal state, but an old UI revision.
  auto other = NativeCordonRequest{nativeCordons(foreign)};
  other.records.front().name = "Foreign";
  assertRefused(other);
  auto invalid = NativeCordonRequest{nativeCordons(map)};
  invalid.records.front().name = "Valid earlier field";
  invalid.active = true;
  invalid.records.front().boxes.push_back({{}, {0, 0, 0}, {1, 1, 0}});
  assertRefused(invalid); // No partial name/activation or handle consumption.
  invalid = NativeCordonRequest{nativeCordons(map)};
  invalid.records.front().boxes.front().id =
    nativeCordons(foreign).records.front().boxes.front().id;
  assertRefused(invalid);
  auto unchanged = NativeCordonRequest{nativeCordons(map)};
  CHECK(applyNativeCordons(map, unchanged).status == NativeCordonApplyStatus::Unchanged);
  CHECK(map.redoCommandName() != nullptr);
  CHECK(nativeCordons(map).revision == revision);
  map.redoCommand();
  CHECK(nativeCordons(map).records.front().name == "First");
  auto queued = NativeCordonRequest{nativeCordons(map)};
  queued.records.front().name = "Queued after deletion";
  REQUIRE(
    applyNativeCordons(map, removeRequest(map)).status
    == NativeCordonApplyStatus::Changed);
  CHECK(applyNativeCordons(map, queued).status == NativeCordonApplyStatus::Refused);
  REQUIRE(
    applyNativeCordons(map, createRequest(map)).status
    == NativeCordonApplyStatus::Changed);
  CHECK(nativeCordons(map).records.front().id != baseline.records.front().id);
  CHECK(applyNativeCordons(map, queued).status == NativeCordonApplyStatus::Refused);
}

TEST_CASE("NativeCordon history modern complete record and box operations", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "source.vmf";
  write(path, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto request = NativeCordonRequest{nativeCordons(map)};
  request.active.reset();
  request.records.front().active.reset();
  request.records.front().boxes.push_back({{}, {-64, -64, -64}, {-32, -32, -32}});
  request.records.push_back({{}, "Second", std::nullopt, {{{}, {1, 2, 3}, {4, 5, 6}}}});
  REQUIRE(applyNativeCordons(map, request).status == NativeCordonApplyStatus::Changed);
  const auto multi = nativeCordons(map);
  const auto multiBytes = serialize(map);
  CHECK(multi.records.size() == 2);
  CHECK(multi.records.front().boxes.size() == 2);
  CHECK_FALSE(multi.active.has_value());
  CHECK_FALSE(multi.records.front().active.has_value());
  auto edit = NativeCordonRequest{multi};
  edit.records.front().boxes.erase(edit.records.front().boxes.begin());
  edit.records.back().name = "Renamed";
  REQUIRE(applyNativeCordons(map, edit).status == NativeCordonApplyStatus::Changed);
  auto removal = NativeCordonRequest{nativeCordons(map)};
  removal.records.erase(removal.records.begin());
  REQUIRE(applyNativeCordons(map, removal).status == NativeCordonApplyStatus::Changed);
  REQUIRE(
    applyNativeCordons(map, removeRequest(map)).status
    == NativeCordonApplyStatus::Changed);
  CHECK(nativeCordons(map).form == NativeCordonForm::None);
  CHECK(serialize(map).find("cordons") == std::string::npos);
  map.undoCommand();
  CHECK(nativeCordons(map).records.front().id == multi.records.back().id);
  map.undoCommand();
  map.undoCommand();
  CHECK(serialize(map) == multiBytes);
  CHECK(nativeCordons(map).records == multi.records);
  map.undoCommand();
  CHECK(serialize(map) == original);
  CHECK_FALSE(map.modified());
  map.redoCommand();
  CHECK(serialize(map) == multiBytes);
  CHECK(nativeCordons(map).records == multi.records);
}

TEST_CASE("NativeCordon history legacy and empty controller removal", "[vmf]")
{
  const auto controller = GENERATE(
    std::string{
      "cordon { \"mins\" \"(-01 0 0)\" \"maxs\" \"(64 64 64)\" \"active\" \"0\" }\r\n"},
    std::string{"cordons {}\r\n"});
  const auto source =
    std::string{"world { \"id\" \"1\" \"classname\" \"worldspawn\" }\r\n"} + controller;
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "source.vmf";
  write(path, source);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  const auto initial = nativeCordons(map);
  CHECK(
    applyNativeCordons(map, NativeCordonRequest{initial}).status
    == NativeCordonApplyStatus::Unchanged);
  if (initial.form == NativeCordonForm::Legacy)
  {
    auto edit = NativeCordonRequest{initial};
    edit.records.front().active = true;
    edit.records.front().boxes.front().maxs = vm::vec3d{100, 200, 300};
    REQUIRE(applyNativeCordons(map, edit).status == NativeCordonApplyStatus::Changed);
    auto expected = source;
    expected.replace(expected.find("(64 64 64)"), 10, "(100 200 300)");
    expected.replace(expected.find("\"active\" \"0\""), 12, "\"active\" \"1\"");
    CHECK(serialize(map) == expected);
    map.undoCommand();
    CHECK(serialize(map) == source);
    map.redoCommand();
  }
  const auto beforeRemoval = serialize(map);
  REQUIRE(
    applyNativeCordons(map, removeRequest(map)).status
    == NativeCordonApplyStatus::Changed);
  CHECK(nativeCordons(map).form == NativeCordonForm::None);
  CHECK(serialize(map).find("cordon") == std::string::npos);
  map.undoCommand();
  CHECK(serialize(map) == beforeRemoval);
  CHECK(nativeCordons(map).form == initial.form);
  if (!initial.records.empty())
  {
    CHECK(nativeCordons(map).records.front().id == initial.records.front().id);
  }
  map.redoCommand();
  REQUIRE(
    applyNativeCordons(map, createRequest(map)).status
    == NativeCordonApplyStatus::Changed);
  CHECK(nativeCordons(map).form == NativeCordonForm::Modern);
  map.undoCommand();
  CHECK(nativeCordons(map).form == NativeCordonForm::None);
  map.undoCommand();
  CHECK(serialize(map) == beforeRemoval);
}

TEST_CASE("NativeCordon history fresh composition and native namespace", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto starter = env.dir() / "starter.vmf";
  write(starter, R"(world { id 1 classname worldspawn })");
  auto freshConfig = config;
  freshConfig.gameInfo.gameConfig.fileFormats = {{"Source VMF", starter}};
  auto fixture = MapFixture{};
  auto& map = fixture.create(freshConfig);
  auto controlFixture = MapFixture{};
  auto& control = controlFixture.create(freshConfig);
  REQUIRE(
    applyNativeCordons(map, createRequest(map)).status
    == NativeCordonApplyStatus::Changed);
  const auto accepted = nativeCordons(map);
  auto invalid = NativeCordonRequest{accepted};
  invalid.records.front().boxes.front().maxs =
    vm::vec3d{std::numeric_limits<double>::infinity(), 16, 32};
  CHECK(applyNativeCordons(map, invalid).status == NativeCordonApplyStatus::Refused);
  auto addPoint = [](Map& target) {
    auto entity = std::make_unique<EntityNode>(
      Entity{{{"classname", "info_target"}, {"origin", "1 2 3"}}});
    auto* node = entity.get();
    REQUIRE(addNodes(target, {{target.worldNode().defaultLayer(), {node}}}).size() == 1);
    entity.release();
    return node;
  };
  auto* point = addPoint(map);
  auto* controlPoint = addPoint(control);
  CHECK(point->lineNumber() == controlPoint->lineNumber());
  const auto bytes = serialize(map);
  REQUIRE(
    applyNativeCordons(map, removeRequest(map)).status
    == NativeCordonApplyStatus::Changed);
  CHECK(serialize(map) == serialize(control));
  map.undoCommand();
  CHECK(serialize(map) == bytes);
  CHECK(nativeCordons(map).records == accepted.records);
  const auto path = env.dir() / "composed.vmf";
  REQUIRE(map.saveAs(path).is_success());
  CHECK(read(path) == bytes);
  REQUIRE(map.save().is_success());
  CHECK(read(path) == bytes);
  auto reopenedFixture = MapFixture{};
  auto& reopened = reopenedFixture.load(path, config);
  CHECK(nativeCordons(reopened).records.size() == 1);
  CHECK(
    reopened.worldNode().defaultLayer()->childCount()
    == map.worldNode().defaultLayer()->childCount());
}

TEST_CASE("NativeCordon history value notifications and scene invariants", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "scene.vmf";
  const auto entity = std::string{
    "entity { \"id\" \"2\" \"classname\" \"info_target\" \"origin\" \"1 2 3\" "
    "connections { \"OnUser1\" \"x,Enable,,0,-1\" \"OnUser1\" \"x,Disable,,1,-1\" } "
    "}\r\n"};
  auto grouped = original;
  const auto worldEnd = grouped.find("\"worldspawn\" }");
  REQUIRE(worldEnd != std::string::npos);
  grouped.insert(worldEnd + 13, " group { id 3 editor { color \"1 2 3\" } } ");
  auto groupedEntity = entity;
  groupedEntity.insert(groupedEntity.size() - 3, " editor { groupid 3 } ");
  write(path, grouped + groupedEntity);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto* group =
    dynamic_cast<GroupNode*>(map.worldNode().defaultLayer()->children().front());
  REQUIRE(group);
  openGroup(map, *group);
  REQUIRE(group->childCount() == 1);
  auto* point = group->children().front();
  selectNodes(map, {point});
  const auto nodes = map.selection().nodes;
  const auto faces = map.selection().brushFaces;
  const auto bounds = map.selectionBounds();
  const auto children = map.worldNode().defaultLayer()->children();
  const auto count = map.worldNode().descendantCount();
  const auto binding = point->lineNumber();
  const auto indexed = map.findNodes("*");
  const auto visible = point->visible();
  auto* context = map.editorContext().currentGroup();
  const auto* document = &map.vmfScene()->document();
  size_t done = 0, undone = 0, modificationSignals = 0, nodeEvents = 0;
  auto connections = NotifierConnection{};
  connections += map.commandProcessor().transactionDoneNotifier.connect(
    [&](const std::string&, bool observable, bool modification) {
      if (observable && modification)
      {
        ++done;
      }
    });
  connections += map.commandProcessor().transactionUndoneNotifier.connect(
    [&](const std::string&, bool observable, bool modification) {
      if (observable && modification)
      {
        ++undone;
      }
    });
  connections +=
    map.modificationStateDidChangeNotifier.connect([&] { ++modificationSignals; });
  connections += map.nodesDidChangeNotifier.connect([&](const auto&) { ++nodeEvents; });
  auto first = NativeCordonRequest{nativeCordons(map)};
  first.records.front().name = "Dirty first";
  REQUIRE(applyNativeCordons(map, first).status == NativeCordonApplyStatus::Changed);
  const auto signalCount = modificationSignals;
  auto second = NativeCordonRequest{nativeCordons(map)};
  second.records.front().name = "Dirty second";
  REQUIRE(applyNativeCordons(map, second).status == NativeCordonApplyStatus::Changed);
  CHECK(done == 2);
  CHECK(modificationSignals == signalCount + 1);
  CHECK(map.modified());
  map.undoCommand();
  CHECK(undone == 1);
  map.redoCommand();
  CHECK(done == 3);
  CHECK(nodeEvents == 0);
  CHECK(map.selection().nodes == nodes);
  CHECK(map.selection().brushFaces == faces);
  CHECK(map.selectionBounds() == bounds);
  CHECK(map.worldNode().defaultLayer()->children() == children);
  CHECK(map.worldNode().descendantCount() == count);
  CHECK(point->lineNumber() == binding);
  CHECK(point->visible() == visible);
  CHECK(map.editorContext().currentGroup() == context);
  CHECK(&map.vmfScene()->document() == document);
  CHECK(map.findNodes("*") == indexed);
  CHECK(serialize(map).find(groupedEntity) != std::string::npos);
}

TEST_CASE(
  "NativeCordon history disabled capabilities preserve independent edits", "[vmf]")
{
  auto nonNativeFixture = MapFixture{};
  auto& nonNative = nonNativeFixture.create();
  CHECK_FALSE(nativeCordons(nonNative).refusalReason.empty());
  CHECK(
    applyNativeCordons(nonNative, NativeCordonRequest{nativeCordons(nonNative)}).status
    == NativeCordonApplyStatus::Refused);
  CHECK_FALSE(nonNative.undoCommandName());
  const auto suffix = GENERATE(
    std::string{"cordons { unknown { \"opaque\" \"keep\" } }"},
    std::string{"cordons {} cordon { \"mins\" \"(0 0 0)\" \"maxs\" \"(1 1 1)\" }"},
    std::string{"Cordons {}"},
    std::string{"cordons { cordon { box { \"mins\" \"(0 0 0)\" } } }"});
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "unsupported.vmf";
  const auto source =
    std::string{"world { \"id\" \"1\" \"classname\" \"worldspawn\" }\r\n"} + suffix;
  write(path, source);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  CHECK_FALSE(nativeCordons(map).refusalReason.empty());
  auto request = NativeCordonRequest{nativeCordons(map)};
  CHECK(applyNativeCordons(map, request).status == NativeCordonApplyStatus::Refused);
  CHECK_FALSE(map.modified());
  CHECK_FALSE(map.undoCommandName());
  CHECK(serialize(map) == source);
  selectNodes(map, {&map.worldNode()});
  REQUIRE(setEntityProperty(map, "message", "independent"));
  CHECK(serialize(map).find(suffix) != std::string::npos);
  map.undoCommand();
  CHECK(serialize(map) == source);
}

TEST_CASE("NativeCordon history preview and unsupported edit versions", "[vmf]")
{
  const auto preview = GENERATE(false, true);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "restricted.vmf";
  auto source = original;
  if (!preview)
  {
    source.replace(source.find("\"100\""), 5, "\"999\"");
  }
  write(path, source);
  auto fixture = MapFixture{};
  auto options = config;
  options.readOnlyVmfPreview = preview;
  auto& map = fixture.load(path, options);
  CHECK(map.isReadOnlyVmfPreview() == preview);
  CHECK_FALSE(nativeCordons(map).refusalReason.empty());
  auto request = NativeCordonRequest{nativeCordons(map)};
  request.records.front().name = "Not admitted";
  CHECK(applyNativeCordons(map, request).status == NativeCordonApplyStatus::Refused);
  CHECK(serialize(map) == source);
  CHECK_FALSE(map.modified());
  CHECK_FALSE(map.undoCommandName());
}

TEST_CASE(
  "NativeCordon history expected state replay uses existing failure disposal", "[vmf]")
{
  const auto redo = GENERATE(false, true);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "replay.vmf";
  write(path, original);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, config);
  auto request = NativeCordonRequest{nativeCordons(map)};
  request.records.front().name = "Command";
  REQUIRE(applyNativeCordons(map, request).status == NativeCordonApplyStatus::Changed);
  if (redo)
  {
    map.undoCommand();
  }
  // Disturb authority to exercise expected-state refusal; never retain a borrowed
  // command pointer across generic failed-history disposal.
  auto disturbance = NativeCordonRequest{nativeCordons(map)};
  disturbance.records.front().name = "Out of expected state";
  auto staged = map.vmfScene()->prepareCordonChange(disturbance);
  REQUIRE(staged);
  map.vmfScene()->applyCordonChange(*staged, NativeCordonDirection::Forward);
  const auto bytes = serialize(map);
  const auto count = map.modificationCount();
  if (redo)
  {
    CHECK_FALSE(map.commandProcessor().redo());
  }
  else
  {
    CHECK_FALSE(map.commandProcessor().undo());
  }
  CHECK(serialize(map) == bytes);
  CHECK(map.modificationCount() == count);
  CHECK_FALSE(map.undoCommandName());
  CHECK_FALSE(map.redoCommandName());
}

TEST_CASE(
  "NativeCordon history delayed first command dispatch refuses stale preparation",
  "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto starter = env.dir() / "starter.vmf";
  write(starter, R"(world { id 1 classname worldspawn })");
  auto freshConfig = config;
  freshConfig.gameInfo.gameConfig.fileFormats = {{"Source VMF", starter}};
  auto fixture = MapFixture{};
  auto& map = fixture.create(freshConfig);
  auto request = createRequest(map);
  auto prepared = map.vmfScene()->prepareCordonChange(request);
  REQUIRE(prepared);
  REQUIRE(applyNativeCordons(map, request).status == NativeCordonApplyStatus::Changed);
  map.undoCommand();
  const auto bytes = serialize(map);
  const auto revision = nativeCordons(map).revision;
  CHECK_FALSE(
    map.executeAndStore(std::make_unique<NativeCordonCommand>(std::move(*prepared))));
  CHECK(serialize(map) == bytes);
  CHECK(nativeCordons(map).revision == revision);
  CHECK_FALSE(map.modified());
  CHECK_FALSE(map.undoCommandName());
  CHECK(map.redoCommandName() != nullptr);
}
} // namespace tb::mdl
