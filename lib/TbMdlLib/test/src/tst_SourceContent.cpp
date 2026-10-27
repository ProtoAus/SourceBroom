/*
 Copyright (C) 2026 SourceBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

/* Synthetic Source content controls; no installed assets are redistributed. */
#include "base/Logger.h"
#include "fs/File.h"
#include "fs/TestEnvironment.h"
#include "gl/Material.h"
#include "gl/MaterialCollection.h"
#include "gl/MaterialManager.h"
#include "gl/Resource.h"
#include "gl/ResourceManager.h"
#include "gl/Texture.h"
#include "mdl/EnvironmentConfig.h"
#include "mdl/GameConfig.h"
#include "mdl/GameFileSystem.h"
#include "mdl/GameInfo.h"
#include "mdl/LoadMaterialCollections.h"
#include "mdl/LoadTexture.h"
#include "mdl/Map.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_Assets.h"
#include "mdl/SourceContent.h"

#include "kd/result.h"
#include "kd/task_manager.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_set>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
TEST_CASE("Source terrain material requires an explicit bounded blend preview mode")
{
  const auto ordinary = sourceMaterial(
    R"(LightmappedGeneric { $basetexture ground/low $nocull 1 })",
    SourceMaterialMode::BaseTexturePreview);
  REQUIRE(ordinary.is_success());
  const auto one = ordinary | kdl::value();
  CHECK(one.baseTexture == "materials/ground/low.vtf");
  CHECK(one.noCull);
  CHECK_FALSE(one.secondaryBaseTexture.has_value());
  const auto text = std::string{
    R"(WorldVertexTransition { $basetexture ground/low $basetexture2 ground/high })"};
  CHECK(sourceMaterial(text, SourceMaterialMode::Strict).is_error());
  CHECK(sourceMaterial(text, SourceMaterialMode::BaseTexturePreview).is_error());
  const auto result = sourceMaterial(text, SourceMaterialMode::TerrainBlendPreview);
  REQUIRE(result.is_success());
  const auto info = result | kdl::value();
  CHECK(info.baseTexture == "materials/ground/low.vtf");
  REQUIRE(info.secondaryBaseTexture.has_value());
  CHECK(*info.secondaryBaseTexture == "materials/ground/high.vtf");
  CHECK(info.alphaDomain == img::ImageAlphaDomain::Opaque);
  CHECK(sourceMaterial(
          R"(LightmappedGeneric { $basetexture ground/low $nocull 1 })",
          SourceMaterialMode::TerrainBlendPreview)
          .is_success());
  for (
    const auto* invalid :
    {R"(WorldVertexTransition { $basetexture ground/low })",
     R"(WorldVertexTransition { $basetexture ground/low $basetexture2 "" })",
     R"(WorldVertexTransition { $basetexture ground/low $basetexture2 a $basetexture2 b })",
     R"(WorldVertexTransition { $basetexture ground/low $basetexture2 ../outside })",
     R"(WorldVertexTransition { $basetexture ground/low $basetexture2 C:/outside })",
     R"(WorldVertexTransition { $basetexture ground/low $basetexture2 { nested yes } })",
     R"(WorldVertexTransition { $basetexture ground/low $basetexture2 a $blendmodulatetexture mask })",
     R"(WorldVertexTransition { $basetexture ground/low $basetexture2 a $basetexturetransform2 "center 0 0" })",
     R"(WorldVertexTransition { $basetexture ground/low $basetexture2 a Proxies {} })",
     R"(LightmappedGeneric { $basetexture ground/low $basetexture2 a })"})
  {
    CAPTURE(invalid);
    CHECK(sourceMaterial(invalid, SourceMaterialMode::TerrainBlendPreview).is_error());
  }
}

