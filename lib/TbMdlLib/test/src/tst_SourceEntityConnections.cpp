/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fs/TestEnvironment.h"
#include "mdl/EntityNode.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_Entities.h"
#include "mdl/Map_Selection.h"
#include "mdl/VmfScene.h"

#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
std::string sourceOutputs(bool opaque = true)
{
  auto source = std::string{
    "// synthetic output preservation control\r\n"
    "world { \"id\" \"1\" \"classname\" \"worldspawn\" }\r\n"
    "entity { \"id\" \"2\" \"classname\" \"logic_relay\" \"targetname\" "
    "\"relay\"\r\n"
    "connections {\r\n"
    "\"OnTrigger\" \"!self,Enable,,0.000,-1\" // row one\r\n"
    "\"OnTrigger\" \"!activator\x1b"
    "AddOutput\x1b"
    "message a,b\x1b"
    "0.25\x1b"
    "1\"\r\n"
    "\"OnTrigger\" \"malformed native opaque row\"\r\n"
    "extension { \"opaque\" \"keep\" }\r\n"
    "}\r\neditor { \"color\" \"0 128 255\" } }\r\n"};
  if (!opaque)
  {
    const auto start = source.find("\"OnTrigger\" \"malformed");
    const auto end = source.find("\r\n}\r\n", start) + 2;
    source.replace(start, end - start, "\"OnTrigger\" \"!self,Use,,0,-1\"\r\n");
  }
  return source;
}
EntityNode& outputEntity(WorldNode& world)
{
  return *static_cast<EntityNode*>(world.defaultLayer()->children().at(0));
}
std::string readOutputFile(const std::filesystem::path& path)
{
  auto stream = std::ifstream{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}
} // namespace

TEST_CASE("SourceEntityConnection fields", "[vmf]")
{
  REQUIRE(parseSourceConnection("!self,Use,,0,-1"));
  const auto esc = parseSourceConnection(
    "!caller\x1b"
    "AddOutput\x1b"
    "a,b\x1b"
    "0.25\x1b"
    "1");
  REQUIRE(esc);
  CHECK(esc->parameter == "a,b");
  CHECK(esc->delimiter == '\x1b');
  const auto row = SourceEntityConnection{"OnUser1", "!self,Use,,0.000,01", size_t{42}};
  for (const auto field :
       {SourceConnectionField::Event,
        SourceConnectionField::Target,
        SourceConnectionField::Input,
        SourceConnectionField::Parameter,
        SourceConnectionField::Delay,
        SourceConnectionField::MaxFires,
        SourceConnectionField::Delimiter})
  {
    const auto text = sourceConnectionFieldText(row, field);
    REQUIRE(text);
    REQUIRE(editSourceConnectionField(row, field, *text));
    CHECK(*editSourceConnectionField(row, field, *text) == row);
  }
  const auto fires = editSourceConnectionField(row, SourceConnectionField::MaxFires, "2");
  REQUIRE(fires);
  CHECK(fires->value == "!self,Use,,0.000,2");
  CHECK_FALSE(editSourceConnectionField(row, SourceConnectionField::Parameter, "a,b"));
  CHECK_FALSE(editSourceConnectionField(
    row,
    SourceConnectionField::Parameter,
    "a"
    "b"));
  CHECK_FALSE(editSourceConnectionField(
    {"OnUser1", "malformed", {}}, SourceConnectionField::Delay, "1"));
  for (const auto& invalid :
       {"a,b,c,0,-1,extra",
        "a,b,c,-0.1,-1",
        "a,b,c,nan,-1",
        "a,b,c,inf,-1",
        "a,b,c,0,-2",
        "a,b,c,0,1.5",
        "a,b,c,0,",
        ",b,c,0,-1"})
  {
    CHECK_FALSE(parseSourceConnection(invalid));
  }
}

