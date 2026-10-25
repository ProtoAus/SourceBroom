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