TEST_CASE("Source terrain demand owns two VTF layers within one retained budget")
{
  auto env = fs::TestEnvironment{};
  env.createDirectory("materials/ground");
  env.createFile(
    "gameinfo.txt", "GameInfo { FileSystem { SearchPaths { Game |gameinfo_path|. } } }");
  const auto vtf = [](
                     const unsigned char r,
                     const unsigned char g,
                     const unsigned char b,
                     const unsigned char a) {
    auto data = std::string(84, '\0');
    const auto put =
      [&](const size_t offset, const uint32_t value, const size_t bytes = 4) {
        for (size_t i = 0; i < bytes; ++i)
        {
          data[offset + i] = static_cast<char>(value >> (i * 8));
        }
      };
    put(0, 0x00465456);
    put(4, 7);
    put(8, 2);
    put(12, 80);
    put(16, 1, 2);
    put(18, 1, 2);
    put(20, 0x2000);
    put(24, 1, 2);
    put(52, 0);
    put(56, 1, 1);
    put(63, 1, 2);
    data[80] = static_cast<char>(r);
    data[81] = static_cast<char>(g);
    data[82] = static_cast<char>(b);
    data[83] = static_cast<char>(a);
    return data;
  };
  env.createFile("materials/ground/low.vtf", vtf(255, 0, 0, 255));
  env.createFile("materials/ground/high.vtf", vtf(0, 255, 0, 17));
  env.createFile(
    "materials/ordinary.vmt", "LightmappedGeneric { $basetexture ground/low }");
  const auto source = std::string{R"(WorldVertexTransition {
    $basetexture ground/low $basetexture2 ground/high $surfaceprop2 dirt
  })"};
  env.createFile("materials/terrain.vmt", source);
  env.createFile("materials/patch.vmt", R"(Patch {
    include materials/terrain.vmt replace { $basetexture2 ground/low }
  })");
  auto fs = GameFileSystem{};
  auto logger = NullLogger{};
  REQUIRE(fs.initializeSource(env.dir(), logger).is_success());
  const auto beforeFile = fs.openFile("materials/terrain.vmt") | kdl::value();
  const auto before = beforeFile->reader().readString(beforeFile->size());
  auto single = loadTexture("materials/ordinary.vmt", "ordinary", fs, std::nullopt, 4);
  REQUIRE(single.is_success());
  const auto one = std::move(single) | kdl::value();
  CHECK(one.buffersIfLoaded()[0].data()[0] == 255);
  CHECK(one.sourceBlendTexture() == nullptr);
  auto raw = loadTexture("materials/ground/high.vtf", "high", fs, std::nullopt, 4);
  REQUIRE(raw.is_success());
  const auto rawTexture = std::move(raw) | kdl::value();
  CHECK(rawTexture.alphaDomain() == img::ImageAlphaDomain::Graduated);
  auto result = loadTexture("materials/terrain.vmt", "terrain", fs, std::nullopt, 8);
  INFO((result.is_error() ? std::get<Error>(result.error()).msg : "success"));
  REQUIRE(result.is_success());
  const auto texture = std::move(result) | kdl::value();
  REQUIRE(texture.sourceBlendTexture());
  REQUIRE(texture.buffersIfLoaded().size() == 1);
  REQUIRE(texture.sourceBlendTexture()->buffersIfLoaded().size() == 1);
  CHECK(texture.buffersIfLoaded()[0].size() == 4);
  CHECK(texture.sourceBlendTexture()->buffersIfLoaded()[0].size() == 4);
  CHECK(texture.buffersIfLoaded()[0].data()[0] == 255);
  CHECK(texture.sourceBlendTexture()->buffersIfLoaded()[0].data()[1] == 255);
  CHECK(texture.sourceBlendTexture()->buffersIfLoaded()[0].data()[3] == 17);
  CHECK(texture.alphaDomain() == img::ImageAlphaDomain::Opaque);
  CHECK(texture.sourceBlendTexture()->alphaDomain() == img::ImageAlphaDomain::Opaque);
  for (const auto budget : {4u, 7u})
  {
    const auto rejected =
      loadTexture("materials/terrain.vmt", "terrain", fs, std::nullopt, budget);
    REQUIRE(rejected.is_error());
    CHECK(
      std::get<Error>(rejected.error()).msg
      == "VTF exceeds remaining decoded image budget");
  }
  auto inherited = loadTexture("materials/patch.vmt", "patch", fs, std::nullopt, 8);
  REQUIRE(inherited.is_success());
  const auto patched = std::move(inherited) | kdl::value();
  REQUIRE(patched.sourceBlendTexture());
  CHECK(patched.sourceBlendTexture()->buffersIfLoaded()[0].data()[0] == 255);
  const auto original = fs.openFile("materials/terrain.vmt") | kdl::value();
  CHECK(original->reader().readString(original->size()) == before);
}