TEST_CASE("VmfDocument structural edits preserve untouched spans", "[vmf]")
{
  const auto source =
    std::string{"root { \"a\" \"1\" /* comment */ child { \"x\" \"2\" } }\r\n"};
  const auto d = VmfDocument::parse(source);
  CHECK(d.entries()[0].end == source.size() - 2);
  const auto rename =
    std::array{VmfStructuralEdit{VmfStructuralEdit::Type::Key, 1, "\"b\""}};
  CHECK(
    d.serialize({}, rename)
    == "root { \"b\" \"1\" /* comment */ child { \"x\" \"2\" } }\r\n");
  const auto remove =
    std::array{VmfStructuralEdit{VmfStructuralEdit::Type::Remove, 2, {}}};
  CHECK(d.serialize({}, remove) == "root { \"a\" \"1\" /* comment */  }\r\n");
  const auto append =
    std::array{VmfStructuralEdit{VmfStructuralEdit::Type::Append, 0, "\"c\" \"3\""}};
  const auto added = d.serialize({}, append);
  CHECK(VmfDocument::parse(added).entries().size() == 5);
  CHECK(added.find(source.substr(0, source.size() - 3)) == 0);
  const auto overlap =
    std::array{remove[0], VmfStructuralEdit{VmfStructuralEdit::Type::Key, 3, "\"y\""}};
  CHECK_THROWS_AS(d.serialize({}, overlap), std::invalid_argument);
  const auto duplicate = std::array{append[0], append[0]};
  CHECK_THROWS_AS(d.serialize({}, duplicate), std::invalid_argument);
  const auto badAppend =
    std::array{VmfStructuralEdit{VmfStructuralEdit::Type::Append, 0, "} hacked {"}};
  CHECK_THROWS_AS(d.serialize({}, badAppend), std::invalid_argument);
  const auto badId =
    std::array{VmfStructuralEdit{VmfStructuralEdit::Type::Remove, 99, {}}};
  CHECK_THROWS_AS(d.serialize({}, badId), std::invalid_argument);
}

