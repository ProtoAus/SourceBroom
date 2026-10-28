/*
SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/generators/catch_generators.hpp"
#include "mdl/SourceDisplacement.h"
#include "mdl/VmfDocument.h"

#include "vm/mat_ext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

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
std::string alphaFixture()
{
  auto rows = std::string{"alphas { // preserve scalar spelling\n"};
  for (size_t row = 0; row < 5; ++row)
  {
    rows += "\"row" + std::to_string(row) + "\" \"000.0 +64  128.00   192 255\"\n";
  }
  rows += "}\ntriangle_tags {";
  return replaceOnce(displacementFixture(), "triangle_tags {", rows);
}
std::string offsetFixture(const size_t rowCount = 5)
{
  auto rows = std::string{"offsets { // preserve vector spelling\n"};
  for (size_t row = 0; row < rowCount; ++row)
  {
    rows += "\"row" + std::to_string(row)
            + "\" \"+0 0.00 -0   +0 0.00 -0   +0 0.00 -0   +0 0.00 -0   +0 0.00 -0\"\n";
  }
  rows += "}\ntriangle_tags {";
  return replaceOnce(displacementFixture(), "triangle_tags {", rows);
}
const auto sewTileShifts =
  std::array<vm::vec3d, 4>{vm::vec3d{0, 0, 0}, {64, 0, 0}, {0, 64, 0}, {64, 64, 0}};
size_t sewCorner(const SourceDisplacement& mesh, const vm::vec3d& point = {64, 64, 0})
{
  const auto it =
    std::find(mesh.basePositions().begin(), mesh.basePositions().end(), point);
  REQUIRE(it != mesh.basePositions().end());
  return size_t(it - mesh.basePositions().begin());
}
std::vector<SourceDisplacement> sewTiles(const size_t count, const bool raised = true)
{
  const auto original = loadDisplacement(alphaFixture());
  auto result = std::vector<SourceDisplacement>{};
  for (size_t i = 0; i < count; ++i)
  {
    result.push_back(original.translated(sewTileShifts.at(i)));
    if (raised)
    {
      const auto index = sewCorner(result.back());
      result.back().setDistance(index / 5, index % 5, 4.0 * double(i + 1));
    }
  }
  return result;
}
std::vector<SourceDisplacement*> sewPointers(std::vector<SourceDisplacement>& meshes)
{
  auto result = std::vector<SourceDisplacement*>{};
  for (auto& mesh : meshes)
  {
    result.push_back(&mesh);
  }
  return result;
}
} // namespace

TEST_CASE("SourceDisplacement batch admits mixed powers with coarse edge interpolation")
{
  auto meshes = sewTiles(3);
  auto first = meshes[0];
  auto second = meshes[1];
  REQUIRE(first.canSewWith(second));
  REQUIRE(first.sewWith(second));
  REQUIRE(first.positions()[sewCorner(first)].z() == 6); // Same-power pair ACTS.
  meshes[2] =
    loadDisplacement("side { dispinfo { power 3 startposition \"[0 0 0]\" } }")
      .translated({0, 64, 0}); // Exact fixture promoted from the old batch refusal.
  REQUIRE(meshes[2].power() == 3);
  REQUIRE(meshes[2].canInitializeOffsets());
  CHECK_FALSE(meshes[0].canSewWith(meshes[2])); // Legacy strict pair API stays strict.
  const auto before = meshes;
  const auto pointers = sewPointers(meshes);
  REQUIRE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  REQUIRE(SourceDisplacement::sewTogether(pointers));
  for (size_t i = 0; i < meshes.size(); ++i)
  {
    CHECK(meshes[i].positions()[sewCorner(meshes[i])].z() == 4); // mean(4,8,0).
    CHECK(meshes[i].basePositions() == before[i].basePositions());
    CHECK(meshes[i].indices() == before[i].indices());
    CHECK(meshes[i].alphas() == before[i].alphas());
    CHECK(meshes[i].power() == before[i].power());
  }
  CHECK(meshes[0] == before[0]); // Original4 is already at the common mean4.
  CHECK(meshes[0].canInitializeOffsets());
  const auto sewn = meshes;
  CHECK_FALSE(SourceDisplacement::sewTogether(pointers));
  CHECK(meshes == sewn);
}

TEST_CASE("SourceDisplacement sews connected position groups without corner bias")
{
  const auto count = GENERATE(size_t{3}, size_t{4});
  const auto reverse = GENERATE(false, true);
  const auto existingOffsets = GENERATE(false, true);
  auto meshes = sewTiles(count);
  if (existingOffsets)
  {
    meshes[0].initializeOffsets();
  }
  const auto before = meshes;
  auto first = meshes[0];
  auto second = meshes[1];
  REQUIRE(first.canSewWith(second));
  REQUIRE(first.sewWith(second));
  REQUIRE(first.positions()[sewCorner(first)].z() == 6); // Pair ACTS before batch.
  auto pair = std::vector<SourceDisplacement>{meshes[0], meshes[1]};
  REQUIRE(SourceDisplacement::sewTogether(sewPointers(pair)));
  CHECK(pair[0] == first);
  CHECK(pair[1] == second);
  auto pointers = sewPointers(meshes);
  if (reverse)
  {
    std::reverse(pointers.begin(), pointers.end());
  }
  REQUIRE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  REQUIRE(SourceDisplacement::sewTogether(pointers));
  const auto expected = count == 3 ? 8.0 : 10.0;
  const auto source = alphaFixture();
  const auto document = VmfDocument::parse(source);
  const auto original = loadDisplacement(source);
  for (size_t i = 0; i < count; ++i)
  {
    const auto& mesh = meshes[i];
    CHECK(mesh.positions()[sewCorner(mesh)].z() == expected);
    CHECK(mesh.basePositions() == before[i].basePositions());
    CHECK(mesh.indices() == before[i].indices());
    CHECK(mesh.alphas() == before[i].alphas());
    for (size_t j = 0; j < mesh.positions().size(); ++j)
    {
      CHECK(mesh.distance(j / 5, j % 5) == before[i].distance(j / 5, j % 5));
      if (mesh.basePositions()[j].x() != 64 && mesh.basePositions()[j].y() != 64)
      {
        CHECK(mesh.positions()[j] == before[i].positions()[j]);
      }
    }
    auto structural = std::vector<VmfStructuralEdit>{};
    const auto edits = mesh.editsFrom(original, structural);
    const auto expectedAppend = count == 3 && i == 1 ? 0u : 1u; // Original8 IS mean8.
    REQUIRE(structural.size() == expectedAppend);
    const auto saved = document.serialize(edits, structural);
    CHECK(saved.find("future { key \"opaque\" }") != std::string::npos);
    CHECK(saved.find("000.0 +64  128.00   192 255") != std::string::npos);
    auto shiftedCorners = corners;
    for (auto& corner : shiftedCorners)
    {
      corner = corner + sewTileShifts[i];
    }
    const auto parsed = VmfDocument::parse(saved);
    const auto reopened = SourceDisplacement::read(
      parsed, parsed.roots()[0], shiftedCorners, {0, 0, 1}, bounds);
    REQUIRE(reopened);
    CHECK(reopened->positions() == mesh.positions());
    CHECK(reopened->alphas() == mesh.alphas());
  }
  const auto sewn = meshes;
  CHECK_FALSE(SourceDisplacement::sewTogether(pointers));
  CHECK(meshes == sewn);
}

TEST_CASE("SourceDisplacement mixed powers close the full analytic XYZ edge curve")
{
  const auto source = [](const unsigned power) {
    return "side { dispinfo { power " + std::to_string(power)
           + " startposition \"[0 0 0]\" } }";
  };
  for (const auto powers : {std::pair{2u, 3u}, std::pair{2u, 6u}, std::pair{3u, 5u}})
  {
    for (const auto reversedFrame : {false, true})
    {
      for (const auto reversedSelection : {false, true})
      {
        INFO(
          powers.first << "/" << powers.second << " frame=" << reversedFrame
                       << " selection=" << reversedSelection);
        auto low = loadDisplacement(source(powers.first));
        auto high = loadDisplacement(source(powers.second));
        if (reversedFrame)
        {
          auto rotation = vm::mat4x4d::identity();
          rotation[0][0] = -1;
          rotation[1][1] = -1;
          high = high.rigidlyTransformed(rotation).translated({128, 64, 0});
        }
        else
        {
          high = high.translated({64, 0, 0});
        }
        const auto lowPeak = sewCorner(low, {64, 32, 0});
        const auto highStep = 64.0 / double(high.size() - 1);
        const auto highBump = sewCorner(high, {64, 32 + highStep, 0});
        low.initializeOffsets();
        high.initializeOffsets();
        low.setOffset(lowPeak / low.size(), lowPeak % low.size(), {8, 4, 8});
        high.setOffset(highBump / high.size(), highBump % high.size(), {6, -2, 6});
        const auto beforeLow = low;
        const auto beforeHigh = high;
        REQUIRE(beforeHigh.positions()[highBump].z() == 6);
        auto pointers = std::vector<SourceDisplacement*>{&low, &high};
        if (reversedSelection)
        {
          std::reverse(pointers.begin(), pointers.end());
        }
        REQUIRE(
          SourceDisplacement::canSewTogether(
            std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
        REQUIRE(SourceDisplacement::sewTogether(pointers));
        // XYZ mean, not Z only.
        REQUIRE(low.positions()[lowPeak] == vm::vec3d{68, 34, 4});
        const auto spacing = 64.0 / double(low.size() - 1);
        const auto expectedBump = 4 * (1 - highStep / spacing);
        REQUIRE(high.positions()[highBump].z() == expectedBump); // Fine-only6 must ACT.
        for (const auto& pair :
             {std::pair{&low, &beforeLow}, std::pair{&high, &beforeHigh}})
        {
          const auto& mesh = *pair.first;
          const auto& before = *pair.second;
          CHECK(mesh.basePositions() == before.basePositions());
          CHECK(mesh.indices() == before.indices());
          CHECK(mesh.alphas() == before.alphas());
          CHECK(mesh.power() == before.power());
          CHECK(mesh.canEditDistances() == before.canEditDistances());
          for (size_t i = 0; i < mesh.positions().size(); ++i)
          {
            const auto& base = mesh.basePositions()[i];
            if (base.x() == 64)
            {
              const auto t = std::max(0.0, 1 - std::abs(base.y() - 32) / spacing);
              CHECK(
                mesh.positions()[i] == vm::vec3d{64 + 4 * t, base.y() + 2 * t, 4 * t});
            }
            else
            {
              CHECK(mesh.positions()[i] == before.positions()[i]);
            }
          }
        }
        const auto sewnLow = low;
        const auto sewnHigh = high;
        CHECK_FALSE(SourceDisplacement::sewTogether(pointers));
        CHECK(low == sewnLow);
        CHECK(high == sewnHigh);
        if (
          powers.first == 2 && powers.second == 6 && reversedFrame && !reversedSelection)
        {
          const auto text = source(powers.second);
          const auto document = VmfDocument::parse(text);
          auto structural = std::vector<VmfStructuralEdit>{};
          const auto edits = high.editsFrom(loadDisplacement(text), structural);
          const auto saved = document.serialize(edits, structural);
          const auto parsed = VmfDocument::parse(saved);
          const auto size = high.size();
          const auto& base = high.basePositions();
          const auto face = std::vector<vm::vec3d>{
            base[0], base[size - 1], base[size * size - 1], base[(size - 1) * size]};
          const auto reopened =
            SourceDisplacement::read(parsed, parsed.roots()[0], face, {0, 0, 1}, bounds);
          REQUIRE(reopened);
          CHECK(reopened->positions() == high.positions());
          CHECK(reopened->indices() == high.indices());
        }
      }
    }
  }
}

TEST_CASE("SourceDisplacement mixed-power junctions share one original-coordinate mean")
{
  const auto count = GENERATE(size_t{3}, size_t{4});
  const auto reverse = GENERATE(false, true);
  const auto powers = std::array<unsigned, 4>{2, 3, 4, 6};
  auto meshes = std::vector<SourceDisplacement>{};
  for (size_t i = 0; i < count; ++i)
  {
    meshes.push_back(loadDisplacement(
                       "side { dispinfo { power " + std::to_string(powers[i])
                       + " startposition \"[0 0 0]\" } }")
                       .translated(sewTileShifts[i]));
    auto& mesh = meshes.back();
    const auto corner = sewCorner(mesh);
    mesh.initializeOffsets();
    mesh.setOffset(
      corner / mesh.size(), corner % mesh.size(), {0, 0, 4.0 * double(i + 1)});
  }
  const auto before = meshes;
  auto pointers = sewPointers(meshes);
  if (reverse)
  {
    std::reverse(pointers.begin(), pointers.end());
  }
  REQUIRE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  REQUIRE(SourceDisplacement::sewTogether(pointers));
  const auto mean = count == 3 ? 8.0 : 10.0;
  // Shared edges by tile index; each fine sample follows that edge's coarser spacing.
  const auto shared = std::array<std::pair<size_t, size_t>, 4>{
    std::pair{size_t{0}, size_t{1}}, {0, 2}, {1, 3}, {2, 3}};
  for (size_t i = 0; i < count; ++i)
  {
    const auto& mesh = meshes[i];
    CHECK(mesh.positions()[sewCorner(mesh)] == vm::vec3d{64, 64, mean});
    CHECK(mesh.basePositions() == before[i].basePositions());
    CHECK(mesh.indices() == before[i].indices());
    CHECK(mesh.power() == before[i].power());
    for (size_t j = 0; j < mesh.positions().size(); ++j)
    {
      const auto& base = mesh.basePositions()[j];
      auto expected = before[i].positions()[j];
      for (const auto& [a, b] : shared)
      {
        if ((a != i && b != i) || a >= count || b >= count)
        {
          continue;
        }
        const auto other = a == i ? b : a;
        const auto alongX = sewTileShifts[i].y() != sewTileShifts[other].y();
        if ((alongX ? base.y() : base.x()) != 64)
        {
          continue;
        }
        const auto spacing =
          64.0 / double(std::min(mesh.size(), meshes[other].size()) - 1);
        const auto along = std::abs((alongX ? base.x() : base.y()) - 64);
        expected = base + vm::vec3d{0, 0, mean * std::max(0.0, 1 - along / spacing)};
      }
      CHECK(mesh.positions()[j] == expected);
    }
  }
  const auto sewn = meshes;
  CHECK_FALSE(SourceDisplacement::sewTogether(pointers));
  CHECK(meshes == sewn);
}

TEST_CASE("SourceDisplacement mixed-power seams keep protected data and atomic bounds")
{
  // Power-3 rows are 9 wide; a relabelled 5-wide fixture throws in read, not in sew.
  const auto power3 = [](const size_t offsetRows, const std::string& table = "offsets") {
    auto source = std::string{"side { dispinfo { power 3 startposition \"[0 0 0]\"\n"};
    if (offsetRows != 0)
    {
      source += table + " {\n";
      for (size_t row = 0; row < offsetRows; ++row)
      {
        source += "row" + std::to_string(row) + " \"";
        for (size_t column = 0; column < 9; ++column)
        {
          source += "0 0 0 ";
        }
        source += "\"\n";
      }
      source += "}\n";
    }
    auto result = loadDisplacement(source + "} }");
    REQUIRE(result.power() == 3);
    return result;
  };
  const auto eligible = [](const SourceDisplacement& a, const SourceDisplacement& b) {
    return SourceDisplacement::canSewTogether({&a, &b});
  };
  auto meshes = sewTiles(3);
  meshes[2] = power3(9).translated({0, 64, 0});
  auto control = meshes;
  REQUIRE(SourceDisplacement::sewTogether(sewPointers(control)));
  REQUIRE(control[2].positions()[sewCorner(control[2])].z() == 4); // Full table ACTS.
  SECTION("partial higher-power offsets")
  {
    meshes[2] = power3(1).translated({0, 64, 0});
  }
  SECTION("aliased higher-power offsets")
  {
    meshes[2] = power3(9, "Offsets").translated({0, 64, 0});
  }
  SECTION("ambiguous mixed overlap")
  {
    meshes[2] = power3(0); // Exactly over meshes[0]: every edge matches.
    // Ambiguous as a pair, not merely an edge with two partners.
    CHECK_FALSE(eligible(meshes[0], meshes[2]));
  }
  SECTION("disconnected mixed mesh")
  {
    meshes[2] = power3(0).translated({-128, 0, 0});
  }
  SECTION("non-manifold mixed edge")
  {
    auto rotation = vm::mat4x4d::identity();
    rotation[0][0] = 0;
    rotation[2][2] = 0;
    rotation[0][2] = 1;
    rotation[2][0] = -1;
    meshes[2] = power3(0).rigidlyTransformed(rotation).translated({64, 0, 0});
    REQUIRE(eligible(meshes[0], meshes[2]));
    REQUIRE(eligible(meshes[1], meshes[2]));
  }
  SECTION("mixed base tolerance cannot chain into a wider corner cluster")
  {
    meshes[1] = meshes[1].translated({-0.75e-6, 0, 0});
    meshes[2] = power3(0).translated({0, 64 - 0.75e-6, 0});
    REQUIRE(eligible(meshes[0], meshes[1]));
    REQUIRE(eligible(meshes[0], meshes[2]));
  }
  const auto before = meshes;
  const auto pointers = sewPointers(meshes);
  CHECK_FALSE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  CHECK_THROWS(SourceDisplacement::sewTogether(pointers));
  CHECK(meshes == before);
}

TEST_CASE("SourceDisplacement batch refuses T-junctions that sewing would open")
{
  const auto source = [](const unsigned power) {
    return "side { dispinfo { power " + std::to_string(power)
           + " startposition \"[0 0 0]\" } }";
  };
  const auto document = VmfDocument::parse(source(3));
  // x 64..128, y 0..128: 16-unit spacing, like a power-2 tile beside it.
  const auto tall = SourceDisplacement::read(
    document,
    document.roots()[0],
    {{0, 0, 0}, {64, 0, 0}, {64, 128, 0}, {0, 128, 0}},
    {0, 0, 1},
    bounds);
  REQUIRE(tall);
  const auto raise = [](
                       SourceDisplacement& mesh, const vm::vec3d& point, const double z) {
    const auto index = sewCorner(mesh, point);
    if (mesh.canInitializeOffsets())
    {
      mesh.initializeOffsets();
    }
    mesh.setOffset(index / mesh.size(), index % mesh.size(), {0, 0, z});
  };
  const auto at = [](const SourceDisplacement& mesh, const vm::vec3d& point) {
    return mesh.positions()[sewCorner(mesh, point)];
  };
  // Tall, B1 left of it, B2 above B1, C below tall, D below B1; full edges link
  // tall-C-D-B1(-B2), but tall's left edge only touches B1 and B2.
  auto meshes = std::vector<SourceDisplacement>{
    tall->translated({64, 0, 0}),
    loadDisplacement(source(2)),
    loadDisplacement(source(2)).translated({0, 64, 0}),
    loadDisplacement(source(2)).translated({64, -64, 0}),
    loadDisplacement(source(2)).translated({0, -64, 0})};
  auto subset = std::vector<size_t>{1, 2, 3, 4};
  auto crack = std::function<double()>{};
  SECTION("junction on a coarser edge sample")
  {
    raise(meshes[0], {64, 64, 0}, 8);
    raise(meshes[1], {64, 64, 0}, 8); // Tall and B1 already meet; B2 is at 0.
    crack = [&] {
      return at(meshes[0], {64, 64, 0}).z() - at(meshes[1], {64, 64, 0}).z();
    };
  }
  SECTION("finer edge lying along part of a coarser edge")
  {
    meshes[1] = loadDisplacement(source(3));
    meshes.erase(meshes.begin() + 2); // No B2.
    subset = {1, 2, 3};
    raise(meshes[2], {64, 0, 0}, 8); // C's corner moves the tall/B1/C/D junction.
    crack = [&] {
      const auto edge = (at(meshes[0], {64, 0, 0}) + at(meshes[0], {64, 16, 0})) / 2.0;
      return edge.z() - at(meshes[1], {64, 8, 0}).z();
    };
  }
  REQUIRE(crack() == 0);
  auto control = std::vector<SourceDisplacement>{};
  for (const auto i : subset)
  {
    control.push_back(meshes[i]);
  }
  REQUIRE(SourceDisplacement::sewTogether(sewPointers(control))); // Without tall: ACTS.
  const auto before = meshes;
  const auto pointers = sewPointers(meshes);
  CHECK_FALSE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  CHECK_THROWS(SourceDisplacement::sewTogether(pointers));
  CHECK(meshes == before);
  CHECK(crack() == 0);
}

TEST_CASE("SourceDisplacement mixed-power seams move finer-only samples alone")
{
  auto low = loadDisplacement("side { dispinfo { power 2 startposition \"[0 0 0]\" } }");
  auto high = loadDisplacement("side { dispinfo { power 3 startposition \"[0 0 0]\" } }")
                .translated({64, 0, 0});
  const auto bump = sewCorner(high, {64, 40, 0}); // Between low's samples at 32 and 48.
  high.initializeOffsets();
  high.setOffset(bump / high.size(), bump % high.size(), {0, 0, 6});
  const auto beforeLow = low;
  auto pointers = std::vector<SourceDisplacement*>{&low, &high};
  REQUIRE(SourceDisplacement::sewTogether(pointers)); // No coincident group changes.
  CHECK(high.positions()[bump] == vm::vec3d{64, 40, 0});
  CHECK(low == beforeLow);
  CHECK(low.canInitializeOffsets()); // The unchanged coarse mesh gets no table.
  const auto sewn = high;
  CHECK_FALSE(SourceDisplacement::sewTogether(pointers));
  CHECK(high == sewn);
}

TEST_CASE("SourceDisplacement mixed-power seams close a rotated inexact edge")
{
  const auto turn = vm::rotation_matrix(vm::vec3d{0, 0, 1}, 0.37);
  const auto lowFlat =
    loadDisplacement("side { dispinfo { power 2 startposition \"[0 0 0]\" } }");
  const auto highFlat =
    loadDisplacement("side { dispinfo { power 3 startposition \"[0 0 0]\" } }")
      .translated({64, 0, 0});
  auto lowEdge = std::vector<size_t>{};
  for (size_t k = 0; k < 5; ++k)
  {
    lowEdge.push_back(sewCorner(lowFlat, {64, 16.0 * double(k), 0}));
  }
  auto highEdge = std::vector<size_t>{};
  for (size_t k = 0; k < 9; ++k)
  {
    highEdge.push_back(sewCorner(highFlat, {64, 8.0 * double(k), 0}));
  }
  auto low = lowFlat.rigidlyTransformed(turn);
  auto high = highFlat.rigidlyTransformed(turn);
  REQUIRE(low.basePositions()[lowEdge[2]] != lowFlat.basePositions()[lowEdge[2]]);
  low.initializeOffsets();
  low.setOffset(lowEdge[2] / 5, lowEdge[2] % 5, {0.3, -0.2, 8});
  high.initializeOffsets();
  high.setOffset(highEdge[5] / 9, highEdge[5] % 9, {0.1, 0.4, 6}); // Finer-only, y=40.
  auto pointers = std::vector<SourceDisplacement*>{&low, &high};
  REQUIRE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  REQUIRE(SourceDisplacement::sewTogether(pointers));
  for (size_t k = 0; k < 9; ++k)
  {
    const auto& a = low.positions()[lowEdge[k / 2]];
    const auto& b = low.positions()[lowEdge[std::min(k / 2 + 1, size_t{4})]];
    const auto fraction = double(k % 2) / 2;
    CHECK(
      vm::length(high.positions()[highEdge[k]] - (a * (1 - fraction) + b * fraction))
      <= 1e-9);
  }
  CHECK(vm::length(high.positions()[highEdge[5]] - low.positions()[lowEdge[2]]) > 1);
  const auto sewnLow = low;
  const auto sewnHigh = high;
  CHECK_FALSE(SourceDisplacement::sewTogether(pointers));
  CHECK(low == sewnLow);
  CHECK(high == sewnHigh);
}

TEST_CASE("SourceDisplacement mixed-power seam stages a later world-bound failure")
{
  const auto smallCorners =
    std::vector<vm::vec3d>{{0, 0, 0}, {8, 0, 0}, {8, 8, 0}, {0, 8, 0}};
  const auto read = [&](const unsigned power, const double extent) {
    const auto document = VmfDocument::parse(
      "side { dispinfo { power " + std::to_string(power)
      + " startposition \"[0 0 0]\" } }");
    return *SourceDisplacement::read(
      document, document.roots()[0], smallCorners, {0, 0, 1}, vm::bbox3d{extent});
  };
  auto meshes = std::vector<SourceDisplacement>{
    read(2, 128), read(2, 128).translated({8, 0, 0}), read(6, 16).translated({0, 8, 0})};
  meshes[0].initializeOffsets();
  meshes[0].setOffset(4, 4, {0, 0, 8});
  auto control = meshes;
  REQUIRE(SourceDisplacement::sewTogether(sewPointers(control)));
  REQUIRE(control[2].positions()[sewCorner(control[2], {8, 8, 0})].z() > 2);
  meshes[0].setOffset(4, 4, {0, 0, 96});
  const auto before = meshes;
  const auto pointers = sewPointers(meshes);
  REQUIRE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  CHECK_THROWS(SourceDisplacement::sewTogether(pointers)); // Mean32 exceeds LAST box16.
  CHECK(meshes == before);
}

TEST_CASE("SourceDisplacement batch refuses unsupported seam graphs without mutation")
{
  auto meshes = sewTiles(3);
  auto control = meshes;
  REQUIRE(SourceDisplacement::sewTogether(sewPointers(control)));
  REQUIRE(control[2].positions()[sewCorner(control[2])].z() == 8);
  auto pointers = sewPointers(meshes);
  SECTION("empty")
  {
    pointers.clear();
  }
  SECTION("single")
  {
    pointers.resize(1);
  }
  SECTION("null")
  {
    pointers[2] = nullptr;
  }
  SECTION("duplicate pointer")
  {
    pointers[2] = pointers[0];
  }
  SECTION("disconnected")
  {
    meshes[2] = loadDisplacement(alphaFixture()).translated({-128, 0, 0});
  }
  SECTION("vertex only")
  {
    meshes[2] = loadDisplacement(alphaFixture()).translated({-64, -64, 0});
  }
  SECTION("ambiguous overlap")
  {
    meshes[2] = loadDisplacement(alphaFixture());
  }
  // The exact former mixed-power refusal is promoted to the positive batch case above.
  SECTION("partial offsets")
  {
    meshes[2] = loadDisplacement(offsetFixture(1)).translated({0, 64, 0});
  }
  SECTION("aliased offsets")
  {
    meshes[2] = loadDisplacement(replaceOnce(offsetFixture(), "offsets {", "Offsets {"))
                  .translated({0, 64, 0});
  }
  SECTION("unsupported mesh")
  {
    meshes[2] = SourceDisplacement{};
  }
  SECTION("more than one selected partner for a base edge")
  {
    auto rotation = vm::mat4x4d::identity();
    rotation[0][0] = 0;
    rotation[2][2] = 0;
    rotation[0][2] = 1;
    rotation[2][0] = -1;
    meshes[2] = loadDisplacement(alphaFixture())
                  .rigidlyTransformed(rotation)
                  .translated({64, 0, 0});
    REQUIRE(meshes[0].canSewWith(meshes[2]));
    REQUIRE(meshes[1].canSewWith(meshes[2]));
  }
  SECTION("base tolerance cannot chain into a wider corner cluster")
  {
    meshes[1] = loadDisplacement(alphaFixture()).translated({64 - 0.75e-6, 0, 0});
    meshes[2] = loadDisplacement(alphaFixture()).translated({0, 64 - 0.75e-6, 0});
    REQUIRE(meshes[0].canSewWith(meshes[1]));
    REQUIRE(meshes[0].canSewWith(meshes[2]));
  }
  const auto before = meshes;
  CHECK_FALSE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  CHECK_THROWS(SourceDisplacement::sewTogether(pointers));
  CHECK(meshes == before);
}

TEST_CASE("SourceDisplacement batch preserves close and unchanged missing offset tables")
{
  auto meshes = sewTiles(4, false);
  const auto before = meshes;
  auto pointers = sewPointers(meshes);
  REQUIRE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  CHECK_FALSE(SourceDisplacement::sewTogether(pointers));
  CHECK(meshes == before);
  for (const auto& mesh : meshes)
  {
    CHECK(mesh.canInitializeOffsets());
  }
  meshes.resize(3);
  const auto firstCorner = sewCorner(meshes[0]);
  const auto lastCorner = sewCorner(meshes[2]);
  meshes[0].setDistance(firstCorner / 5, firstCorner % 5, 8);
  meshes[2].setDistance(lastCorner / 5, lastCorner % 5, 16);
  const auto unchanged = meshes[0];
  REQUIRE(SourceDisplacement::sewTogether(sewPointers(meshes)));
  CHECK(meshes[0] == unchanged); // mean(8,0,16)=8 needs no first-mesh table/rebuild.
  CHECK(meshes[0].canInitializeOffsets());
  CHECK(meshes[1].canEditOffsets());
  CHECK(meshes[2].canEditOffsets());
  CHECK(meshes[2].positions()[lastCorner].z() == 8);
}

TEST_CASE("SourceDisplacement batch bounds mesh count while sewing a connected strip")
{
  const auto scale = 1.0 / 64.0;
  const auto original =
    loadDisplacement(alphaFixture())
      .uniformlyScaled(vm::scaling_matrix(vm::vec3d{scale, scale, scale}));
  auto meshes = std::vector<SourceDisplacement>{};
  for (size_t i = 0; i < SourceDisplacement::MaxSewMeshes; ++i)
  {
    meshes.push_back(original.translated({double(i), 0, 0}));
  }
  meshes[0].setDistance(2, 4, 8);
  const auto untouchedLast = meshes.back();
  auto pointers = sewPointers(meshes);
  REQUIRE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  REQUIRE(SourceDisplacement::sewTogether(pointers));
  CHECK(meshes[0].positions()[14].z() == 4);
  CHECK(meshes[1].positions()[10].z() == 4);
  CHECK(meshes.back() == untouchedLast);
  CHECK(meshes.back().canInitializeOffsets());
  meshes.push_back(original.translated({double(SourceDisplacement::MaxSewMeshes), 0, 0}));
  const auto before = meshes;
  pointers = sewPointers(meshes);
  CHECK_FALSE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  CHECK_THROWS(SourceDisplacement::sewTogether(pointers));
  CHECK(meshes == before);
}

TEST_CASE("SourceDisplacement batch stages every mesh before a later world-bound failure")
{
  const auto document = VmfDocument::parse(displacementFixture());
  const auto smallCorners =
    std::vector<vm::vec3d>{{0, 0, 0}, {8, 0, 0}, {8, 8, 0}, {0, 8, 0}};
  const auto read = [&](const double extent) {
    return *SourceDisplacement::read(
      document, document.roots()[0], smallCorners, {0, 0, 1}, vm::bbox3d{extent});
  };
  auto meshes = std::vector<SourceDisplacement>{
    read(128), read(128).translated({8, 0, 0}), read(16).translated({0, 8, 0})};
  meshes[0].initializeOffsets();
  meshes[0].setOffset(4, 4, {0, 0, 8});
  auto control = meshes;
  REQUIRE(SourceDisplacement::sewTogether(sewPointers(control)));
  const auto common = sewCorner(control[2], {8, 8, 0});
  REQUIRE(control[2].positions()[common].z() > 2);
  meshes[0].setOffset(4, 4, {0, 0, 96});
  const auto before = meshes;
  const auto pointers = sewPointers(meshes);
  REQUIRE(
    SourceDisplacement::canSewTogether(
      std::vector<const SourceDisplacement*>{pointers.begin(), pointers.end()}));
  CHECK_THROWS(SourceDisplacement::sewTogether(pointers)); // Mean32 exceeds LAST box16.
  CHECK(meshes == before);
}

TEST_CASE("SourceDisplacement batch joins maximum65 grids without topology changes")
{
  const auto original =
    loadDisplacement("side { dispinfo { power 6 startposition \"[0 0 0]\" } }");
  auto meshes = std::vector<SourceDisplacement>{
    original, original.translated({64, 0, 0}), original.translated({0, 64, 0})};
  meshes[0].initializeOffsets();
  meshes[0].setOffset(64, 64, {0, 0, 12});
  const auto before = meshes;
  REQUIRE(SourceDisplacement::sewTogether(sewPointers(meshes)));
  for (size_t i = 0; i < meshes.size(); ++i)
  {
    const auto& mesh = meshes[i];
    CHECK(mesh.size() == 65);
    CHECK(mesh.positions()[sewCorner(mesh)].z() == 4);
    CHECK(mesh.indices() == before[i].indices());
    CHECK(mesh.basePositions() == before[i].basePositions());
    CHECK(mesh.alphas() == before[i].alphas());
  }
}

TEST_CASE("SourceDisplacement sews only one supported same-power base edge")
{
  const auto rotated = GENERATE(false, true);
  const auto source = displacementFixture();
  const auto document = VmfDocument::parse(source);
  const auto original = loadDisplacement(source);
  auto first = original;
  auto second = original;
  auto rotation = vm::mat4x4d::identity();
  if (rotated)
  {
    rotation[0][0] = 0;
    rotation[1][1] = 0;
    rotation[0][1] = 1;
    rotation[1][0] = -1;
    second = second.rigidlyTransformed(rotation).translated({128, 0, 0});
  }
  else
  {
    second = second.translated({64, 0, 0});
  }
  first.setDistance(2, 4, 16);
  REQUIRE(first.positions()[14].z() == 16);
  REQUIRE(document.serialize(first.editsFrom(original)) != source);
  REQUIRE(first.canSewWith(second));
  const auto firstBefore = first;
  const auto secondBefore = second;
  auto controlFirst = first;
  auto controlSecond = second;
  REQUIRE(controlFirst.sewWith(controlSecond));
  REQUIRE(controlFirst.offset(2, 4).z() == -8);
  SECTION("staged offset surgery preserves interiors and round trips rotated orientation")
  {
    REQUIRE(first.sewWith(second));
    CHECK(first.positions()[14].z() == 8);
    CHECK(first.canEditOffsets());
    CHECK(second.canEditOffsets());
    CHECK(first.basePositions() == firstBefore.basePositions());
    CHECK(second.basePositions() == secondBefore.basePositions());
    CHECK(first.indices() == firstBefore.indices());
    CHECK(second.indices() == secondBefore.indices());
    CHECK(first.alphas() == firstBefore.alphas());
    CHECK(second.alphas() == secondBefore.alphas());
    auto unchanged = true;
    for (size_t i = 0; i < 25; ++i)
    {
      unchanged &= first.distance(i / 5, i % 5) == firstBefore.distance(i / 5, i % 5);
      unchanged &= second.distance(i / 5, i % 5) == secondBefore.distance(i / 5, i % 5);
      if (first.basePositions()[i].x() != 64)
      {
        unchanged &= first.positions()[i] == firstBefore.positions()[i];
      }
      if (second.basePositions()[i].x() != 64)
      {
        unchanged &= second.positions()[i] == secondBefore.positions()[i];
      }
    }
    CHECK(unchanged);
    auto structure = std::vector<VmfStructuralEdit>{};
    const auto values = first.editsFrom(original, structure);
    REQUIRE(structure.size() == 1);
    const auto saved = document.serialize(values, structure);
    CHECK(loadDisplacement(saved).positions() == first.positions());
    structure.clear();
    const auto otherValues = second.editsFrom(original, structure);
    REQUIRE(structure.size() == 1);
    const auto otherSaved = document.serialize(otherValues, structure);
    auto newCorners = corners;
    for (auto& corner : newCorners)
    {
      corner =
        rotated ? rotation * corner + vm::vec3d{128, 0, 0} : corner + vm::vec3d{64, 0, 0};
    }
    const auto parsed = VmfDocument::parse(otherSaved);
    const auto reopened =
      SourceDisplacement::read(parsed, parsed.roots()[0], newCorners, {0, 0, 1}, bounds);
    REQUIRE(reopened);
    CHECK(reopened->positions() == second.positions());
    CHECK(otherSaved.find("future { key \"opaque\" }") != std::string::npos);
    const auto sameFirst = first;
    const auto sameSecond = second;
    CHECK_FALSE(first.sewWith(second));
    CHECK(first == sameFirst);
    CHECK(second == sameSecond);
  }
  SECTION("ambiguous disconnected partial aliased and different-power sources refuse")
  {
    const auto smaller =
      original.uniformlyScaled(vm::scaling_matrix(vm::vec3d{0.5, 0.5, 0.5}));
    const auto differentPower =
      loadDisplacement("side { dispinfo { power 3 startposition \"[0 0 0]\" } }");
    for (auto invalid : std::vector<SourceDisplacement>{
           original,
           original.translated({64, 64, 0}),
           smaller.translated({65, 0, 0}),
           loadDisplacement(offsetFixture(1)).translated({64, 0, 0}),
           loadDisplacement(replaceOnce(offsetFixture(), "offsets {", "Offsets {"))
             .translated({64, 0, 0}),
           differentPower.translated({64, 0, 0}),
           SourceDisplacement{}})
    {
      const auto before = invalid;
      CHECK_FALSE(first.canSewWith(invalid));
      CHECK_THROWS(first.sewWith(invalid));
      CHECK(first == firstBefore);
      CHECK(invalid == before);
    }
    CHECK_FALSE(first.canSewWith(first));
    CHECK_THROWS(first.sewWith(first));
    CHECK(first == firstBefore);
  }
  SECTION("already-close missing tables are lexical noops without initialization")
  {
    first = original;
    CHECK_FALSE(first.sewWith(second));
    CHECK(first == original);
    CHECK(second == secondBefore);
    CHECK(first.canInitializeOffsets());
    CHECK(document.serialize(first.editsFrom(original)) == source);
  }
  SECTION("maximum65 grid uses its full shared edge without topology edits")
  {
    auto large =
      loadDisplacement("side { dispinfo { power 6 startposition \"[0 0 0]\" } }");
    auto adjacent = large.translated({64, 0, 0});
    large.initializeOffsets();
    large.setOffset(32, 64, {0, 0, 8});
    const auto topology = large.indices();
    REQUIRE(large.canSewWith(adjacent));
    REQUIRE(large.sewWith(adjacent));
    CHECK(large.positions()[32 * 65 + 64].z() == 4);
    CHECK(adjacent.positions()[32 * 65].z() == 4);
    CHECK(large.indices() == topology);
    CHECK(adjacent.indices() == topology);
  }
}

TEST_CASE("SourceDisplacement seam staging is atomic on a later world-bound failure")
{
  const auto source = displacementFixture();
  const auto parsed = VmfDocument::parse(source);
  const auto smallCorners =
    std::vector<vm::vec3d>{{0, 0, 0}, {8, 0, 0}, {8, 8, 0}, {0, 8, 0}};
  const auto read = [&](const double extent) {
    return *SourceDisplacement::read(
      parsed, parsed.roots()[0], smallCorners, {0, 0, 1}, vm::bbox3d{extent});
  };
  auto first = read(128);
  auto second = read(16).translated({8, 0, 0});
  auto controlFirst = first;
  auto controlSecond = second;
  controlFirst.setDistance(2, 4, 8);
  REQUIRE(controlFirst.sewWith(controlSecond));
  REQUIRE(controlFirst.positions()[14].z() == 4);
  first.initializeOffsets();
  first.setOffset(2, 4, {70, 0, 0});
  REQUIRE(first.positions()[14].x() == 78);
  REQUIRE(first.canSewWith(second));
  const auto beforeFirst = first;
  const auto beforeSecond = second;
  CHECK_THROWS(first.sewWith(second)); // Midpoint43 fits first, not second's16-unit box.
  CHECK(first == beforeFirst);
  CHECK(second == beforeSecond);
}

TEST_CASE("SourceDisplacement XYZ offset edits preserve native bindings")
{
  const auto source = offsetFixture();
  const auto document = VmfDocument::parse(source);
  const auto original = loadDisplacement(source);
  REQUIRE(original.canEditOffsets());
  REQUIRE_FALSE(original.canInitializeOffsets());
  auto control = original;
  control.setDistance(2, 2, 24);
  REQUIRE(document.serialize(control.editsFrom(original)) != source);
  control.setDistance(2, 2, 16);
  REQUIRE(document.serialize(control.editsFrom(original)) == source);
  auto edited = original;
  SECTION("one vector is surgical with derived geometry picking and exact reversal")
  {
    const auto offset = vm::vec3d{1.25, -2.5, 3.75};
    edited.setOffset(2, 2, offset);
    CHECK(edited.offset(2, 2) == offset);
    CHECK(original.offset(2, 2) == vm::vec3d{});
    CHECK(edited.positions()[12] == original.positions()[12] + offset);
    CHECK(edited.basePositions() == original.basePositions());
    CHECK(edited.indices() == original.indices());
    CHECK(edited.alphas() == original.alphas());
    CHECK(edited.distance(2, 2) == original.distance(2, 2));
    CHECK(edited.normals() != original.normals());
    CHECK(edited.bounds().max.z() == 19.75);
    const auto hit = edited.intersect(vm::ray3d{{33.25, 29.5, 100}, {0, 0, -1}});
    REQUIRE(hit);
    CHECK(*hit == Catch::Approx(80.25));
    const auto edits = edited.editsFrom(original);
    REQUIRE(edits.size() == 1);
    const auto saved = document.serialize(edits);
    CHECK(
      saved
      == replaceOnce(
        source,
        "\"row2\" \"+0 0.00 -0   +0 0.00 -0   +0 0.00 -0   +0 0.00 -0   +0 0.00 -0\"",
        "\"row2\" \"+0 0.00 -0   +0 0.00 -0   1.25 -2.5 3.75   +0 0.00 -0   +0 0.00 "
        "-0\""));
    const auto reopened = loadDisplacement(saved);
    CHECK(reopened.offsets() == edited.offsets());
    CHECK(reopened.positions() == edited.positions());
    CHECK(reopened.normals() == edited.normals());
    CHECK(reopened.bounds() == edited.bounds());
    CHECK(reopened.canEditOffsets());
    edited.setOffset(2, 2, {});
    CHECK(edited == original);
    CHECK(document.serialize(edited.editsFrom(original)) == source);
  }
  SECTION("same parsed zeros preserve their original spelling")
  {
    edited.setOffset(2, 2, {});
    CHECK(edited == original);
    CHECK(edited.editsFrom(original).empty());
    CHECK(document.serialize(edited.editsFrom(original)) == source);
  }
  SECTION("nonfinite out of world and invalid coordinates refuse atomically")
  {
    for (const auto& value : std::vector<vm::vec3d>{
           {1000, 0, 0},
           {0, 1000, 0},
           {0, 0, 1000},
           {std::numeric_limits<double>::infinity(), 0, 0},
           {0, std::numeric_limits<double>::quiet_NaN(), 0}})
    {
      CHECK_THROWS_AS(edited.setOffset(2, 2, value), std::invalid_argument);
      CHECK(edited == original);
    }
    CHECK_THROWS_AS(edited.setOffset(5, 2, {}), std::out_of_range);
    CHECK_THROWS_AS(edited.setOffset(2, 5, {}), std::out_of_range);
    CHECK_THROWS_AS(edited.offset(5, 2), std::out_of_range);
    CHECK_THROWS_AS(edited.offset(2, 5), std::out_of_range);
    CHECK(edited == original);
  }
  SECTION("foreign same-value row spelling is not native edit authority")
  {
    const auto foreign = loadDisplacement(
      replaceOnce(source, "\"row2\" \"+0 0.00 -0", "\"row2\" \"0e0 0.00 -0"));
    CHECK(foreign.offsets() == original.offsets());
    CHECK_THROWS_AS(foreign.editsFrom(original), std::invalid_argument);
  }
  SECTION("offsets compose with alpha distance and admitted base motion")
  {
    edited.setOffset(2, 2, {1, 2, 3});
    edited.setDistance(2, 2, 24);
    edited.initializeAlphas();
    edited.setAlpha(2, 2, 17.5);
    const auto delta = vm::vec3d{4, 8, 0};
    auto rotation = vm::mat4x4d::identity();
    rotation[1][1] = 0;
    rotation[2][2] = 0;
    rotation[1][2] = 1;
    rotation[2][1] = -1;
    const auto scale = 1.25;
    const auto beforeMotion = edited;
    edited = edited.translated(delta).rigidlyTransformed(rotation).uniformlyScaled(
      vm::scaling_matrix(vm::vec3d{scale, scale, scale}));
    CHECK(edited.offset(2, 2) == vm::vec3d{1.25, -3.75, 2.5});
    CHECK(
      edited.positions()[12]
      == scale * (rotation * (beforeMotion.positions()[12] + delta)));
    CHECK(edited.alpha(2, 2) == 17.5);
    edited.setOffset(2, 2, edited.offset(2, 2) + vm::vec3d{0.5, 0.25, -0.5});
    auto structure = std::vector<VmfStructuralEdit>{};
    const auto values = edited.editsFrom(original, structure);
    REQUIRE(structure.size() == 1);
    const auto saved = document.serialize(values, structure);
    auto newCorners = corners;
    for (auto& corner : newCorners)
    {
      corner = scale * (rotation * (corner + delta));
    }
    const auto parsed = VmfDocument::parse(saved);
    const auto reopened = SourceDisplacement::read(
      parsed,
      parsed.roots()[0],
      newCorners,
      vm::slice<3, 3>(rotation, 0, 0) * vm::vec3d{0, 0, 1},
      bounds);
    REQUIRE(reopened);
    CHECK(reopened->positions() == edited.positions());
    CHECK(reopened->offsets() == edited.offsets());
    CHECK(reopened->alphas() == edited.alphas());
  }
}

TEST_CASE("SourceDisplacement explicitly initializes only absent offset tables")
{
  const auto source = displacementFixture();
  const auto document = VmfDocument::parse(source);
  const auto original = loadDisplacement(source);
  REQUIRE(original.canInitializeOffsets());
  REQUIRE_FALSE(original.canEditOffsets());
  auto control = original;
  control.setDistance(2, 2, 24);
  REQUIRE(document.serialize(control.editsFrom(original)) != source);
  control.setDistance(2, 2, 16);
  REQUIRE(document.serialize(control.editsFrom(original)) == source);
  auto edited = original;
  CHECK_THROWS(edited.setOffset(2, 2, {1, 2, 3}));
  CHECK(edited == original);
  SECTION(
    "explicit bounded zero table then vector edit and independent alpha initialization")
  {
    edited.initializeOffsets();
    REQUIRE(edited.canEditOffsets());
    CHECK_FALSE(edited.canInitializeOffsets());
    CHECK_THROWS(edited.initializeOffsets());
    CHECK(edited.offsets() == std::vector<vm::vec3d>(25));
    CHECK(edited.positions() == original.positions());
    CHECK_THROWS(edited.editsFrom(original));
    edited.setOffset(2, 2, {1.25, -2.5, 3.75});
    edited.initializeAlphas();
    edited.setAlpha(2, 2, 17.5);
    auto structure = std::vector<VmfStructuralEdit>{};
    const auto values = edited.editsFrom(original, structure);
    CHECK(values.empty());
    REQUIRE(structure.size() == 1); // Both tables share the same native parent.
    const auto output = document.serialize(values, structure);
    const auto reopened = loadDisplacement(output);
    CHECK(reopened.canEditOffsets());
    CHECK_FALSE(reopened.canInitializeOffsets());
    CHECK(reopened.offset(2, 2) == vm::vec3d{1.25, -2.5, 3.75});
    CHECK(reopened.positions() == edited.positions());
    CHECK(reopened.basePositions() == original.basePositions());
    CHECK(reopened.alpha(2, 2) == 17.5);
    CHECK(output.find("future { key \"opaque\" }") != std::string::npos);
    CHECK(output.find("triangle_tags { row0 \"0 0 0 0 0 0 0 0\" }") != std::string::npos);
    CHECK(VmfDocument::parse(output).serialize(reopened.editsFrom(reopened)) == output);
    edited = original;
    structure.clear();
    CHECK(document.serialize(edited.editsFrom(original, structure), structure) == source);
    CHECK(structure.empty());
  }
  SECTION("present empty partial and complete tables are not overwritten")
  {
    for (const auto rows : {size_t{0}, size_t{1}, size_t{5}})
    {
      const auto input = offsetFixture(rows);
      auto mesh = loadDisplacement(input);
      const auto before = mesh;
      CHECK_FALSE(mesh.canInitializeOffsets());
      CHECK_THROWS(mesh.initializeOffsets());
      CHECK(mesh == before);
      CHECK(mesh.canEditOffsets() == (rows == 5));
      if (rows != 5)
      {
        CHECK_THROWS(mesh.setOffset(2, 2, {1, 2, 3}));
        CHECK(mesh == before);
      }
      mesh.setDistance(2, 2, 24);
      CHECK_FALSE(mesh.editsFrom(before).empty());
    }
  }
  SECTION("maximum bounded grid writes every vector without changing geometry")
  {
    const auto input =
      std::string{"side { dispinfo { power 6 startposition \"[0 0 0]\" } }"};
    auto mesh = loadDisplacement(input);
    REQUIRE(mesh.size() == 65);
    REQUIRE(mesh.canInitializeOffsets());
    mesh.initializeOffsets();
    auto structure = std::vector<VmfStructuralEdit>{};
    const auto values = mesh.editsFrom(loadDisplacement(input), structure);
    REQUIRE(structure.size() == 1);
    const auto reopened =
      loadDisplacement(VmfDocument::parse(input).serialize(values, structure));
    CHECK(reopened.offsets() == std::vector<vm::vec3d>(65 * 65));
    CHECK(reopened.positions() == mesh.positions());
  }
  SECTION("case-aliased table names preserve loading without granting authoring")
  {
    for (const auto& input : std::vector<std::string>{
           replaceOnce(offsetFixture(), "offsets {", "Offsets {"),
           replaceOnce(offsetFixture(), "offsets {", "Offsets {} offsets {"),
           replaceOnce(source, "triangle_tags {", "Offsets scalar triangle_tags {")})
    {
      auto mesh = loadDisplacement(input);
      const auto before = mesh;
      CHECK_FALSE(mesh.canEditOffsets());
      CHECK_FALSE(mesh.canInitializeOffsets());
      CHECK_THROWS(mesh.initializeOffsets());
      CHECK_THROWS(mesh.setOffset(2, 2, {1, 2, 3}));
      CHECK(mesh == before);
      mesh.setDistance(2, 2, 24);
      CHECK_FALSE(mesh.editsFrom(before).empty());
    }
  }
  SECTION("foreign present bindings and malformed rows remain protected")
  {
    edited.initializeOffsets();
    edited.setOffset(2, 2, {1, 2, 3});
    auto structure = std::vector<VmfStructuralEdit>{};
    CHECK_THROWS(edited.editsFrom(loadDisplacement(offsetFixture()), structure));
    CHECK(structure.empty());
    CHECK_THROWS(loadDisplacement(replaceOnce(offsetFixture(), "+0 0.00 -0", "nan 0 0")));
    CHECK_THROWS(loadDisplacement(replaceOnce(offsetFixture(), "\"row1\"", "\"row0\"")));
  }
}

TEST_CASE("SourceDisplacement explicitly initializes only absent alpha tables")
{
  const auto source = displacementFixture();
  const auto document = VmfDocument::parse(source);
  const auto original = loadDisplacement(source);
  REQUIRE(original.canInitializeAlphas());
  REQUIRE_FALSE(original.canEditAlphas());
  auto control = original;
  control.setDistance(2, 2, 24);
  CHECK(document.serialize(control.editsFrom(original)) != source);
  control.setDistance(2, 2, 16);
  CHECK(document.serialize(control.editsFrom(original)) == source);
  CHECK_THROWS(original.editsFrom(loadDisplacement(alphaFixture())));
  auto edited = original;
  CHECK_THROWS(edited.setAlpha(2, 2, 17.5));
  CHECK(edited == original);

  SECTION("explicit zero table then fractional edit round trips without geometry changes")
  {
    edited.initializeAlphas();
    CHECK(edited.canEditAlphas());
    CHECK_FALSE(edited.canInitializeAlphas());
    CHECK_THROWS(edited.initializeAlphas());
    CHECK(edited.alphas() == std::vector<double>(25, 0));
    CHECK_THROWS(
      edited.editsFrom(original)); // The value-only caller must not drop creation.
    auto structure = std::vector<VmfStructuralEdit>{};
    const auto values = edited.editsFrom(original, structure);
    REQUIRE(values.empty());
    REQUIRE(structure.size() == 1);
    CHECK(structure.front().type == VmfStructuralEdit::Type::Append);
    const auto initialized = document.serialize(values, structure);
    const auto reloaded = loadDisplacement(initialized);
    CHECK(reloaded.canEditAlphas());
    CHECK_FALSE(reloaded.canInitializeAlphas());
    CHECK(reloaded.alphas() == std::vector<double>(25, 0));
    edited.setAlpha(2, 2, 17.5);
    CHECK_THROWS(edited.setAlpha(0, 0, -1));
    CHECK_THROWS(edited.setAlpha(0, 0, 256));
    CHECK_THROWS(edited.setAlpha(0, 0, std::numeric_limits<double>::quiet_NaN()));
    structure.clear();
    const auto editedValues = edited.editsFrom(original, structure);
    const auto output = document.serialize(editedValues, structure);
    const auto roundTrip = loadDisplacement(output);
    CHECK(roundTrip.alpha(2, 2) == 17.5);
    CHECK(roundTrip.positions() == original.positions());
    CHECK(roundTrip.basePositions() == original.basePositions());
    CHECK(roundTrip.normals() == original.normals());
    CHECK(roundTrip.indices() == original.indices());
    CHECK(roundTrip.bounds() == original.bounds());
    CHECK(output.find("// keep these spellings") != std::string::npos);
    CHECK(output.find("future { key \"opaque\" }") != std::string::npos);
    CHECK(output.find("triangle_tags { row0 \"0 0 0 0 0 0 0 0\" }") != std::string::npos);
    CHECK(VmfDocument::parse(output).serialize(roundTrip.editsFrom(roundTrip)) == output);
    edited = original;
    structure.clear();
    CHECK(document.serialize(edited.editsFrom(original, structure), structure) == source);
    CHECK(structure.empty());
  }
  SECTION("present empty partial and complete tables are never overwritten")
  {
    for (const auto& candidate : std::vector<std::string>{
           replaceOnce(source, "triangle_tags {", "alphas {} triangle_tags {"),
           replaceOnce(
             source, "triangle_tags {", "alphas { row0 \"0 0 0 0 0\" } triangle_tags {"),
           alphaFixture()})
    {
      auto mesh = loadDisplacement(candidate);
      const auto before = mesh;
      CHECK_FALSE(mesh.canInitializeAlphas());
      CHECK_THROWS(mesh.initializeAlphas());
      CHECK(mesh == before);
      CHECK(VmfDocument::parse(candidate).serialize(mesh.editsFrom(before)) == candidate);
    }
  }
  SECTION("existing maximum power is bounded and serializes all zero samples")
  {
    const auto input =
      std::string{"side { dispinfo { power 6 startposition \"[0 0 0]\" } }"};
    auto mesh = loadDisplacement(input);
    REQUIRE(mesh.size() == 65);
    REQUIRE(mesh.canInitializeAlphas());
    mesh.initializeAlphas();
    auto structure = std::vector<VmfStructuralEdit>{};
    const auto values = mesh.editsFrom(loadDisplacement(input), structure);
    const auto result =
      loadDisplacement(VmfDocument::parse(input).serialize(values, structure));
    CHECK(result.alphas() == std::vector<double>(65 * 65, 0));
    CHECK(result.positions() == mesh.positions());
  }
  SECTION("case-aliased alpha tables preserve loading but refuse authoring")
  {
    for (const auto& input : std::vector<std::string>{
           replaceOnce(alphaFixture(), "alphas {", "Alphas {"),
           replaceOnce(alphaFixture(), "alphas {", "Alphas {} alphas {")})
    {
      auto mesh = loadDisplacement(input);
      const auto before = mesh;
      CHECK_FALSE(mesh.canInitializeAlphas());
      CHECK_FALSE(mesh.canEditAlphas());
      CHECK_THROWS(mesh.initializeAlphas());
      CHECK_THROWS(mesh.setAlpha(2, 2, 17.5));
      CHECK(mesh == before);
      mesh.setDistance(2, 2, 24);
      CHECK_FALSE(mesh.editsFrom(before).empty());
    }
  }
  SECTION("foreign present-table provenance cannot become missing-table authority")
  {
    edited.initializeAlphas();
    auto structure = std::vector<VmfStructuralEdit>{};
    CHECK_THROWS(edited.editsFrom(loadDisplacement(alphaFixture()), structure));
    CHECK(structure.empty());
  }
}

TEST_CASE("SourceDisplacement alpha samples preserve geometry and numeric provenance")
{
  const auto source = alphaFixture();
  const auto document = VmfDocument::parse(source);
  const auto original = loadDisplacement(source);
  REQUIRE(original.canEditAlphas());
  CHECK(original.alpha(2, 2) == 128);
  // Existing distance authoring acts before every alpha arm.
  auto control = original;
  control.setDistance(2, 2, 24);
  CHECK(document.serialize(control.editsFrom(original)) != source);
  control.setDistance(2, 2, 16);
  CHECK(document.serialize(control.editsFrom(original)) == source);
  auto edited = original;
  SECTION("one fractional alpha edit is surgical and does not rebuild geometry")
  {
    edited.setAlpha(2, 2, 17.5);
    CHECK(edited.alpha(2, 2) == 17.5);
    CHECK(original.alpha(2, 2) == 128);
    CHECK(edited.positions() == original.positions());
    CHECK(edited.basePositions() == original.basePositions());
    CHECK(edited.normals() == original.normals());
    CHECK(edited.indices() == original.indices());
    CHECK(edited.bounds() == original.bounds());
    CHECK(edited.distance(2, 2) == original.distance(2, 2));
    const auto edits = edited.editsFrom(original);
    REQUIRE(edits.size() == 1);
    const auto saved = document.serialize(edits);
    CHECK(
      saved
      == replaceOnce(
        source,
        "\"row2\" \"000.0 +64  128.00   192 255\"",
        "\"row2\" \"000.0 +64  17.5   192 255\""));
    const auto reopened = loadDisplacement(saved);
    CHECK(reopened.alpha(2, 2) == 17.5);
    CHECK(reopened.positions() == original.positions());
    CHECK(reopened.alphas() == edited.alphas());
    CHECK(document.serialize(edited.editsFrom(original)) == saved);
    edited.setAlpha(2, 2, 128);
    CHECK(edited == original);
    CHECK(document.serialize(edited.editsFrom(original)) == source);
  }
  SECTION("same parsed value is a lexical no-op")
  {
    edited.setAlpha(2, 2, 128);
    CHECK(edited == original);
    CHECK(edited.editsFrom(original).empty());
    CHECK(document.serialize(edited.editsFrom(original)) == source);
  }
  SECTION("invalid finite range or coordinates refuse before mutation")
  {
    const auto value = GENERATE(
      -1.0,
      255.01,
      std::numeric_limits<double>::infinity(),
      std::numeric_limits<double>::quiet_NaN());
    CHECK_THROWS_AS(edited.setAlpha(2, 2, value), std::invalid_argument);
    CHECK(edited == original);
    CHECK_THROWS_AS(edited.setAlpha(5, 2, 0), std::out_of_range);
    CHECK_THROWS_AS(edited.setAlpha(2, 5, 0), std::out_of_range);
    CHECK_THROWS_AS(edited.alpha(5, 2), std::out_of_range);
    CHECK_THROWS_AS(edited.alpha(2, 5), std::out_of_range);
    CHECK(edited == original);
  }
  SECTION("missing or partial alpha rows do not silently become authored")
  {
    const auto partial = GENERATE(false, true);
    const auto bytes =
      partial ? replaceOnce(source, "\"row2\" \"000.0 +64  128.00   192 255\"\n", "")
              : displacementFixture();
    auto sparse = loadDisplacement(bytes);
    const auto before = sparse;
    CHECK_FALSE(sparse.canEditAlphas());
    CHECK(sparse.canEditDistances());
    CHECK(sparse.alpha(2, 2) == 0);
    CHECK_THROWS_AS(sparse.setAlpha(2, 2, 17), std::invalid_argument);
    CHECK(sparse == before);
    sparse.setDistance(2, 2, 24);
    CHECK_FALSE(sparse.editsFrom(before).empty());
  }
  SECTION("foreign same-value alpha row text is not numeric authority")
  {
    const auto foreign = loadDisplacement(replaceOnce(
      source,
      "\"row2\" \"000.0 +64  128.00   192 255\"",
      "\"row2\" \"000.0 +64  1.28e2   192 255\""));
    CHECK(foreign.alphas() == original.alphas());
    CHECK_THROWS_AS(foreign.editsFrom(original), std::invalid_argument);
  }
  SECTION("alpha composes with existing distance and admitted base translation")
  {
    edited.setAlpha(2, 2, 17.5);
    edited.setDistance(2, 2, 24);
    const auto delta = vm::vec3d{16, 8, 4};
    edited = edited.translated(delta);
    CHECK(edited.alpha(2, 2) == 17.5);
    CHECK(edited.positions()[12] == vm::vec3d{48, 40, 28});
    const auto saved = document.serialize(edited.editsFrom(original));
    const auto parsed = VmfDocument::parse(saved);
    auto newCorners = corners;
    for (auto& corner : newCorners)
    {
      corner = corner + delta;
    }
    const auto reopened =
      SourceDisplacement::read(parsed, parsed.roots()[0], newCorners, {0, 0, 1}, bounds);
    REQUIRE(reopened);
    CHECK(reopened->alphas() == edited.alphas());
    CHECK(reopened->positions() == edited.positions());
    edited = edited.translated(-delta);
    edited.setDistance(2, 2, 16);
    edited.setAlpha(2, 2, 128);
    CHECK(edited == original);
    CHECK(document.serialize(edited.editsFrom(original)) == source);
  }
}

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
  SECTION("positive uniform scale preserves magnitude and native scalar vector bindings")
  {
    const auto factor = GENERATE(2.0, 0.5);
    const auto text = replaceOnce(
      source,
      "normals {",
      "elevation +2.00 offsets { row2 \"0 0 0 0 0 0 1 +2 3.00 0 0 0 0 0 0\" } "
      "offset_normals { row2 \"0 0 0 0 0 0 4 5 6 0 0 0 0 0 0\" } normals {");
    const auto authored = loadDisplacement(text);
    const auto transform = vm::scaling_matrix(vm::vec3d{factor, factor, factor});
    auto scaled = authored.uniformlyScaled(transform);
    CHECK(scaled.distance(2, 2) == 16 * factor);
    for (size_t i = 0; i < scaled.positions().size(); ++i)
    {
      CHECK(scaled.positions()[i] == transform * authored.positions()[i]);
      CHECK(scaled.basePositions()[i] == transform * authored.basePositions()[i]);
      CHECK(vm::is_equal(scaled.normals()[i], authored.normals()[i], 0.000001));
    }
    const auto saved = VmfDocument::parse(text).serialize(scaled.editsFrom(authored));
    CHECK(saved.find("4 5 6") != std::string::npos);
    CHECK(saved.find("0 0 1  0 0 1") != std::string::npos);
    CHECK(
      saved.find("elevation \"" + std::to_string(int(2 * factor)) + "\"")
      != std::string::npos);
    CHECK(saved.find("future { key \"opaque\" }") != std::string::npos);
    const auto rotation = vm::rotation_matrix(vm::vec3d{1, 0, 0}, 0.37);
    scaled = scaled.rigidlyTransformed(rotation).translated({-1, 2, 3});
    scaled.setDistance(2, 2, 24 * factor);
    const auto expected =
      rotation * (transform * vm::vec3d{33, 34, 29}) + vm::vec3d{-1, 2, 3};
    CHECK(vm::is_equal(scaled.positions()[12], expected, 0.000001));
    const auto continued = VmfDocument::parse(text).serialize(scaled.editsFrom(authored));
    auto newCorners = corners;
    for (auto& corner : newCorners)
    {
      corner = rotation * (transform * corner) + vm::vec3d{-1, 2, 3};
    }
    const auto parsed = VmfDocument::parse(continued);
    const auto reopened = SourceDisplacement::read(
      parsed,
      parsed.roots()[0],
      newCorners,
      vm::slice<3, 3>(rotation, 0, 0) * vm::vec3d{0, 0, 1},
      bounds);
    REQUIRE(reopened);
    CHECK(vm::is_equal(reopened->positions()[12], expected, 0.000001));
    CHECK(reopened->canEditDistances());
    CHECK(reopened->editsFrom(*reopened).empty());
  }
  SECTION("uniform scale rejects invalid matrices and overflowing scalar outputs")
  {
    for (const auto factors :
         {vm::vec3d{0, 0, 0},
          vm::vec3d{-1, -1, -1},
          vm::vec3d{1.0000000000001, 1, 1},
          vm::vec3d{2, 1, 1},
          vm::vec3d{std::numeric_limits<double>::infinity(), 2, 2}})
    {
      CHECK_THROWS_AS(
        original.uniformlyScaled(vm::scaling_matrix(factors)), std::invalid_argument);
    }
    auto shear = vm::mat4x4d::identity();
    shear[1][0] = 1e-14;
    CHECK_THROWS_AS(original.uniformlyScaled(shear), std::invalid_argument);
    const auto subnormal = loadDisplacement(replaceOnce(
      source,
      "normals {",
      "offsets { row0 \"5e-324 0 0 0 0 0 0 0 0 0 0 0 0 0 0\" } normals {"));
    CHECK_THROWS_AS(
      subnormal.uniformlyScaled(vm::scaling_matrix(vm::vec3d{0.5, 0.5, 0.5})),
      std::invalid_argument);
    const auto text = replaceOnce(source, "normals {", "elevation 1e308 normals {");
    // Finite cancelling scalar inputs must not be accepted just because their samples
    // fit.
    auto cancelled = text;
    for (const auto row : {"0.00  +0  0  0 0", "0.00  +0  16  0 0"})
    {
      while (cancelled.find(row) != std::string::npos)
      {
        cancelled = replaceOnce(cancelled, row, "-1e308 -1e308 -1e308 -1e308 -1e308");
      }
    }
    const auto finite = loadDisplacement(cancelled);
    CHECK_THROWS_AS(
      finite.uniformlyScaled(vm::scaling_matrix(vm::vec3d{2, 2, 2})),
      std::invalid_argument);
    CHECK_THROWS_AS(
      finite.uniformlyScaled(
        vm::translation_matrix(vm::vec3d{0, 0, 1})
        * vm::scaling_matrix(vm::vec3d{0.5, 0.5, 0.5})),
      std::invalid_argument);
    CHECK(loadDisplacement(source) == original);
  }
  SECTION("proper rigid rotation preserves native vector tokens and cumulative motion")
  {
    const auto text = replaceOnce(
      source,
      "normals {",
      "elevation 2 offsets { row2 \"0.00 0 0 0 0 0 1  +2 3.00 0 0 0 0 0 0\" } "
      "offset_normals { row2 \"0 0 0 0 0 0 4 5 6 0 0 0 0 0 0\" } "
      "alphas { row2 \"0 0 127 0 0\" } normals {");
    const auto authored = loadDisplacement(text);
    const auto delta = vm::vec3d{16, 8, 4};
    const auto moved = authored.translated(delta);
    auto rotation = vm::mat4x4d::identity();
    rotation[1][1] = 0;
    rotation[2][2] = 0;
    rotation[1][2] = 1;
    rotation[2][1] = -1;
    auto turned = moved.rigidlyTransformed(rotation);
    REQUIRE(turned.baseTransformFrom(moved));
    CHECK(*turned.baseTransformFrom(moved) == rotation);
    CHECK_FALSE(turned.baseTranslationFrom(authored));
    CHECK(turned.positions()[12] == vm::vec3d{49, -25, 42});
    for (size_t i = 0; i < authored.positions().size(); ++i)
    {
      CHECK(turned.positions()[i] == rotation * (authored.positions()[i] + delta));
      CHECK(
        turned.basePositions()[i] == rotation * (authored.basePositions()[i] + delta));
      CHECK(
        vm::is_equal(
          turned.normals()[i],
          vm::slice<3, 3>(rotation, 0, 0) * authored.normals()[i],
          0.000001));
    }
    CHECK(turned.indices() == authored.indices());
    CHECK(turned.alphas() == authored.alphas());
    auto expected = replaceOnce(text, "[0 0 0]", "[16 -4 8]");
    for (size_t y = 0; y < 5; ++y)
    {
      expected = replaceOnce(
        expected,
        "0 0 1  0 0 1  0 0 1  0 0 1  0 0 1",
        "0 -1 0  0 -1 0  0 -1 0  0 -1 0  0 -1 0");
    }
    expected = replaceOnce(expected, "1  +2 3.00", "1  -3 2");
    expected = replaceOnce(expected, "4 5 6", "4 -6 5");
    const auto saved = VmfDocument::parse(text).serialize(turned.editsFrom(authored));
    CHECK(saved == expected);
    auto newCorners = corners;
    for (auto& corner : newCorners)
    {
      corner = rotation * (corner + delta);
    }
    const auto newNormal = vm::slice<3, 3>(rotation, 0, 0) * vm::vec3d{0, 0, 1};
    const auto parsed = VmfDocument::parse(saved);
    const auto reopened =
      SourceDisplacement::read(parsed, parsed.roots()[0], newCorners, newNormal, bounds);
    REQUIRE(reopened);
    CHECK(reopened->positions() == turned.positions());
    CHECK_THROWS_AS(reopened->editsFrom(authored), std::invalid_argument);
    const auto hit = turned.intersect(vm::ray3d{{49, -100, 42}, {0, 1, 0}});
    REQUIRE(hit);
    CHECK(*hit == Catch::Approx(75));
    turned.setDistance(2, 2, 24);
    CHECK(turned.positions()[12] == vm::vec3d{49, -33, 42});
    const auto savedAgain =
      VmfDocument::parse(text).serialize(turned.editsFrom(authored));
    CHECK(savedAgain == replaceOnce(expected, "+0  16  0", "+0  24  0"));
    const auto again = VmfDocument::parse(savedAgain);
    CHECK(
      SourceDisplacement::read(again, again.roots()[0], newCorners, newNormal, bounds)
        ->positions()
      == turned.positions());
    CHECK(loadDisplacement(text) == authored);
    const auto oblique =
      authored.rigidlyTransformed(vm::rotation_matrix(vm::vec3d{0, 0, 1}, 0.37));
    const auto step = vm::vec3d{1, 2, 3};
    const auto stepped = oblique.translated(step);
    REQUIRE(stepped.baseTranslationFrom(oblique));
    CHECK(vm::is_equal(*stepped.baseTranslationFrom(oblique), step, 0.000001));
  }
  SECTION("unbound ignored vectors preserve loading and translation but block rotation")
  {
    auto rotation = vm::mat4x4d::identity();
    rotation[1][1] = 0;
    rotation[2][2] = 0;
    rotation[1][2] = 1;
    rotation[2][1] = -1;
    const auto malformed =
      replaceOnce(source, "normals {", "offset_normals { row0 \"unbound\" } normals {");
    const auto opaque = loadDisplacement(malformed);
    CHECK(opaque.positions() == original.positions());
    CHECK(VmfDocument::parse(malformed).serialize(opaque.editsFrom(opaque)) == malformed);
    CHECK(
      VmfDocument::parse(malformed).serialize(
        opaque.translated({16, 8, 4}).editsFrom(opaque))
      == replaceOnce(malformed, "[0 0 0]", "[16 8 4]"));
    CHECK_THROWS_AS(opaque.rigidlyTransformed(rotation), std::invalid_argument);
    CHECK_THROWS_AS(
      opaque.uniformlyScaled(vm::scaling_matrix(vm::vec3d{2, 2, 2})),
      std::invalid_argument);
    const auto flat =
      loadDisplacement("side { dispinfo { power 2 startposition \"[0 0 0]\" } }");
    const auto turned = flat.rigidlyTransformed(rotation);
    CHECK(turned.editsFrom(flat).empty());
    CHECK(turned.positions()[12] == vm::vec3d{32, 0, 32});
    CHECK_FALSE(turned.canEditDistances());
    const auto sparseScaled =
      flat.uniformlyScaled(vm::scaling_matrix(vm::vec3d{0.5, 0.5, 0.5}));
    CHECK(sparseScaled.editsFrom(flat).empty());
    CHECK(sparseScaled.positions()[12] == vm::vec3d{16, 16, 0});
    CHECK_FALSE(sparseScaled.canEditDistances());
    auto projective = rotation;
    projective[0][3] = 0.1;
    auto nonfinite = rotation;
    nonfinite[0][0] = std::numeric_limits<double>::infinity();
    auto shear = vm::mat4x4d::identity();
    shear[1][0] = 0.1;
    for (const auto& bad :
         {projective,
          nonfinite,
          shear,
          vm::scaling_matrix(vm::vec3d{2, 2, 2}),
          vm::scaling_matrix(vm::vec3d{-1, 1, 1}),
          vm::translation_matrix(vm::vec3d{1000, 0, 0})})
    {
      CHECK_THROWS_AS(original.rigidlyTransformed(bad), std::invalid_argument);
    }
    CHECK(loadDisplacement(source) == original);
  }
  SECTION("finite unused native vectors cannot overflow during rotation")
  {
    const auto overflow = GENERATE(false, true);
    const auto vector =
      overflow ? std::string{"1.7e308 -1.7e308 0"} : std::string{"1e100 -1e100 0"};
    const auto text = replaceOnce(
      source,
      "normals {",
      "offset_normals { row0 \"" + vector + " 0 0 0 0 0 0 0 0 0 0 0 0\" } normals {");
    const auto large = loadDisplacement(text);
    CHECK(large.positions() == original.positions());
    CHECK_NOTHROW(large.uniformlyScaled(vm::scaling_matrix(vm::vec3d{0.5, 0.5, 0.5})));
    CHECK(VmfDocument::parse(text).serialize(large.editsFrom(large)) == text);
    const auto rotation =
      vm::rotation_matrix(vm::vec3d{0, 0, 1}, vm::constants<double>::quarter_pi());
    if (overflow)
    {
      CHECK_THROWS_AS(large.rigidlyTransformed(rotation), std::invalid_argument);
    }
    else
    {
      const auto turned = large.rigidlyTransformed(rotation);
      CHECK(
        vm::is_equal(turned.positions()[12], rotation * large.positions()[12], 0.000001));
      const auto saved = VmfDocument::parse(text).serialize(turned.editsFrom(large));
      CHECK(saved != text);
      CHECK_NOTHROW(loadDisplacement(saved).rigidlyTransformed(rotation));
    }
    CHECK(loadDisplacement(text) == large);
  }
  SECTION("finite cancellation cannot silently deform a requested rigid motion")
  {
    auto text = std::string{
      "side { dispinfo { power 2 startposition \"[0 0 0]\" elevation 1e18 normals {"};
    for (size_t y = 0; y < 5; ++y)
    {
      text += "row" + std::to_string(y) + " \"0 0 -1 0 0 -1 0 0 -1 0 0 -1 0 0 -1\" ";
    }
    text += "} distances {";
    for (size_t y = 0; y < 5; ++y)
    {
      text += "row" + std::to_string(y) + " \"1e18 1e18 1e18 1e18 1e18\" ";
    }
    text += "} } }";
    const auto cancellation = loadDisplacement(text);
    CHECK(cancellation.positions()[12] == vm::vec3d{32, 32, 0});
    CHECK(
      VmfDocument::parse(text).serialize(cancellation.editsFrom(cancellation)) == text);
    CHECK_THROWS_AS(cancellation.translated({16, 8, 4}), std::invalid_argument);
    CHECK_THROWS_AS(
      cancellation.rigidlyTransformed(vm::rotation_matrix(vm::vec3d{1, 0, 0}, 0.37)),
      std::invalid_argument);
    CHECK(loadDisplacement(text) == cancellation);
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
