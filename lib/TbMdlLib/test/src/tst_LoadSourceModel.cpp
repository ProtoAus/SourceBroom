/* Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later */
#include "SourceModelFixture.h"
#include "TestEnvironment.h"
#include "TestParserStatus.h"
#include "base/Logger.h"
#include "fs/DiskFileSystem.h"
#include "fs/TestFileSystem.h"
#include "gl/CreateResource.h"
#include "gl/Material.h"
#include "gl/MaterialCollection.h"
#include "gl/MaterialIndexRangeRenderer.h"
#include "gl/MockGl.h"
#include "gl/Resource.h"
#include "gl/TestUtils.h"
#include "gl/Texture.h"
#include "gl/VboManager.h"
#include "mdl/Entity.h"
#include "mdl/EntityDefinition.h"
#include "mdl/EntityModelManager.h"
#include "mdl/FgdParser.h"
#include "mdl/GameConfig.h"
#include "mdl/GameInfo.h"
#include "mdl/LoadEntityModel.h"
#include "mdl/LoadMaterialCollections.h"
#include "mdl/LoadSourceModel.h"
#include "mdl/LoadTexture.h"
#include "mdl/ModelSpecification.h"

#include "kd/invoke.h"
#include "kd/task_manager.h"

#include "vm/ray.h"

#include <algorithm>
#include <set>
#include <tuple>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

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
test::SyntheticProp withMaterialSkins(test::SyntheticProp prop)
{
  prop.mdl.resize(1200);
  test::put(prop.mdl, 76, uint32_t(prop.mdl.size()));
  test::put(prop.mdl, 204, 3);
  test::put(prop.mdl, 208, 760);
  for (size_t i = 0; i < 3; ++i)
  {
    test::put(prop.mdl, 760 + 64 * i, uint32_t(960 + 10 * i - (760 + 64 * i)));
  }
  prop.mdl.replace(960, 5, "metal");
  prop.mdl.replace(970, 5, "other");
  prop.mdl.replace(980, 5, "third");
  test::put(prop.mdl, 216, 1100);
  test::put(prop.mdl, 1100, 1104);
  prop.mdl.replace(1104, 12, "models/test/");
  test::put(prop.mdl, 224, 4);
  test::put(prop.mdl, 228, 1000);
  for (size_t i = 0; i < 4; ++i)
  {
    test::put(prop.mdl, 1000 + 2 * i, uint32_t(i % 3), 2);
  }
  return prop;
}