// Explicit opt-in uses installed content read-only, never a public asset fixture.
TEST_CASE("Source content catalog profiling opt-in", "[.][source][profiling]")
{
  const auto* directory = std::getenv("SOURCEBROOM_PROFILE_GAME");
  REQUIRE(directory);
  const auto start = std::chrono::steady_clock::now();
  auto fs = GameFileSystem{};
  auto logger = NullLogger{};
  REQUIRE(fs.initializeSource(directory, logger).is_success());
  const auto mounted = std::chrono::steady_clock::now();
  REQUIRE(fs.reload().is_success());
  const auto reloaded = std::chrono::steady_clock::now();
  auto config = MaterialConfig{};
  config.root = "materials";
  config.extensions = {".vmt"};
  auto taskManager = kdl::task_manager{4};
  auto resources = std::vector<std::shared_ptr<gl::TextureResource>>{};
  auto mutex = std::mutex{};
  auto decodes = std::atomic_size_t{0};
  const auto create = [&](gl::ResourceLoader<gl::Texture> loader) {
    auto resource =
      std::make_shared<gl::TextureResource>([&, loader = std::move(loader)]() {
        ++decodes;
        return loader();
      });
    resource->setLoadingPolicy(gl::ResourceLoadingPolicy::OnDemand);
    const auto lock = std::lock_guard{mutex};
    resources.push_back(resource);
    return resource;
  };
  const auto collections =
    loadMaterialCollections(fs, config, create, taskManager, logger) | kdl::value();
  const auto catalogued = std::chrono::steady_clock::now();
  auto enabledPaths = std::vector<std::filesystem::path>{};
  for (const auto& collection : collections)
  {
    enabledPaths.push_back(collection.path());
  }
  const auto selectionStart = std::chrono::steady_clock::now();
  auto linear = std::vector<std::filesystem::path>{};
  for (const auto& collection : collections)
  {
    if (std::ranges::find(enabledPaths, collection.path()) != enabledPaths.end())
    {
      linear.push_back(collection.path());
    }
  }
  const auto linearSelected = std::chrono::steady_clock::now();
  const auto enabledSet =
    std::unordered_set<std::filesystem::path>{enabledPaths.begin(), enabledPaths.end()};
  auto hashed = std::vector<std::filesystem::path>{};
  for (const auto& collection : collections)
  {
    if (enabledSet.contains(collection.path()))
    {
      hashed.push_back(collection.path());
    }
  }
  const auto hashedSelected = std::chrono::steady_clock::now();
  CHECK(linear == hashed);
  CHECK(hashed == enabledPaths);
  auto names = std::vector<std::string>{};
  const auto* requestedProbe = std::getenv("SOURCEBROOM_PROFILE_MATERIAL");
  const auto probeName =
    std::string{requestedProbe ? requestedProbe : "brick/brickwall001a"};
  auto probeId = std::optional<gl::ResourceId>{};
  for (const auto& collection : collections)
  {
    for (const auto& material : collection.materials())
    {
      names.push_back(material.name());
      if (material.name() == probeName)
      {
        probeId = material.textureResource().id();
      }
    }
  }
  std::ranges::sort(names);
  auto signature = uint64_t{14695981039346656037ULL};
  for (const auto& name : names)
  {
    for (const auto c : name)
    {
      signature = (signature ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
    }
    signature *= 1099511628211ULL;
  }
  REQUIRE(names.size() > 1000);
  CHECK(resources.size() == names.size());
  CHECK(decodes == 0);
  REQUIRE(probeId.has_value());
  const auto probe = std::ranges::find_if(
    resources, [&](const auto& resource) { return resource->id() == *probeId; });
  REQUIRE(probe != resources.end());
  (*probe)->loadSync();
  CHECK(decodes == 1);
  REQUIRE((*probe)->get());
  CHECK((*probe)->get()->width() > 0);
  CHECK((*probe)->get()->height() > 0);
  const auto probed = std::chrono::steady_clock::now();
  const auto ms = [](const auto a, const auto b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
  };
  std::cout << "SOURCE_PROFILE mount_ms=" << ms(start, mounted)
            << " reload_ms=" << ms(mounted, reloaded)
            << " catalog_ms=" << ms(reloaded, catalogued)
            << " linear_selection_ms=" << ms(selectionStart, linearSelected)
            << " hashed_selection_ms=" << ms(linearSelected, hashedSelected)
            << " probe_ms=" << ms(hashedSelected, probed)
            << " collections=" << collections.size() << " materials=" << names.size()
            << " registrations=" << resources.size() << " decoded=" << decodes
            << " names_fnv64=" << signature << '\n';
}

TEST_CASE("Source map initial catalog and explicit reload retain loose-file freshness")
{
  auto env = fs::TestEnvironment{};
  env.createFile(
    "gameinfo.txt", "GameInfo { FileSystem { SearchPaths { Game |gameinfo_path|. } } }");
  env.createFile("map.vmf", "world { id 1 classname worldspawn }");
  env.createDirectory("materials");
  env.createFile("materials/one.vmt", "LightmappedGeneric { $basetexture one }");
  REQUIRE(env.fileExists("materials/one.vmt"));
  auto config = GameConfig{};
  config.name = "Source fresh catalog control";
  config.fileSystemConfig.packageFormat.format = "vpk";
  config.materialConfig.root = "materials";
  config.materialConfig.extensions = {".vmt"};
  auto gameInfo = makeGameInfo(std::move(config));
  auto taskManager = kdl::task_manager{2};
  auto resourceManager = gl::ResourceManager{};
  auto logger = NullLogger{};
  auto map = Map::loadMap(
               EnvironmentConfig{},
               gameInfo,
               env.dir(),
               MapFormat::SourceVMF,
               vm::bbox3d{65536},
               env.dir() / "map.vmf",
               taskManager,
               resourceManager,
               logger)
             | kdl::value();
  REQUIRE(map->materialManager().materials().size() == 1);
  CHECK(map->materialManager().materials()[0]->name() == "one");
  env.createFile("materials/two.vmt", "LightmappedGeneric { $basetexture two }");
  CHECK(map->materialManager().materials().size() == 1);
  reloadMaterialCollections(*map);
  REQUIRE(map->materialManager().materials().size() == 2);
  REQUIRE(env.remove("materials/one.vmt"));
  reloadMaterialCollections(*map);
  REQUIRE(map->materialManager().materials().size() == 1);
  CHECK(map->materialManager().materials()[0]->name() == "two");
}

TEST_CASE("Source material Patch resolution")
{
  auto env = fs::TestEnvironment{};
  env.createDirectory("materials");
  env.createFile("gameinfo.txt", R"(GameInfo { FileSystem { SearchPaths {
    game "|gameinfo_path|."
  } } })");
  env.createFile("materials/base.vmt", R"(VertexLitGeneric {
    $basetexture wrong $translucent 1 $nocull 1 $alphatestreference 0.25
  })");
  env.createFile("materials/middle.vmt", R"(Patch {
    include "materials/base.vmt" replace { $basetexture brick/wall }
  })");
  const auto patch = R"(pAtCh {
    include "MATERIALS\\middle.VMT"
    insert { $translucent 0 $alphatest 1 $surfaceprop brick $nocull 0 $alphatestreference 0.75 }
  })";
  env.createFile("materials/derived.vmt", patch);
  env.createFile("materials/bad-threshold.vmt", R"(Patch {
    include "materials/derived.vmt" replace { $alphatestreference nan }
  })");
  env.createFile("materials/zero-threshold.vmt", R"(Patch {
    include "materials/derived.vmt" replace { $alphatestreference 0 }
  })");
  REQUIRE(env.fileExists("materials/derived.vmt"));
  auto fs = GameFileSystem{};
  auto logger = NullLogger{};
  REQUIRE(fs.initializeSource(env.dir(), logger).is_success());
  const auto beforeFile = fs.openFile("materials/derived.vmt") | kdl::value();
  const auto before = beforeFile->reader().readString(beforeFile->size());
  CHECK(sourceMaterial(patch, SourceMaterialMode::BaseTexturePreview).is_error());
  const auto result = loadSourceMaterial(
    "materials/derived.vmt", fs, SourceMaterialMode::BaseTexturePreview);
  INFO((result.is_error() ? std::get<Error>(result.error()).msg : "success"));
  REQUIRE(result.is_success());
  const auto info = result | kdl::value();
  CHECK(info.baseTexture == "materials/brick/wall.vtf");
  CHECK(info.alphaDomain == img::ImageAlphaDomain::Binary);
  CHECK_FALSE(info.noCull);
  CHECK(info.alphaTestReference == 0.75f);
  const auto inherited = loadSourceMaterial(
    "materials/middle.vmt", fs, SourceMaterialMode::BaseTexturePreview);
  REQUIRE(inherited.is_success());
  CHECK((inherited | kdl::value()).noCull);
  CHECK((inherited | kdl::value()).alphaTestReference == 0.25f);
  CHECK(loadSourceMaterial(
          "materials/bad-threshold.vmt", fs, SourceMaterialMode::BaseTexturePreview)
          .is_error());
  CHECK(loadSourceMaterial(
          "materials/zero-threshold.vmt", fs, SourceMaterialMode::BaseTexturePreview)
          .is_error());
  CHECK(loadSourceMaterial("materials/derived.vmt", fs).is_error());
  const auto original = fs.openFile("materials/derived.vmt") | kdl::value();
  CHECK(original->reader().readString(original->size()) == before);
}

