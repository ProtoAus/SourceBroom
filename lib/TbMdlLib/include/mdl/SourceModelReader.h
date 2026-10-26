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
  std::array<float, 3> normal;
};
struct SourceModelMesh
{
  size_t bodyPartIndex = 0;
  size_t modelIndex = 0;
  std::vector<uint32_t> indices;
  std::vector<std::string> materials; // One material per skin family, in family order.
};
// Ranges refer to the shared LOD0 vertex table and flat mesh array. A blank
// model has zero vertices and meshes; no Cartesian product is materialized.
struct SourceModelVariant
{
  size_t firstVertex = 0;
  size_t vertexCount = 0;
  size_t firstMesh = 0;
  size_t meshCount = 0;
};
struct SourceModelBodyPart
{
  // Packed body selects (body / base) % models.size() for multi-model parts.
  // A single-model part always selects zero; its on-disk base may be zero.
  uint32_t base = 0;
  std::vector<SourceModelVariant> models;
};
struct SourceModelGeometry
{
  std::vector<SourceModelBodyPart> bodyParts;
  std::vector<SourceModelVertex> vertices;
  std::vector<SourceModelMesh> meshes;
  std::vector<std::string> materialDirectories;
  // Union of all emitted variants, not the bounds of an entity selection.
  std::array<float, 3> min;
  std::array<float, 3> max;
};
// Classic static-prop LOD0 only. Throws std::runtime_error with a Source diagnostic.
// Byte views need only remain alive for this call; the result owns all data.
SourceModelGeometry readSourceModel(
  std::string_view mdl, std::string_view vvd, std::string_view vtx);
constexpr size_t SourceModelMaxFileSize = 64u * 1024u * 1024u;
} // namespace tb::mdl
