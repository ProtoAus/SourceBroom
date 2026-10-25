/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/SourceModelReader.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tb::mdl
{
namespace
{
[[noreturn]] void refuse(const char* message)
{
  throw std::runtime_error(std::string{"Source static prop: "} + message);
}
void require(bool condition, const char* message)
{
  if (!condition)
  {
    refuse(message);
  }
}
// Disk layouts, not native C++ structs: MDL header texture/skin/body fields
// start at 204/220/232; model and mesh records are 148/116 bytes. VVD vertices
// are 48 bytes. Classic VTX mesh/group/strip/vertex records are 9/25/27/9 bytes.
// In particular, VTX is packed and its offsets must never be read via casts.
constexpr size_t MaxVertices = 1'000'000;
constexpr size_t MaxIndices = 3'000'000;
constexpr size_t MaxRecords = 4096;
struct Bytes
{
  std::string_view bytes;
  explicit Bytes(std::string_view value)
    : bytes{value}
  {
    require(value.size() <= SourceModelMaxFileSize, "file exceeds 64 MiB limit");
  }
  void range(size_t offset, size_t count, size_t stride = 1) const
  {
    require(
      offset <= bytes.size() && count <= (bytes.size() - offset) / stride,
      "truncated file or invalid table range");
  }
  uint32_t u32(size_t p) const
  {
    range(p, 4);
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i)
    {
      value |= uint32_t(static_cast<unsigned char>(bytes[p + i])) << (8 * i);
    }
    return value;
  }
  size_t number(size_t p, size_t limit = SourceModelMaxFileSize) const
  {
    const auto n = u32(p);
    require(n <= limit, "negative or excessive count/offset");
    return n;
  }
  size_t relative(size_t base, size_t field) const
  {
    const auto offset = number(field);
    require(
      base <= bytes.size() && offset <= bytes.size() - base, "invalid relative offset");
    return base + offset;
  }
  size_t table(
    size_t base,
    size_t countField,
    size_t offsetField,
    size_t stride,
    size_t limit = MaxRecords) const
  {
    const auto n = number(countField, limit);
    const auto p = relative(base, offsetField);
    range(p, n, stride);
    return p;
  }
  uint16_t u16(size_t p) const
  {
    range(p, 2);
    return uint16_t(static_cast<unsigned char>(bytes[p]))
           | uint16_t(static_cast<unsigned char>(bytes[p + 1]) << 8);
  }
  uint8_t u8(size_t p) const
  {
    range(p, 1);
    return static_cast<uint8_t>(bytes[p]);
  }
  float real(size_t p) const
  {
    const auto value = std::bit_cast<float>(u32(p));
    require(
      std::isfinite(value) && std::abs(value) <= 10'000'000.0f,
      "nonfinite or excessive vertex/UV");
    return value;
  }
  std::string text(size_t p) const
  {
    range(p, 1);
    const auto bounded = bytes.substr(p, std::min(size_t{1025}, bytes.size() - p));
    const auto end = bounded.find(char{0});
    require(end != std::string_view::npos && end <= 1024, "invalid material string");
    auto s = std::string{bounded.substr(0, end)};
    std::replace(s.begin(), s.end(), '\\', '/');
    require(
      s.find(':') == std::string::npos && (s.empty() || s.front() != '/'),
      "absolute material path");
    size_t start = 0;
    while (start < s.size())
    {
      const auto next = s.find('/', start);
      require(s.substr(start, next - start) != "..", "material path traversal");
      if (next == std::string::npos)
      {
        break;
      }
      start = next + 1;
    }
    for (auto& c : s)
    {
      if (c >= 'A' && c <= 'Z')
      {
        c = char(c - 'A' + 'a');
      }
    }
    return s;
  }
};
} // namespace

