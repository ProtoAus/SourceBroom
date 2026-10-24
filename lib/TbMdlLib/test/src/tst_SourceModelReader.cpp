/* Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later */
#include "SourceModelFixture.h"
#include "mdl/SourceModelReader.h"

#include <bit>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
using namespace test;
TEST_CASE("SourceModelReader")
{
  auto prop = SyntheticProp{};
  SECTION("authored classic versions produce vertices, winding, UVs, material and bounds")
  {
    for (const auto version : {44u, 48u, 49u})
    {
      put(prop.mdl, 4, version);
      const auto model = prop.read();
      REQUIRE(model.vertices.size() == 4);
      REQUIRE(model.meshes.size() == 1);
      CHECK(model.meshes[0].indices == std::vector<uint32_t>{0, 2, 1});
      CHECK(model.meshes[0].materials == std::vector<std::string>{"metal"});
      CHECK(model.materialDirectories == std::vector<std::string>{"models/test/"});
      CHECK(model.vertices[2].uv == std::array<float, 2>{0, 1});
      CHECK(model.min == std::array<float, 3>{0, 0, 0});
      CHECK(model.max == std::array<float, 3>{2, 3, 0});
    }
  }
  SECTION("equivalent lists and strips preserve oriented faces and skip connectors")
  {
    const auto indices =
      [](
        SyntheticProp fixture, const std::vector<uint32_t>& input, const uint32_t flags) {
        fixture.vtx.resize(227);
        put(fixture.vtx, 81, uint32_t(input.size()));
        put(fixture.vtx, 93, 127); // Move the strip header beyond the indices.
        for (size_t i = 0; i < input.size(); ++i)
          put(fixture.vtx, 134 + 2 * i, input[i], 2);
        put(fixture.vtx, 200, uint32_t(input.size()));
        put(fixture.vtx, 208, 4);
        put(fixture.vtx, 218, flags, 1);
        return fixture.read();
      };
    const auto orientedAreas = [](const SourceModelGeometry& geometry) {
      auto areas = std::vector<float>{};
      const auto& mesh = geometry.meshes[0];
      for (size_t i = 0; i < mesh.indices.size(); i += 3)
      {
        const auto& a = geometry.vertices[mesh.indices[i]].position;
        const auto& b = geometry.vertices[mesh.indices[i + 1]].position;
        const auto& c = geometry.vertices[mesh.indices[i + 2]].position;
        areas.push_back((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]));
      }
      return areas;
    };
    auto list = std::vector<uint32_t>{};
    auto strip = std::vector<uint32_t>{};
    SECTION("single triangle")
    {
      list = {0, 1, 2};
      strip = {0, 1, 2};
    }
    SECTION("both parities")
    {
      list = {0, 1, 2, 2, 1, 3};
      strip = {0, 1, 2, 3};
    }
    SECTION("degenerate connectors do not reset parity")
    {
      list = {0, 1, 2, 2, 1, 3, 0, 1, 2, 2, 1, 3};
      strip = {0, 1, 2, 3, 3, 0, 0, 1, 2, 3};
    }
    const auto listAreas = orientedAreas(indices(prop, list, 1));
    const auto stripAreas = orientedAreas(indices(prop, strip, 2));
    REQUIRE(listAreas.size() == list.size() / 3);
    CHECK(stripAreas == listAreas);
    for (const auto area : listAreas)
      CHECK(area < 0);
  }
  SECTION("all truncations refuse")
  {
    for (size_t i = 0; i < prop.mdl.size(); ++i)
      CHECK_THROWS_AS(
        readSourceModel(std::string_view{prop.mdl}.substr(0, i), prop.vvd, prop.vtx),
        std::runtime_error);
    for (size_t i = 0; i < 320; ++i)
      CHECK_THROWS_AS(
        readSourceModel(prop.mdl, std::string_view{prop.vvd}.substr(0, i), prop.vtx),
        std::runtime_error);
    for (size_t i = 0; i < 169; ++i)
      CHECK_THROWS_AS(
        readSourceModel(prop.mdl, prop.vvd, std::string_view{prop.vtx}.substr(0, i)),
        std::runtime_error);
  }
  SECTION("checksum, version, overflow, invalid offsets, indices and nonfinite controls")
  {
    const auto mutations = std::vector<std::array<uint32_t, 3>>{
      {0, 4, 50},
      {1, 4, 5},
      {2, 0, 8},
      {1, 8, 124},
      {2, 16, 124},
      {0, 236, 0xffffffff},
      {1, 16, 0x7fffffff},
      {2, 77, 0x7fffffff},
      {2, 81, 0xffffffff},
      {2, 134, 0xffff},
      {2, 102, 0xffff},
      {1, 80, 0x7f800000},
      {0, 152, 0},
      {0, 484, 2},
      {2, 160, 4}};
    for (const auto& mutation : mutations)
    {
      auto bad = prop;
      auto& file = mutation[0] == 0 ? bad.mdl : mutation[0] == 1 ? bad.vvd : bad.vtx;
      put(file, mutation[1], mutation[2]);
      CHECK_THROWS_AS(bad.read(), std::runtime_error);
    }
  }
  SECTION("LOD0 fixups reorder source vertices and reject overlap")
  {
    prop.vvd.resize(344);
    put(prop.vvd, 48, 2);
    put(prop.vvd, 52, 320);
    put(prop.vvd, 324, 2);
    put(prop.vvd, 328, 2);
    put(prop.vvd, 336, 0);
    put(prop.vvd, 340, 2);
    const auto geometry = prop.read();
    CHECK(geometry.vertices[0].position == std::array<float, 3>{0, 3, 0});
    CHECK(geometry.vertices[2].position == std::array<float, 3>{0, 0, 0});
    put(prop.vvd, 336, 1);
    CHECK_THROWS_AS(prop.read(), std::runtime_error);
  }
  SECTION("skin families remap material indices in order")
  {
    put(prop.mdl, 204, 2);
    put(prop.mdl, 208, 760);
    prop.mdl.resize(1100);
    put(prop.mdl, 76, uint32_t(prop.mdl.size()));
    put(prop.mdl, 760, 200);
    put(prop.mdl, 824, 146);
    prop.mdl.replace(960, 5, "first");
    prop.mdl.replace(970, 6, "second");
    put(prop.mdl, 212, 0);
    put(prop.mdl, 224, 2);
    put(prop.mdl, 228, 1000);
    put(prop.mdl, 1000, 1, 2);
    put(prop.mdl, 1002, 0, 2);
    CHECK(prop.read().meshes[0].materials == std::vector<std::string>{"second", "first"});
    put(prop.mdl, 1000, 2, 2);
    CHECK_THROWS_AS(prop.read(), std::runtime_error);
  }
  SECTION("invalid primitives, empty mesh and path traversal refuse")
  {
    put(prop.vtx, 160, 3, 1);
    CHECK_THROWS_AS(prop.read(), std::runtime_error);
    put(prop.vtx, 160, 1, 1);
    put(prop.vtx, 136, 0, 2);
    put(prop.vtx, 138, 0, 2);
    CHECK_THROWS_AS(prop.read(), std::runtime_error);
    prop = SyntheticProp{};
    prop.mdl.replace(808, 5, "../xx");
    CHECK_THROWS_AS(prop.read(), std::runtime_error);
  }
  SECTION("owned geometry survives input destruction")
  {
    const auto geometry = SyntheticProp{}.read();
    CHECK(geometry.meshes[0].materials[0] == "metal");
    CHECK(geometry.vertices[1].position[0] == 2);
  }
}
} // namespace tb::mdl
