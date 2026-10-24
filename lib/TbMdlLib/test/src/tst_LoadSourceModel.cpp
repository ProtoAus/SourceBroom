/* Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later */
#include "SourceModelFixture.h"
#include "TestEnvironment.h"
#include "base/Logger.h"
#include "fs/DiskFileSystem.h"
#include "fs/TestFileSystem.h"
#include "gl/Material.h"
#include "mdl/GameConfig.h"
#include "mdl/LoadEntityModel.h"
#include "mdl/LoadSourceModel.h"

#include <algorithm>
#include <set>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
std::shared_ptr<fs::File> file(const std::string& bytes)
{
  auto buffer = std::make_unique<char[]>(bytes.size());
  std::copy(bytes.begin(), bytes.end(), buffer.get());
  return std::make_shared<fs::OwningBufferFile>(std::move(buffer), bytes.size());
}
fs::TestFileSystem modelFileSystem(
  const test::SyntheticProp& prop, bool sidecars = true, bool mipmapped = false)
{
  auto entries = std::vector<fs::Entry>{fs::FileEntry{"prop.mdl", file(prop.mdl)}};
  if (sidecars)
  {
    entries.push_back(fs::FileEntry{"prop.vvd", file(prop.vvd)});
    entries.push_back(fs::FileEntry{"prop.dx90.vtx", file(prop.vtx)});
  }
  // Public, authored one-pixel RGBA VTF and mixed-case VMT path control.
  auto vtf = std::string(84, '\0');
  test::put(vtf, 0, 0x00465456);
  test::put(vtf, 4, 7);
  test::put(vtf, 8, 2);
  test::put(vtf, 12, 80);
  test::put(vtf, 16, 1, 2);
  test::put(vtf, 18, 1, 2);
  test::put(vtf, 24, 1, 2);
  test::put(vtf, 52, 0);
  test::put(vtf, 56, 1, 1);
  test::put(vtf, 57, 0xffffffff);
  test::put(vtf, 63, 1, 2);
  test::put(vtf, 80, 0xff0000ff);
  if (mipmapped)
  {
    vtf.resize(100);
    test::put(vtf, 16, 2, 2);
    test::put(vtf, 18, 2, 2);
    test::put(vtf, 56, 2, 1);
  }
  entries.push_back(
    fs::DirectoryEntry{
      "materials",
      {fs::DirectoryEntry{
        "models",
        {fs::DirectoryEntry{
          "test",
          {fs::FileEntry{
             "metal.vmt", file("VertexLitGeneric { $basetexture Models/Test/Pixel }")},
           fs::FileEntry{
             "other.vmt", file("VertexLitGeneric { $basetexture models/test/other }")},
           fs::FileEntry{
             "third.vmt", file("VertexLitGeneric { $basetexture models/test/third }")},
           fs::FileEntry{"other.vtf", file(vtf)},
           fs::FileEntry{"third.vtf", file(vtf)},
           fs::FileEntry{"pixel.vtf", file(vtf)}}}}}}});
  return fs::TestFileSystem{fs::DirectoryEntry{"", std::move(entries)}, {}};
}
} // namespace
TEST_CASE("loadSourceModel")
{
  auto prop = test::SyntheticProp{};
  auto logger = NullLogger{};
  SECTION("registered sync loader resolves sidecars and Source VMT/VTF")
  {
    auto fs = modelFileSystem(prop);
    bool legacyMaterialCalled = false;
    auto loadMaterial = [&](const auto&) -> gl::Material {
      legacyMaterialCalled = true;
      throw std::runtime_error("Source must not retain the legacy callback");
    };
    auto result =
      loadEntityModelSync(fs, MaterialConfig{}, "prop.mdl", loadMaterial, logger);
    INFO((result.is_error() ? std::get<Error>(result.error()).msg : "success"));
    REQUIRE(!result.is_error());
    const auto model = std::move(result) | kdl::value();
    REQUIRE(model.data() != nullptr);
    CHECK(model.data()->surfaceCount() == 1);
    CHECK(model.data()->frame(0)->bounds() == vm::bbox3f{{0, 0, 0}, {2, 3, 0}});
    const auto* skin = model.data()->surface(0).skin(0);
    REQUIRE(skin != nullptr);
    REQUIRE(skin->texture() != nullptr);
    CHECK(skin->texture()->width() == 1);
    CHECK(skin->texture()->height() == 1);
    CHECK_FALSE(legacyMaterialCalled);
  }
  SECTION("completed resource loader owns model after filesystem and callbacks close")
  {
    auto pending = gl::ResourceLoader<EntityModelData>{};
    {
      auto fs = modelFileSystem(prop);
      auto localLogger = NullLogger{};
      auto model = loadEntityModelAsync(
        fs,
        MaterialConfig{},
        "prop.mdl",
        [](const auto&) -> gl::Material {
          throw std::runtime_error("legacy material callback");
        },
        [&](auto loader) {
          pending = std::move(loader);
          return std::make_shared<EntityModelDataResource>(
            EntityModelData{PitchType::Normal, Orientation::Oriented});
        },
        localLogger);
    }
    REQUIRE(bool(pending));
    auto result = pending();
    INFO((result.is_error() ? std::get<Error>(result.error()).msg : "success"));
    REQUIRE(!result.is_error());
    auto data = std::move(result) | kdl::value();
    CHECK(data.surfaceCount() == 1);
    REQUIRE(data.surface(0).skin(0)->texture() != nullptr);
    CHECK(data.surface(0).skin(0)->texture()->width() == 1);
  }
  SECTION("registered loaders leave public GoldSrc v10 to Assimp")
  {
    auto fs = fs::DiskFileSystem{getFixtureRoot() / "test/mdl/LoadAssimpModel/cube"};
    const auto mdl = fs.openFile("cube.mdl") | kdl::value();
    CHECK_FALSE(canLoadSourceModel("cube.mdl", mdl->reader()));
    const auto loadMaterial = [](const auto&) -> gl::Material {
      throw std::runtime_error("unexpected material callback");
    };
    SECTION("sync")
    {
      auto result =
        loadEntityModelSync(fs, MaterialConfig{}, "cube.mdl", loadMaterial, logger);
      INFO((result.is_error() ? std::get<Error>(result.error()).msg : "success"));
      REQUIRE(!result.is_error());
      const auto model = std::move(result) | kdl::value();
      REQUIRE(model.data() != nullptr);
      CHECK(model.data()->surfaceCount() == 4);
      CHECK(model.data()->frameCount() == 3);
    }
    SECTION("async")
    {
      auto pending = gl::ResourceLoader<EntityModelData>{};
      auto model = loadEntityModelAsync(
        fs,
        MaterialConfig{},
        "cube.mdl",
        loadMaterial,
        [&](auto loader) {
          pending = std::move(loader);
          return std::make_shared<EntityModelDataResource>(
            EntityModelData{PitchType::Normal, Orientation::Oriented});
        },
        logger);
      REQUIRE(bool(pending));
      auto result = pending(); // Legacy references stay alive until completion.
      INFO((result.is_error() ? std::get<Error>(result.error()).msg : "success"));
      REQUIRE(!result.is_error());
      const auto data = std::move(result) | kdl::value();
      CHECK(data.surfaceCount() == 4);
      CHECK(data.frameCount() == 3);
    }
  }
  SECTION("aggregate budget charges unique retained mips and shared fallback")
  {
    prop.mdl.resize(1200);
    test::put(prop.mdl, 76, uint32_t(prop.mdl.size()));
    test::put(prop.mdl, 204, 3);
    test::put(prop.mdl, 208, 760);
    for (size_t i = 0; i < 3; ++i)
      test::put(prop.mdl, 760 + 64 * i, uint32_t(960 + 10 * i - (760 + 64 * i)));
    prop.mdl.replace(960, 5, "metal");
    prop.mdl.replace(970, 5, "other");
    prop.mdl.replace(980, 5, "third");
    test::put(prop.mdl, 216, 1100);
    test::put(prop.mdl, 1100, 1104);
    prop.mdl.replace(1104, 12, "models/test/");
    test::put(prop.mdl, 224, 4);
    test::put(prop.mdl, 228, 1000);
    for (size_t i = 0; i < 4; ++i)
      test::put(prop.mdl, 1000 + 2 * i, uint32_t(i % 3), 2);
    auto fs = modelFileSystem(prop, true, true);
    for (const auto allowance : {36u, 40u})
    {
      CAPTURE(allowance);
      const size_t budget = 4096 + allowance;
      auto result = loadSourceModel("prop.mdl", fs, logger, budget);
      INFO((result.is_error() ? std::get<Error>(result.error()).msg : "success"));
      REQUIRE(!result.is_error());
      const auto data = std::move(result) | kdl::value();
      const auto& surface = data.surface(0);
      REQUIRE(surface.skinCount() == 4);
      CHECK(surface.skin(0)->texture()->width() == 2);
      CHECK(surface.skin(1)->texture()->width() == (allowance == 36 ? 32 : 2));
      CHECK(surface.skin(2)->texture()->width() == 32);
      CHECK(surface.skin(3)->texture() == surface.skin(0)->texture());
      if (allowance == 36)
        CHECK(surface.skin(1)->texture() == surface.skin(2)->texture());
      auto seen = std::set<const gl::Texture*>{};
      size_t retained = 0;
      for (size_t i = 0; i < surface.skinCount(); ++i)
      {
        const auto* texture = surface.skin(i)->texture();
        REQUIRE(texture != nullptr);
        if (seen.insert(texture).second)
          for (const auto& buffer : texture->buffersIfLoaded())
            retained += buffer.size();
      }
      CHECK(retained == 4096 + (allowance == 36 ? 20 : 40));
      CHECK(retained <= budget);
    }
    CHECK(loadSourceModel("prop.mdl", fs, logger, 4095).is_error());
  }
  SECTION("Source signature owns unsupported versions and missing sidecar diagnostics")
  {
    test::put(prop.mdl, 4, 53);
    auto fs = modelFileSystem(prop);
    CHECK(canLoadSourceModel("PROP.MDL", file(prop.mdl)->reader()));
    CHECK(loadSourceModel("prop.mdl", fs, logger).is_error());
    CHECK(loadEntityModelSync(fs, MaterialConfig{}, "prop.mdl", {}, logger).is_error());
    auto pending = gl::ResourceLoader<EntityModelData>{};
    auto model = loadEntityModelAsync(
      fs,
      MaterialConfig{},
      "prop.mdl",
      {},
      [&](auto loader) {
        pending = std::move(loader);
        return std::make_shared<EntityModelDataResource>(
          EntityModelData{PitchType::Normal, Orientation::Oriented});
      },
      logger);
    REQUIRE(bool(pending));
    CHECK(pending().is_error());
    auto missing = modelFileSystem(test::SyntheticProp{}, false);
    CHECK(loadSourceModel("prop.mdl", missing, logger).is_error());
  }
}
} // namespace tb::mdl
