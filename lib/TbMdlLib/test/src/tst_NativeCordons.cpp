/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/EntityNode.h"
#include "mdl/LayerNode.h"
#include "mdl/NativeCordons.h"
#include "mdl/VmfScene.h"

#include <limits>
#include <memory>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::mdl
{
namespace
{
const auto bounds = vm::bbox3d{131072.0};
const auto prefix = std::string{
  "\xef\xbb\xbf// synthetic cordon control\r\nversioninfo { \"formatversion\" \"100\" "
  "}\r\n"
  "world { \"id\" \"1\" \"classname\" \"worldspawn\" }\r\n"
  "entity { \"id\" \"2\" \"classname\" \"info_target\" \"targetname\" \"marker\" }\r\n"};
const auto modern = std::string{
  "cordons { \"active\" \"0\"\r\n"
  "cordon { \"name\" \"before\" \"active\" \"1\"\r\n"
  "box { \"mins\" \"(00 -2.50 0e0)\" \"maxs\" \"(10 20 30)\" }\r\n"
  "box { \"mins\" \"(-8 -8 -8)\" \"maxs\" \"(-1 -1 -1)\" } }\r\n"
  "cordon { box { \"mins\" \"(1 2 3)\" \"maxs\" \"(4 5 6)\" } } }\r\n"};
void replace(std::string& text, const std::string& before, const std::string& after)
{
  const auto pos = text.find(before);
  REQUIRE(pos != std::string::npos);
  text.replace(pos, before.size(), after);
}
} // namespace
TEST_CASE("NativeCordon scene live staging and exact serialization")
{
  const auto source = prefix + modern;
  auto scene = VmfScene::read(source, bounds);
  REQUIRE(scene.cordons().refusalReason.empty());
  CHECK(scene.serialize(scene.world()) == source);
  auto unchanged = NativeCordonRequest{scene.cordons()};
  CHECK_FALSE(scene.prepareCordonChange(unchanged));

  // Act a separate native property edit and restore it; no production toggles.
  auto* entity =
    dynamic_cast<EntityNode*>(scene.world().defaultLayer()->children().front());
  REQUIRE(entity);
  const auto original = entity->entity();
  auto changed = original;
  changed.addOrUpdateProperty("targetname", "acted-control");
  entity->setEntity(std::move(changed));
  auto control = source;
  replace(control, "\"marker\"", "\"acted-control\"");
  CHECK(scene.serialize(scene.world()) == control);
  entity->setEntity(original);
  CHECK(scene.serialize(scene.world()) == source);

  auto request = NativeCordonRequest{scene.cordons()};
  request.active = true;
  request.records[0].name = "distinctive-live";
  request.records[0].active = false;
  request.records[0].boxes[0].mins = vm::vec3d{-11, -12, -13};
  auto change = scene.prepareCordonChange(request);
  REQUIRE(change);
  CHECK(scene.cordons().records[0].name == "before");
  scene.applyCordonChange(*change, NativeCordonDirection::Forward);
  CHECK(scene.cordons().records[0].name == "distinctive-live");
  CHECK(scene.cordons().records[0].boxes[0].mins == vm::vec3d{-11, -12, -13});
  auto expected = source;
  replace(expected, "cordons { \"active\" \"0\"", "cordons { \"active\" \"1\"");
  replace(
    expected,
    "\"name\" \"before\" \"active\" \"1\"",
    "\"name\" \"distinctive-live\" \"active\" \"0\"");
  replace(expected, "(00 -2.50 0e0)", "(-11 -12 -13)");
  CHECK(scene.serialize(scene.world()) == expected);
  CHECK(scene.serialize(scene.world()) == expected);
  const auto reopened = VmfScene::read(expected, bounds);
  CHECK(reopened.cordons().records[0].name == "distinctive-live");
  scene.applyCordonChange(*change, NativeCordonDirection::Backward);
  CHECK(scene.serialize(scene.world()) == source);
  scene.applyCordonChange(*change, NativeCordonDirection::Forward);
  CHECK(scene.serialize(scene.world()) == expected);
}
namespace
{
NativeCordonRecord newRecord(std::string name = "created")
{
  return {{}, std::move(name), false, {{{}, {-12, -24, -36}, {12, 24, 36}}}};
}
NativeCordonRequest createRequest(const NativeCordonSnapshot& snapshot)
{
  auto request = NativeCordonRequest{snapshot};
  request.form = NativeCordonForm::Modern;
  request.active = false;
  request.records.push_back(newRecord());
  return request;
}
PreparedNativeCordonChange apply(VmfScene& scene, const NativeCordonRequest& request)
{
  auto change = scene.prepareCordonChange(request);
  REQUIRE(change);
  scene.applyCordonChange(*change, NativeCordonDirection::Forward);
  return std::move(*change);
}
NativeCordonRequest clearRequest(const NativeCordonSnapshot& snapshot)
{
  auto request = NativeCordonRequest{snapshot};
  request.form = NativeCordonForm::None;
  request.active.reset();
  request.records.clear();
  return request;
}
std::string addPoint(VmfScene& scene)
{
  auto entity = std::make_unique<EntityNode>(Entity{{{"classname", "info_target"}}});
  auto* node = entity.get();
  scene.validateAdditions(*scene.world().defaultLayer(), {node}, false);
  scene.registerAdditions({node}, false);
  scene.world().defaultLayer()->addChild(entity.release());
  return scene.nativeId(*node);
}
size_t rootsNamed(const VmfDocument& d, const std::string& name)
{
  size_t count = 0;
  for (const auto id : d.roots())
  {
    if (d.tokenContents(d.entries()[id].key) == name)
    {
      ++count;
    }
  }
  return count;
}
} // namespace

TEST_CASE("NativeCordon scene fresh creation root composition and namespace")
{
  auto scene = VmfScene::read(prefix, bounds);
  auto control = VmfScene::read(prefix, bounds);
  const auto originalEntryCount = scene.document().entries().size();
  auto bad = createRequest(scene.cordons());
  bad.records.push_back(newRecord("later invalid"));
  bad.records.back().boxes[0].maxs = bad.records.back().boxes[0].mins;
  CHECK_THROWS_AS(scene.prepareCordonChange(bad), std::invalid_argument);
  CHECK(scene.cordons().revision == 0);
  auto change = apply(scene, createRequest(scene.cordons()));
  const auto id = scene.cordons().records[0].id;
  const auto boxId = scene.cordons().records[0].boxes[0].id;
  CHECK(scene.document().source() == prefix);
  CHECK(scene.document().entries().size() == originalEntryCount);
  CHECK(scene.world().defaultLayer()->childCount() == 1);
  CHECK(addPoint(scene) == addPoint(control));
  const auto saved = scene.serialize(scene.world());
  const auto document = VmfDocument::parse(saved);
  CHECK(rootsNamed(document, "cordons") == 1);
  CHECK(rootsNamed(document, "entity") == 2);
  CHECK(scene.serialize(scene.world()) == saved);
  auto reopened = VmfScene::read(saved, bounds);
  CHECK(reopened.cordons().records[0].name == "created");
  CHECK(reopened.cordons().records[0].active == false);
  auto edit = NativeCordonRequest{reopened.cordons()};
  edit.records[0].name = "reopened further edit";
  auto editChange = apply(reopened, edit);
  CHECK(
    reopened.serialize(reopened.world()).find("reopened further edit")
    != std::string::npos);
  reopened.applyCordonChange(editChange, NativeCordonDirection::Backward);
  CHECK(reopened.serialize(reopened.world()) == saved);
  scene.applyCordonChange(change, NativeCordonDirection::Backward);
  CHECK(scene.serialize(scene.world()) == control.serialize(control.world()));
  scene.applyCordonChange(change, NativeCordonDirection::Forward);
  CHECK(scene.cordons().records[0].id == id);
  CHECK(scene.cordons().records[0].boxes[0].id == boxId);
  CHECK(scene.serialize(scene.world()) == saved);
}

TEST_CASE("NativeCordon scene modern children optional fields and removal")
{
  const auto source = prefix + modern;
  auto scene = VmfScene::read(source, bounds);
  const auto original = scene.cordons();
  SECTION("append boxes records and absent scalars")
  {
    auto request = NativeCordonRequest{original};
    request.active.reset();
    request.records[0].name.reset();
    request.records[0].active.reset();
    request.records[1].name = "explicit name";
    request.records[1].active = false;
    request.records[1].boxes.push_back({{}, {0.1, 0.2, 0.3}, {1.1, 1.2, 1.3}});
    request.records.push_back(newRecord("appended"));
    auto change = apply(scene, request);
    const auto saved = scene.serialize(scene.world());
    const auto reopened = VmfScene::read(saved, bounds);
    REQUIRE(reopened.cordons().refusalReason.empty());
    CHECK_FALSE(reopened.cordons().active);
    CHECK_FALSE(reopened.cordons().records[0].name);
    CHECK_FALSE(reopened.cordons().records[0].active);
    CHECK(reopened.cordons().records[1].name == "explicit name");
    CHECK(reopened.cordons().records[1].active == false);
    CHECK(reopened.cordons().records[1].boxes[1].mins == vm::vec3d{0.1, 0.2, 0.3});
    CHECK(reopened.cordons().records[1].boxes[1].maxs == vm::vec3d{1.1, 1.2, 1.3});
    CHECK(reopened.cordons().records.size() == 3);
    CHECK(saved.find("(00 -2.50 0e0)") != std::string::npos);
    CHECK(scene.serialize(scene.world()) == saved);
    scene.applyCordonChange(change, NativeCordonDirection::Backward);
    CHECK(scene.serialize(scene.world()) == source);
  }
  SECTION("remove edited parent without overlapping child patches")
  {
    auto request = NativeCordonRequest{original};
    request.records[0].boxes[0].mins = {-19, -18, -17};
    auto child = apply(scene, request);
    auto remove = NativeCordonRequest{scene.cordons()};
    remove.records.erase(remove.records.begin());
    auto parent = apply(scene, remove);
    auto reopened = VmfScene::read(scene.serialize(scene.world()), bounds);
    REQUIRE(reopened.cordons().records.size() == 1);
    CHECK_FALSE(reopened.cordons().records[0].name);
    auto clear = apply(scene, clearRequest(scene.cordons()));
    CHECK(rootsNamed(VmfDocument::parse(scene.serialize(scene.world())), "cordons") == 0);
    scene.applyCordonChange(clear, NativeCordonDirection::Backward);
    scene.applyCordonChange(parent, NativeCordonDirection::Backward);
    CHECK(scene.cordons().records[0].boxes[0].mins == vm::vec3d{-19, -18, -17});
    scene.applyCordonChange(child, NativeCordonDirection::Backward);
    CHECK(scene.serialize(scene.world()) == source);
  }
  SECTION("remove one box only")
  {
    auto request = NativeCordonRequest{original};
    request.records[0].boxes.erase(request.records[0].boxes.begin());
    auto change = apply(scene, request);
    const auto reopened = VmfScene::read(scene.serialize(scene.world()), bounds);
    REQUIRE(reopened.cordons().records[0].boxes.size() == 1);
    CHECK(reopened.cordons().records[0].boxes[0].mins == vm::vec3d{-8, -8, -8});
    scene.applyCordonChange(change, NativeCordonDirection::Backward);
    CHECK(scene.serialize(scene.world()) == source);
  }
  SECTION("remove controller then explicitly recreate with unrelated identities")
  {
    auto clear = apply(scene, clearRequest(original));
    auto created = apply(scene, createRequest(scene.cordons()));
    CHECK(scene.cordons().records[0].id != original.records[0].id);
    const auto saved = scene.serialize(scene.world());
    CHECK(rootsNamed(VmfDocument::parse(saved), "cordons") == 1);
    CHECK(VmfScene::read(saved, bounds).cordons().records[0].name == "created");
    scene.applyCordonChange(created, NativeCordonDirection::Backward);
    scene.applyCordonChange(clear, NativeCordonDirection::Backward);
    CHECK(scene.serialize(scene.world()) == source);
  }
}

TEST_CASE("NativeCordon scene loaded legacy in place and empty modern removal")
{
  SECTION("legacy preserves name and absent activation")
  {
    const auto legacy = std::string{
      "cordon { \"name\" \"legacy retained\" \"mins\" \"(00 0 0)\" \"maxs\" \"(32 32 "
      "32)\" }"};
    auto scene = VmfScene::read(prefix + legacy, bounds);
    REQUIRE(scene.cordons().refusalReason.empty());
    CHECK(scene.cordons().form == NativeCordonForm::Legacy);
    CHECK_FALSE(scene.prepareCordonChange(NativeCordonRequest{scene.cordons()}));
    auto request = NativeCordonRequest{scene.cordons()};
    request.records[0].active = true;
    request.records[0].boxes[0].maxs = {64, 65, 66};
    auto change = apply(scene, request);
    const auto saved = scene.serialize(scene.world());
    const auto reopened = VmfScene::read(saved, bounds);
    CHECK(reopened.cordons().form == NativeCordonForm::Legacy);
    CHECK(reopened.cordons().records[0].active == true);
    CHECK(reopened.cordons().records[0].name == "legacy retained");
    CHECK(reopened.cordons().records[0].boxes[0].maxs == vm::vec3d{64, 65, 66});
    CHECK(saved.find("(00 0 0)") != std::string::npos);
    CHECK(rootsNamed(VmfDocument::parse(saved), "cordons") == 0);
    auto rename = NativeCordonRequest{scene.cordons()};
    rename.records[0].name = "not admitted";
    CHECK_THROWS_AS(scene.prepareCordonChange(rename), std::invalid_argument);
    auto append = NativeCordonRequest{scene.cordons()};
    append.records.push_back(newRecord());
    CHECK_THROWS_AS(scene.prepareCordonChange(append), std::invalid_argument);
    auto clear = apply(scene, clearRequest(scene.cordons()));
    CHECK(rootsNamed(VmfDocument::parse(scene.serialize(scene.world())), "cordon") == 0);
    auto fresh = apply(scene, createRequest(scene.cordons()));
    CHECK(rootsNamed(VmfDocument::parse(scene.serialize(scene.world())), "cordons") == 1);
    scene.applyCordonChange(fresh, NativeCordonDirection::Backward);
    scene.applyCordonChange(clear, NativeCordonDirection::Backward);
    scene.applyCordonChange(change, NativeCordonDirection::Backward);
    CHECK(scene.serialize(scene.world()) == prefix + legacy);
  }
  SECTION("empty modern remains inspectable and explicitly removable")
  {
    const auto source = prefix + "cordons { // empty retained\n }";
    auto scene = VmfScene::read(source, bounds);
    REQUIRE(scene.cordons().refusalReason.empty());
    CHECK(scene.cordons().records.empty());
    CHECK_FALSE(scene.prepareCordonChange(NativeCordonRequest{scene.cordons()}));
    auto change = apply(scene, clearRequest(scene.cordons()));
    CHECK(rootsNamed(VmfDocument::parse(scene.serialize(scene.world())), "cordons") == 0);
    scene.applyCordonChange(change, NativeCordonDirection::Backward);
    CHECK(scene.serialize(scene.world()) == source);
  }
}

TEST_CASE("NativeCordon scene admission isolation and malformed controllers")
{
  const auto bad = GENERATE(
    "cordons {} cordons {}",
    "cordon { mins \"(0 0 0)\" maxs \"(1 1 1)\" } cordons {}",
    "Cordons {}",
    "cordons \"scalar\"",
    "cordon \"scalar\"",
    "cordons { active 1 active 0 }",
    "cordons { active {} }",
    "cordons { active 2 }",
    "cordons { future keep }",
    "cordons { cordon { name \"x\" future {} box { mins \"(0 0 0)\" maxs \"(1 1 1)\" } } "
    "}",
    "cordons { cordon { box { mins \"(0 0 0)\" maxs \"(1 1 1)\" } box { mins \"(0 0 0)\" "
    "} } }",
    "cordons { cordon { box { mins \"(0 0 0)\" maxs \"(1 1 1)\" future 0 } } }",
    "cordons { cordon { box { mins \"(0 0 0)\" mins \"(0 0 0)\" maxs \"(1 1 1)\" } } }",
    "cordons { cordon { box { mins \"(nan 0 0)\" maxs \"(1 1 1)\" } } }",
    "cordons { cordon { box { mins \"(0 0 0)\" maxs \"(1e999 1 1)\" } } }",
    "cordons { cordon { box { mins \"(1 0 0)\" maxs \"(0 1 1)\" } } }",
    "cordons { cordon { name \"empty record\" } }",
    "cordon { mins \"(0 0 0)\" maxs \"(1 1 1)\" unknown keep }",
    "cordons { cordon scalar }",
    "cordons { cordon { box scalar } }");
  const auto source = prefix + bad;
  auto scene = VmfScene::read(source, bounds);
  CAPTURE(bad);
  CHECK_FALSE(scene.cordons().refusalReason.empty());
  CHECK(scene.serialize(scene.world()) == source);
  CHECK_THROWS_AS(
    scene.prepareCordonChange(createRequest(scene.cordons())), std::invalid_argument);
  // Unsupported cordons do not make an independent entity field read-only.
  auto* entity =
    dynamic_cast<EntityNode*>(scene.world().defaultLayer()->children().front());
  REQUIRE(entity);
  auto value = entity->entity();
  value.addOrUpdateProperty("targetname", "independent");
  entity->setEntity(std::move(value));
  auto expected = source;
  replace(expected, "\"marker\"", "\"independent\"");
  CHECK(scene.serialize(scene.world()) == expected);
}

TEST_CASE("NativeCordon scene unknown outside preview and version isolation")
{
  SECTION("unknown outside retained does not disable cordon authoring")
  {
    const auto outside =
      std::string{"future { output one output two opaque { arbitrary 7 } }\r\n"};
    auto scene = VmfScene::read(prefix + outside + modern, bounds);
    REQUIRE(scene.cordons().refusalReason.empty());
    auto request = NativeCordonRequest{scene.cordons()};
    request.records[0].name = "independent cordon";
    apply(scene, request);
    CHECK(scene.serialize(scene.world()).find(outside) != std::string::npos);
  }
  SECTION("preview refuses while preserving all source bytes")
  {
    auto scene = VmfScene::readReadOnlyPreview(prefix + modern, bounds);
    CHECK_FALSE(scene.cordons().refusalReason.empty());
    CHECK_THROWS_AS(
      scene.prepareCordonChange(clearRequest(scene.cordons())), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == prefix + modern);
  }
  SECTION("unsupported version refuses cordon authoring")
  {
    auto source = prefix + modern;
    replace(source, "\"100\"", "\"101\"");
    auto scene = VmfScene::read(source, bounds);
    CHECK_FALSE(scene.cordons().refusalReason.empty());
    CHECK_THROWS_AS(
      scene.prepareCordonChange(clearRequest(scene.cordons())), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == source);
  }
}

TEST_CASE("NativeCordon model stale handles revision replay and move scope")
{
  auto scene = VmfScene::read(prefix + modern, bounds);
  auto foreign = VmfScene::read(prefix + modern, bounds);
  const auto initial = scene.cordons();
  auto request = NativeCordonRequest{initial};
  request.records[0].name = "first edit";
  auto change = apply(scene, request);
  CHECK_THROWS_AS(scene.prepareCordonChange(request), std::invalid_argument);
  CHECK_THROWS_AS(
    foreign.applyCordonChange(change, NativeCordonDirection::Forward),
    std::invalid_argument);
  CHECK_THROWS_AS(
    scene.applyCordonChange(change, NativeCordonDirection::Forward),
    std::invalid_argument);
  auto foreignBox = NativeCordonRequest{scene.cordons()};
  foreignBox.records[0].boxes[0].id = foreign.cordons().records[0].boxes[0].id;
  CHECK_THROWS_AS(scene.prepareCordonChange(foreignBox), std::invalid_argument);
  auto movedBox = NativeCordonRequest{scene.cordons()};
  movedBox.records[1].boxes[0].id = scene.cordons().records[0].boxes[0].id;
  CHECK_THROWS_AS(scene.prepareCordonChange(movedBox), std::invalid_argument);
  auto duplicated = NativeCordonRequest{scene.cordons()};
  duplicated.records.push_back(duplicated.records[0]);
  CHECK_THROWS_AS(scene.prepareCordonChange(duplicated), std::invalid_argument);
  const auto revision = scene.cordons().revision;
  auto moved = std::move(scene);
  moved.applyCordonChange(change, NativeCordonDirection::Backward);
  CHECK(moved.cordons().revision > revision);
  CHECK_THROWS_AS(moved.prepareCordonChange(request), std::invalid_argument);
  moved.applyCordonChange(change, NativeCordonDirection::Forward);
  CHECK(moved.cordons().records[0].id == initial.records[0].id);
  auto later = NativeCordonRequest{moved.cordons()};
  later.records[0].name = "later edit";
  auto laterChange = apply(moved, later);
  const auto saved = moved.serialize(moved.world());
  CHECK_THROWS_AS(
    moved.applyCordonChange(change, NativeCordonDirection::Backward),
    std::invalid_argument);
  CHECK(moved.serialize(moved.world()) == saved);
  moved.applyCordonChange(laterChange, NativeCordonDirection::Backward);
  moved.applyCordonChange(change, NativeCordonDirection::Backward);
  CHECK(moved.serialize(moved.world()) == prefix + modern);
  const auto owned = moved.cordons();
  moved = VmfScene::read(prefix + modern, bounds);
  CHECK(owned.records[0].name == "before");
  CHECK_THROWS_AS(
    moved.prepareCordonChange(NativeCordonRequest{owned}), std::invalid_argument);
}

TEST_CASE("NativeCordon model invalid later staging budgets and handle nonconsumption")
{
  auto scene = VmfScene::read(prefix, bounds);
  const auto invalid = GENERATE(0, 1, 2, 3, 4, 5, 6);
  auto request = createRequest(scene.cordons());
  request.records.push_back(newRecord("later"));
  switch (invalid)
  {
  case 0:
    request.records.back().boxes[0].maxs[0] = -100;
    break;
  case 1:
    request.records.back().boxes[0].mins[0] = std::numeric_limits<double>::quiet_NaN();
    break;
  case 2:
    request.records.back().boxes[0].maxs[0] = std::numeric_limits<double>::infinity();
    break;
  case 3:
    request.records.back().boxes[0].mins[0] = -std::numeric_limits<double>::max();
    request.records.back().boxes[0].maxs[0] = std::numeric_limits<double>::max();
    break;
  case 4:
    request.records.back().boxes.clear();
    break;
  case 5:
    request.records.back().name = std::string(NativeCordons::MaxNameBytes + 1, 'a');
    break;
  case 6:
    request.records.back().name = "line\nbreak";
    break;
  }
  // Pure preparations may propose the same identities, never consume them.
  auto candidate = scene.prepareCordonChange(createRequest(scene.cordons()));
  REQUIRE(candidate);
  CHECK_THROWS_AS(scene.prepareCordonChange(request), std::invalid_argument);
  CHECK(scene.cordons().revision == 0);
  CHECK(scene.serialize(scene.world()) == prefix);
  auto accepted = scene.prepareCordonChange(createRequest(scene.cordons()));
  REQUIRE(accepted);
  CHECK(accepted->after().records[0].id == candidate->after().records[0].id);
  scene.applyCordonChange(*accepted, NativeCordonDirection::Forward);
  scene.applyCordonChange(*accepted, NativeCordonDirection::Backward);
  // A different prepared owner cannot reuse a published handle after undo.
  CHECK_THROWS_AS(
    scene.applyCordonChange(*candidate, NativeCordonDirection::Forward),
    std::invalid_argument);
  auto next = apply(scene, createRequest(scene.cordons()));
  CHECK(next.after().records[0].id != accepted->after().records[0].id);
  CHECK_THROWS_AS(scene.prepareCordonChange(request), std::invalid_argument);
}

TEST_CASE("NativeCordon model local name text record and box limits")
{
  auto scene = VmfScene::read(prefix, bounds);
  SECTION("name boundary and text aggregate")
  {
    auto request = createRequest(scene.cordons());
    request.records[0].name = std::string(NativeCordons::MaxNameBytes, 'n');
    REQUIRE(scene.prepareCordonChange(request));
    request.records[0].name->push_back('n');
    CHECK_THROWS_AS(scene.prepareCordonChange(request), std::invalid_argument);
    request.records.clear();
    for (size_t i = 0; i < NativeCordons::MaxTextBytes / NativeCordons::MaxNameBytes; ++i)
    {
      request.records.push_back(newRecord(std::string(NativeCordons::MaxNameBytes, 'x')));
    }
    REQUIRE(scene.prepareCordonChange(request));
    request.records.push_back(newRecord("x"));
    CHECK_THROWS_AS(scene.prepareCordonChange(request), std::invalid_argument);
  }
  SECTION("combined batch boundary below individual record and box limits")
  {
    auto request = createRequest(scene.cordons());
    auto record = newRecord("x");
    const auto box = record.boxes[0];
    record.boxes.assign(4, box);
    request.records.assign(NativeCordons::MaxBatchItems / 5, record);
    REQUIRE(scene.prepareCordonChange(request));
    request.records[0].boxes.push_back(record.boxes[0]);
    CHECK_THROWS_AS(scene.prepareCordonChange(request), std::invalid_argument);
  }
  SECTION("record and box boundaries")
  {
    auto request = createRequest(scene.cordons());
    request.records.assign(NativeCordons::MaxRecords, newRecord("x"));
    REQUIRE(scene.prepareCordonChange(request));
    request.records.push_back(newRecord("x"));
    CHECK_THROWS_AS(scene.prepareCordonChange(request), std::invalid_argument);
    request.records.assign(1, newRecord("x"));
    const auto box = request.records[0].boxes[0];
    request.records[0].boxes.assign(NativeCordons::MaxBoxes, box);
    REQUIRE(scene.prepareCordonChange(request));
    request.records[0].boxes.push_back(box);
    CHECK_THROWS_AS(scene.prepareCordonChange(request), std::invalid_argument);
  }
  CHECK(scene.serialize(scene.world()) == prefix);
  CHECK(scene.cordons().revision == 0);
}
TEST_CASE("NativeCordon model initial prepared freshness differs from replay")
{
  auto scene = VmfScene::read(prefix + modern, bounds);
  auto request = NativeCordonRequest{scene.cordons()};
  request.records[0].name = "delayed";
  auto delayed = scene.prepareCordonChange(request);
  REQUIRE(delayed);
  request.records[0].name = "acted interleaving";
  auto acted = apply(scene, request);
  scene.applyCordonChange(acted, NativeCordonDirection::Backward);
  const auto current = scene.serialize(scene.world());
  CHECK_THROWS_AS(
    scene.applyCordonChange(*delayed, NativeCordonDirection::Forward),
    std::invalid_argument);
  CHECK(scene.serialize(scene.world()) == current);
  scene.applyCordonChange(acted, NativeCordonDirection::Forward);
  CHECK(scene.cordons().records[0].name == "acted interleaving");
}

TEST_CASE("NativeCordon scene numeric emission round trips finite extremes")
{
  const auto tiny = std::numeric_limits<double>::denorm_min();
  const auto huge = std::numeric_limits<double>::max();
  auto scene = VmfScene::read(prefix, bounds);
  auto request = createRequest(scene.cordons());
  request.records[0].boxes[0].mins = {-huge, -tiny, 0.0};
  request.records[0].boxes[0].maxs = {-huge / 2, tiny, huge};
  apply(scene, request);
  const auto saved = scene.serialize(scene.world());
  const auto reopened = VmfScene::read(saved, bounds);
  REQUIRE(reopened.cordons().refusalReason.empty());
  CHECK(reopened.cordons().records[0].boxes[0].mins == request.records[0].boxes[0].mins);
  CHECK(reopened.cordons().records[0].boxes[0].maxs == request.records[0].boxes[0].maxs);
}
TEST_CASE("NativeCordon scene legacy existing activation changes and optional removal")
{
  const auto source = prefix + "cordon { active 0 mins \"(0 0 0)\" maxs \"(1 1 1)\" }";
  auto scene = VmfScene::read(source, bounds);
  auto request = NativeCordonRequest{scene.cordons()};
  request.records[0].active = true;
  auto changed = apply(scene, request);
  auto expected = source;
  replace(expected, "active 0", "active \"1\"");
  CHECK(scene.serialize(scene.world()) == expected);
  request = NativeCordonRequest{scene.cordons()};
  request.records[0].active.reset();
  auto removed = apply(scene, request);
  const auto reopened = VmfScene::read(scene.serialize(scene.world()), bounds);
  CHECK_FALSE(reopened.cordons().records[0].active);
  scene.applyCordonChange(removed, NativeCordonDirection::Backward);
  CHECK(scene.serialize(scene.world()) == expected);
  scene.applyCordonChange(changed, NativeCordonDirection::Backward);
  CHECK(scene.serialize(scene.world()) == source);
}

} // namespace tb::mdl
