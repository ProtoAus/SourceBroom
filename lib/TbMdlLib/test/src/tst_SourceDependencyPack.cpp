/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fs/DiskIO.h"
#include "fs/File.h"
#include "fs/ZipFileSystem.h"
#include "mdl/SourceDependencyPack.h"
#include "mdl/VmfDocument.h"

#include "kd/result.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
namespace disk = std::filesystem;

struct PackFixture
{
  disk::path base;
  SourceDependencyPackOptions options;

  PackFixture()
  {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    for (size_t i = 0; i < 100; ++i)
    {
      auto candidate =
        disk::temp_directory_path()
        / ("sourcebroom-pack-" + std::to_string(stamp) + "-" + std::to_string(i));
      if (disk::create_directory(candidate))
      {
        base = std::move(candidate);
        break;
      }
    }
    if (base.empty())
    {
      throw std::runtime_error("Cannot create synthetic pack fixture");
    }
    disk::create_directory(base / "custom");
    options.customRoots = {base / "custom"};
    options.redistributionRightsAcknowledged = true;
  }
  ~PackFixture()
  {
    std::error_code error;
    disk::remove_all(base, error);
  }
  void put(const std::string& relative, const std::string_view bytes) const
  {
    const auto path = base / relative;
    disk::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), std::streamsize(bytes.size()));
    if (!file)
    {
      throw std::runtime_error("Cannot write synthetic fixture");
    }
  }
};

bool hasIssue(
  const SourceDependencyPackPlan& plan,
  SourceDependencyPackIssueKind kind,
  std::string_view reference = {})
{
  return std::any_of(plan.issues().begin(), plan.issues().end(), [&](const auto& issue) {
    return issue.kind == kind && (reference.empty() || issue.reference == reference);
  });
}

std::vector<std::string> names(const SourceDependencyPackPlan& plan)
{
  std::vector<std::string> result;
  for (const auto& file : plan.files())
  {
    result.push_back(file.archivePath);
  }
  return result;
}

std::string binaryRead(const disk::path& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file)
  {
    throw std::runtime_error("Cannot read synthetic ZIP");
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Independent production ZIP reader verifies central directory, CRC and extraction.
std::string extracted(const disk::path& zip, const std::string& path)
{
  auto file = fs::Disk::openFile(zip) | kdl::value();
  auto archive = fs::createImageFileSystem<fs::ZipFileSystem>(file) | kdl::value();
  auto entry = archive->openFile(path) | kdl::value();
  auto reader = entry->reader();
  std::string result(entry->size(), '\0');
  reader.read(result.data(), result.size());
  return result;
}

void put32(std::string& bytes, const size_t offset, const uint32_t value)
{
  for (size_t i = 0; i < 4; ++i)
  {
    bytes.at(offset + i) = char((value >> (8u * i)) & 255u);
  }
}
} // namespace