TEST_CASE("VmfScene ordered Source outputs and property authoring", "[vmf]")
{
  const auto original = sourceOutputs();
  auto scene = VmfScene::read(original, vm::bbox3d{4096});
  auto& node = outputEntity(scene.world());
  auto e = node.entity();
  const auto baseline = e.sourceConnections();
  REQUIRE(baseline.size() == 3);
  CHECK(scene.serialize(scene.world()) == original);
  CHECK(e == Entity{e});

  SECTION("one event and value change affects exactly that row")
  {
    auto rows = baseline;
    rows[0].event = "OnUser1";
    rows[0].value = "!self,Disable,,0.5,-1";
    e.setSourceConnections(rows);
    node.setEntity(e);
    auto expected = original;
    const auto old = std::string{"\"OnTrigger\" \"!self,Enable,,0.000,-1\""};
    expected.replace(
      expected.find(old), old.size(), "\"OnUser1\" \"!self,Disable,,0.5,-1\"");
    CHECK(scene.serialize(scene.world()) == expected);
    CHECK(scene.serialize(scene.world()) == expected);
    auto reopened = VmfScene::read(expected, vm::bbox3d{4096});
    CHECK(reopened.serialize(reopened.world()) == expected);
  }
  SECTION("explicit row removal preserves remaining rows comments and opaque child")
  {
    auto rows = baseline;
    rows.erase(rows.begin());
    e.setSourceConnections(rows);
    node.setEntity(e);
    auto expected = original;
    const auto old = std::string{"\"OnTrigger\" \"!self,Enable,,0.000,-1\""};
    expected.erase(expected.find(old), old.size());
    CHECK(scene.serialize(scene.world()) == expected);
  }
  SECTION("append preserves the complete native baseline")
  {
    auto rows = baseline;
    rows.push_back({"OnUser1", "!self,Use,,0,-1", {}});
    e.setSourceConnections(rows);
    node.setEntity(e);
    const auto saved = scene.serialize(scene.world());
    CHECK(saved.find("extension { \"opaque\" \"keep\" }") != std::string::npos);
    auto reopened = VmfScene::read(saved, vm::bbox3d{4096});
    CHECK(outputEntity(reopened.world()).entity().sourceConnections().size() == 4);
    CHECK(reopened.serialize(reopened.world()) == saved);
  }
  SECTION("ordinary property addition and removal")
  {
    auto propertyScene = VmfScene::read(sourceOutputs(false), vm::bbox3d{4096});
    auto& propertyNode = outputEntity(propertyScene.world());
    auto properties = propertyNode.entity();
    properties.addOrUpdateProperty("spawnflags", "0");
    properties.removeProperty("targetname");
    propertyNode.setEntity(properties);
    const auto saved = propertyScene.serialize(propertyScene.world());
    auto reopened = VmfScene::read(saved, vm::bbox3d{4096});
    const auto& actual = outputEntity(reopened.world()).entity();
    REQUIRE(actual.property("spawnflags"));
    CHECK(*actual.property("spawnflags") == "0");
    CHECK_FALSE(actual.hasProperty("targetname"));
    CHECK(actual.sourceConnections().size() == 3);
    CHECK(reopened.serialize(reopened.world()) == saved);
  }
  SECTION("duplicate property edits are ambiguous and refused")
  {
    auto repeated = original;
    const auto marker = repeated.find("\"targetname\" \"relay\"");
    repeated.insert(marker, "\"targetname\" \"first\" ");
    auto ambiguous = VmfScene::read(repeated, vm::bbox3d{4096});
    auto& target = outputEntity(ambiguous.world());
    CHECK(ambiguous.serialize(ambiguous.world()) == repeated);
    auto changed = target.entity();
    changed.removeProperty("targetname");
    target.setEntity(changed);
    CHECK_THROWS_AS(ambiguous.serialize(ambiguous.world()), std::invalid_argument);
  }
  SECTION("a missing connections block can be appended without regenerating entity")
  {
    const auto source = std::string{
      "world { \"id\" \"1\" \"classname\" \"worldspawn\" }\r\n"
      "entity { \"id\" \"2\" \"classname\" \"logic_relay\" /* retain */ }\r\n"};
    auto empty = VmfScene::read(source, vm::bbox3d{4096});
    auto& target = outputEntity(empty.world());
    auto changed = target.entity();
    changed.setSourceConnections({{"OnTrigger", "!self,Use,,0,-1", {}}});
    target.setEntity(changed);
    const auto saved = empty.serialize(empty.world());
    CHECK(saved.find("/* retain */") != std::string::npos);
    auto reopened = VmfScene::read(saved, vm::bbox3d{4096});
    CHECK(outputEntity(reopened.world()).entity().sourceConnections().size() == 1);
    CHECK(empty.serialize(empty.world()) == saved);
  }
  SECTION("ambiguous repeated connection blocks refuse changes")
  {
    auto repeated = original;
    repeated.insert(repeated.find("editor {"), "connections {} ");
    auto ambiguous = VmfScene::read(repeated, vm::bbox3d{4096});
    CHECK(ambiguous.serialize(ambiguous.world()) == repeated);
    auto& target = outputEntity(ambiguous.world());
    auto changed = target.entity();
    auto rows = changed.sourceConnections();
    rows[0].event = "OnUser1";
    changed.setSourceConnections(rows);
    target.setEntity(changed);
    CHECK_THROWS_AS(ambiguous.serialize(ambiguous.world()), std::invalid_argument);
  }
  SECTION("unknown entity block is preserved or refuses property edits")
  {
    auto opaque = original;
    opaque.insert(
      opaque.find("editor {"), "future { nested { \"unknown\" \"bytes\" } } ");
    auto restricted = VmfScene::read(opaque, vm::bbox3d{4096});
    CHECK(restricted.serialize(restricted.world()) == opaque);
    auto& target = outputEntity(restricted.world());
    auto changed = target.entity();
    changed.addOrUpdateProperty("origin", "1 2 3");
    target.setEntity(changed);
    CHECK_THROWS_AS(restricted.serialize(restricted.world()), std::invalid_argument);
  }
  SECTION("reserved id refused")
  {
    e.addOrUpdateProperty("id", "99");
    node.setEntity(e);
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
  SECTION("stale identity refused")
  {
    auto rows = baseline;
    rows[0].sourceEntry = 999;
    e.setSourceConnections(rows);
    node.setEntity(e);
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
  SECTION("malformed untouched row is not silently removed")
  {
    auto rows = baseline;
    rows.pop_back();
    e.setSourceConnections(rows);
    node.setEntity(e);
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
  SECTION("implicit reorder refused")
  {
    auto rows = baseline;
    std::swap(rows[0], rows[1]);
    e.setSourceConnections(rows);
    node.setEntity(e);
    CHECK_THROWS_AS(scene.serialize(scene.world()), std::invalid_argument);
  }
}

TEST_CASE("VmfMap adjacent property removal saves and reopens", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "adjacent.vmf";
  const auto source = std::string{
    "world { id 1 classname worldspawn } "
    "entity { id 2 classname info_target a b\"remove\" \"x\"c d }"};
  {
    auto file = std::ofstream{path, std::ios::binary};
    file << source;
  }
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path, {.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  REQUIRE(map.save().is_success());
  CHECK(readOutputFile(path) == source);
  selectNodes(map, {&outputEntity(map.worldNode())});
  REQUIRE(removeEntityProperty(map, "remove"));
  REQUIRE(map.save().is_success());
  const auto saved = readOutputFile(path);
  auto reopened = VmfScene::read(saved, vm::bbox3d{4096});
  const auto& entity = outputEntity(reopened.world()).entity();
  REQUIRE(entity.property("a"));
  REQUIRE(entity.property("c"));
  CHECK(*entity.property("a") == "b");
  CHECK(*entity.property("c") == "d");
  CHECK_FALSE(entity.hasProperty("remove"));
  CHECK(reopened.serialize(reopened.world()) == saved);
}

TEST_CASE(
  "Source connection single field edits preserve raw bytes through commands", "[vmf]")
{
  const auto event = std::string{"OnCaf\xe9"};
  const auto value = std::string{
    "!self\x1b"
    "AddOutput\x1b"
    "message caf\xe9,a,b\x1b"
    "0.000\x1b"
    "01"};
  const auto original =
    std::string{
      "world { id 1 classname worldspawn } "
      "entity { id 2 classname logic_relay connections { \""}
    + event + "\" \"" + value + "\" } }";
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "raw-output.vmf";
  {
    auto file = std::ofstream{path, std::ios::binary};
    file << original;
  }
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path, {.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto& node = outputEntity(map.worldNode());
  selectNodes(map, {&node});
  const auto baseline = node.entity().sourceConnections();
  REQUIRE(baseline.size() == 1);
  const auto edit =
    editSourceConnectionField(baseline[0], SourceConnectionField::Delay, "0.5");
  REQUIRE(edit);
  CHECK(edit->event == event);
  CHECK(
    edit->value
    == std::string{"!self\x1b"
                   "AddOutput\x1b"
                   "message caf\xe9,a,b\x1b"
                   "0.5\x1b"
                   "01"});
  CHECK(edit->sourceEntry == baseline[0].sourceEntry);
  CHECK_FALSE(
    editSourceConnectionField(baseline[0], SourceConnectionField::Delimiter, ","));
  CHECK_FALSE(
    editSourceConnectionField(baseline[0], SourceConnectionField::Delay, "nan"));
  REQUIRE(setSourceEntityConnections(map, {*edit}));
  REQUIRE(map.save().is_success());
  const auto saved = readOutputFile(path);
  auto reopened = VmfScene::read(saved, vm::bbox3d{4096});
  const auto row = outputEntity(reopened.world()).entity().sourceConnections().at(0);
  CHECK(row.event == event);
  CHECK(row.value == edit->value);
  CHECK(reopened.serialize(reopened.world()) == saved);
  map.undoCommand();
  CHECK(node.entity().sourceConnections() == baseline);
  REQUIRE(map.save().is_success());
  CHECK(readOutputFile(path) == original);
  map.redoCommand();
  REQUIRE(map.save().is_success());
  CHECK(readOutputFile(path) == saved);
}

TEST_CASE("VmfMap Source output commands undo redo and repeated saves", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "outputs.vmf";
  const auto original = sourceOutputs(false);
  {
    auto file = std::ofstream{path, std::ios::binary};
    file << original;
  }
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    path, {.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  auto& node = outputEntity(map.worldNode());
  selectNodes(map, {&node});
  const auto baseline = node.entity().sourceConnections();
  auto edited = baseline;
  edited[1].event = "OnUser2";
  REQUIRE(setSourceEntityConnections(map, edited));
  CHECK(node.entity().sourceConnections() == edited);
  map.undoCommand();
  CHECK(node.entity().sourceConnections() == baseline);
  REQUIRE(map.save().is_success());
  CHECK(readOutputFile(path) == original);
  map.redoCommand();
  CHECK(node.entity().sourceConnections() == edited);
  REQUIRE(map.save().is_success());
  const auto firstSave = readOutputFile(path);
  CHECK_FALSE(setEntityProperty(map, "id", "99"));
  CHECK_FALSE(renameEntityProperty(map, "classname", "id"));
  CHECK_FALSE(removeEntityProperty(map, "id"));
  REQUIRE(setEntityProperty(map, "spawnflags", "1"));
  map.undoCommand();
  CHECK_FALSE(node.entity().hasProperty("spawnflags"));
  map.redoCommand();
  REQUIRE(node.entity().property("spawnflags"));
  CHECK(*node.entity().property("spawnflags") == "1");
  edited.push_back({"OnUser1", "!self,Use,,0,-1", {}});
  REQUIRE(setSourceEntityConnections(map, edited));
  REQUIRE(map.save().is_success());
  const auto secondSave = readOutputFile(path);
  CHECK(secondSave != firstSave);
  REQUIRE(map.save().is_success());
  CHECK(readOutputFile(path) == secondSave);
  auto reopenedFixture = MapFixture{};
  auto& reopened = reopenedFixture.load(
    path, {.mapFormat = MapFormat::SourceVMF, .worldBounds = vm::bbox3d{4096}});
  CHECK(outputEntity(reopened.worldNode()).entity().sourceConnections().size() == 4);
  REQUIRE(reopened.save().is_success());
  CHECK(readOutputFile(path) == secondSave);
  auto invalid = edited;
  invalid[0].value = "!self,Use,,nan,-1";
  CHECK_FALSE(setSourceEntityConnections(map, invalid));
  CHECK(node.entity().sourceConnections() == edited);
  REQUIRE(removeEntityProperty(map, "spawnflags"));
  map.undoCommand();
  REQUIRE(node.entity().property("spawnflags"));
  map.redoCommand();
  CHECK_FALSE(node.entity().hasProperty("spawnflags"));
  auto removed = edited;
  removed.erase(removed.begin());
  REQUIRE(setSourceEntityConnections(map, removed));
  CHECK(node.entity().sourceConnections() == removed);
  map.undoCommand();
  CHECK(node.entity().sourceConnections() == edited);
  map.redoCommand();
  REQUIRE(map.save().is_success());
  auto finalScene = VmfScene::read(readOutputFile(path), vm::bbox3d{4096});
  CHECK(outputEntity(finalScene.world()).entity().sourceConnections().size() == 3);
}
} // namespace tb::mdl
