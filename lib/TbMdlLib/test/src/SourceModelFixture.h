/* Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "mdl/SourceModelReader.h"

#include <bit>

namespace tb::mdl::test
{
inline void put(std::string& bytes, size_t p, uint32_t value, size_t width = 4)
{
  for (size_t i = 0; i < width; ++i)
    bytes.at(p + i) = char(value >> (8 * i));
}
struct SyntheticProp
{
  std::string mdl = std::string(900, '\0');
  std::string vvd = std::string(64 + 4 * 48 + 4 * 16, '\0');
  std::string vtx = std::string(220, '\0');
  SyntheticProp()
  {
    put(mdl, 0, 0x54534449);
    put(mdl, 4, 48);
    put(mdl, 8, 123);
    put(mdl, 76, uint32_t(mdl.size()));
    put(mdl, 152, 16);
    put(mdl, 204, 1);
    put(mdl, 208, 408); // textures
    put(mdl, 408, 400);
    mdl.replace(808, 5, "metal");
    put(mdl, 212, 1);
    put(mdl, 216, 472);
    put(mdl, 472, 816);
    mdl.replace(816, 12, "models/test/");
    put(mdl, 220, 1);
    put(mdl, 224, 1);
    put(mdl, 228, 476);
    put(mdl, 232, 1);
    put(mdl, 236, 480);
    put(mdl, 484, 1);
    put(mdl, 492, 16); // body/model
    put(mdl, 496 + 72, 1);
    put(mdl, 496 + 76, 148);
    put(mdl, 496 + 80, 4);
    put(mdl, 644 + 8, 4); // mesh
    put(vvd, 0, 0x56534449);
    put(vvd, 4, 4);
    put(vvd, 8, 123);
    put(vvd, 12, 1);
    put(vvd, 16, 4);
    put(vvd, 56, 64);
    put(vvd, 60, 256);
    const std::array<std::array<float, 3>, 4> positions = {
      {{0, 0, 0}, {0, 3, 0}, {2, 0, 0}, {2, 3, 0}}};
    for (size_t i = 0; i < 4; ++i)
    {
      for (size_t j = 0; j < 3; ++j)
        put(vvd, 64 + i * 48 + 16 + j * 4, std::bit_cast<uint32_t>(positions[i][j]));
      put(vvd, 64 + i * 48 + 36, std::bit_cast<uint32_t>(1.0f)); // Outward +Z normal.
      put(vvd, 64 + i * 48 + 40, std::bit_cast<uint32_t>(float(i % 2)));
      put(vvd, 64 + i * 48 + 44, std::bit_cast<uint32_t>(float(i / 2)));
    }
    put(vtx, 0, 7);
    put(vtx, 16, 123);
    put(vtx, 20, 1);
    put(vtx, 28, 1);
    put(vtx, 32, 36);
    put(vtx, 36, 1);
    put(vtx, 40, 8); // body
    put(vtx, 44, 1);
    put(vtx, 48, 8); // model
    put(vtx, 52, 1);
    put(vtx, 56, 12); // lod
    put(vtx, 64, 1);
    put(vtx, 68, 9); // mesh
    put(vtx, 73, 4);
    put(vtx, 77, 25); // group vertices at 98
    put(vtx, 81, 3);
    put(vtx, 85, 61); // indices at 134
    put(vtx, 89, 1);
    put(vtx, 93, 69); // strip at 142
    for (size_t i = 0; i < 4; ++i)
      put(vtx, 98 + 9 * i + 4, uint32_t(i), 2);
    put(vtx, 134, 0, 2);
    put(vtx, 136, 1, 2);
    put(vtx, 138, 2, 2);
    put(vtx, 142, 3);
    put(vtx, 150, 4);
    put(vtx, 160, 1, 1);
  }
  SourceModelGeometry read() const { return readSourceModel(mdl, vvd, vtx); }
};
// Owned synthetic byte generator. Public record offsets allow targeted corruption
// without a second decoder in tests. Defaults: two parts, two choices, last blank.
struct SyntheticBodyProp
{
  std::string mdl = std::string(408, '\0');
  std::string vvd = std::string(64, '\0');
  std::string vtx = std::string(36, '\0');
  std::vector<size_t> bodies, xbodies;
  std::vector<std::vector<size_t>> models, xmodels, meshes;

  explicit SyntheticBodyProp(
    const std::vector<std::vector<bool>>& blank = {{false, false}, {false, true}},
    const std::vector<uint32_t>& bases = {1, 2})
  {
    const auto allocate = [](std::string& bytes, size_t count) {
      const auto start = bytes.size();
      bytes.resize(start + count, '\0');
      return start;
    };
    const auto text = [&](const std::string& value) {
      const auto start = allocate(mdl, value.size() + 1);
      mdl.replace(start, value.size(), value);
      return start;
    };
    const auto materialCount = size_t{8};
    const auto textures = allocate(mdl, materialCount * 64);
    for (size_t i = 0; i < materialCount; ++i)
    {
      const auto record = textures + 64 * i;
      put(mdl, record, uint32_t(text("choice" + std::to_string(i)) - record));
    }
    const auto skins = allocate(mdl, materialCount * 2);
    // Four mesh slots, two independent skin families.
    for (size_t i = 0; i < materialCount; ++i)
      put(mdl, skins + 2 * i, uint32_t(i), 2);
    put(mdl, 204, uint32_t(materialCount));
    put(mdl, 208, uint32_t(textures));
    put(mdl, 220, 4);
    put(mdl, 224, 2);
    put(mdl, 228, uint32_t(skins));
    const auto bodyTable = allocate(mdl, blank.size() * 16);
    const auto xbodyTable = allocate(vtx, blank.size() * 8);
    put(mdl, 232, uint32_t(blank.size()));
    put(mdl, 236, uint32_t(bodyTable));
    put(vtx, 28, uint32_t(blank.size()));
    put(vtx, 32, uint32_t(xbodyTable));
    size_t vertexCount = 0, choice = 0;
    const auto prototype = SyntheticProp{};
    for (size_t b = 0; b < blank.size(); ++b)
    {
      const auto body = bodyTable + 16 * b, xb = xbodyTable + 8 * b;
      bodies.push_back(body);
      xbodies.push_back(xb);
      models.emplace_back();
      xmodels.emplace_back();
      meshes.emplace_back();
      const auto modelTable = allocate(mdl, blank[b].size() * 148);
      const auto xmodelTable = allocate(vtx, blank[b].size() * 8);
      put(mdl, body + 4, uint32_t(blank[b].size()));
      put(mdl, body + 8, bases.at(b));
      put(mdl, body + 12, uint32_t(modelTable - body));
      put(vtx, xb, uint32_t(blank[b].size()));
      put(vtx, xb + 4, uint32_t(xmodelTable - xb));
      for (size_t m = 0; m < blank[b].size(); ++m, ++choice)
      {
        const auto model = modelTable + 148 * m, xm = xmodelTable + 8 * m;
        models.back().push_back(model);
        xmodels.back().push_back(xm);
        put(mdl, model + 84, uint32_t(vertexCount * 48));
        const auto lod = allocate(vtx, 12);
        put(vtx, xm, 1);
        put(vtx, xm + 4, uint32_t(lod - xm));
        if (blank[b][m])
        {
          meshes.back().push_back(0);
          continue;
        }
        const auto mesh = allocate(mdl, 116);
        meshes.back().push_back(mesh);
        put(mdl, model + 72, 1);
        put(mdl, model + 76, uint32_t(mesh - model));
        put(mdl, model + 80, 5);
        put(mdl, mesh, uint32_t(choice % 4));
        put(mdl, mesh + 8, 4);
        put(mdl, mesh + 12, 1); // Nonzero mesh-local base too.
        const auto xmesh = allocate(vtx, 105);
        vtx.replace(xmesh, 105, prototype.vtx.substr(64, 105));
        put(vtx, lod, 1);
        put(vtx, lod + 4, uint32_t(xmesh - lod));
        for (size_t i = 0; i < 5; ++i)
        {
          const auto vertex = allocate(vvd, 48);
          const auto source = 64 + (i == 0 ? 0 : i - 1) * 48;
          vvd.replace(vertex, 48, prototype.vvd.substr(source, 48));
          const auto x = float(b * 100 + m * 10) + (i >= 3 ? 2.0f : 0.0f);
          put(vvd, vertex + 16, std::bit_cast<uint32_t>(x));
        }
        vertexCount += 5;
      }
    }
    put(mdl, 0, 0x54534449);
    put(mdl, 4, 48);
    put(mdl, 8, 123);
    put(mdl, 76, uint32_t(mdl.size()));
    put(mdl, 152, 16);
    put(vvd, 0, 0x56534449);
    put(vvd, 4, 4);
    put(vvd, 8, 123);
    put(vvd, 12, 1);
    put(vvd, 16, uint32_t(vertexCount));
    put(vvd, 56, 64);
    put(vvd, 60, uint32_t(allocate(vvd, vertexCount * 16)));
    put(vtx, 0, 7);
    put(vtx, 16, 123);
    put(vtx, 20, 1);
  }
  SourceModelGeometry read() const { return readSourceModel(mdl, vvd, vtx); }
};
} // namespace tb::mdl::test