SourceModelGeometry readSourceModel(
  std::string_view mdlBytes, std::string_view vvdBytes, std::string_view vtxBytes)
{
  const auto mdl = Bytes{mdlBytes}, vvd = Bytes{vvdBytes}, vtx = Bytes{vtxBytes};
  mdl.range(0, 408);
  vvd.range(0, 64);
  vtx.range(0, 36);
  require(mdl.u32(0) == 0x54534449, "expected IDST MDL");
  const auto version = mdl.u32(4);
  require(
    version == 44 || version == 48 || version == 49,
    "unsupported MDL version (expected 44/48/49)");
  require(mdl.number(76) == mdlBytes.size(), "MDL declared length mismatch");
  require((mdl.u32(152) & 0x10) != 0, "animated/non-static MDL is unsupported");
  require(
    (mdl.u32(152) & ((1u << 19) | (1u << 26))) == 0,
    "subdivision/extra vertex data variant is unsupported");
  require(vvd.u32(0) == 0x56534449 && vvd.u32(4) == 4, "unsupported VVD magic/version");
  require(vtx.u32(0) == 7, "unsupported VTX version (expected 7)");
  require(
    vvd.u32(8) == mdl.u32(8) && vtx.u32(16) == mdl.u32(8),
    "MDL/VVD/VTX checksum mismatch");
  const auto lods = vvd.number(12, 8);
  require(lods > 0 && vtx.number(20, 8) == lods, "sidecar LOD mismatch");
  const auto vertexCount = vvd.number(16, MaxVertices);
  require(vertexCount > 0, "empty vertex table");
  for (size_t i = 0; i < lods; ++i)
  {
    require(
      vvd.number(16 + 4 * i, MaxVertices) <= vertexCount, "invalid LOD vertex count");
  }
  const auto vertexStart = vvd.number(56);
  vvd.range(vertexStart, vertexCount, 48);
  vvd.range(vvd.number(60), vertexCount, 16);
  auto order = std::vector<uint32_t>{};
  order.reserve(vertexCount);
  const auto fixups = vvd.table(0, 48, 52, 12, MaxVertices);
  const auto fixupCount = vvd.number(48, MaxVertices);
  if (!fixupCount)
  {
    for (size_t i = 0; i < vertexCount; ++i)
    {
      order.push_back(static_cast<uint32_t>(i));
    }
  }
  else
  {
    auto seen = std::vector<bool>(vertexCount, false);
    for (size_t i = 0; i < fixupCount; ++i)
    {
      const auto f = fixups + 12 * i;
      require(vvd.number(f, 8) < lods, "invalid fixup LOD");
      const auto first = vvd.number(f + 4, vertexCount),
                 count = vvd.number(f + 8, vertexCount);
      require(
        count <= vertexCount - first && count <= vertexCount - order.size(),
        "invalid fixup range");
      for (size_t j = first; j < first + count; ++j)
      {
        require(!seen[j], "overlapping VVD fixups");
        seen[j] = true;
        order.push_back(static_cast<uint32_t>(j));
      }
    }
    require(order.size() == vertexCount, "incomplete LOD0 fixups");
  }
  auto out = SourceModelGeometry{};
  out.min.fill(std::numeric_limits<float>::max());
  out.max.fill(std::numeric_limits<float>::lowest());
  out.vertices.reserve(vertexCount);
  for (const auto i : order)
  {
    const auto p = vertexStart + 48 * i;
    SourceModelVertex vertex{};
    for (size_t j = 0; j < 3; ++j)
    {
      vertex.position[j] = vvd.real(p + 16 + 4 * j);
    }
    for (size_t j = 0; j < 2; ++j)
    {
      vertex.uv[j] = vvd.real(p + 40 + 4 * j);
    }
    out.vertices.push_back(vertex);
  }
  const auto textures = mdl.table(0, 204, 208, 64);
  const auto textureCount = mdl.number(204, MaxRecords);
  require(textureCount > 0, "no MDL materials");
  auto names = std::vector<std::string>{};
  for (size_t i = 0; i < textureCount; ++i)
  {
    const auto t = textures + 64 * i;
    names.push_back(mdl.text(mdl.relative(t, t)));
    require(!names.back().empty(), "empty material name");
  }
  const auto directories = mdl.table(0, 212, 216, 4, 256);
  for (size_t i = 0; i < mdl.number(212, 256); ++i)
  {
    out.materialDirectories.push_back(mdl.text(mdl.number(directories + 4 * i)));
  }
  if (out.materialDirectories.empty())
  {
    out.materialDirectories.emplace_back();
  }
  const auto refs = mdl.number(220, MaxRecords), families = mdl.number(224, 256);
  require(refs > 0 && families > 0, "missing MDL skin table");
  const auto skins = mdl.number(228);
  mdl.range(skins, refs * families, 2);
  for (size_t i = 0; i < refs * families; ++i)
  {
    require(mdl.u16(skins + 2 * i) < textureCount, "invalid skin material index");
  }
  const auto bodies = mdl.table(0, 232, 236, 16);
  const auto bodyCount = mdl.number(232, MaxRecords);
  const auto xbodies = vtx.table(0, 28, 32, 8);
  require(
    bodyCount > 0 && bodyCount == vtx.number(28, MaxRecords),
    "bodypart sidecar mismatch");
  if (vtx.number(24))
  {
    const auto replacements = vtx.number(24);
    vtx.range(replacements, lods, 8);
    require(
      vtx.number(replacements) == 0, "LOD0 VTX material replacements are unsupported");
  }
  size_t totalIndices = 0, totalRecords = 0, totalMaterialSlots = 0;
  auto work = [&](size_t n) {
    require(n <= MaxRecords - totalRecords, "nested table resource limit");
    totalRecords += n;
  };
  for (size_t b = 0; b < bodyCount; ++b)
  {
    const auto body = bodies + 16 * b, xb = xbodies + 8 * b;
    require(
      mdl.number(body + 4) == 1 && vtx.number(xb) == 1,
      "bodygroup alternatives are unsupported");
    const auto model = mdl.table(body, body + 4, body + 12, 148);
    const auto xm = vtx.table(xb, xb, xb + 4, 8);
    require(vtx.number(xm, 8) == lods, "model LOD mismatch");
    const auto lod = vtx.table(xm, xm, xm + 4, 12, 8);
    const auto meshes = mdl.table(model, model + 72, model + 76, 116);
    const auto meshCount = mdl.number(model + 72, MaxRecords);
    require(vtx.number(lod, MaxRecords) == meshCount, "mesh sidecar mismatch");
    const auto xmeshes = vtx.table(lod, lod, lod + 4, 9);
    const auto modelVertices = mdl.number(model + 80, vertexCount);
    const auto modelByteOffset = mdl.number(model + 84);
    require(modelByteOffset % 48 == 0, "unaligned model vertex offset");
    const auto modelFirst = modelByteOffset / 48;
    require(
      modelFirst <= vertexCount && modelVertices <= vertexCount - modelFirst,
      "model vertices outside VVD");
    work(meshCount);
    for (size_t h = 0; h < meshCount; ++h)
    {
      const auto mesh = meshes + 116 * h, xmesh = xmeshes + 9 * h;
      require(vtx.u8(xmesh + 8) == 0, "unsupported teeth/eyes VTX mesh");
      const auto material = mdl.number(mesh, refs - 1);
      const auto meshVertices = mdl.number(mesh + 8, modelVertices);
      const auto meshFirst = mdl.number(mesh + 12, modelVertices);
      require(meshVertices <= modelVertices - meshFirst, "mesh vertices outside model");
      require(mdl.number(mesh + 16) == 0, "flex mesh is unsupported");
      require(families <= MaxRecords - totalMaterialSlots, "skin resource limit");
      totalMaterialSlots += families;
      auto result = SourceModelMesh{};
      for (size_t f = 0; f < families; ++f)
      {
        result.materials.push_back(names[mdl.u16(skins + 2 * (f * refs + material))]);
      }
      const auto groups = vtx.table(xmesh, xmesh, xmesh + 4, 25);
      const auto groupCount = vtx.number(xmesh, MaxRecords);
      work(groupCount);
      for (size_t g = 0; g < groupCount; ++g)
      {
        const auto group = groups + 25 * g;
        require((vtx.u8(group + 24) & ~2u) == 0, "unsupported VTX strip group variant");
        const auto verts = vtx.table(group, group, group + 4, 9, MaxVertices);
        const auto nverts = vtx.number(group, MaxVertices);
        const auto indices = vtx.table(group, group + 8, group + 12, 2, MaxIndices);
        const auto nindices = vtx.number(group + 8, MaxIndices);
        const auto strips = vtx.table(group, group + 16, group + 20, 27);
        const auto nstrips = vtx.number(group + 16, MaxRecords);
        work(nstrips);
        auto index = [&](size_t i, size_t firstVertex, size_t stripVertices) -> uint32_t {
          require(i < nindices, "strip index outside group");
          const auto local = vtx.u16(indices + 2 * i);
          require(
            local < nverts && local >= firstVertex && local - firstVertex < stripVertices,
            "index outside strip vertices");
          const auto original = vtx.u16(verts + 9 * local + 4);
          require(original < meshVertices, "VTX vertex outside MDL mesh");
          return static_cast<uint32_t>(modelFirst + meshFirst + original);
        };
        for (size_t s = 0; s < nstrips; ++s)
        {
          const auto strip = strips + 27 * s;
          const auto count = vtx.number(strip, nindices),
                     first = vtx.number(strip + 4, nindices);
          require(count <= nindices - first && count >= 3, "invalid strip index range");
          const auto nv = vtx.number(strip + 8, nverts),
                     fv = vtx.number(strip + 12, nverts);
          require(nv <= nverts - fv, "invalid strip vertex range");
          const auto flags = vtx.u8(strip + 18);
          require(flags == 1 || flags == 2, "unsupported VTX primitive flags");
          require(flags != 1 || count % 3 == 0, "incomplete triangle list");
          vtx.table(strip, strip + 19, strip + 23, 8);
          const auto expanded = flags == 1 ? count : (count - 2) * 3;
          require(expanded <= MaxIndices - totalIndices, "triangle resource limit");
          totalIndices += expanded;
          auto emit = [&](size_t a, size_t b, size_t c) {
            const auto ia = index(first + a, fv, nv), ib = index(first + b, fv, nv),
                       ic = index(first + c, fv, nv);
            if (ia != ib && ib != ic && ia != ic)
            {
              result.indices.push_back(ia);
              result.indices.push_back(ib);
              result.indices.push_back(ic);
            }
          };
          // VTX winding is already clockwise, as required by the renderer.
          if (flags == 1)
          {
            for (size_t i = 0; i < count; i += 3)
            {
              emit(i, i + 1, i + 2);
            }
          }
          else
          {
            for (size_t i = 0; i + 2 < count; ++i)
            {
              if (i & 1)
              {
                emit(i, i + 2, i + 1);
              }
              else
              {
                emit(i, i + 1, i + 2);
              }
            }
          }
        }
      }
      if (!result.indices.empty())
      {
        out.meshes.push_back(std::move(result));
      }
    }
  }
  require(!out.meshes.empty(), "no nondegenerate static-prop triangles");
  for (const auto& mesh : out.meshes)
  {
    for (const auto i : mesh.indices)
    {
      for (size_t j = 0; j < 3; ++j)
      {
        out.min[j] = std::min(out.min[j], out.vertices[i].position[j]);
        out.max[j] = std::max(out.max[j], out.vertices[i].position[j]);
      }
    }
  }
  return out;
}
} // namespace tb::mdl