TEST_CASE("Source material Patch safety and mounted precedence")
{
  auto env = fs::TestEnvironment{};
  env.createDirectory("materials");
  env.createFile("gameinfo.txt", R"(GameInfo { FileSystem { SearchPaths {
    game "|gameinfo_path|."
  } } })");
  const auto base = std::string{"LightmappedGeneric { $basetexture root/base }"};
  env.createFile("materials/base.vmt", base);
  REQUIRE(env.fileExists("materials/base.vmt"));
  const auto attempt = [&](const std::string& text) -> Result<SourceMaterialInfo> {
    env.createFile("materials/patch.vmt", text);
    REQUIRE(env.fileExists("materials/patch.vmt"));
    auto fs = GameFileSystem{};
    auto logger = NullLogger{};
    REQUIRE(fs.initializeSource(env.dir(), logger).is_success());
    return loadSourceMaterial(
      "materials/patch.vmt", fs, SourceMaterialMode::BaseTexturePreview);
  };
  SECTION("scalar operations act and missing replacement is not insertion")
  {
    for (const auto* operation : {"insert", "replace"})
    {
      const auto result = attempt(
        "Patch { include materials/base.vmt " + std::string{operation}
        + " { $basetexture new/path } }");
      REQUIRE(result.is_success());
      CHECK((result | kdl::value()).baseTexture == "materials/new/path.vtf");
    }
    const auto missing =
      attempt("Patch { include materials/base.vmt replace { $alphatest 1 $unknown 1 } }");
    REQUIRE(missing.is_success());
    CHECK((missing | kdl::value()).alphaDomain == img::ImageAlphaDomain::Opaque);
    const auto inserted =
      attempt("Patch { include materials/base.vmt insert { $alphatest 1 } }");
    REQUIRE(inserted.is_success());
    CHECK((inserted | kdl::value()).alphaDomain == img::ImageAlphaDomain::Binary);
  }
  SECTION("unsupported and ambiguous patch structures refuse")
  {
    env.createFile("materials/water.vmt", "Water { $basetexture water/a }");
    env.createFile("materials/proxy.vmt", "UnlitGeneric { $basetexture a Proxies {} }");
    env.createFile(
      "materials/ambiguous.vmt", "UnlitGeneric { $basetexture a $basetexture b }");
    for (const auto* text :
         {"Patch {}",
          "Patch { include materials/missing.vmt }",
          "Patch { include materials/water.vmt }",
          "Patch { include materials/proxy.vmt }",
          "Patch { include materials/base.vmt include materials/base.vmt }",
          "Patch { include {} }",
          "Patch { include materials/base.vmt insert 1 }",
          "Patch { include materials/base.vmt insert {} insert {} }",
          "Patch { include materials/base.vmt insert { $basetexture a $BaseTexture b } }",
          "Patch { include materials/base.vmt replace { $basetexture a } insert { "
          "$basetexture b } }",
          "Patch { include materials/base.vmt insert { $basetexture \"\" } }",
          "Patch { include materials/base.vmt insert { Proxies {} } }",
          "Patch { include materials/base.vmt insert { $unknown 1 } }",
          "Patch { include materials/base.vmt remove {} }",
          "Patch { include materials/ambiguous.vmt replace { $basetexture x } }",
          "Patch { include \"/materials/base.vmt\" }",
          "Patch { include \"C:/materials/base.vmt\" }",
          "Patch { include \"materials/../base.vmt\" }",
          "Patch { include \"../materials/base.vmt\" }",
          "Patch { include \"models/base.vmt\" }",
          "Patch { include \"materials/base\" }"})
    {
      CAPTURE(text);
      CHECK(attempt(text).is_error());
    }
    const auto normalized = attempt("Patch { include materials/./base.vmt }");
    REQUIRE(normalized.is_success());
    CHECK((normalized | kdl::value()).baseTexture == "materials/root/base.vtf");
  }
  SECTION("cycle and exact sixteen-include boundary")
  {
    env.createFile("materials/cycle.vmt", "Patch { include materials/./CYCLE.vmt }");
    const auto cycle = attempt("Patch { include materials/cycle.vmt }");
    REQUIRE(cycle.is_error());
    CHECK(std::get<Error>(cycle.error()).msg.find("cycle") != std::string::npos);
    for (size_t i = 0; i <= 16; ++i)
    {
      env.createFile(
        "materials/depth" + std::to_string(i) + ".vmt",
        i == 16 ? base
                : "Patch { include materials/depth" + std::to_string(i + 1) + ".vmt }");
    }
    CHECK(attempt("Patch { include materials/depth0.vmt }").is_error());
    env.createFile("materials/depth15.vmt", base);
    const auto bounded = attempt("Patch { include materials/depth0.vmt }");
    REQUIRE(bounded.is_success());
    CHECK((bounded | kdl::value()).baseTexture == "materials/root/base.vtf");
  }
  SECTION("file and aggregate text limits have successful boundary controls")
  {
    constexpr size_t fileLimit = 1024u * 1024u;
    auto large = base;
    large.resize(fileLimit, ' ');
    env.createFile("materials/large.vmt", large);
    CHECK(attempt("Patch { include materials/large.vmt }").is_success());
    large.push_back(' ');
    env.createFile("materials/large.vmt", large);
    CHECK(attempt("Patch { include materials/large.vmt }").is_error());
    CHECK(attempt(large).is_error());
    for (size_t i = 0; i < 4; ++i)
    {
      auto text = i == 3
                    ? base
                    : "Patch { include materials/bulk" + std::to_string(i + 1) + ".vmt }";
      text.resize(fileLimit, ' ');
      env.createFile("materials/bulk" + std::to_string(i) + ".vmt", text);
    }
    CHECK(attempt("Patch { include materials/bulk0.vmt }").is_error());
    large = base;
    large.resize(fileLimit, ' ');
    env.createFile("materials/bulk2.vmt", large);
    const auto bounded = attempt("Patch { include materials/bulk0.vmt }");
    REQUIRE(bounded.is_success());
    CHECK((bounded | kdl::value()).baseTexture == "materials/root/base.vtf");
  }
  SECTION("NUL bytes cannot hide trailing unsupported data in root or includes")
  {
    auto text = base;
    text.push_back('\0');
    text += "unparsed trailing bytes";
    CHECK(sourceMaterial(text, SourceMaterialMode::BaseTexturePreview).is_error());
    CHECK(attempt(text).is_error());
    env.createFile("materials/nul.vmt", text);
    CHECK(attempt("Patch { include materials/nul.vmt }").is_error());
    CHECK(attempt(base).is_success());
    CHECK(attempt("Patch { include materials/base.vmt }").is_success());
  }
  SECTION("many-key scalar patches use bounded indexed lookup")
  {
    auto manyBase = std::string{"LightmappedGeneric { $basetexture many/base "};
    auto manyPatch = std::string{"Patch { include materials/many.vmt insert { "};
    for (size_t i = 0; i < 40000; ++i)
    {
      manyBase += "%base" + std::to_string(i) + " 1 ";
      manyPatch += "%patch" + std::to_string(i) + " 1 ";
    }
    manyBase += "}";
    manyPatch += "$basetexture many/changed } }";
    env.createFile("materials/many.vmt", manyBase);
    const auto result = attempt(manyPatch);
    REQUIRE(result.is_success());
    CHECK((result | kdl::value()).baseTexture == "materials/many/changed.vtf");
  }
  SECTION("included base follows existing mount order not root host-relative lookup")
  {
    env.createDirectory("override");
    env.createDirectory("override/materials");
    env.createFile(
      "override/materials/base.vmt", "UnlitGeneric { $basetexture priority/winner }");
    REQUIRE(env.fileExists("override/materials/base.vmt"));
    env.createFile("gameinfo.txt", R"(GameInfo { FileSystem { SearchPaths {
      game "|gameinfo_path|override" game "|gameinfo_path|."
    } } })");
    const auto result = attempt("Patch { include materials/base.vmt }");
    REQUIRE(result.is_success());
    CHECK((result | kdl::value()).baseTexture == "materials/priority/winner.vtf");
    CHECK(env.loadFile("materials/base.vmt") == base);
  }
}

