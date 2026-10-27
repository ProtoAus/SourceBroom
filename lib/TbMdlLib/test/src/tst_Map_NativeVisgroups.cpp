/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "base/NotifierConnection.h"
#include "fs/TestEnvironment.h"
#include "mdl/AddRemoveNodesCommand.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/EditorContext.h"
#include "mdl/EntityDefinition.h"
#include "mdl/EntityNode.h"
#include "mdl/Group.h"
#include "mdl/GroupNode.h"
#include "mdl/LayerNode.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/Map_Brushes.h"
#include "mdl/Map_Entities.h"
#include "mdl/Map_Geometry.h"
#include "mdl/Map_Groups.h"
#include "mdl/Map_NativeCordons.h"
#include "mdl/Map_NativeVisgroups.h"
#include "mdl/Map_NodeVisibility.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/NativeVisgroupCommand.h"
#include "mdl/SelectionChange.h"
#include "mdl/Transaction.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"

#include <algorithm>
#include <fstream>
#include <iterator>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
namespace tb::mdl
{
namespace
{
const auto loadedVisgroups = R"(versioninfo { formatversion 100 }
visgroups { visgroup { name "A" visgroupid "10" color "1 2 3" }
 visgroup { name "B" visgroupid "11" color "4 5 6" } }
world { id 1 classname worldspawn group { id 2 editor { visgroupid 10 } } }
entity { id 3 classname info_target origin "0 0 0" editor { groupid 2 visgroupid 10 } }
entity { id 4 classname info_target origin "64 0 0" editor { visgroupid 11 } }
)";
Node& node(Map& map, const std::string& id)
{
  Node* result = nullptr;
  const auto find = [&](auto&& self, Node& current) -> void {
    if (current.lineNumber() && map.vmfScene()->nativeId(current) == id)
      result = &current;
    for (auto* child : current.children())
      self(self, *child);
  };
  find(find, map.worldNode());
  REQUIRE(result);
  return *result;
}
NativeVisgroupMemberId member(const Map& map, const Node& node)
{
  const auto snapshot = nativeVisgroups(map);
  const auto id = map.vmfScene()->nativeId(node);
  const auto found =
    std::ranges::find(snapshot.members, id, &NativeVisgroupMember::nativeId);
  REQUIRE(found != snapshot.members.end());
  return found->id;
}
NativeVisgroupApplyResult manual(
  Map& map, const std::vector<NativeVisgroupTarget>& roots, const bool shown)
{
  auto request = NativeVisgroupRequest{nativeVisgroups(map)};
  request.manual = NativeVisgroupManualAction{roots, shown};
  return applyNativeVisgroups(map, request);
}
void assign(
  Map& map,
  const std::vector<Node*>& roots,
  const NativeVisgroupId group,
  const bool assigned = true)
{
  auto request = NativeVisgroupRequest{nativeVisgroups(map)};
  auto members = std::vector<NativeVisgroupMemberId>{};
  for (const auto* root : roots)
    members.push_back(member(map, *root));
  request.memberships.push_back({members, group, assigned});
  REQUIRE(
    applyNativeVisgroups(map, request).status == NativeVisgroupApplyStatus::Changed);
}
std::string bytes(const Map& map)
{
  return map.vmfScene()->serialize(map.worldNode());
}
std::vector<vm::vec3d> cubePoints()
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
void loadFixture(fs::TestEnvironment& env, const std::string& source = loadedVisgroups)
{
  auto stream = std::ofstream{env.dir() / "loaded.vmf", std::ios::binary};
  stream << source;
}

} // namespace
TEST_CASE("Map native visgroups dispatch and backward apply")
{
  auto env = fs::TestEnvironment{};
  env.createFile("loaded.vmf", loadedVisgroups);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  const auto before = nativeVisgroups(map);
  REQUIRE(before.records.size() == 2);
  REQUIRE(before.refusalReason.empty());
  const auto bytes = map.vmfScene()->serialize(map.worldNode());
  const auto id = before.records[0].id;
  auto request = NativeVisgroupRequest{before};
  request.records[0].name = "Changed";
  REQUIRE(
    applyNativeVisgroups(map, request).status == NativeVisgroupApplyStatus::Changed);
  CHECK(nativeVisgroups(map).records[0].name == "Changed");
  CHECK(map.undoCommandName() != nullptr);
  if (map.undoCommandName() != nullptr)
  {
    map.undoCommand();
    CHECK(nativeVisgroups(map).records[0].name == "A");
    CHECK(map.vmfScene()->serialize(map.worldNode()) == bytes);
    CHECK(map.redoCommandName() != nullptr);
  }
}