fs::TestFileSystem modelFileSystem(
  const test::SyntheticProp& prop,
  bool sidecars = true,
  bool mipmapped = false,
  const std::string& materialParameters = {},
  const uint32_t textureFlags = 0,
  const bool patchMaterial = false,
  const bool distinctSkinPixels = false)
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
  test::put(vtf, 20, textureFlags);
  test::put(vtf, 24, 1, 2);
  test::put(vtf, 52, 0);
  test::put(vtf, 56, 1, 1);
  test::put(vtf, 57, 0xffffffff);
  test::put(vtf, 63, 1, 2);
  test::put(vtf, 80, textureFlags ? 0x800000ff : 0xff0000ff);
  if (mipmapped)
  {
    vtf.resize(100);
    test::put(vtf, 16, 2, 2);
    test::put(vtf, 18, 2, 2);
    test::put(vtf, 56, 2, 1);
  }
  auto otherVtf = vtf;
  if (distinctSkinPixels)
  {
    test::put(otherVtf, 80, 0xff00ff00); // Green rather than family-zero red.
  }
  auto materials = std::vector<fs::Entry>{
    fs::FileEntry{
      "metal.vmt",
      file(
        patchMaterial ? "Patch { include materials/models/test/base.vmt replace { "
                        "$basetexture Models/Test/Pixel } }"
                      : "VertexLitGeneric { $basetexture Models/Test/Pixel "
                          + materialParameters + " }")},
    fs::FileEntry{
      "other.vmt",
      file(
        distinctSkinPixels ? "VertexLitGeneric { $basetexture models/test/other }"
                           : "VertexLitGeneric { $basetexture models/test/pixel }")},
    fs::FileEntry{
      "third.vmt", file("VertexLitGeneric { $basetexture models/test/third }")},
    fs::FileEntry{"other.vtf", file(otherVtf)},
    fs::FileEntry{"third.vtf", file(vtf)},
    fs::FileEntry{"pixel.vtf", file(vtf)}};
  if (patchMaterial)
  {
    auto baseImage = vtf;
    baseImage.resize(96);
    test::put(baseImage, 16, 2, 2);
    test::put(baseImage, 18, 2, 2);
    test::put(baseImage, 56, 1, 1);
    for (const auto offset : {80, 84, 88, 92})
    {
      test::put(baseImage, offset, 0xffff0000);
    }
    materials.push_back(fs::FileEntry{"basepixel.vtf", file(baseImage)});
    materials.push_back(
      fs::FileEntry{
        "base.vmt",
        file(
          "VertexLitGeneric { $basetexture Models/Test/BasePixel " + materialParameters
          + " }")});
  }
  entries.push_back(
    fs::DirectoryEntry{
      "materials",
      {fs::DirectoryEntry{
        "models", {fs::DirectoryEntry{"test", std::move(materials)}}}}});
  return fs::TestFileSystem{fs::DirectoryEntry{"", std::move(entries)}, {}};
}
fs::TestFileSystem bodyFileSystem(const test::SyntheticBodyProp& prop)
{
  auto materials = std::vector<fs::Entry>{};
  for (size_t i = 0; i < 8; ++i)
  {
    const auto name = "choice" + std::to_string(i);
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
    test::put(vtf, 80, 0xff000000 | uint32_t(i + 1));
    materials.push_back(fs::FileEntry{name + ".vtf", file(vtf)});
    materials.push_back(
      fs::FileEntry{
        name + ".vmt", file("VertexLitGeneric { $basetexture " + name + " }")});
  }
  return fs::TestFileSystem{
    fs::DirectoryEntry{
      "",
      {
        fs::FileEntry{"prop.mdl", file(prop.mdl)},
        fs::FileEntry{"prop.vvd", file(prop.vvd)},
        fs::FileEntry{"prop.dx90.vtx", file(prop.vtx)},
        fs::DirectoryEntry{"materials", std::move(materials)},
      }},
    {}};
}