TEST_CASE("Source VMT cutout reference policy")
{
  const auto parse = [](const std::string& shader, const std::string& flags) {
    return sourceMaterial(
      shader + " { $basetexture test/pixel " + flags + " }",
      SourceMaterialMode::BaseTexturePreview);
  };
  for (const auto* shader : {"UnlitGeneric", "VertexLitGeneric", "LightmappedGeneric"})
  {
    const auto absent = parse(shader, "$alphatest 1");
    REQUIRE(absent.is_success());
    CHECK((absent | kdl::value()).alphaTestReference == 0.5f);
    for (const auto& [text, threshold] :
         {std::pair{"0.25", 0.25f},
          std::pair{".75", 0.75f},
          std::pair{"1", 1.0f},
          std::pair{"+2.5e-1", 0.25f},
          std::pair{"\" 0.75 \"", 0.75f}})
    {
      for (const auto* flags :
           {"",
            "$alphatest 0",
            "$alphatest 1",
            "$translucent 1",
            "$alphatest 1 $translucent 1"})
      {
        CAPTURE(shader, text, flags);
        const auto result =
          parse(shader, std::string{flags} + " $AlphaTestReference " + text);
        REQUIRE(result.is_success());
        CHECK((result | kdl::value()).alphaTestReference == threshold);
        const auto domain = (result | kdl::value()).alphaDomain;
        CHECK(domain == (std::string_view{flags}.find("$alphatest 1") != std::string_view::npos
          ? img::ImageAlphaDomain::Binary
          : std::string_view{flags}.find("$translucent 1") != std::string_view::npos
            ? img::ImageAlphaDomain::Graduated : img::ImageAlphaDomain::Opaque));
      }
    }
    for (const auto* flags :
         {"$alphatestreference 0",
          "$alphatestreference -0",
          "$alphatestreference -0.1",
          "$alphatestreference 1.01",
          "$alphatestreference nan",
          "$alphatestreference inf",
          "$alphatestreference 1e999",
          "$alphatestreference nope",
          "$alphatestreference \"\"",
          "$alphatestreference \"0.25 trailing\"",
          "$alphatestreference \"[0.25]\"",
          "$alphatestreference {}",
          "$alphatestreference 0.25 $AlphaTestReference 0.75"})
    {
      CAPTURE(shader, flags);
      CHECK(parse(shader, flags).is_error());
      CHECK(parse(shader, std::string{"$alphatest 1 "} + flags).is_error());
    }
    CHECK(sourceMaterial(
            std::string{shader} + " { $basetexture test/pixel $alphatestreference 0.25 }")
            .is_error());
    CHECK(parse(shader, "$alpha 0.5").is_error());
  }
  CHECK(parse("WorldVertexTransition", "$alphatestreference 0.25").is_error());
}