TEST_CASE(
  "Map native visgroups loaded tree membership visibility save and continued history")
{
  auto env = fs::TestEnvironment{};
  loadFixture(env);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  const auto original = bytes(map);
  const auto a = nativeVisgroups(map).records[0].id;
  const auto b = nativeVisgroups(map).records[1].id;
  auto& grouped = node(map, "2");
  auto& first = node(map, "3");
  auto& second = node(map, "4");
  auto tree = NativeVisgroupRequest{nativeVisgroups(map)};
  tree.records.push_back({NativeVisgroupDraftKey{1}, a, "Child"});
  tree.records[1].parent = a;
  tree.records[0].name = "Root";
  REQUIRE(applyNativeVisgroups(map, tree).status == NativeVisgroupApplyStatus::Changed);
  REQUIRE(nativeVisgroups(map).records.size() == 3);
  const auto child = nativeVisgroups(map).records[2].id;
  assign(map, {&second}, child);
  auto members = nativeVisgroups(map).members;
  const auto secondId = member(map, second);
  CHECK(
    std::ranges::find(members, secondId, &NativeVisgroupMember::id)->memberships.size()
    == 2);
  selectNodes(map, {&second, &grouped});
  const auto selection = map.selection().nodes;
  auto closure = std::vector<Node*>{};
  auto connection = NotifierConnection{};
  connection += map.nodeVisibilityDidChangeNotifier.connect([&](const auto& changed) {
    closure = changed;
    CHECK(map.selection().nodes.empty());
    CHECK_FALSE(map.editorContext().visible(grouped));
    CHECK_FALSE(map.editorContext().visible(first));
  });
  REQUIRE(manual(map, {a, b}, false).status == NativeVisgroupApplyStatus::Changed);
  CHECK(map.selection().nodes.empty());
  CHECK(std::ranges::find(closure, &grouped) != closure.end());
  CHECK(std::ranges::find(closure, &first) != closure.end());
  connection = {};
  const auto hidden = bytes(map);
  CHECK(manual(map, {a}, false).status == NativeVisgroupApplyStatus::Unchanged);
  map.undoCommand();
  CHECK(map.selection().nodes == selection);
  CHECK(map.editorContext().visible(grouped));
  map.redoCommand();
  CHECK(bytes(map) == hidden);
  REQUIRE(manual(map, {a}, true).status == NativeVisgroupApplyStatus::Changed);
  CHECK(map.editorContext().visible(first));
  CHECK(map.editorContext().visible(second));
  const auto destination = env.dir() / "saved.vmf";
  const auto saved = bytes(map);
  REQUIRE(map.saveAs(destination).is_success());
  auto reopenedFixture = MapFixture{};
  auto& reopened = reopenedFixture.load(
    destination, MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  CHECK(bytes(reopened) == saved);
  auto rename = NativeVisgroupRequest{nativeVisgroups(reopened)};
  rename.records[0].name = "Continued";
  REQUIRE(
    applyNativeVisgroups(reopened, rename).status == NativeVisgroupApplyStatus::Changed);
  reopened.undoCommand();
  CHECK(bytes(reopened) == saved);
  reopened.redoCommand();
  CHECK(nativeVisgroups(reopened).records[0].name == "Continued");
  auto remove = NativeVisgroupRequest{nativeVisgroups(map)};
  std::erase_if(remove.records, [&](const auto& row) {
    return std::get<NativeVisgroupId>(row.key) == child;
  });
  REQUIRE(applyNativeVisgroups(map, remove).status == NativeVisgroupApplyStatus::Changed);
  CHECK(nativeVisgroups(map).records.size() == 2);
  map.undoCommand();
  CHECK(bytes(map) == saved);
  map.redoCommand();
  CHECK(nativeVisgroups(map).records.size() == 2);
  auto originalFile = std::ifstream{env.dir() / "loaded.vmf", std::ios::binary};
  const auto retained = std::string{std::istreambuf_iterator<char>{originalFile}, {}};
  CHECK(retained == original);
}
TEST_CASE(
  "Map native visgroups retained wrapper automatic and initially hidden show remove save "
  "undo")
{
  const auto mode =
    GENERATE(std::string{"manual"}, std::string{"wrapper"}, std::string{"auto"});
  CAPTURE(mode);
  auto source = std::string{loadedVisgroups};
  const auto start = source.find("entity { id 4");
  const auto entity =
    std::string{
      R"(entity { id 4 classname info_target origin "64 0 0" editor { visgroupid 11 visgroupshown 0 )"}
    + (mode == "auto" ? "visgroupautoshown 0 " : "") + "} }";
  source.replace(
    start,
    source.size() - start,
    mode == "wrapper" ? "hidden { " + entity + " }" : entity);
  auto env = fs::TestEnvironment{};
  loadFixture(env, source);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  auto& second = node(map, "4");
  const auto b = nativeVisgroups(map).records[1].id;
  CHECK_FALSE(map.editorContext().visible(second));
  REQUIRE(manual(map, {b}, true).status == NativeVisgroupApplyStatus::Changed);
  CHECK(map.editorContext().visible(second) == (mode == "manual"));
  if (mode != "manual")
  {
    selectNodes(map, {&second});
    CHECK_FALSE(second.selected());
    CHECK(map.selection().nodes.empty());
    // Deliberately UNSAFE low-level precondition: backend refusal must still act,
    // independently of ordinary SelectionCommand's native veto.
    map.selectionWillChangeNotifier();
    second.select();
    auto selection = SelectionChange{};
    selection.selectedNodes = {&second};
    map.selectionDidChangeNotifier(selection);
    REQUIRE(second.selected());
    REQUIRE(map.selection().nodes == std::vector<Node*>{&second});
    const auto preserved = bytes(map);
    const auto count = map.modificationCount();
    const auto undo = *map.undoCommandName();
    const auto context = map.editorContext().currentGroup();
    CHECK_FALSE(removeSelectedNodes(map));
    CHECK(bytes(map) == preserved);
    CHECK(map.modificationCount() == count);
    CHECK(*map.undoCommandName() == undo);
    CHECK(map.editorContext().currentGroup() == context);
    CHECK(map.selection().nodes == std::vector<Node*>{&second});
    CHECK_FALSE(map.editorContext().visible(second));
    return;
  }
  const auto shown = bytes(map);
  selectNodes(map, {&second});
  REQUIRE(removeSelectedNodes(map));
  const auto removed = bytes(map);
  REQUIRE(map.saveAs(env.dir() / "removed.vmf").is_success());
  map.undoCommand();
  CHECK(bytes(map) == shown);
  CHECK(map.editorContext().visible(second));
  map.undoCommand();
  map.undoCommand(); // selection, then Show
  CHECK_FALSE(map.editorContext().visible(second));
  CHECK(bytes(map) == source);
  map.redoCommand();
  map.redoCommand();
  map.redoCommand();
  CHECK(bytes(map) == removed);
}
TEST_CASE(
  "Map native visgroups stale refusal context last content and namespace are atomic")
{
  auto env = fs::TestEnvironment{};
  loadFixture(env);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  const auto initial = nativeVisgroups(map);
  const auto a = initial.records[0].id;
  auto stale = NativeVisgroupRequest{initial};
  stale.records[0].name = "stale";
  const auto prepared = map.vmfScene()->prepareVisgroupChange(stale);
  REQUIRE(prepared);
  auto edit = NativeVisgroupRequest{initial};
  edit.records[0].name = "current";
  REQUIRE(applyNativeVisgroups(map, edit).status == NativeVisgroupApplyStatus::Changed);
  const auto expected = bytes(map);
  const auto count = map.modificationCount();
  CHECK_FALSE(map.executeAndStore(std::make_unique<NativeVisgroupCommand>(*prepared)));
  CHECK(bytes(map) == expected);
  CHECK(map.modificationCount() == count);
  CHECK(applyNativeVisgroups(map, stale).status == NativeVisgroupApplyStatus::Refused);
  auto invalid = NativeVisgroupRequest{nativeVisgroups(map)};
  invalid.records[0].parent = invalid.records[0].key;
  CHECK(applyNativeVisgroups(map, invalid).status == NativeVisgroupApplyStatus::Refused);
  auto& group = dynamic_cast<GroupNode&>(node(map, "2"));
  openGroup(map, group);
  const auto contextBytes = bytes(map);
  const auto contextCount = map.modificationCount();
  CHECK(manual(map, {a}, false).status == NativeVisgroupApplyStatus::Refused);
  CHECK(map.editorContext().currentGroup() == &group);
  CHECK(bytes(map) == contextBytes);
  CHECK(map.modificationCount() == contextCount);
  auto childOnly = NativeVisgroupRequest{nativeVisgroups(map)};
  childOnly.memberships.push_back(
    {{member(map, node(map, "3"))}, initial.records[1].id, true});
  REQUIRE(
    applyNativeVisgroups(map, childOnly).status == NativeVisgroupApplyStatus::Changed);
  const auto last = bytes(map);
  const auto lastCount = map.modificationCount();
  CHECK(
    manual(map, {initial.records[1].id}, false).status
    == NativeVisgroupApplyStatus::Refused);
  CHECK(bytes(map) == last);
  CHECK(map.modificationCount() == lastCount);
  CHECK(map.editorContext().currentGroup() == &group);
  closeGroup(map);
  auto otherFixture = MapFixture{};
  auto& other = otherFixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  auto foreign = NativeVisgroupRequest{nativeVisgroups(other)};
  CHECK(applyNativeVisgroups(map, foreign).status == NativeVisgroupApplyStatus::Refused);
}
TEST_CASE(
  "Map native visgroups unsaved overlay clone clip Both fresh park group and wrap")
{
  auto env = fs::TestEnvironment{};
  loadFixture(env);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  const auto a = nativeVisgroups(map).records[0].id;
  const auto b = nativeVisgroups(map).records[1].id;
  REQUIRE(createBrush(map, cubePoints()));
  auto* brush = map.selection().brushes.front();
  assign(map, {brush}, a);
  assign(map, {brush}, b);
  const auto assigned = bytes(map);
  SECTION("clone and clips use LIVE unsaved metadata through parking and replay")
  {
    duplicateSelectedNodes(map);
    auto* clone = map.selection().brushes.front();
    REQUIRE(clone != brush);
    const auto clonedId = member(map, *clone);
    auto snapshot = nativeVisgroups(map);
    CHECK(
      std::ranges::find(snapshot.members, clonedId, &NativeVisgroupMember::id)
        ->memberships.size()
      == 2);
    REQUIRE(clipBrushes(map, {clone}, {{{0, 0, 0}, {0, 0, 64}, {0, 64, 0}}}));
    REQUIRE(clipBrushesBoth(map, {clone}, {{{-16, 0, 0}, {-16, 0, 64}, {-16, 64, 0}}}));
    REQUIRE(map.selection().brushes.size() == 2);
    auto* sibling = map.selection().brushes.back();
    const auto siblingId = member(map, *sibling);
    snapshot = nativeVisgroups(map);
    CHECK(
      std::ranges::find(snapshot.members, siblingId, &NativeVisgroupMember::id)
        ->memberships.size()
      == 2);
    const auto both = bytes(map);
    map.undoCommand();
    CHECK_FALSE(sibling->parent());
    CHECK_NOTHROW(nativeVisgroups(map));
    map.redoCommand();
    CHECK(bytes(map) == both);
    REQUIRE(manual(map, {a}, false).status == NativeVisgroupApplyStatus::Changed);
    CHECK_FALSE(map.editorContext().visible(*clone));
    CHECK_FALSE(map.editorContext().visible(*sibling));
    const auto hidden = bytes(map);
    CHECK_FALSE(clipBrushes(map, {clone}, {{{-24, 0, 0}, {-24, 0, 64}, {-24, 64, 0}}}));
    CHECK(bytes(map) == hidden);
    map.undoCommand();
    map.undoCommand();
    map.undoCommand();
    map.undoCommand();
    CHECK(bytes(map) == assigned);
    map.redoCommand();
    map.redoCommand();
    map.redoCommand();
    CHECK(bytes(map) == both);
  }
  SECTION("fresh hidden owner is recoverable and exact node face selection order replays")
  {
    deselectAll(map);
    auto& group = dynamic_cast<GroupNode&>(node(map, "2"));
    openGroup(map, group);
    REQUIRE(createBrush(map, cubePoints()));
    auto* fresh = map.selection().brushes.front();
    const auto freshId = member(map, *fresh);
    auto snapshot = nativeVisgroups(map);
    CHECK(
      std::ranges::find(snapshot.members, freshId, &NativeVisgroupMember::id)
        ->memberships.empty());
    closeGroup(map);
    deselectAll(map);
    const auto faces =
      std::vector<BrushFaceHandle>{{*brush, 3}, {*fresh, 1}, {*brush, 0}};
    selectBrushFaces(map, faces);
    CHECK(nativeVisgroupSelection(map).size() == 2);
    REQUIRE(manual(map, {a}, false).status == NativeVisgroupApplyStatus::Changed);
    CHECK(map.selection().brushFaces.empty());
    CHECK_FALSE(map.editorContext().visible(*fresh));
    map.undoCommand();
    CHECK(map.selection().brushFaces == faces);
    map.redoCommand();
    CHECK(map.selection().brushFaces.empty());
    map.undoCommand();
  }
  SECTION("loaded deletion observer metadata remains coherent and active after restore")
  {
    deselectAll(map);
    auto* victim = &node(map, "4");
    const auto victimId = member(map, *victim);
    auto observed = 0;
    auto connection = NotifierConnection{};
    const auto check = [&](const auto&) {
      const auto snapshot = nativeVisgroups(map);
      const auto found =
        std::ranges::find(snapshot.members, victimId, &NativeVisgroupMember::id);
      REQUIRE(found != snapshot.members.end());
      CHECK(found->active == (victim->parent() != nullptr));
      ++observed;
    };
    connection += map.nodesWereRemovedNotifier.connect(check);
    connection += map.nodesWereAddedNotifier.connect(check);
    selectNodes(map, {victim});
    REQUIRE(removeSelectedNodes(map));
    const auto removed = bytes(map);
    map.undoCommand();
    CHECK(bytes(map) == assigned);
    map.redoCommand();
    CHECK(bytes(map) == removed);
    CHECK(observed == 3);
  }
  SECTION(
    "loaded Ungroup and generated Group retain child membership and park only own "
    "container")
  {
    deselectAll(map);
    auto* group = &node(map, "2");
    auto* child = &node(map, "3");
    selectNodes(map, {group});
    ungroupSelectedNodes(map);
    REQUIRE(!group->parent());
    CHECK(child->parent() == map.worldNode().defaultLayer());
    CHECK(map.editorContext().visible(*child));
    const auto ungrouped = bytes(map);
    map.undoCommand();
    CHECK(bytes(map) == assigned);
    map.redoCommand();
    CHECK(bytes(map) == ungrouped);
    deselectAll(map);
    selectNodes(map, {child, brush});
    auto* generated = groupSelectedNodes(map, "New physical group");
    REQUIRE(generated);
    const auto grouped = bytes(map);
    const auto generatedId = member(map, *generated);
    map.undoCommand();
    CHECK_FALSE(generated->parent());
    auto snapshot = nativeVisgroups(map);
    CHECK_FALSE(
      std::ranges::find(snapshot.members, generatedId, &NativeVisgroupMember::id)
        ->active);
    map.redoCommand();
    CHECK(bytes(map) == grouped);
    REQUIRE(manual(map, {a}, false).status == NativeVisgroupApplyStatus::Changed);
    CHECK_FALSE(map.editorContext().visible(*generated));
    map.undoCommand();
    CHECK(map.editorContext().visible(*generated));
  }
  SECTION("brush entity wrap retains child overlay through original parent replay")
  {
    REQUIRE(createBrushEntity(map, EntityDefinition{.name = "func_detail"}));
    auto* owner = brush->parent();
    REQUIRE(dynamic_cast<EntityNode*>(owner));
    const auto wrapped = bytes(map);
    map.undoCommand();
    CHECK(bytes(map) == assigned);
    map.redoCommand();
    CHECK(bytes(map) == wrapped);
    REQUIRE(manual(map, {b}, false).status == NativeVisgroupApplyStatus::Changed);
    CHECK_FALSE(map.editorContext().visible(*owner));
    map.undoCommand();
    CHECK(map.editorContext().visible(*owner));
  }
}
TEST_CASE(
  "Map native visgroups semantic replay refuses changed metadata selection and context "
  "atomically")
{
  auto env = fs::TestEnvironment{};
  loadFixture(env);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  selectNodes(map, {&node(map, "4"), &node(map, "2")});
  auto request = NativeVisgroupRequest{nativeVisgroups(map)};
  request.manual = NativeVisgroupManualAction{{request.records[1].key}, false};
  auto prepared = map.vmfScene()->prepareVisgroupChange(request);
  REQUIRE(prepared);
  auto command = NativeVisgroupCommand{*prepared};
  REQUIRE(command.performDo(map));
  const auto hidden = bytes(map);
  SECTION("metadata changed independently")
  {
    auto rename = NativeVisgroupRequest{nativeVisgroups(map)};
    rename.records[0].name = "other";
    auto external = map.vmfScene()->prepareVisgroupChange(rename);
    REQUIRE(external);
    map.vmfScene()->applyVisgroupChange(*external, NativeVisgroupDirection::Forward);
  }
  SECTION("selection changed independently")
  {
    deselectAll(map);
  }
  SECTION("context changed independently")
  {
    openGroup(map, dynamic_cast<GroupNode&>(node(map, "2")));
  }
  const auto before = bytes(map);
  const auto nodes = map.selection().nodes;
  const auto context = map.editorContext().currentGroup();
  const auto count = map.modificationCount();
  auto notified = 0;
  auto connection = NotifierConnection{};
  connection +=
    map.nodeVisibilityDidChangeNotifier.connect([&](const auto&) { ++notified; });
  connection += map.selectionDidChangeNotifier.connect([&](const auto&) { ++notified; });
  CHECK_FALSE(command.performUndo(map));
  CHECK(bytes(map) == before);
  CHECK(map.selection().nodes == nodes);
  CHECK(map.editorContext().currentGroup() == context);
  CHECK(map.modificationCount() == count);
  CHECK(notified == 0);
}
TEST_CASE(
  "Map native visgroups required selection native veto retains visible and nonSource "
  "controls")
{
  auto env = fs::TestEnvironment{};
  loadFixture(env);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  REQUIRE(createBrush(map, cubePoints()));
  auto* hidden = map.selection().brushes.front();
  REQUIRE(createBrush(map, cubePoints()));
  auto* visible = map.selection().brushes.front();
  deselectAll(map);
  selectNodes(map, {hidden, visible});
  REQUIRE(createBrushEntity(map, EntityDefinition{.name = "func_detail"}));
  auto* entity = hidden->parent();
  const auto b = nativeVisgroups(map).records[1].id;
  assign(map, {hidden}, b);
  REQUIRE(manual(map, {b}, false).status == NativeVisgroupApplyStatus::Changed);
  REQUIRE(map.editorContext().visible(*entity));
  REQUIRE(map.editorContext().visible(*visible));
  deselectAll(map);
  selectNodes(map, {entity, &node(map, "3")});
  CHECK_FALSE(hidden->selected());
  CHECK_FALSE(visible->selected());
  CHECK(node(map, "3").selected());
  deselectAll(map);
  selectBrushFaces(map, {{*hidden, 0}, {*visible, 1}});
  CHECK_FALSE(hidden->brush().face(0).selected());
  CHECK(visible->brush().face(1).selected());
  auto ordinaryFixture = MapFixture{};
  auto& ordinary = ordinaryFixture.create();
  REQUIRE(createBrush(ordinary, cubePoints()));
  auto* ordinaryBrush = ordinary.selection().brushes.front();
  deselectAll(ordinary);
  ordinaryBrush->setVisibilityState(VisibilityState::Hidden);
  selectNodes(ordinary, {ordinaryBrush});
  CHECK(ordinaryBrush->selected());
}
TEST_CASE(
  "Map native visgroups value only and empty context preserve exact selection without "
  "false deltas")
{
  auto source = std::string{loadedVisgroups};
  source.replace(source.find("group { id 2"), 0, "group { id 5 } ");
  const auto secondStart = source.find("entity { id 4");
  source.replace(
    secondStart,
    source.size() - secondStart,
    R"(entity { id 4 classname info_target origin "64 0 0" editor { groupid 2 visgroupid 11 visgroupshown 0 } })");
  auto env = fs::TestEnvironment{};
  loadFixture(env, source);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  auto* group = &dynamic_cast<GroupNode&>(node(map, "2"));
  REQUIRE(map.editorContext().visible(*group));
  REQUIRE_FALSE(map.editorContext().visible(node(map, "4")));
  // Seed an existing aggregate inspection selection, without expanding its descendants.
  group->select();
  auto selection = SelectionChange{};
  selection.selectedNodes = {group};
  map.selectionDidChangeNotifier(selection);
  const auto selected = map.selection().nodes;
  auto notifications = 0;
  auto connection = NotifierConnection{};
  connection +=
    map.selectionDidChangeNotifier.connect([&](const auto&) { ++notifications; });
  auto rename = NativeVisgroupRequest{nativeVisgroups(map)};
  rename.records[0].name = "Renamed";
  REQUIRE(applyNativeVisgroups(map, rename).status == NativeVisgroupApplyStatus::Changed);
  CHECK(map.selection().nodes == selected);
  CHECK(notifications == 0);
  map.undoCommand();
  CHECK(map.selection().nodes == selected);
  CHECK(notifications == 0);
  connection = {};
  deselectAll(map);
  auto& empty = dynamic_cast<GroupNode&>(node(map, "5"));
  REQUIRE(empty.children().empty());
  openGroup(map, empty);
  REQUIRE(map.editorContext().currentGroup() == &empty);
  auto emptyRename = NativeVisgroupRequest{nativeVisgroups(map)};
  emptyRename.records[1].name = "Empty context edit";
  REQUIRE(
    applyNativeVisgroups(map, emptyRename).status == NativeVisgroupApplyStatus::Changed);
  CHECK(map.editorContext().currentGroup() == &empty);
  map.undoCommand();
  CHECK(map.editorContext().currentGroup() == &empty);
}

TEST_CASE("Map native visgroups wrapping after observers see only final live closure")
{
  auto env = fs::TestEnvironment{};
  loadFixture(env);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  REQUIRE(createBrush(map, cubePoints()));
  auto* brush = map.selection().brushes.front();
  assign(map, {brush}, nativeVisgroups(map).records[0].id);
  const auto id = member(map, *brush);
  auto observed = 0;
  auto connection = NotifierConnection{};
  const auto check = [&](const auto&) {
    const auto snapshot = nativeVisgroups(map);
    const auto row = std::ranges::find(snapshot.members, id, &NativeVisgroupMember::id);
    REQUIRE(row != snapshot.members.end());
    CHECK(row->active);
    CHECK(row->projected);
    CHECK(row->memberships.size() == 1);
    CHECK(brush->parent() != nullptr);
    ++observed;
  };
  connection += map.nodesWereRemovedNotifier.connect(check);
  connection += map.nodesWereAddedNotifier.connect(check);
  REQUIRE(createBrushEntity(map, EntityDefinition{.name = "func_detail"}));
  CHECK(dynamic_cast<EntityNode*>(brush->parent()));
  const auto wrapped = bytes(map);
  map.undoCommand();
  CHECK(brush->parent() == map.worldNode().defaultLayer());
  map.redoCommand();
  CHECK(bytes(map) == wrapped);
  CHECK(observed > 0);
}
TEST_CASE(
  "Map native visgroups mixed addition closure refuses without orphaning registered "
  "child")
{
  auto env = fs::TestEnvironment{};
  loadFixture(env);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  REQUIRE(createBrush(map, cubePoints()));
  auto* brush = map.selection().brushes.front();
  deselectAll(map);
  auto* parent = brush->parent();
  parent->removeChild(brush);
  auto* unregistered = new GroupNode{Group{"Unregistered container"}};
  unregistered->addChild(brush);
  auto command = AddRemoveNodesCommand::add({{parent, {unregistered}}});
  const auto count = map.modificationCount();
  const auto added = command->performDo(map);
  CHECK_FALSE(added);
  CHECK(map.modificationCount() == count);
  CHECK_FALSE(unregistered->parent());
  if (added)
    command->performUndo(map); // Keep a failing-control run safely owned.
  unregistered->removeChild(brush);
  parent->addChild(brush);
  CHECK_NOTHROW(bytes(map));
}
TEST_CASE(
  "Map native visgroups live unassign clone reopen and combined append repeated save")
{
  auto env = fs::TestEnvironment{};
  auto fixture = MapFixture{};
  SECTION("loaded unsaved unassign clone SaveAs reopen uses live direct memberships")
  {
    loadFixture(env);
    auto& map = fixture.load(
      env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
    auto* source = &node(map, "4");
    const auto a = nativeVisgroups(map).records[0].id;
    const auto b = nativeVisgroups(map).records[1].id;
    assign(map, {source}, a);
    assign(map, {source}, b, false);
    selectNodes(map, {source});
    duplicateSelectedNodes(map);
    REQUIRE(map.selection().nodes.size() == 1);
    auto* clone = map.selection().nodes.front();
    REQUIRE(clone != source);
    const auto cloneId = map.vmfScene()->nativeId(*clone);
    auto snapshot = nativeVisgroups(map);
    const auto row =
      std::ranges::find(snapshot.members, member(map, *clone), &NativeVisgroupMember::id);
    REQUIRE(row != snapshot.members.end());
    CHECK(row->memberships == std::vector<std::string>{"10"});
    const auto saved = bytes(map);
    const auto destination = env.dir() / "clone.vmf";
    REQUIRE(map.saveAs(destination).is_success());
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(
      destination, MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
    CHECK(bytes(reopened) == saved);
    auto reopenedSnapshot = nativeVisgroups(reopened);
    const auto reopenedRow = std::ranges::find(
      reopenedSnapshot.members, cloneId, &NativeVisgroupMember::nativeId);
    REQUIRE(reopenedRow != reopenedSnapshot.members.end());
    CHECK(reopenedRow->memberships == std::vector<std::string>{"10"});
    map.undoCommand();
    CHECK_FALSE(clone->parent());
    map.redoCommand();
    CHECK(bytes(map) == saved);
  }
  SECTION(
    "new visgroups controller entity and cordons share one append without duplicate "
    "allocation")
  {
    loadFixture(
      env,
      "// synthetic root composition\r\nversioninfo { formatversion 100 }\r\nworld { id "
      "1 classname worldspawn }\r\n");
    auto& map = fixture.load(
      env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
    const auto original = bytes(map);
    auto tree = NativeVisgroupRequest{nativeVisgroups(map)};
    tree.records.push_back({NativeVisgroupDraftKey{1}, {}, "Root"});
    tree.records.push_back(
      {NativeVisgroupDraftKey{2}, NativeVisgroupDraftKey{1}, "Child"});
    REQUIRE(applyNativeVisgroups(map, tree).status == NativeVisgroupApplyStatus::Changed);
    REQUIRE(createPointEntity(
      map,
      EntityDefinition{
        .name = "info_target",
        .pointEntityDefinition = PointEntityDefinition{.bounds = vm::bbox3d{8}}},
      vm::vec3d{1, 2, 3}));
    auto cordon = NativeCordonRequest{nativeCordons(map)};
    cordon.form = NativeCordonForm::Modern;
    cordon.active = true;
    cordon.records.push_back(
      {{}, "Cordon", true, {{{}, vm::vec3d{-16, -16, -16}, vm::vec3d{16, 16, 16}}}});
    REQUIRE(applyNativeCordons(map, cordon).status == NativeCordonApplyStatus::Changed);
    const auto saved = bytes(map);
    const auto snapshot = nativeVisgroups(map);
    CHECK(bytes(map) == saved);
    REQUIRE(map.saveAs(env.dir() / "combined.vmf").is_success());
    REQUIRE(map.save().is_success());
    CHECK(bytes(map) == saved);
    CHECK(nativeVisgroups(map).records == snapshot.records);
    auto reopenedFixture = MapFixture{};
    auto& reopened = reopenedFixture.load(
      env.dir() / "combined.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
    CHECK(bytes(reopened) == saved);
    CHECK(nativeVisgroups(reopened).records.size() == 2);
    CHECK(nativeCordons(reopened).records.size() == 1);
    CHECK(reopened.worldNode().defaultLayer()->children().size() == 1);
    map.undoCommand();
    map.undoCommand();
    map.undoCommand();
    CHECK(bytes(map) == original);
    map.redoCommand();
    map.redoCommand();
    map.redoCommand();
    CHECK(bytes(map) == saved);
  }
}
TEST_CASE("Map native visgroups Both replay refuses independent keeper metadata")
{
  const auto mode = GENERATE(0, 1, 2); // hidden undo, visible undo, visible redo
  CAPTURE(mode);
  auto env = fs::TestEnvironment{};
  loadFixture(env);
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  const auto initial = nativeVisgroups(map);
  const auto a = initial.records[0].id;
  const auto b = initial.records[1].id;
  REQUIRE(createBrush(map, cubePoints()));
  auto* front = map.selection().brushes.front();
  assign(map, {front}, a);
  const auto original = front->brush();
  REQUIRE(clipBrushesBoth(map, {front}, {{{0, 0, 0}, {0, 0, 64}, {0, 64, 0}}}));
  REQUIRE(map.selection().brushes.size() == 2);
  auto* back = map.selection().brushes.back();
  REQUIRE(back != front);
  const auto backId = member(map, *back);
  const auto clipped = bytes(map);
  map.undoCommand();
  REQUIRE_FALSE(back->parent());
  REQUIRE(front->brush() == original);
  map.redoCommand();
  REQUIRE(bytes(map) == clipped); // normal stored replay must genuinely act
  if (mode == 2)
  {
    map.undoCommand();
    REQUIRE_FALSE(back->parent());
  }
  auto external = NativeVisgroupRequest{nativeVisgroups(map)};
  external.memberships.push_back({{member(map, *front)}, a, false});
  external.memberships.push_back({{member(map, *front)}, b, true});
  if (mode == 0)
  {
    external.manual = NativeVisgroupManualAction{{b}, false};
  }
  auto prepared = map.vmfScene()->prepareVisgroupChange(external);
  REQUIRE(prepared);
  map.vmfScene()->applyVisgroupChange(*prepared, NativeVisgroupDirection::Forward);
  REQUIRE(front->visible() == (mode != 0));
  const auto before = bytes(map);
  const auto frontBefore = front->brush();
  const auto backBefore = back->brush();
  auto* parent = front->parent();
  const auto children = parent->children();
  const auto selection = map.selection().nodes;
  const auto faces = map.selection().brushFaces;
  const auto count = map.modificationCount();
  auto notifications = 0;
  auto connection = NotifierConnection{};
  const auto notified = [&](const auto&) { ++notifications; };
  connection += map.selectionDidChangeNotifier.connect(notified);
  connection += map.nodesWereAddedNotifier.connect(notified);
  connection += map.nodesWereRemovedNotifier.connect(notified);
  connection += map.nodesDidChangeNotifier.connect(notified);
  if (mode == 2)
  {
    map.redoCommand();
  }
  else
  {
    map.undoCommand();
  }
  CHECK(front->brush() == frontBefore);
  if (mode != 2)
  {
    CHECK(back->brush() == backBefore);
  }
  else
  {
    // Default failed redo disposes parked siblings; inspect the tombstone,
    // not a dead node.
    const auto after = nativeVisgroups(map);
    const auto retired =
      std::ranges::find(after.members, backId, &NativeVisgroupMember::id);
    REQUIRE(retired != after.members.end());
    CHECK_FALSE(retired->active);
    CHECK_FALSE(retired->projected);
  }
  CHECK(parent->children() == children);
  CHECK(map.selection().nodes == selection);
  CHECK(map.selection().brushFaces == faces);
  CHECK(map.modificationCount() == count);
  CHECK(notifications == 0);
  CHECK(bytes(map) == before);
}

TEST_CASE("Map native visgroups session visibility is not native persistence authority")
{
  auto env = fs::TestEnvironment{};
  loadFixture(env);
  auto seedFixture = MapFixture{};
  auto& seed = seedFixture.load(
    env.dir() / "loaded.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  REQUIRE(createBrush(seed, cubePoints()));
  REQUIRE(seed.saveAs(env.dir() / "solid.vmf").is_success());
  auto fixture = MapFixture{};
  auto& map = fixture.load(
    env.dir() / "solid.vmf", MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  BrushNode* original = nullptr;
  for (auto* child : map.worldNode().defaultLayer()->children())
    if (auto* brush = dynamic_cast<BrushNode*>(child))
      original = brush;
  REQUIRE(original);
  REQUIRE(clipBrushes(map, {original}, {{{0, 0, 0}, {0, 0, 64}, {0, 64, 0}}}));
  deselectAll(map);
  selectNodes(map, {original});
  REQUIRE(createBrushEntity(map, EntityDefinition{.name = "func_detail"}));
  auto* owner = original->parent();
  REQUIRE(dynamic_cast<EntityNode*>(owner));
  REQUIRE(createBrush(map, cubePoints()));
  auto* generated = map.selection().brushes.front();
  const auto before = bytes(map);
  const auto revision = nativeVisgroups(map).revision;
  const auto count = map.modificationCount();
  auto* layer = map.worldNode().defaultLayer();
  hideNodes(map, {&node(map, "4"), original, owner, generated, layer});
  CHECK_FALSE(node(map, "4").sessionVisible());
  CHECK_FALSE(original->sessionVisible());
  CHECK_FALSE(owner->sessionVisible());
  CHECK_FALSE(generated->sessionVisible());
  CHECK_FALSE(layer->sessionVisible());
  CHECK(bytes(map) == before);
  CHECK(nativeVisgroups(map).revision == revision);
  CHECK(map.modificationCount() == count);
  REQUIRE(map.saveAs(env.dir() / "session-hidden.vmf").is_success());
  CHECK(bytes(map) == before);
  // Geometry admission remains operation=true: session hidden is not editable.
  CHECK_FALSE(clipBrushes(map, {original}, {{{-8, 0, 0}, {-8, 0, 64}, {-8, 64, 0}}}));
  CHECK(bytes(map) == before);
  map.undoCommand();
  CHECK(original->sessionVisible());
  CHECK(owner->sessionVisible());
  CHECK(generated->sessionVisible());
  CHECK(layer->sessionVisible());
  CHECK(bytes(map) == before);
  map.redoCommand();
  CHECK_FALSE(original->sessionVisible());
  CHECK(bytes(map) == before);
  auto reopenedFixture = MapFixture{};
  auto& reopened = reopenedFixture.load(
    env.dir() / "session-hidden.vmf",
    MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  CHECK(bytes(reopened) == before);
  CHECK(node(reopened, "4").sessionVisible());
}
} // namespace tb::mdl
