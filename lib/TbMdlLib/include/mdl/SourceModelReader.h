/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tb::mdl
{
struct SourceModelVertex
{
  std::array<float, 3> position;
  std::array<float, 2> uv;
};
struct SourceModelMesh
{
  std::vector<uint32_t> indices;
  std::vector<std::string> materials; // One material per skin family, in family order.
};
struct SourceModelGeometry
{
  std::vector<SourceModelVertex> vertices;
  std::vector<SourceModelMesh> meshes;
  std::vector<std::string> materialDirectories;
  std::array<float, 3> min;
  std::array<float, 3> max;
};
// Classic static-prop LOD0 only. Throws std::runtime_error with a Source diagnostic.
// Byte views need only remain alive for this call; the result owns all data.
SourceModelGeometry readSourceModel(
  std::string_view mdl, std::string_view vvd, std::string_view vtx);
constexpr size_t SourceModelMaxFileSize = 64u * 1024u * 1024u;
} // namespace tb::mdl