TEST_CASE("Source VMT no-cull policy")
{
  const auto parse = [](const std::string& flags) {
    return sourceMaterial(
      "UnlitGeneric { $basetexture test/pixel " + flags + " }",
      SourceMaterialMode::BaseTexturePreview);
  };
  for (const auto* flags : {"", "$nocull 0", "$nocull false", "$NoCull 0.0"})
  {
    const auto result = parse(flags);
    REQUIRE(result.is_success());
    CHECK_FALSE((result | kdl::value()).noCull);
    CHECK((result | kdl::value()).alphaDomain == img::ImageAlphaDomain::Opaque);
  }
  for (const auto* flags : {"$nocull 1", "$nocull true", "$NoCull 1.0"})
  {
    const auto result = parse(flags);
    REQUIRE(result.is_success());
    CHECK((result | kdl::value()).noCull);
    CHECK((result | kdl::value()).alphaDomain == img::ImageAlphaDomain::Opaque);
  }
  for (const auto* flags :
       {"$nocull 2", "$nocull nan", "$nocull nope", "$nocull {}", "$nocull 0 $NoCull 1"})
  {
    CAPTURE(flags);
    CHECK(parse(flags).is_error());
  }
  CHECK(sourceMaterial("UnlitGeneric { $basetexture test/pixel $nocull 1 }").is_error());
}