TEST_CASE("SourceDependencyPack")
{
  PackFixture fixture;
  const auto vmf = VmfDocument::parse(R"(world { "material" "custom/start" })");
  fixture.put(
    "custom/materials/custom/start.vmt",
    R"(LightmappedGeneric { "$basetexture" "custom/pixels" })");
  const auto pixels = std::string("VTF\0synthetic\0\xff\x80", 16);
  fixture.put("custom/materials/custom/pixels.vtf", pixels);

  SECTION("default deny and separate approval leave all originals untouched")
  {
    auto denied = fixture.options;
    denied.redistributionRightsAcknowledged = false;
    denied.customRoots = {fixture.base / "does-not-exist"};
    const auto plan = planSourceDependencyPack(vmf, denied);
    CHECK(plan.files().empty());
    CHECK_FALSE(plan.canWrite());
    CHECK(hasIssue(plan, SourceDependencyPackIssueKind::Excluded, "rights"));
    CHECK_FALSE(plan.writeZip(fixture.base / "denied.zip", true).written);
    const auto approved = planSourceDependencyPack(vmf, fixture.options);
    CHECK(approved.canWrite());
    CHECK_FALSE(approved.writeZip(fixture.base / "not-approved.zip").written);
    CHECK_FALSE(disk::exists(fixture.base / "not-approved.zip"));
    CHECK(vmf.serialize() == vmf.source());
    CHECK(binaryRead(fixture.base / "custom/materials/custom/pixels.vtf") == pixels);
  }
  SECTION(
    "transitive patches cycles repeated refs and binary extraction are deterministic")
  {
    fixture.put(
      "custom/materials/custom/start.vmt",
      R"(patch { include "custom/second.vmt" insert { "$basetexture" "custom/pixels" } })");
    fixture.put(
      "custom/materials/custom/second.vmt", R"(patch { include "custom/start.vmt" })");
    fixture.put("custom/sound/custom/beep.wav", std::string("RIFF\0WAVE\0", 10));
    const auto graph = VmfDocument::parse(
      R"(world { "material" "CUSTOM/start" "material" "custom/start" } entity { "noise" "custom/beep.wav" "message" "script.name" })");
    const auto plan = planSourceDependencyPack(graph, fixture.options);
    REQUIRE(plan.canWrite());
    CHECK_FALSE(plan.assessmentComplete());
    CHECK(
      names(plan)
      == std::vector<std::string>{
        "materials/custom/pixels.vtf",
        "materials/custom/second.vmt",
        "materials/custom/start.vmt",
        "sound/custom/beep.wav"});
    CHECK(hasIssue(plan, SourceDependencyPackIssueKind::Unresolved, "script.name"));
    const auto first = fixture.base / "first.zip";
    const auto second = fixture.base / "second.zip";
    REQUIRE(plan.writeZip(first, true).written);
    REQUIRE(plan.writeZip(second, true).written);
    CHECK(binaryRead(first) == binaryRead(second));
    CHECK(extracted(first, "materials/custom/pixels.vtf") == pixels);
    CHECK(extracted(first, "sound/custom/beep.wav") == std::string("RIFF\0WAVE\0", 10));
    CHECK(
      extracted(first, "materials/custom/second.vmt")
      == R"(patch { include "custom/start.vmt" })");
    CHECK_FALSE(plan.writeZip(first, true).written);
    CHECK(binaryRead(first) == binaryRead(second));
    // Review snapshots, not a mutable filesystem manifest.
    fixture.put("custom/materials/custom/pixels.vtf", "changed after review");
    REQUIRE(plan.writeZip(fixture.base / "snapshot.zip", true).written);
    CHECK(
      extracted(fixture.base / "snapshot.zip", "materials/custom/pixels.vtf") == pixels);
  }
  SECTION("ZIP CRC known answer and exact budget boundaries")
  {
    fixture.put("custom/sound/custom/check.wav", "123456789");
    const auto refs = VmfDocument::parse(R"(entity { "noise" "custom/check.wav" })");
    fixture.options.limits.maxFileBytes = 9;
    fixture.options.limits.maxTotalBytes = 9;
    fixture.options.limits.maxFiles = 1;
    fixture.options.limits.maxReferences = 1;
    const auto plan = planSourceDependencyPack(refs, fixture.options);
    REQUIRE(plan.canWrite());
    REQUIRE(plan.files().size() == 1);
    CHECK(plan.files().front().crc32 == 0xcbf43926u);
    REQUIRE(plan.writeZip(fixture.base / "crc.zip", true).written);
    CHECK(extracted(fixture.base / "crc.zip", "sound/custom/check.wav") == "123456789");
  }
  SECTION("missing stock outside selected roots is never searched or included")
  {
    fixture.put(
      "installed/materials/stock/floor.vmt",
      R"(shader { "$basetexture" "stock/pixels" })");
    fixture.put(
      "installed/materials/stock/pixels.vtf", "stock-placeholder-authored-test-only");
    const auto refs = VmfDocument::parse(
      R"(world { "material" "stock/floor" "material" "custom/start" })");
    const auto plan = planSourceDependencyPack(refs, fixture.options);
    CHECK(hasIssue(
      plan, SourceDependencyPackIssueKind::Missing, "materials/stock/floor.vmt"));
    CHECK(
      names(plan)
      == std::vector<std::string>{
        "materials/custom/pixels.vtf", "materials/custom/start.vmt"});
    fixture.options.customRoots = {fixture.base / "steamapps/common/fake"};
    disk::create_directories(fixture.options.customRoots.front());
    CHECK_FALSE(planSourceDependencyPack(refs, fixture.options).canWrite());
  }
  SECTION("escape absolute drive stream reserved and extraction aliases are excluded")
  {
    const auto refs = VmfDocument::parse(R"(world {
      "material" "../escape" "material" "/absolute" "material" "C:/drive"
      "material" "custom/file:stream" "material" "custom/../escape"
      "material" "custom//empty" "material" "custom/trailing."
      "material" "custom/NUL" "material" "custom/start"
    })");
    const auto plan = planSourceDependencyPack(refs, fixture.options);
    CHECK(plan.canWrite());
    CHECK(
      names(plan)
      == std::vector<std::string>{
        "materials/custom/pixels.vtf", "materials/custom/start.vmt"});
    CHECK(
      std::count_if(
        plan.issues().begin(),
        plan.issues().end(),
        [](const auto& issue) {
          return issue.kind == SourceDependencyPackIssueKind::Excluded;
        })
      == 8);
    CHECK_FALSE(plan.writeZip(fixture.base / "unsafe.zip:stream", true).written);
    CHECK_FALSE(disk::exists(fixture.base / "unsafe.zip:stream"));
  }
  SECTION(
    "case insensitive resolution and cross-root duplicates refuse ambiguous precedence")
  {
    fixture.put("other/materials/custom/start.vmt", "shader {}");
    fixture.options.customRoots.push_back(fixture.base / "other");
    const auto plan = planSourceDependencyPack(vmf, fixture.options);
    CHECK_FALSE(plan.canWrite());
    CHECK(
      hasIssue(plan, SourceDependencyPackIssueKind::Error, "materials/custom/start.vmt"));
  }
  SECTION("case variants resolve to one portable lowercase archive path")
  {
    fixture.put("custom/materials/Other/Mixed.vmt", "shader {}");
    const auto refs = VmfDocument::parse(R"(world { "material" "other/MIXED" })");
    const auto plan = planSourceDependencyPack(refs, fixture.options);
    REQUIRE(plan.canWrite());
    CHECK(names(plan) == std::vector<std::string>{"materials/other/mixed.vmt"});
  }
  SECTION("all byte file count and graph budgets block ZIP creation")
  {
    SECTION("per-file")
    {
      fixture.options.limits.maxFileBytes = 8;
    }
    SECTION("aggregate")
    {
      fixture.options.limits.maxTotalBytes = 64;
    }
    SECTION("file count")
    {
      fixture.options.limits.maxFiles = 1;
    }
    SECTION("graph")
    {
      fixture.options.limits.maxReferences = 1;
    }
    SECTION("invalid ZIP32 limits")
    {
      fixture.options.limits.maxTotalBytes = uint64_t{1} << 32u;
    }
    const auto plan = planSourceDependencyPack(vmf, fixture.options);
    CHECK_FALSE(plan.canWrite());
    CHECK_FALSE(plan.writeZip(fixture.base / "over-budget.zip", true).written);
    CHECK_FALSE(disk::exists(fixture.base / "over-budget.zip"));
  }
  SECTION("symlinks in asset roots asset ancestors and output ancestors refuse")
  {
    fixture.put("outside/materials/custom/start.vmt", "shader {}");
    std::error_code error;
    disk::create_directory_symlink(
      fixture.base / "outside", fixture.base / "linked", error);
    if (error)
    {
      WARN(
        "Symlink control unavailable (Windows privilege/developer mode); parent must "
        "exercise reparse-point control separately");
    }
    else
    {
      auto linked = fixture.options;
      linked.customRoots = {fixture.base / "linked"};
      CHECK_FALSE(planSourceDependencyPack(vmf, linked).canWrite());
      const auto plan = planSourceDependencyPack(vmf, fixture.options);
      CHECK_FALSE(plan.writeZip(fixture.base / "linked/output.zip", true).written);
      CHECK_FALSE(disk::exists(fixture.base / "outside/output.zip"));
      const auto redirected =
        VmfDocument::parse(R"(entity { "model" "redirect/custom/start.mdl" })");
      // An in-root directory link must fail even before a final file is found.
      disk::create_directories(fixture.base / "custom/models");
      disk::create_directory_symlink(
        fixture.base / "outside/materials",
        fixture.base / "custom/models/redirect",
        error);
      REQUIRE_FALSE(error);
      CHECK_FALSE(planSourceDependencyPack(redirected, fixture.options).canWrite());
      CHECK(hasIssue(
        planSourceDependencyPack(redirected, fixture.options),
        SourceDependencyPackIssueKind::Error));
      disk::remove(fixture.base / "custom/materials/custom/pixels.vtf");
      disk::create_symlink(
        fixture.base / "outside/materials/custom/start.vmt",
        fixture.base / "custom/materials/custom/pixels.vtf",
        error);
      REQUIRE_FALSE(error);
      CHECK_FALSE(planSourceDependencyPack(vmf, fixture.options).canWrite());
    }
  }
  SECTION("explicit BSP is copied read-only and validity is not certified")
  {
    auto bsp = std::string(1036, '\0');
    bsp.replace(0, 4, "VBSP");
    put32(bsp, 4, 20);
    fixture.put("outputs/SafeMap.bsp", bsp);
    fixture.options.compiledBsp = fixture.base / "outputs/SafeMap.bsp";
    fixture.options.compiledBspExpectedVersion = 20;
    const auto plan = planSourceDependencyPack(vmf, fixture.options);
    REQUIRE(plan.canWrite());
    REQUIRE(plan.writeZip(fixture.base / "bsp.zip", true).written);
    CHECK(extracted(fixture.base / "bsp.zip", "maps/safemap.bsp") == bsp);
    CHECK(binaryRead(*fixture.options.compiledBsp) == bsp);
    CHECK_FALSE(plan.assessmentComplete());
  }
  SECTION("complete BSP archive path boundary is checked during planning")
  {
    // Pure policy controls do not depend on the host's legacy Windows MAX_PATH.
    // The separate real-BSP ZIP control above proves snapshot/write/extraction.
    const auto validName = std::string(231, 'a') + ".bsp";
    CHECK(
      sourceDependencyBspArchivePath(fixture.base / "outputs" / validName)
      == "maps/" + validName);
    const auto invalidName = std::string(232, 'a') + ".bsp";
    const auto candidate = fixture.base / "outputs" / invalidName;
    CHECK_THROWS_AS(sourceDependencyBspArchivePath(candidate), std::runtime_error);
    fixture.options.compiledBsp = candidate;
    const auto plan = planSourceDependencyPack(vmf, fixture.options);
    CHECK_FALSE(plan.canWrite());
    CHECK(std::any_of(plan.issues().begin(), plan.issues().end(), [](const auto& issue) {
      return issue.kind == SourceDependencyPackIssueKind::Error
             && issue.message == "Empty/long path";
    }));
    CHECK_FALSE(plan.writeZip(fixture.base / "boundary-invalid.zip", true).written);
    CHECK_FALSE(disk::exists(fixture.base / "boundary-invalid.zip"));
    CHECK_FALSE(disk::exists(candidate));
  }
  SECTION("selected BSP snapshot itself must meet header and target contract")
  {
    auto wrongVersion = std::string(1036, '\0');
    wrongVersion.replace(0, 4, "VBSP");
    put32(wrongVersion, 4, 25);
    for (const auto& bad : {std::string{"VBSP"}, wrongVersion, std::string(1036, 'x')})
    {
      fixture.put("outputs/Rejected.bsp", bad);
      fixture.options.compiledBsp = fixture.base / "outputs/Rejected.bsp";
      fixture.options.compiledBspExpectedVersion = 20;
      const auto plan = planSourceDependencyPack(vmf, fixture.options);
      CHECK_FALSE(plan.canWrite());
      CHECK(hasIssue(plan, SourceDependencyPackIssueKind::Error));
      CHECK_FALSE(plan.writeZip(fixture.base / "must-not-exist.zip", true).written);
      CHECK_FALSE(disk::exists(fixture.base / "must-not-exist.zip"));
      CHECK(binaryRead(*fixture.options.compiledBsp) == bad);
    }
  }
  SECTION("classic model material tables includes and binary sidecars are discovered")
  {
    // Original synthetic IDST dependency tables only; not a Valve model or SDK fixture.
    std::string mdl(640, '\0');
    mdl.replace(0, 4, "IDST");
    put32(mdl, 4, 48);
    put32(mdl, 76, uint32_t(mdl.size()));
    put32(mdl, 204, 1);
    put32(mdl, 208, 408);
    put32(mdl, 212, 1);
    put32(mdl, 216, 472);
    put32(mdl, 472, 480);
    mdl.replace(480, 14, "models/custom/");
    put32(mdl, 408, 100);
    mdl.replace(508, 5, "paint");
    put32(mdl, 336, 1);
    put32(mdl, 340, 520);
    put32(mdl, 524, 20);
    mdl.replace(540, 21, "models/custom/sub.mdl");
    fixture.put("custom/models/custom/prop.mdl", mdl);
    fixture.put("custom/models/custom/prop.vvd", std::string("VVD\0body", 8));
    fixture.put("custom/models/custom/prop.dx90.vtx", std::string("VTX\0body", 8));
    fixture.put(
      "custom/materials/models/custom/paint.vmt",
      R"(shader { "$basetexture" "custom/pixels" })");
    fixture.put("custom/models/custom/sub.mdl", "unsupported-authored-model");
    const auto refs =
      VmfDocument::parse(R"(entity { "model" "models/custom/prop.mdl" })");
    const auto plan = planSourceDependencyPack(refs, fixture.options);
    REQUIRE(plan.canWrite());
    const auto paths = names(plan);
    CHECK(std::find(paths.begin(), paths.end(), "models/custom/sub.mdl") != paths.end());
    CHECK(
      std::find(paths.begin(), paths.end(), "materials/models/custom/paint.vmt")
      != paths.end());
    CHECK(
      std::find(paths.begin(), paths.end(), "materials/custom/pixels.vtf")
      != paths.end());
    CHECK(
      hasIssue(plan, SourceDependencyPackIssueKind::Unresolved, "models/custom/sub.mdl"));
    CHECK(
      hasIssue(plan, SourceDependencyPackIssueKind::Missing, "models/custom/prop.phy"));
    REQUIRE(plan.writeZip(fixture.base / "model.zip", true).written);
    CHECK(extracted(fixture.base / "model.zip", "models/custom/prop.mdl") == mdl);
    CHECK(
      extracted(fixture.base / "model.zip", "models/custom/prop.dx90.vtx")
      == std::string("VTX\0body", 8));
  }
  SECTION("unparsed VMT and shader fields are visible incomplete assessments")
  {
    fixture.put("custom/materials/custom/start.vmt", "malformed {");
    auto plan = planSourceDependencyPack(vmf, fixture.options);
    CHECK(hasIssue(
      plan, SourceDependencyPackIssueKind::Unresolved, "materials/custom/start.vmt"));
    fixture.put(
      "custom/materials/custom/start.vmt",
      R"(shader { "$unknowntexture" "not-assessed" })");
    plan = planSourceDependencyPack(vmf, fixture.options);
    CHECK(hasIssue(
      plan,
      SourceDependencyPackIssueKind::Unresolved,
      "materials/custom/start.vmt:$unknowntexture"));
    CHECK_FALSE(plan.assessmentComplete());
  }
}
} // namespace tb::mdl
