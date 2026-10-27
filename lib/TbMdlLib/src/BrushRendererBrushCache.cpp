/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mdl/BrushRendererBrushCache.h"

#include "mdl/BrushFace.h"
#include "mdl/BrushGeometry.h"
#include "mdl/BrushNode.h"

#include "kd/contracts.h"

#include <algorithm>

namespace tb::mdl
{

BrushRendererBrushCache::CachedFace::CachedFace(
  const mdl::BrushFace* i_face, const size_t i_indexOfFirstVertexRelativeToBrush)
  : material{i_face->material()}
  , face{i_face}
  , vertexCount{i_face->sourceDisplacement() ? i_face->sourceDisplacement()->positions().size() : i_face->vertexCount()}
  , indexCount{i_face->sourceDisplacement() ? i_face->sourceDisplacement()->indices().size() : 3 * (i_face->vertexCount() - 2)}
  , indexOfFirstVertexRelativeToBrush{i_indexOfFirstVertexRelativeToBrush}
{
}

void BrushRendererBrushCache::CachedFace::writeIndices(
  const std::span<GLuint> target, const size_t firstVertex) const
{
  contract_pre(target.size() == indexCount);
  if (const auto& displacement = face->sourceDisplacement())
  {
    for (size_t i = 0; i < indexCount; ++i)
    {
      target[i] = static_cast<GLuint>(firstVertex + displacement->indices()[i]);
    }
  }
  else
  {
    for (size_t i = 0; i + 2 < vertexCount; ++i)
    {
      target[3 * i] = static_cast<GLuint>(firstVertex);
      target[3 * i + 1] = static_cast<GLuint>(firstVertex + i + 1);
      target[3 * i + 2] = static_cast<GLuint>(firstVertex + i + 2);
    }
  }
}

BrushRendererBrushCache::BrushRendererBrushCache()
  : m_rendererCacheValid{false}
{
}

void BrushRendererBrushCache::invalidateVertexCache()
{
  m_rendererCacheValid = false;
  m_cachedVertices.clear();
  m_cachedEdges.clear();
  m_cachedFacesSortedByMaterial.clear();
}

void BrushRendererBrushCache::validateVertexCache(const mdl::BrushNode& brushNode)
{
  if (m_rendererCacheValid)
  {
    return;
  }

  // build vertex cache and face cache
  const auto& brush = brushNode.brush();

  m_cachedVertices.clear();
  m_cachedVertices.reserve(brush.vertexCount());

  m_cachedFacesSortedByMaterial.clear();
  m_cachedFacesSortedByMaterial.reserve(brush.faceCount());

  for (const auto& face : brush.faces())
  {
    auto indexOfFirstVertexRelativeToBrush = m_cachedVertices.size();

    // The boundary is in CCW order, but the renderer expects CW order:
    auto& boundary = face.geometry()->boundary();
    for (auto it = std::rbegin(boundary), end = std::rend(boundary); it != end; ++it)
    {
      auto* currentHalfEdge = *it;
      auto* vertex = currentHalfEdge->origin();

      // Set the vertex payload to the index, relative to the brush's first vertex being
      // 0. This is used below when building the edge cache. NOTE: we'll overwrite the
      // payload as we visit the same vertex several times while visiting different faces,
      // this is fine.
      const auto currentIndex = m_cachedVertices.size();
      vertex->setPayload(static_cast<GLuint>(currentIndex));

      const auto& position = vertex->position();
      m_cachedVertices.emplace_back(
        vm::vec3f{position},
        vm::vec3f{face.boundary().normal},
        face.uvCoords(position),
        vm::vec<float, 1>{0.0f});

      currentHalfEdge = currentHalfEdge->previous();
    }

    if (const auto& displacement = face.sourceDisplacement())
    {
      indexOfFirstVertexRelativeToBrush = m_cachedVertices.size();
      for (size_t i = 0; i < displacement->positions().size(); ++i)
      {
        m_cachedVertices.emplace_back(
          vm::vec3f{displacement->positions()[i]},
          vm::vec3f{displacement->normals()[i]},
          face.uvCoords(displacement->basePositions()[i]),
          vm::vec<float, 1>{static_cast<float>(displacement->alphas()[i] / 255.0)});
      }
    }
    // face cache
    m_cachedFacesSortedByMaterial.emplace_back(&face, indexOfFirstVertexRelativeToBrush);
  }

  // Sort by material so BrushRenderer can efficiently step through the BrushFaces
  // grouped by material (via `BrushRendererBrushCache::cachedFacesSortedByMaterial()`),
  // without needing to build an std::map

  std::ranges::sort(
    m_cachedFacesSortedByMaterial,
    [](const CachedFace& a, const CachedFace& b) { return a.material < b.material; });

  // Build edge index cache

  m_cachedEdges.clear();
  m_cachedEdges.reserve(brush.edgeCount());

  for (const auto* currentEdge : brush.edges())
  {
    const auto faceIndex1 = currentEdge->firstFace()->payload();
    const auto faceIndex2 = currentEdge->secondFace()->payload();
    contract_assert(faceIndex1 && faceIndex2);

    const auto& face1 = brush.face(*faceIndex1);
    const auto& face2 = brush.face(*faceIndex2);

    if (face1.sourceDisplacement() && face2.sourceDisplacement())
    {
      continue;
    }
    const auto vertexIndex1RelativeToBrush = currentEdge->firstVertex()->payload();
    const auto vertexIndex2RelativeToBrush = currentEdge->secondVertex()->payload();

    m_cachedEdges.push_back(
      CachedEdge{
        face1.sourceDisplacement() ? &face2 : &face1,
        face2.sourceDisplacement() ? &face1 : &face2,
        vertexIndex1RelativeToBrush,
        vertexIndex2RelativeToBrush});
  }

  for (const auto& cached : m_cachedFacesSortedByMaterial)
  {
    if (const auto& displacement = cached.face->sourceDisplacement())
    {
      const auto side = displacement->size();
      const auto edge = [&](const size_t a, const size_t b) {
        m_cachedEdges.push_back(
          CachedEdge{
            cached.face,
            cached.face,
            static_cast<GLuint>(cached.indexOfFirstVertexRelativeToBrush + a),
            static_cast<GLuint>(cached.indexOfFirstVertexRelativeToBrush + b)});
      };
      for (size_t y = 0; y < side; ++y)
      {
        for (size_t x = 0; x < side; ++x)
        {
          const auto i = y * side + x;
          if (x + 1 < side)
          {
            edge(i, i + 1);
          }
          if (y + 1 < side)
          {
            edge(i, i + side);
          }
          if (x + 1 < side && y + 1 < side)
          {
            if ((x + y) % 2)
            {
              edge(i + 1, i + side);
            }
            else
            {
              edge(i, i + side + 1);
            }
          }
        }
      }
    }
  }
  m_rendererCacheValid = true;
}

const std::vector<BrushRendererBrushCache::Vertex>& BrushRendererBrushCache::
  cachedVertices() const
{
  contract_pre(m_rendererCacheValid);

  return m_cachedVertices;
}

const std::vector<BrushRendererBrushCache::CachedFace>& BrushRendererBrushCache::
  cachedFacesSortedByMaterial() const
{
  contract_pre(m_rendererCacheValid);

  return m_cachedFacesSortedByMaterial;
}

const std::vector<BrushRendererBrushCache::CachedEdge>& BrushRendererBrushCache::
  cachedEdges() const
{
  contract_pre(m_rendererCacheValid);

  return m_cachedEdges;
}

} // namespace tb::mdl
