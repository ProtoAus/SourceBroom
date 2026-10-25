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
} // namespace tb::mdl::test
