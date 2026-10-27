/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fs/TestEnvironment.h"
#include "mdl/Map.h"
#include "mdl/MapFixture.h"
#include "mdl/Map_Entities.h"
#include "mdl/Map_NativeCameras.h"
#include "mdl/Map_Selection.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"

#include <fstream>
#include <iterator>
#include <limits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
namespace tb::mdl
{
TEST_CASE("NativeCamera history actual dispatch preserves saved original", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto original = std::string{
    "\xef\xbb\xbf// native camera history\r\n"
    "world { id 1 classname worldspawn }\r\n"
    "cameras { activecamera 0 camera { position \"[1 2 3]\" look \"[1 3 3]\" } }\r\n"};
  const auto path = env.dir() / "source.vmf";
  {
    auto stream = std::ofstream{path, std::ios::binary};
    stream << original;
    REQUIRE(stream);
  }
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  selectNodes(map, {&map.worldNode()});
  REQUIRE(setEntityProperty(map, "message", "acted-camera-control"));
  CHECK(
    map.vmfScene()->serialize(map.worldNode()).find("acted-camera-control")
    != std::string::npos);
  map.undoCommand();
  CHECK_FALSE(map.modified());
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  auto request = NativeCameraRequest{nativeCameras(map)};
  REQUIRE(request.records.size() == 1);
  request.records[0].look = {5, 6, 7};
  const auto count = map.modificationCount();
  REQUIRE(applyNativeCameras(map, request).status == NativeCameraApplyStatus::Changed);
  CHECK(map.modificationCount() == count + 1);
  CHECK(map.modified());
  const auto expected = map.vmfScene()->serialize(map.worldNode());
  CHECK(expected.find("[5 6 7]") != std::string::npos);
  const auto output = env.dir() / "edited.vmf";
  REQUIRE(map.saveAs(output).is_success());
  auto stream = std::ifstream{path, std::ios::binary};
  CHECK(
    std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}}
    == original);
  map.undoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == original);
  map.redoCommand();
  CHECK(map.vmfScene()->serialize(map.worldNode()) == expected);
  CHECK_FALSE(map.modified());
}
namespace
{
const auto cameraSource = std::string{
  "versioninfo { formatversion 100 }\r\n"
  "world { id 1 classname worldspawn }\r\n"
  "cameras { activecamera 0 camera { position \"[1 2 3]\" look \"[1 3 3]\" } }\r\n"};
void write(const std::filesystem::path& path, const std::string& bytes)
{
  auto stream = std::ofstream{path, std::ios::binary};
  stream << bytes;
  REQUIRE(stream);
}
std::string serialize(const Map& map)
{
  return map.vmfScene()->serialize(map.worldNode());
}
} // namespace
TEST_CASE("NativeCamera history no-op and refusal preserve pending redo", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "source.vmf";
  write(path, cameraSource);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  auto request = NativeCameraRequest{nativeCameras(map)};
  request.records[0].eye = {3, 4, 5};
  REQUIRE(applyNativeCameras(map, request).status == NativeCameraApplyStatus::Changed);
  const auto edited = serialize(map);
  map.undoCommand();
  REQUIRE(map.redoCommandName());
  const auto count = map.modificationCount();
  CHECK(
    applyNativeCameras(map, NativeCameraRequest{nativeCameras(map)}).status
    == NativeCameraApplyStatus::Unchanged);
  auto bad = NativeCameraRequest{nativeCameras(map)};
  bad.records[0].look[2] = std::numeric_limits<double>::infinity();
  CHECK(applyNativeCameras(map, bad).status == NativeCameraApplyStatus::Refused);
  CHECK(applyNativeCameras(map, request).status == NativeCameraApplyStatus::Refused);
  CHECK(map.redoCommandName());
  CHECK(map.modificationCount() == count);
  CHECK(serialize(map) == cameraSource);
  map.redoCommand();
  CHECK(serialize(map) == edited);
}
TEST_CASE("NativeCamera history CRUD active selection save reopen twice", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "source.vmf";
  write(path, cameraSource);
  auto fixture = MapFixture{};
  auto& map = fixture.load(path, MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  auto request = NativeCameraRequest{nativeCameras(map)};
  request.records.push_back({{}, {10, 20, 30}, {10, 21, 30}});
  REQUIRE(applyNativeCameras(map, request).status == NativeCameraApplyStatus::Changed);
  request = NativeCameraRequest{nativeCameras(map)};
  const auto addedId = request.records.back().id;
  request.active = {NativeCameraActiveKind::Camera, addedId};
  REQUIRE(applyNativeCameras(map, request).status == NativeCameraApplyStatus::Changed);
  request = NativeCameraRequest{nativeCameras(map)};
  request.records.erase(request.records.begin());
  REQUIRE(applyNativeCameras(map, request).status == NativeCameraApplyStatus::Changed);
  CHECK(nativeCameras(map).active.camera == addedId);
  const auto saved = serialize(map);
  const auto output = env.dir() / "copy.vmf";
  REQUIRE(map.saveAs(output).is_success());
  REQUIRE(map.save().is_success());
  CHECK(serialize(map) == saved);
  auto reopenedFixture = MapFixture{};
  auto& reopened =
    reopenedFixture.load(output, MapFixtureConfig{.mapFormat = MapFormat::SourceVMF});
  REQUIRE(nativeCameras(reopened).records.size() == 1);
  CHECK(nativeCameras(reopened).active.camera == nativeCameras(reopened).records[0].id);
  CHECK(nativeCameras(reopened).records[0].eye == vm::vec3d{10, 20, 30});
  CHECK(serialize(reopened) == saved);
  request = NativeCameraRequest{nativeCameras(map)};
  request.present = false;
  request.active = {};
  request.records.clear();
  REQUIRE(applyNativeCameras(map, request).status == NativeCameraApplyStatus::Changed);
  CHECK_FALSE(nativeCameras(map).present);
  map.undoCommand();
  CHECK(serialize(map) == saved);
  CHECK_FALSE(map.modified());
  auto stream = std::ifstream{path, std::ios::binary};
  CHECK(
    std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}}
    == cameraSource);
}
TEST_CASE("NativeCamera history preview and unsupported version remain refused", "[vmf]")
{
  const auto preview = GENERATE(false, true);
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "restricted.vmf";
  auto source = cameraSource;
  if (!preview)
  {
    source.replace(source.find("100"), 3, "999");
  }
  write(path, source);
  auto fixture = MapFixture{};
  auto config = MapFixtureConfig{.mapFormat = MapFormat::SourceVMF};
  config.readOnlyVmfPreview = preview;
  auto& map = fixture.load(path, config);
  CHECK_FALSE(nativeCameras(map).refusalReason.empty());
  auto request = NativeCameraRequest{nativeCameras(map)};
  request.records[0].eye = {-1, -2, -3};
  CHECK(applyNativeCameras(map, request).status == NativeCameraApplyStatus::Refused);
  CHECK(serialize(map) == source);
  CHECK_FALSE(map.modified());
  CHECK_FALSE(map.undoCommandName());
}
} // namespace tb::mdl
