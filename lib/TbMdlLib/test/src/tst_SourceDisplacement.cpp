/*
SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"
#include "mdl/SourceDisplacement.h"
#include "mdl/VmfDocument.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace tb::mdl
{
namespace
{
std::string displacementFixture()
{
  auto source = std::string{
    "side { \"id\" \"3\" dispinfo { \"power\" \"2\" \"startposition\" \"[0 0 0]\"\n"};
  source += "normals {\n";
  for (size_t y = 0; y < 5; ++y)
  {
    source += "\"row" + std::to_string(y) + "\" \"0 0 1  0 0 1  0 0 1  0 0 1  0 0 1\"\n";
  }
  source += "}\ndistances { // keep these spellings\n";
  for (size_t y = 0; y < 5; ++y)
  {
    source += "\"row" + std::to_string(y) + "\" \"0.00  +0  " + (y == 2 ? "16" : "0")
              + "  0 0\"\n";
  }
  source += "}\ntriangle_tags { row0 \"0 0 0 0 0 0 0 0\" } future { key \"opaque\" } } }";
  return source;
}
const auto corners =
  std::vector<vm::vec3d>{{0, 0, 0}, {64, 0, 0}, {64, 64, 0}, {0, 64, 0}};
const auto bounds = vm::bbox3d{vm::vec3d{-128, -128, -128}, vm::vec3d{128, 128, 128}};
SourceDisplacement loadDisplacement(const std::string& source)
{
  const auto document = VmfDocument::parse(source);
  auto result =
    SourceDisplacement::read(document, document.roots()[0], corners, {0, 0, 1}, bounds);
  if (!result)
  {
    throw std::runtime_error{"fixture has no displacement"};
  }
  return std::move(*result);
}
std::string replaceOnce(
  std::string source, const std::string& from, const std::string& to)
{
  const auto pos = source.find(from);
  REQUIRE(pos != std::string::npos);
  source.replace(pos, from.size(), to);
  return source;
}
} // namespace

TEST_CASE("SourceDisplacement")
{
  const auto source = displacementFixture();
  const auto document = VmfDocument::parse(source);
  const auto original = loadDisplacement(source);
  SECTION("native samples tessellate with alternating triangle diagonals")
  {
    CHECK(original.power() == 2);
    CHECK(original.size() == 5);
    REQUIRE(original.positions().size() == 25);
    CHECK(original.indices().size() == 96);
    CHECK(original.positions()[12] == vm::vec3d{32, 32, 16});
    CHECK(original.basePositions()[12] == vm::vec3d{32, 32, 0});
    CHECK(original.bounds().max.z() == 16);
    CHECK(original.bounds().min.z() == 0);
    const auto first = std::vector<uint32_t>{0, 5, 6, 0, 6, 1, 1, 6, 2, 2, 6, 7};
    CHECK(std::equal(first.begin(), first.end(), original.indices().begin()));
    for (const auto& normal : original.normals())
    {
      CHECK(normal.z() > 0);
      CHECK(vm::dot(normal, normal) == Catch::Approx(1.0));
    }
    const auto hit = original.intersect(vm::ray3d{{32, 32, 64}, {0, 0, -1}});
    REQUIRE(hit);
    CHECK(*hit == Catch::Approx(48.0));
    CHECK_FALSE(original.intersect(vm::ray3d{{80, 80, 64}, {0, 0, -1}}));
  }
  SECTION("one sample edit is surgical, independently copied and reversible")
  {
    auto edited = original;
    REQUIRE(edited.canEditDistances());
    edited.setDistance(2, 2, 24);
    CHECK(original.distance(2, 2) == 16);
    CHECK(edited.positions()[12] == vm::vec3d{32, 32, 24});
    CHECK(
      document.serialize(edited.editsFrom(original))
      == replaceOnce(source, "+0  16  0", "+0  24  0"));
    edited.setDistance(2, 2, 16);
    CHECK(edited == original);
    CHECK(document.serialize(edited.editsFrom(original)) == source);
    CHECK_THROWS_AS(edited.setDistance(2, 2, 1000), std::invalid_argument);
    CHECK(edited == original);
    CHECK_THROWS_AS(
      edited.setDistance(2, 2, std::numeric_limits<double>::infinity()),
      std::invalid_argument);
    CHECK(edited == original);
    CHECK_THROWS_AS(edited.setDistance(5, 2, 0), std::out_of_range);
  }
  SECTION("pure translation moves the mesh and patches only start coordinates")
  {
    const auto delta = vm::vec3d{16, 8, 4};
    const auto moved = original.translated(delta);
    REQUIRE(moved.baseTranslationFrom(original));
    CHECK(*moved.baseTranslationFrom(original) == delta);
    for (size_t i = 0; i < original.positions().size(); ++i)
    {
      CHECK(moved.positions()[i] == original.positions()[i] + delta);
      CHECK(moved.basePositions()[i] == original.basePositions()[i] + delta);
      CHECK(vm::is_equal(moved.normals()[i], original.normals()[i], 0.000001));
    }
    CHECK(moved.indices() == original.indices());
    CHECK(moved.alphas() == original.alphas());
    CHECK(moved.distance(2, 2) == 16);
    CHECK(moved.bounds().max == original.bounds().max + delta);
    const auto edits = moved.editsFrom(original);
    REQUIRE(edits.size() == 1);
    const auto saved = document.serialize(edits);
    CHECK(saved == replaceOnce(source, "[0 0 0]", "[16 8 4]"));
    const auto parsed = VmfDocument::parse(saved);
    auto newCorners = corners;
    for (auto& corner : newCorners)
    {
      corner = corner + delta;
    }
    const auto reopened =
      SourceDisplacement::read(parsed, parsed.roots()[0], newCorners, {0, 0, 1}, bounds);
    REQUIRE(reopened);
    CHECK(reopened->positions() == moved.positions());
    CHECK(moved.translated(-delta) == original);
    CHECK(document.serialize(moved.translated(-delta).editsFrom(original)) == source);
    CHECK_THROWS_AS(original.translated({1000, 0, 0}), std::invalid_argument);
    CHECK_THROWS_AS(
      original.translated({0, 0, std::numeric_limits<double>::infinity()}),
      std::invalid_argument);
    CHECK(original.positions()[12] == vm::vec3d{32, 32, 16});

    const auto spelled = replaceOnce(source, "[0 0 0]", "[0.00  +0 0]");
    const auto authored = loadDisplacement(spelled);
    CHECK(
      VmfDocument::parse(spelled).serialize(
        authored.translated({0, 8, 0}).editsFrom(authored))
      == replaceOnce(spelled, "[0.00  +0 0]", "[0.00  8 0]"));
  }
  SECTION("offset and elevation act exactly once")
  {
    auto text = replaceOnce(
      source,
      "normals {",
      "\"elevation\" \"2\" offsets { row2 \"0 0 0 0 0 0 0 0 3 0 0 0 0 0 0\" } normals {");
    const auto edited = loadDisplacement(text);
    CHECK(edited.positions()[12] == vm::vec3d{32, 32, 21});
    CHECK(edited.basePositions()[12].z() == 0);
  }
  SECTION("malformed rows and ambiguous provenance refuse")
  {
    for (const auto& malformed :
         {replaceOnce(source, "\"power\" \"2\"", "\"power\" \"7\""),
          replaceOnce(source, "[0 0 0]", "[1 1 0]"),
          replaceOnce(source, "+0  16  0", "+0  nan  0"),
          replaceOnce(source, "+0  16  0", "+0  16  0 0"),
          replaceOnce(source, "distances {", "distances { row0 \"0 0 0 0 0\""),
          replaceOnce(source, "distances {", "distances { row5 \"0 0 0 0 0\""),
          replaceOnce(source, "normals {", "alphas { row0 \"0 0 256 0 0\" } normals {")})
    {
      CHECK_THROWS_AS(loadDisplacement(malformed), std::invalid_argument);
    }
    auto nonquad = corners;
    nonquad.pop_back();
    CHECK_THROWS_AS(
      SourceDisplacement::read(document, document.roots()[0], nonquad, {0, 0, 1}, bounds),
      std::invalid_argument);
  }
  SECTION("plain sides are unchanged and empty grids stay bounded")
  {
    const auto plain = VmfDocument::parse("side { id 3 }");
    CHECK_FALSE(
      SourceDisplacement::read(plain, plain.roots()[0], corners, {0, 0, 1}, bounds));
    const auto large =
      loadDisplacement("side { dispinfo { power 6 startposition \"[0 0 0]\" } }");
    CHECK(large.positions().size() == 4225);
    CHECK(large.indices().size() == 24576);
    CHECK_FALSE(large.canEditDistances());
  }
}
} // namespace tb::mdl
