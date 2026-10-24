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
#include "mdl/EnvironmentConfig.h"
#include "mdl/GameConfig.h"
#include "mdl/GameFileSystem.h"
#include "mdl/SourceContent.h"

#include "kd/result.h"

#include <fstream>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
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