GameInfo bodyGameInfo()
{
  return makeGameInfo(
    GameConfig{
      .name = "Test",
      .path = {},
      .icon = {},
      .fileFormats = {},
      .fileSystemConfig = {},
      .materialConfig = MaterialConfig{"", {}, "palette.lmp", {}, "", {}},
      .entityConfig = {},
      .faceAttribsConfig = {},
      .smartTags = {},
      .softMapBounds = {},
      .compilationTools = {},
    });
}
} // namespace
TEST_CASE("loadSourceModel")
{
  auto prop = test::SyntheticProp{};
  auto logger = NullLogger{};
  SECTION(
    "bodygroups select independent static surfaces, bounds, picks and skin families")
  {
    auto fs = bodyFileSystem(test::SyntheticBodyProp{{{false, false}, {false, false}}});
    auto result = loadSourceModel("prop.mdl", fs, logger);
    REQUIRE(result.is_success());
    const auto data = std::move(result) | kdl::value();
    REQUIRE(data.surfaceCount() == 4);
    CHECK(data.frameCount() == 1);
    for (uint32_t body = 0; body < 4; ++body)
    {
      CAPTURE(body);
      const auto first = size_t(body % 2);
      const auto second = size_t(2 + (body / 2) % 2);
      const auto* frame = data.frame(0, body);
      REQUIRE(frame != nullptr);
      CHECK(frame->index() == 0);
      CHECK(
        frame->bounds()
        == vm::bbox3f{
          {float(first * 10), 0, 0}, {float(100 + (second - 2) * 10 + 2), 3, 0}});
      for (size_t surface = 0; surface < 4; ++surface)
      {
        const auto active = surface == first || surface == second;
        CHECK(data.surfaceActive(surface, body) == active);
        const auto x = float(surface < 2 ? surface * 10 : 100 + (surface - 2) * 10);
        CHECK(
          frame->intersect(vm::ray3f{{x + 0.5f, 0.5f, 5}, {0, 0, -1}}).has_value()
          == active);
        for (size_t skin = 0; skin < 2; ++skin)
        {
          const auto* material = data.surface(surface).skin(skin);
          REQUIRE(material != nullptr);
          CHECK(
            material->name()
            == "materials/choice" + std::to_string(surface + skin * 4) + ".vmt");
          REQUIRE(material->texture() != nullptr);
          CHECK(
            material->texture()->buffersIfLoaded().front().data()[0]
            == surface + skin * 4 + 1);
        }
      }
      auto renderer = data.buildRenderer(1, 0, body);
      REQUIRE(renderer != nullptr);
      CHECK_FALSE(renderer->empty());
      CHECK(data.frame(0, body + 4) == frame);
      CHECK(data.bounds(0, body) == frame->bounds());
    }
    CHECK(data.frame(0) == data.frame(0, 0));
    CHECK(data.frame("static") == data.frame(0, 0));
    CHECK(data.frames().front().bounds() == data.bounds(0, 0));
    CHECK(data.frame(1, 3) == nullptr);
    CHECK(data.buildRenderer(0, 1, 3) == nullptr);
  }
  SECTION("body renderers share GPU geometry without extending its lifetime")
  {
    auto mockGl = gl::MockGl{};
    gl::installVboSupport(mockGl);
    auto vboManager = gl::VboManager{};
    const auto cleanup =
      kdl::invoke_later{[&]() { vboManager.destroyPendingVbos(mockGl); }};
    auto fs = bodyFileSystem(test::SyntheticBodyProp{{{false, false}, {false, false}}});
    const auto data = loadSourceModel("prop.mdl", fs, logger) | kdl::value();
    auto renderers = std::vector<std::unique_ptr<gl::MaterialRenderer>>{};
    for (size_t skin = 0; skin < 2; ++skin)
    {
      for (uint32_t body = 0; body < 4; ++body)
      {
        auto renderer = data.buildRenderer(skin, 0, body);
        REQUIRE(renderer != nullptr);
        renderer->prepare(mockGl, vboManager);
        renderers.push_back(std::move(renderer));
      }
      CHECK(vboManager.currentVboCount() == 4);
    }
    renderers.clear();
    vboManager.destroyPendingVbos(mockGl);
    CHECK(vboManager.currentVboCount() == 0);
    auto rebuilt = data.buildRenderer(0, 0, 0);
    REQUIRE(rebuilt != nullptr);
    rebuilt->prepare(mockGl, vboManager);
    CHECK(vboManager.currentVboCount() == 2);
    rebuilt.reset();
    vboManager.destroyPendingVbos(mockGl);
    CHECK(vboManager.currentVboCount() == 0);
  }
  SECTION("blank choices remove geometry without poisoning another body")
  {
    auto fs = bodyFileSystem(test::SyntheticBodyProp{{{false, true}, {false, true}}});
    const auto data = loadSourceModel("prop.mdl", fs, logger) | kdl::value();
    const auto* full = data.frame(0, 0);
    REQUIRE(full != nullptr);
    const auto fullBounds = full->bounds();
    CHECK(data.bounds(0, 1) == vm::bbox3f{{100, 0, 0}, {102, 3, 0}});
    CHECK(data.bounds(0, 2) == vm::bbox3f{{0, 0, 0}, {2, 3, 0}});
    const auto* blank = data.frame(0, 3);
    REQUIRE(blank != nullptr);
    CHECK(blank->bounds() == vm::bbox3f{0.0f});
    CHECK_FALSE(blank->intersect(vm::ray3f{{0.5f, 0.5f, 5}, {0, 0, -1}}));
    auto renderer = data.buildRenderer(0, 0, 3);
    REQUIRE(renderer != nullptr);
    CHECK(renderer->empty());
    CHECK(full->bounds() == fullBounds);
    CHECK(full->intersect(vm::ray3f{{0.5f, 0.5f, 5}, {0, 0, -1}}));
  }
  SECTION("body selection honors decoded noncanonical bases rather than surface ordinals")
  {
    auto fs =
      bodyFileSystem(test::SyntheticBodyProp{{{false, false}, {false, false}}, {3, 11}});
    const auto data = loadSourceModel("prop.mdl", fs, logger) | kdl::value();
    CHECK(data.frame(0, 0) == data.frame(0, 2));
    CHECK(data.surfaceActive(1, 3));
    CHECK(data.surfaceActive(2, 3));
    CHECK(data.surfaceActive(1, 11));
    CHECK(data.surfaceActive(3, 11));
    CHECK(data.surfaceActive(1, 4294967295u));
    CHECK(data.surfaceActive(2, 4294967295u));
  }
  SECTION(
    "simultaneous Source entities share geometry but not selected frame or renderer "
    "identity")
  {
    auto fs = bodyFileSystem(test::SyntheticBodyProp{});
    const auto gameInfo = bodyGameInfo();
    auto manager = EntityModelManager{
      gameInfo,
      fs,
      [](auto loader) { return gl::createResourceSync(std::move(loader)); },
      logger};
    auto parser = FgdParser{
      R"(@PointClass studio("prop.mdl") = probe [])", RgbaF{1, 1, 1, 1}, {}, true};
    auto status = TestParserStatus{};
    const auto definitions = parser.parseDefinitions(status) | kdl::value();
    REQUIRE(definitions.size() == 1);
    auto a = Entity{{{"body", "0"}}};
    auto b = Entity{{{"body", "3"}, {"skin", "1"}}};
    a.setDefinition(&definitions.front());
    b.setDefinition(&definitions.front());
    a.setModel(manager.model("prop.mdl"));
    b.setModel(manager.model("prop.mdl"));
    const auto specA = a.modelSpecification() | kdl::value();
    const auto specB = b.modelSpecification() | kdl::value();
    CHECK(specA == ModelSpecification{"prop.mdl", 0, 0, 0});
    CHECK(specB == ModelSpecification{"prop.mdl", 1, 0, 3});
    CHECK(specA != specB);
    CHECK(
      std::hash<ModelSpecification>{}(specA)
      != std::hash<ModelSpecification>{}(ModelSpecification{"prop.mdl", 0, 0, 3}));
    CHECK(a.model() == b.model());
    CHECK(a.modelFrame() == manager.frame(specA));
    CHECK(b.modelFrame() == manager.frame(specB));
    CHECK(a.modelFrame() != b.modelFrame());
    const auto* frameA = a.modelFrame();
    const auto boundsA = frameA->bounds();
    auto* rendererA = manager.renderer(specA);
    auto* rendererB = manager.renderer(specB);
    REQUIRE(rendererA != nullptr);
    REQUIRE(rendererB != nullptr);
    CHECK(rendererA != rendererB);
    CHECK(manager.renderer({"prop.mdl", 0, 0, 4}) == rendererA);
    b.addOrUpdateProperty("body", "0");
    b.addOrUpdateProperty("skin", "0");
    CHECK(b.modelFrame() == frameA);
    CHECK(manager.renderer(b.modelSpecification() | kdl::value()) == rendererA);
    CHECK(frameA->bounds() == boundsA);
    CHECK(manager.renderer(specB) == rendererB);
  }
  SECTION("Source fixed FGD studio entity skin selects material identity and pixels")
  {
    const auto* alias = GENERATE("studio", "studioprop");
    prop = withMaterialSkins(std::move(prop));
    test::put(prop.mdl, 224, 2); // Only metal and other skin families.
    auto fs = modelFileSystem(prop, true, false, {}, 0, false, true);
    const auto source =
      std::string{"@PointClass "} + alias + R"(("prop.mdl") = probe [])";
    auto parser = FgdParser{source, RgbaF{1, 1, 1, 1}, {}, true};
    auto status = TestParserStatus{};
    const auto definitions = parser.parseDefinitions(status) | kdl::value();
    REQUIRE(definitions.size() == 1);
    auto entity = Entity{{{"skin", "0"}, {"unrelated", "01.000"}}};
    entity.setDefinition(&definitions.front());
    for (const auto* raw : {"0", "1", "0"})
    {
      CAPTURE(alias, raw);
      entity.addOrUpdateProperty("skin", raw);
      const auto before = entity.properties();
      const auto spec = entity.modelSpecification() | kdl::value();
      const auto green = std::string{raw} == "1";
      CHECK(spec == ModelSpecification{"prop.mdl", green ? 1u : 0u, 0});
      auto result = loadEntityModelSync(
        fs,
        MaterialConfig{},
        spec.path,
        [](const auto&) -> gl::Material {
          throw std::runtime_error("unexpected legacy material callback");
        },
        logger);
      INFO((result.is_error() ? std::get<Error>(result.error()).msg : "success"));
      REQUIRE(!result.is_error());
      const auto model = std::move(result) | kdl::value();
      REQUIRE(model.data() != nullptr);
      const auto& surface = model.data()->surface(0);
      REQUIRE(surface.skinCount() == 2);
      const auto* selected = surface.skin(spec.skinIndex);
      REQUIRE(selected != nullptr);
      CHECK(
        selected->name()
        == (green ? "materials/models/test/other.vmt" : "materials/models/test/metal.vmt"));
      const auto* texture = selected->texture();
      REQUIRE(texture != nullptr);
      CHECK(texture->width() == 1);
      CHECK(texture->height() == 1);
      const auto& buffers = texture->buffersIfLoaded();
      REQUIRE(buffers.size() == 1);
      REQUIRE(buffers.front().size() == 4);
      const auto pixels = std::vector<unsigned char>{
        buffers.front().data(), buffers.front().data() + buffers.front().size()};
      CHECK(
        pixels
        == (green ? std::vector<unsigned char>{0, 255, 0, 255} : std::vector<unsigned char>{255, 0, 0, 255}));
      CHECK(model.data()->buildRenderer(spec.skinIndex, spec.frameIndex) != nullptr);
      CHECK(entity.properties() == before);
    }
  }
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
    prop = withMaterialSkins(std::move(prop));
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
TEST_CASE("Source VMT alpha and culling control brush and prop render policy")
{
  const auto modes = std::vector<std::tuple<std::string, img::ImageAlphaDomain, float>>{
    {"", img::ImageAlphaDomain::Opaque, 1.0f},
    {"$selfillum 1", img::ImageAlphaDomain::Opaque, 1.0f},
    {"$translucent 0", img::ImageAlphaDomain::Opaque, 1.0f},
    {"$alphatest 0", img::ImageAlphaDomain::Opaque, 1.0f},
    {"$translucent 1", img::ImageAlphaDomain::Graduated, 1.0f},
    {"$alphatest 1", img::ImageAlphaDomain::Binary, 1.0f},
    {"$translucent 1 $alphatest 1", img::ImageAlphaDomain::Binary, 1.0f},
    {"$translucent 1 $alpha 0", img::ImageAlphaDomain::Graduated, 0.0f},
    {"$translucent 1 $alpha 0.25", img::ImageAlphaDomain::Graduated, 0.25f},
    {"$translucent 1 $alpha 1", img::ImageAlphaDomain::Graduated, 1.0f},
  };
  auto taskManager = kdl::task_manager{2};
  auto logger = NullLogger{};
  for (const auto& [flags, patched, noCull] :
       {std::tuple{0u, false, false},
        std::tuple{0x1000u, false, false},
        std::tuple{0x2000u, false, false},
        std::tuple{0u, true, false},
        std::tuple{0x1000u, true, false},
        std::tuple{0x2000u, true, false},
        std::tuple{0u, false, true},
        std::tuple{0x1000u, false, true},
        std::tuple{0x2000u, false, true},
        std::tuple{0u, true, true},
        std::tuple{0x1000u, true, true},
        std::tuple{0x2000u, true, true}})
  {
    for (const auto reference : {0.5f, 0.25f, 0.75f})
    {
      for (const auto& [parameters, expected, opacity] : modes)
      {
        CAPTURE(flags, parameters, patched, noCull, reference, opacity);
        auto fs = modelFileSystem(
          withMaterialSkins(test::SyntheticProp{}),
          true,
          false,
          parameters + (noCull ? " $nocull 1" : " $nocull 0")
            + (reference == 0.5f ? "" : " $alphatestreference " + std::to_string(reference)),
          flags,
          patched);
        const auto raw =
          loadTexture("materials/models/test/pixel.vtf", "raw", fs, {}) | kdl::value();
        CHECK(
        raw.alphaDomain()
        == (flags == 0x2000u   ? img::ImageAlphaDomain::Graduated
            : flags == 0x1000u ? img::ImageAlphaDomain::Binary
                               : img::ImageAlphaDomain::Opaque));
        auto config = MaterialConfig{};
        config.root = "materials";
        config.extensions = {".vmt"};
        const auto collections =
          loadMaterialCollections(
            fs,
            config,
            [](auto loader) {
              return gl::createTextureResource(loader() | kdl::value());
            },
            taskManager,
            logger)
          | kdl::value();
        auto brush = static_cast<const gl::Material*>(nullptr);
        for (const auto& collection : collections)
          for (const auto& material : collection.materials())
            if (material.name() == "models/test/metal")
              brush = &material;
        REQUIRE(brush);
        const auto model = loadSourceModel("prop.mdl", fs, logger) | kdl::value();
        const auto* prop = model.surface(0).skin(0);
        REQUIRE(prop);
        const auto* sameImage = model.surface(0).skin(1);
        REQUIRE(sameImage);
        CHECK(sameImage->effectiveCulling() == gl::MaterialCulling::Default);
        REQUIRE(sameImage->texture());
        CHECK(
          sameImage->texture()->embeddedDefaults()
          == gl::EmbeddedDefaults{gl::SourceEmbeddedDefaults{}});
        CHECK(raw.embeddedDefaults() == gl::EmbeddedDefaults{gl::NoEmbeddedDefaults{}});
        for (const auto* material : {brush, prop})
        {
          REQUIRE(material->texture());
          // Patch base is2x2 and fallback32x32: only acted replacement decodes1x1.
          CHECK(material->texture()->width() == 1);
          CHECK(material->texture()->height() == 1);
          CHECK(material->texture()->alphaDomain() == expected);
          CHECK(
            material->texture()->embeddedDefaults()
            == gl::EmbeddedDefaults{
              gl::SourceEmbeddedDefaults{noCull, reference, opacity}});
          CHECK(material->culling() == gl::MaterialCulling::Default);
          CHECK(
            material->effectiveCulling()
            == (noCull ? gl::MaterialCulling::None : gl::MaterialCulling::Default));
          const auto blend = material->effectiveBlendFunc();
          CHECK(
            blend.enable
            == (expected == img::ImageAlphaDomain::Graduated ? gl::MaterialBlendFunc::Enable::UseFactors : gl::MaterialBlendFunc::Enable::UseDefault));
          if (expected == img::ImageAlphaDomain::Graduated)
          {
            CHECK(blend.srcFactor == GL_SRC_ALPHA);
            CHECK(blend.destFactor == GL_ONE_MINUS_SRC_ALPHA);
          }
          CHECK(
            material->effectiveAlphaFunc().has_value()
            == (expected == img::ImageAlphaDomain::Binary));
          if (const auto alpha = material->effectiveAlphaFunc())
          {
            CHECK(alpha->compare == gl::MaterialAlphaFunc::Compare::GreaterEqual);
            CHECK(alpha->threshold == reference);
          }
        }
      }
    }
  }
}
} // namespace tb::mdl