TEST_CASE("SourceContent")
{
  SECTION("gameinfo search order and tokens")
  {
    const auto paths = sourceSearchPaths(
                         R"(
      GameInfo { FileSystem { SearchPaths {
        Game+Mod "|gameinfo_path|custom"
        game "|all_source_engine_paths|hl2/hl2_textures.vpk"
        game "cstrike" platform "platform"
      } } })",
                         "/games/css/cstrike")
                       | kdl::value();
    REQUIRE(paths.size() == 3);
    CHECK(paths[0] == std::filesystem::path{"/games/css/cstrike/custom"});
    CHECK(paths[1] == std::filesystem::path{"/games/css/hl2/hl2_textures.vpk"});
    CHECK(paths[2] == std::filesystem::path{"/games/css/cstrike"});
    CHECK(sourceSearchPaths("GameInfo {", "/game").is_error());
    CHECK(sourceSearchPaths("GameInfo {}", "/game").is_error());
  }
  SECTION("trailing mod directories preserve token engine and mount fallback roots")
  {
    auto env = fs::TestEnvironment{};
    env.createDirectory("steamapps");
    env.createFile("steamapps/libraryfolders.vdf", "libraryfolders {}");
    const auto gameInfo = R"(GameInfo {
      FileSystem { SearchPaths {
        game "|gameinfo_path|custom" game "|all_source_engine_paths|hl2" game "cstrike"
      } }
      mount { "999999999" { fallback "fallback_mod" } }
    })";
    const auto expected =
      sourceSearchPaths(gameInfo, "/games/css/cstrike", {env.dir()}) | kdl::value();
    const auto trailing =
      sourceSearchPaths(
        gameInfo, std::filesystem::path{"/games/css/cstrike"} / "", {env.dir()})
      | kdl::value();
    REQUIRE(expected.size() == 4);
    CHECK(trailing == expected);
    CHECK(trailing[0] == std::filesystem::path{"/games/css/cstrike/custom"});
    CHECK(trailing[1] == std::filesystem::path{"/games/css/hl2"});
    CHECK(trailing[2] == std::filesystem::path{"/games/css/cstrike"});
    CHECK(trailing[3] == std::filesystem::path{"/games/css/fallback_mod"});
  }
  SECTION("common shader base textures and explicit unsupported fallbacks")
  {
    CHECK(
      (sourceBaseTexture(R"(LightmappedGeneric { "$baseTexture" "BRICK/wall" })")
       | kdl::value())
      == std::filesystem::path{"materials/brick/wall.vtf"});
    CHECK(sourceBaseTexture(R"(Water { "$basetexture" "water/test" })").is_error());
    CHECK(sourceBaseTexture(R"(Patch { include "a" })").is_error());
    CHECK(sourceBaseTexture(R"(UnlitGeneric { "$basetexture" "../escape" })").is_error());
    CHECK(
      sourceBaseTexture(R"(UnlitGeneric { "$basetexture" "x" Proxies {} })").is_error());
    CHECK(sourceBaseTexture(R"(VertexLitGeneric {})").is_error());
  }
  SECTION("explicit base-texture preview omits only enumerated optional effects")
  {
    const auto material = R"(LightmappedGeneric {
      "$basetexture" "brick/wall" "$bumpmap" "brick/normal"
      "$detail" "detail/noise" "$detailscale" "4" "$surfaceprop" "brick"
      "$envmap" "env_cubemap" "$alphatest" "1"
    })";
    CHECK(sourceBaseTexture(material).is_error());
    CHECK(
      (sourceBaseTexture(material, SourceMaterialMode::BaseTexturePreview) | kdl::value())
      == std::filesystem::path{"materials/brick/wall.vtf"});
    for (
      const auto* unresolved :
      {R"(Patch { include "x" })",
       R"(Water { "$basetexture" "x" })",
       R"(UnlitGeneric { "$basetexture" "x" Proxies {} })",
       R"(LightmappedGeneric { "$basetexture" "x" "$basetexturetransform" "scale 2 2" })",
       R"(LightmappedGeneric { "$basetexture" "x" "$bumptransform" "scale 2 2" })",
       R"(LightmappedGeneric { "$basetexture" "x" "$basetexture2" "y" })",
       R"(LightmappedGeneric { "$basetexture" "x" "$unknown" "1" })"})
    {
      CHECK(
        sourceBaseTexture(unresolved, SourceMaterialMode::BaseTexturePreview).is_error());
    }
  }
  SECTION("preview alpha flags are explicit and ambiguous flags refuse")
  {
    for (const auto* flag : {"$alphatest", "$translucent"})
    {
      for (const auto* value :
           {"0", "1", "0.0", "1.0", "0.000000", "1.000000", "false", "true"})
      {
        const auto text =
          std::string{"VertexLitGeneric { $basetexture x "} + flag + " " + value + " }";
        CHECK(sourceMaterial(text).is_error());
        const auto info = sourceMaterial(text, SourceMaterialMode::BaseTexturePreview);
        REQUIRE(info.is_success());
        const auto enabled =
          std::string_view{value}.starts_with('1') || std::string_view{value} == "true";
        CHECK((info | kdl::value()).alphaDomain
          == (!enabled ? img::ImageAlphaDomain::Opaque
              : std::string_view{flag} == "$alphatest" ? img::ImageAlphaDomain::Binary
                                                       : img::ImageAlphaDomain::Graduated));
      }
      for (const auto* suffix :
           {" $variable", " -1", " nan", " 1 $alphatest 0 $translucent 0"})
      {
        const auto text =
          std::string{"VertexLitGeneric { $basetexture x "} + flag + suffix + " }";
        CHECK(sourceMaterial(text, SourceMaterialMode::BaseTexturePreview).is_error());
      }
    }
  }
  SECTION("base-image preview accepts enumerated optional PBR and tint metadata")
  {
    for (const auto* parameter :
         {"$roughnessmap",
          "$ambientocclusionmap",
          "$notint",
          "$treesway",
          "$tintmasktexture",
          "$blendtintbybasealpha",
          "$envmaplightscale",
          "$color",
          "$color2"})
    {
      const auto material =
        std::string{"VertexLitGeneric { $basetexture brick/wall "} + parameter + " 1 }";
      CHECK(sourceBaseTexture(material).is_error());
      CHECK(
        (sourceBaseTexture(material, SourceMaterialMode::BaseTexturePreview)
         | kdl::value())
        == std::filesystem::path{"materials/brick/wall.vtf"});
    }
  }
  SECTION("manual Steam root discovery and loose mount precedence")
  {
    auto env = fs::TestEnvironment{};
    const auto root = env.dir();
    std::filesystem::create_directories(root / "steamapps/common/test/mod/custom/a");
    std::filesystem::create_directories(root / "steamapps/common/test/base");
    const auto write = [&](const auto& path, const auto& text) {
      std::ofstream{root / path} << text;
    };
    write("steamapps/libraryfolders.vdf", "libraryfolders {}");
    write("steamapps/appmanifest_1.acf", "AppState { installdir test }");
    write(
      "steamapps/common/test/mod/gameinfo.txt", R"(GameInfo { FileSystem { SearchPaths {
      game "|gameinfo_path|custom/*" game "|gameinfo_path|." game base
    } } })");
    write("steamapps/common/test/mod/custom/a/control.txt", "custom");
    write("steamapps/common/test/mod/control.txt", "mod");
    write("steamapps/common/test/base/control.txt", "base");
    const auto libraries = sourceSteamLibraries({root}) | kdl::value();
    REQUIRE(libraries.size() == 1);
    CHECK(libraries[0] == root);
    const auto games = sourceInstalledGames({root}) | kdl::value();
    REQUIRE(games.size() == 1);
    CHECK(games[0] == root / "steamapps/common/test/mod");
    auto fs = GameFileSystem{};
    auto logger = NullLogger{};
    REQUIRE(fs.initializeSource(games[0], logger).is_success());
    const auto file = fs.openFile("control.txt") | kdl::value();
    CHECK(file->reader().readString(file->size()) == "custom");

    auto tree = std::string{"txt\0 \0control\0", 14};
    const auto word = [](std::string& data, uint32_t value, size_t bytes) {
      for (size_t i = 0; i < bytes; ++i)
      {
        data += static_cast<char>(value >> (8 * i));
      }
    };
    word(tree, 0, 4);
    word(tree, 0, 2);
    word(tree, 0x7fff, 2);
    word(tree, 0, 4);
    word(tree, 7, 4);
    word(tree, 0xffff, 2);
    tree.append(3, '\0');
    auto package = std::string{};
    word(package, 0x55aa1234, 4);
    word(package, 1, 4);
    word(package, static_cast<uint32_t>(tree.size()), 4);
    package += tree;
    package += "package";
    {
      auto stream =
        std::ofstream{root / "steamapps/common/test/mod/pak_dir.vpk", std::ios::binary};
      stream.write(package.data(), static_cast<std::streamsize>(package.size()));
    }
    write(
      "steamapps/common/test/mod/gameinfo.txt", R"(GameInfo { FileSystem { SearchPaths {
      game "|gameinfo_path|pak.vpk" game "|gameinfo_path|custom/*"
    } } })");
    REQUIRE(fs.initializeSource(games[0], logger).is_success());
    const auto packed = fs.openFile("control.txt") | kdl::value();
    CHECK(packed->reader().readString(packed->size()) == "package");
    write(
      "steamapps/common/test/mod/gameinfo.txt", R"(GameInfo { FileSystem { SearchPaths {
      game "|gameinfo_path|custom/*" game "|gameinfo_path|pak.vpk"
    } } })");
    REQUIRE(fs.initializeSource(games[0], logger).is_success());
    const auto loose = fs.openFile("control.txt") | kdl::value();
    CHECK(loose->reader().readString(loose->size()) == "custom");
    auto config = GameConfig{};
    config.fileSystemConfig.packageFormat.format = "vpk";
    config.fileSystemConfig.searchPath = "mod";
    fs.initialize(EnvironmentConfig{}, config, games[0].parent_path(), {}, logger);
    const auto integrated = fs.openFile("control.txt") | kdl::value();
    CHECK(integrated->reader().readString(integrated->size()) == "custom");

    const auto mounted = sourceSearchPaths(
                           R"(GameInfo {
      mount { 1 { test { vpk content dir downloads } } 999 { fallback "mount/fallback.vpk" } }
      FileSystem { SearchPaths { game "|gameinfo_path|." } }
    })",
                           games[0],
                           {root})
                         | kdl::value();
    REQUIRE(mounted.size() == 4);
    CHECK(mounted[1] == root / "steamapps/common/test/test/content.vpk");
    CHECK(mounted[2] == root / "steamapps/common/test/test/downloads");
    CHECK(mounted[3] == root / "steamapps/common/test/mount/fallback.vpk");
  }
}
} // namespace tb::mdl
