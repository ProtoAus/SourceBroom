/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/EntityNode.h"
#include "mdl/LayerNode.h"
#include "mdl/NativeCameras.h"
#include "mdl/SourceEditorMetadata.h"
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
  "\xef\xbb\xbf// native camera control\r\n"
  "versioninfo { formatversion 100 }\r\nworld { id 1 classname worldspawn }\r\n"
  "entity { id 2 classname info_target targetname \"before\" }\r\n"};
const auto cameras = std::string{
  "cameras { activecamera \"01\"\r\n"
  "camera { position \"[00 1.25 2]\" look \"[1 1.25 2]\" }\r\n"
  "camera { position \"[3 4 5]\" look \"[3 5 5]\" } }\r\n"};
void replace(std::string& text, const std::string& before, const std::string& after)
{
  const auto pos = text.find(before);
  REQUIRE(pos != std::string::npos);
  text.replace(pos, before.size(), after);
}
} // namespace
TEST_CASE("NativeCamera scene scoped pose authoring and exact preservation")
{
  const auto source = prefix + cameras;
  auto scene = VmfScene::read(source, bounds);
  REQUIRE(SourceEditorMetadata::read(scene.document()).cameras.size() == 2);
  REQUIRE(scene.cameras().refusalReason.empty());
  REQUIRE(scene.cameras().records.size() == 2);
  CHECK(scene.serialize(scene.world()) == source);
  auto* entity =
    dynamic_cast<EntityNode*>(scene.world().defaultLayer()->children().front());
  REQUIRE(entity);
  const auto original = entity->entity();
  auto acted = original;
  acted.addOrUpdateProperty("targetname", "acted-control");
  entity->setEntity(std::move(acted));
  auto control = source;
  replace(control, "\"before\"", "\"acted-control\"");
  CHECK(scene.serialize(scene.world()) == control);
  entity->setEntity(original);
  CHECK(scene.serialize(scene.world()) == source);
  auto request = NativeCameraRequest{scene.cameras()};
  request.records[0].eye = {-10, 1.25, 2};
  request.active = {NativeCameraActiveKind::Camera, request.records[0].id};
  auto change = scene.prepareCameraChange(request);
  REQUIRE(change);
  scene.applyCameraChange(*change, NativeCameraDirection::Forward);
  auto expected = source;
  replace(expected, "[00 1.25 2]", "[-10 1.25 2]");
  replace(expected, "activecamera \"01\"", "activecamera \"0\"");
  CHECK(scene.serialize(scene.world()) == expected);
  CHECK(scene.serialize(scene.world()) == expected);
  const auto reopened = VmfScene::read(expected, bounds);
  REQUIRE(reopened.cameras().records.size() == 2);
  CHECK(reopened.cameras().records[0].eye == vm::vec3d{-10, 1.25, 2});
  scene.applyCameraChange(*change, NativeCameraDirection::Backward);
  CHECK(scene.serialize(scene.world()) == source);
  scene.applyCameraChange(*change, NativeCameraDirection::Forward);
  CHECK(scene.serialize(scene.world()) == expected);
}
namespace
{
NativeCameraRecord newRecord()
{
  return {{}, {10, 20, 30}, {10, 21, 30}};
}
NativeCameraRequest createRequest(const NativeCameraSnapshot& snapshot)
{
  auto request = NativeCameraRequest{snapshot};
  request.present = true;
  request.records.push_back(newRecord());
  return request;
}
NativeCameraRequest clearRequest(const NativeCameraSnapshot& snapshot)
{
  auto request = NativeCameraRequest{snapshot};
  request.present = false;
  request.active = {};
  request.records.clear();
  return request;
}
PreparedNativeCameraChange apply(VmfScene& scene, const NativeCameraRequest& request)
{
  auto change = scene.prepareCameraChange(request);
  REQUIRE(change);
  scene.applyCameraChange(*change, NativeCameraDirection::Forward);
  return std::move(*change);
}
size_t rootsNamed(const VmfDocument& document, const std::string& name)
{
  size_t count = 0;
  for (const auto id : document.roots())
  {
    if (document.tokenContents(document.entries()[id].key) == name)
    {
      ++count;
    }
  }
  return count;
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
} // namespace
TEST_CASE("NativeCamera scene record CRUD active ordinals and exact undo")
{
  const auto source = prefix + cameras;
  auto scene = VmfScene::read(source, bounds);
  const auto original = scene.cameras();
  auto deletion = NativeCameraRequest{original};
  deletion.records.erase(deletion.records.begin());
  auto removed = apply(scene, deletion);
  auto saved = scene.serialize(scene.world());
  auto reopened = VmfScene::read(saved, bounds);
  REQUIRE(reopened.cameras().records.size() == 1);
  CHECK(reopened.cameras().records[0].eye == vm::vec3d{3, 4, 5});
  CHECK(reopened.cameras().active.camera == reopened.cameras().records[0].id);
  CHECK(saved.find("activecamera \"0\"") != std::string::npos);
  auto empty = NativeCameraRequest{scene.cameras()};
  empty.records.clear();
  CHECK_THROWS_AS(scene.prepareCameraChange(empty), std::invalid_argument);
  empty.active = {NativeCameraActiveKind::None, {}};
  auto emptied = apply(scene, empty);
  const auto emptyReopened = VmfScene::read(scene.serialize(scene.world()), bounds);
  CHECK(emptyReopened.cameras().present);
  CHECK(emptyReopened.cameras().records.empty());
  CHECK(emptyReopened.cameras().active.kind == NativeCameraActiveKind::None);
  scene.applyCameraChange(emptied, NativeCameraDirection::Backward);
  scene.applyCameraChange(removed, NativeCameraDirection::Backward);
  CHECK(scene.serialize(scene.world()) == source);
  auto add = NativeCameraRequest{scene.cameras()};
  add.records.push_back(newRecord());
  auto added = apply(scene, add);
  saved = scene.serialize(scene.world());
  CHECK(saved.find("activecamera \"01\"") != std::string::npos);
  auto choose = NativeCameraRequest{scene.cameras()};
  choose.active = {NativeCameraActiveKind::Camera, choose.records.back().id};
  auto chosen = apply(scene, choose);
  saved = scene.serialize(scene.world());
  CHECK(saved.find("activecamera \"2\"") != std::string::npos);
  auto absent = NativeCameraRequest{scene.cameras()};
  absent.active = {};
  auto noField = apply(scene, absent);
  CHECK(scene.serialize(scene.world()).find("activecamera") == std::string::npos);
  auto clear = apply(scene, clearRequest(scene.cameras()));
  CHECK(rootsNamed(VmfDocument::parse(scene.serialize(scene.world())), "cameras") == 0);
  auto fresh = apply(scene, createRequest(scene.cameras()));
  CHECK(scene.cameras().records[0].id != original.records[0].id);
  CHECK(rootsNamed(VmfDocument::parse(scene.serialize(scene.world())), "cameras") == 1);
  scene.applyCameraChange(fresh, NativeCameraDirection::Backward);
  scene.applyCameraChange(clear, NativeCameraDirection::Backward);
  scene.applyCameraChange(noField, NativeCameraDirection::Backward);
  scene.applyCameraChange(chosen, NativeCameraDirection::Backward);
  scene.applyCameraChange(added, NativeCameraDirection::Backward);
  CHECK(scene.serialize(scene.world()) == source);
}
TEST_CASE("NativeCamera scene creation composes with cordons and generated namespace")
{
  auto scene = VmfScene::read(prefix, bounds);
  auto control = VmfScene::read(prefix, bounds);
  auto camera = apply(scene, createRequest(scene.cameras()));
  auto cordon = NativeCordonRequest{scene.cordons()};
  cordon.form = NativeCordonForm::Modern;
  cordon.active = false;
  cordon.records.push_back({{}, "composed", false, {{{}, {-8, -8, -8}, {8, 8, 8}}}});
  auto cordonChange = scene.prepareCordonChange(cordon);
  REQUIRE(cordonChange);
  scene.applyCordonChange(*cordonChange, NativeCordonDirection::Forward);
  CHECK(addPoint(scene) == addPoint(control));
  const auto saved = scene.serialize(scene.world());
  const auto document = VmfDocument::parse(saved);
  CHECK(rootsNamed(document, "cameras") == 1);
  CHECK(rootsNamed(document, "cordons") == 1);
  CHECK(rootsNamed(document, "entity") == 2);
  CHECK(scene.document().source() == prefix);
  CHECK(scene.serialize(scene.world()) == saved);
  auto reopened = VmfScene::read(saved, bounds);
  REQUIRE(reopened.cameras().refusalReason.empty());
  CHECK(reopened.cameras().records[0].look == vm::vec3d{10, 21, 30});
  CHECK(reopened.cordons().records[0].name == "composed");
  scene.applyCameraChange(camera, NativeCameraDirection::Backward);
  CHECK(rootsNamed(VmfDocument::parse(scene.serialize(scene.world())), "cameras") == 0);
  CHECK(rootsNamed(VmfDocument::parse(scene.serialize(scene.world())), "cordons") == 1);
  scene.applyCameraChange(camera, NativeCameraDirection::Forward);
  CHECK(scene.serialize(scene.world()) == saved);
}
TEST_CASE("NativeCamera model scoped staging replay and refusal nonconsumption")
{
  auto scene = VmfScene::read(prefix + cameras, bounds);
  auto foreign = VmfScene::read(prefix + cameras, bounds);
  CHECK_FALSE(scene.prepareCameraChange(NativeCameraRequest{scene.cameras()}));
  auto request = NativeCameraRequest{scene.cameras()};
  request.records[0].eye = {-1, -2, -3};
  auto delayed = scene.prepareCameraChange(request);
  REQUIRE(delayed);
  auto acted = apply(scene, request);
  CHECK_THROWS_AS(
    foreign.applyCameraChange(acted, NativeCameraDirection::Forward),
    std::invalid_argument);
  scene.applyCameraChange(acted, NativeCameraDirection::Backward);
  CHECK_THROWS_AS(
    scene.applyCameraChange(*delayed, NativeCameraDirection::Forward),
    std::invalid_argument);
  const auto before = scene.serialize(scene.world());
  const auto revision = scene.cameras().revision;
  const auto refused = [&](const NativeCameraRequest& bad) {
    CHECK_THROWS_AS(scene.prepareCameraChange(bad), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == before);
    CHECK(scene.cameras().revision == revision);
  };
  refused(request);
  refused(NativeCameraRequest{foreign.cameras()});
  auto bad = NativeCameraRequest{scene.cameras()};
  bad.records[0].id = foreign.cameras().records[0].id;
  refused(bad);
  bad = NativeCameraRequest{scene.cameras()};
  bad.records.push_back(bad.records[0]);
  refused(bad);
  bad = NativeCameraRequest{scene.cameras()};
  std::swap(bad.records[0], bad.records[1]);
  refused(bad);
  bad = NativeCameraRequest{scene.cameras()};
  bad.records.insert(bad.records.begin(), newRecord());
  refused(bad);
  auto candidate = scene.prepareCameraChange(createRequest(scene.cameras()));
  REQUIRE(candidate);
  bad = createRequest(scene.cameras());
  bad.records.push_back(newRecord());
  bad.records.back().eye[0] = std::numeric_limits<double>::quiet_NaN();
  refused(bad);
  auto accepted = scene.prepareCameraChange(createRequest(scene.cameras()));
  REQUIRE(accepted);
  CHECK(accepted->after().records.back().id == candidate->after().records.back().id);
  scene.applyCameraChange(*accepted, NativeCameraDirection::Forward);
  scene.applyCameraChange(*accepted, NativeCameraDirection::Backward);
  CHECK_THROWS_AS(
    scene.applyCameraChange(*candidate, NativeCameraDirection::Forward),
    std::invalid_argument);
  auto next = apply(scene, createRequest(scene.cameras()));
  CHECK(next.after().records.back().id != accepted->after().records.back().id);
  scene.applyCameraChange(next, NativeCameraDirection::Backward);
  auto moved = std::move(scene);
  moved.applyCameraChange(acted, NativeCameraDirection::Forward);
  moved.applyCameraChange(acted, NativeCameraDirection::Backward);
  CHECK(moved.serialize(moved.world()) == before);
}
TEST_CASE("NativeCamera unsupported controller protects metadata not entity edits")
{
  const auto bad = GENERATE(
    "cameras {} cameras {}",
    "Cameras {}",
    "cameras scalar",
    "cameras { activecamera -2 }",
    "cameras { activecamera 999999999999999999999999999999 }",
    "cameras { activecamera 0 activecamera -1 }",
    "cameras { activecamera {} }",
    "cameras { future keep }",
    "cameras { camera scalar }",
    "cameras { camera { position \"[0 0 0]\" } }",
    "cameras { camera { position \"[0 0 0]\" look {} } }",
    "cameras { camera { position \"[0 0 0]\" position \"[0 0 0]\" look \"[0 1 0]\" } }",
    "cameras { camera { position \"[0 0 0]\" look \"[0 1 0]\" future keep } }",
    "cameras { camera { position \"[0 0 0]\" look \"[0 1 0]\" future {} } }",
    "cameras { camera { position \"[0 0 0]\" look \"[0 0 0]\" } }",
    "cameras { camera { position \"[nan 0 0]\" look \"[0 1 0]\" } }",
    "cameras { camera { position \"[0 0 0]\" look \"[1e999 1 0]\" } }",
    "cameras { activecamera 1 camera { position \"[0 0 0]\" look \"[0 1 0]\" } }");
  CAPTURE(bad);
  const auto source = prefix + bad;
  auto scene = VmfScene::read(source, bounds);
  CHECK_FALSE(scene.cameras().refusalReason.empty());
  CHECK(scene.serialize(scene.world()) == source);
  CHECK_THROWS_AS(
    scene.prepareCameraChange(createRequest(scene.cameras())), std::invalid_argument);
  auto* entity =
    dynamic_cast<EntityNode*>(scene.world().defaultLayer()->children().front());
  REQUIRE(entity);
  auto value = entity->entity();
  value.addOrUpdateProperty("targetname", "independent");
  entity->setEntity(std::move(value));
  auto expected = source;
  replace(expected, "\"before\"", "\"independent\"");
  CHECK(scene.serialize(scene.world()) == expected);
}
TEST_CASE("NativeCamera admission preview version unknown outside and bounded records")
{
  SECTION("preview")
  {
    auto scene = VmfScene::readReadOnlyPreview(prefix + cameras, bounds);
    CHECK_FALSE(scene.cameras().refusalReason.empty());
    CHECK_THROWS_AS(
      scene.prepareCameraChange(clearRequest(scene.cameras())), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == prefix + cameras);
  }
  SECTION("version")
  {
    auto source = prefix + cameras;
    replace(source, "formatversion 100", "formatversion 101");
    auto scene = VmfScene::read(source, bounds);
    CHECK_FALSE(scene.cameras().refusalReason.empty());
    CHECK_THROWS_AS(
      scene.prepareCameraChange(clearRequest(scene.cameras())), std::invalid_argument);
    CHECK(scene.serialize(scene.world()) == source);
  }
  SECTION("unknown outside and bounded input")
  {
    const auto outside =
      std::string{"future { output one output two opaque { arbitrary 7 } }\r\n"};
    auto scene = VmfScene::read(prefix + outside, bounds);
    auto request = createRequest(scene.cameras());
    request.records.assign(NativeCameras::MaxRecords, newRecord());
    REQUIRE(scene.prepareCameraChange(request));
    request.records.push_back(newRecord());
    CHECK_THROWS_AS(scene.prepareCameraChange(request), std::invalid_argument);
    auto created = apply(scene, createRequest(scene.cameras()));
    const auto saved = scene.serialize(scene.world());
    CHECK(saved.find(outside) != std::string::npos);
    CHECK(VmfScene::read(saved, bounds).cameras().refusalReason.empty());
    scene.applyCameraChange(created, NativeCameraDirection::Backward);
    CHECK(scene.serialize(scene.world()) == prefix + outside);
  }
}
} // namespace tb::mdl
