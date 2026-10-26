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

#include "mdl/EntityModel.h"

#include "gl/IndexRangeMap.h"
#include "gl/MaterialCollection.h"
#include "gl/MaterialIndexRangeMap.h"
#include "gl/MaterialIndexRangeRenderer.h"
#include "gl/PrimType.h"
#include "gl/Texture.h"
#include "gl/VertexArray.h"

#include "kd/const_overload.h"
#include "kd/contracts.h"
#include "kd/reflection_impl.h"

#include "vm/bbox.h"
#include "vm/bbox_io.h" // IWYU pragma: keep
#include "vm/intersection.h"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <string>

namespace tb::mdl
{

std::ostream& operator<<(std::ostream& lhs, const PitchType rhs)
{
  switch (rhs)
  {
  case PitchType::Normal:
    lhs << "Normal";
    break;
  case PitchType::MdlInverted:
    lhs << "MdlInverted";
    break;
  }
  return lhs;
}

std::ostream& operator<<(std::ostream& lhs, const Orientation rhs)
{
  switch (rhs)
  {
  case Orientation::ViewPlaneParallelUpright:
    lhs << "ViewPlaneParallelUpright";
    break;
  case Orientation::FacingUpright:
    lhs << "FacingUpright";
    break;
  case Orientation::ViewPlaneParallel:
    lhs << "ViewPlaneParallel";
    break;
  case Orientation::Oriented:
    lhs << "Oriented";
    break;
  case Orientation::ViewPlaneParallelOriented:
    lhs << "ViewPlaneParallelOriented";
    break;
  }
  return lhs;
}


// EntityModelFrame

kdl_reflect_impl(EntityModelFrame);

EntityModelFrame::EntityModelFrame(
  const size_t index, std::string name, const vm::bbox3f& bounds)
  : m_index{index}
  , m_name{std::move(name)}
  , m_bounds{bounds}
  , m_spacialTree{16.0f}
{
}

EntityModelFrame::EntityModelFrame(
  const vm::bbox3f& bounds, std::vector<const EntityModelFrame*> parts)
  : EntityModelFrame{0, "static", bounds}
{
  m_parts = std::move(parts);
}

size_t EntityModelFrame::index() const
{
  return m_index;
}

size_t EntityModelFrame::skinOffset() const
{
  return m_skinOffset;
}

void EntityModelFrame::setSkinOffset(const size_t skinOffset)
{
  m_skinOffset = skinOffset;
}

const std::string& EntityModelFrame::name() const
{
  return m_name;
}

const vm::bbox3f& EntityModelFrame::bounds() const
{
  return m_bounds;
}

std::optional<float> EntityModelFrame::intersect(const vm::ray3f& ray) const
{
  auto closestDistance = std::optional<float>{};

  for (const auto* part : m_parts)
  {
    closestDistance = vm::safe_min(closestDistance, part->intersect(ray));
  }

  const auto candidates = m_spacialTree.find_intersectors(ray);
  for (const auto triNum : candidates)
  {
    const auto& p1 = m_tris[triNum * 3 + 0];
    const auto& p2 = m_tris[triNum * 3 + 1];
    const auto& p3 = m_tris[triNum * 3 + 2];
    closestDistance =
      vm::safe_min(closestDistance, vm::intersect_ray_triangle(ray, p1, p2, p3));
  }

  return closestDistance;
}

void EntityModelFrame::addToSpacialTree(
  const std::vector<EntityModelVertex>& vertices,
  const gl::PrimType primType,
  const size_t index,
  const size_t count)
{
  switch (primType)
  {
  case gl::PrimType::Points:
  case gl::PrimType::Lines:
  case gl::PrimType::LineStrip:
  case gl::PrimType::LineLoop:
    break;
  case gl::PrimType::Triangles: {
    contract_assert(count % 3 == 0);

    m_tris.reserve(m_tris.size() + count);
    for (size_t i = 0; i < count; i += 3)
    {
      const auto& p1 = gl::getVertexComponent<0>(vertices[index + i + 0]);
      const auto& p2 = gl::getVertexComponent<0>(vertices[index + i + 1]);
      const auto& p3 = gl::getVertexComponent<0>(vertices[index + i + 2]);
      const auto bounds = vm::bbox3f::build(std::array{p1, p2, p3});

      const auto triIndex = m_tris.size() / 3u;
      m_tris.push_back(p1);
      m_tris.push_back(p2);
      m_tris.push_back(p3);

      m_spacialTree.insert(*bounds, triIndex);
    }
    break;
  }
  case gl::PrimType::Polygon:
  case gl::PrimType::TriangleFan: {
    contract_assert(count > 2);

    m_tris.reserve(m_tris.size() + (count - 2) * 3);

    const auto& p1 = gl::getVertexComponent<0>(vertices[index]);
    for (size_t i = 1; i < count - 1; ++i)
    {
      const auto& p2 = gl::getVertexComponent<0>(vertices[index + i]);
      const auto& p3 = gl::getVertexComponent<0>(vertices[index + i + 1]);
      const auto bounds = vm::bbox3f::build(std::array{p1, p2, p3});

      const auto triIndex = m_tris.size() / 3u;
      m_tris.push_back(p1);
      m_tris.push_back(p2);
      m_tris.push_back(p3);
      m_spacialTree.insert(*bounds, triIndex);
    }
    break;
  }
  case gl::PrimType::Quads:
  case gl::PrimType::QuadStrip:
  case gl::PrimType::TriangleStrip: {
    contract_assert(count > 2);

    m_tris.reserve(m_tris.size() + (count - 2) * 3);
    for (size_t i = 0; i < count - 2; ++i)
    {
      const auto& p1 = gl::getVertexComponent<0>(vertices[index + i + 0]);
      const auto& p2 = gl::getVertexComponent<0>(vertices[index + i + 1]);
      const auto& p3 = gl::getVertexComponent<0>(vertices[index + i + 2]);
      const auto bounds = vm::bbox3f::build(std::array{p1, p2, p3});

      const auto triIndex = m_tris.size() / 3u;
      if (i % 2 == 0)
      {
        m_tris.push_back(p1);
        m_tris.push_back(p2);
        m_tris.push_back(p3);
      }
      else
      {
        m_tris.push_back(p1);
        m_tris.push_back(p3);
        m_tris.push_back(p2);
      }
      m_spacialTree.insert(*bounds, triIndex);
    }
    break;
  }
    switchDefault();
  }
}

// EntityModelData::Mesh

/**
 * The mesh associated with a frame and a surface.
 */
class EntityModelMesh
{
private:
  mutable gl::VertexArray::WeakRef m_vertexArray;

protected:
  std::vector<EntityModelVertex> m_vertices;

  kdl_reflect_inline_empty(EntityModelMesh);

  /**
   * Creates a new frame mesh that uses the given vertices.
   *
   * @param vertices the vertices
   */
  explicit EntityModelMesh(std::vector<EntityModelVertex> vertices)
    : m_vertices{std::move(vertices)}
  {
  }

public:
  virtual ~EntityModelMesh() = default;

public:
  /**
   * Returns a renderer that renders this mesh with the given material.
   *
   * @param skin the material to use when rendering the mesh
   * @return the renderer
   */
  std::unique_ptr<gl::MaterialIndexRangeRenderer> buildRenderer(
    const gl::Material* skin) const
  {
    auto vertexArray = m_vertexArray.lock();
    if (!vertexArray)
    {
      vertexArray = gl::VertexArray::ref(m_vertices);
      m_vertexArray = vertexArray->weakRef();
    }
    return doBuildRenderer(skin, *vertexArray);
  }

private:
  /**
   * Creates and returns the actual mesh renderer
   *
   * @param skin the skin to use when rendering the mesh
   * @param vertices the vertices associated with this mesh
   * @return the renderer
   */
  virtual std::unique_ptr<gl::MaterialIndexRangeRenderer> doBuildRenderer(
    const gl::Material* skin, const gl::VertexArray& vertices) const = 0;
};

// EntityModelData::IndexedMesh

namespace
{

/**
 * A model frame mesh for indexed rendering. Stores vertices and vertex indices.
 */
class EntityModelIndexedMesh : public EntityModelMesh
{
private:
  gl::IndexRangeMap m_indices;

  kdl_reflect_inline_empty(EntityModelIndexedMesh);

public:
  /**
   * Creates a new frame mesh with the given vertices and indices.
   *
   * @param frame the frame to which this mesh belongs
   * @param vertices the vertices
   * @param indices the indices
   */
  EntityModelIndexedMesh(
    EntityModelFrame& frame,
    std::vector<EntityModelVertex> vertices,
    gl::IndexRangeMap indices)
    : EntityModelMesh{std::move(vertices)}
    , m_indices{std::move(indices)}
  {
    m_indices.forEachPrimitive(
      [&](const gl::PrimType primType, const size_t index, const size_t count) {
        frame.addToSpacialTree(m_vertices, primType, index, count);
      });
  }

private:
  std::unique_ptr<gl::MaterialIndexRangeRenderer> doBuildRenderer(
    const gl::Material* skin, const gl::VertexArray& vertices) const override
  {
    const gl::MaterialIndexRangeMap indices(skin, m_indices);
    return std::make_unique<gl::MaterialIndexRangeRenderer>(vertices, indices);
  }
};

// EntityModelMaterialMesh

/**
 * A model frame mesh for per material indexed rendering. Stores vertices and per material
 * indices.
 */
class EntityModelMaterialMesh : public EntityModelMesh
{
private:
  gl::MaterialIndexRangeMap m_indices;

  kdl_reflect_inline_empty(EntityModelMaterialMesh);

public:
  /**
   * Creates a new frame mesh with the given vertices and per material indices.
   *
   * @param frame the frame to which this mesh belongs
   * @param vertices the vertices
   * @param indices the per material indices
   */
  EntityModelMaterialMesh(
    EntityModelFrame& frame,
    std::vector<EntityModelVertex> vertices,
    gl::MaterialIndexRangeMap indices)
    : EntityModelMesh{std::move(vertices)}
    , m_indices{std::move(indices)}
  {
    m_indices.forEachPrimitive([&](
                                 const gl::Material* /* material */,
                                 const gl::PrimType primType,
                                 const size_t index,
                                 const size_t count) {
      frame.addToSpacialTree(m_vertices, primType, index, count);
    });
  }

private:
  std::unique_ptr<gl::MaterialIndexRangeRenderer> doBuildRenderer(
    const gl::Material* /* skin */, const gl::VertexArray& vertices) const override
  {
    return std::make_unique<gl::MaterialIndexRangeRenderer>(vertices, m_indices);
  }
};

} // namespace

// EntityModelSurface

kdl_reflect_impl(EntityModelSurface);

EntityModelSurface::EntityModelSurface(std::string name, const size_t frameCount)
  : m_name{std::move(name)}
  , m_meshes{frameCount}
  , m_skins{std::make_unique<gl::MaterialCollection>()}
{
}

EntityModelSurface::~EntityModelSurface() = default;

const std::string& EntityModelSurface::name() const
{
  return m_name;
}

void EntityModelSurface::upload(gl::Gl& gl)
{
  for (auto& material : m_skins->materials())
  {
    if (auto* texture = material.texture())
    {
      texture->upload(gl);
    }
  }
}

void EntityModelSurface::drop(gl::Gl& gl)
{
  for (auto& material : m_skins->materials())
  {
    if (auto* texture = material.texture())
    {
      texture->drop(gl);
    }
  }
}

void EntityModelSurface::addMesh(
  EntityModelFrame& frame,
  std::vector<EntityModelVertex> vertices,
  gl::IndexRangeMap indices)
{
  contract_pre(frame.index() < frameCount());

  m_meshes[frame.index()] = std::make_unique<EntityModelIndexedMesh>(
    frame, std::move(vertices), std::move(indices));
}

void EntityModelSurface::addMesh(
  EntityModelFrame& frame,
  std::vector<EntityModelVertex> vertices,
  gl::MaterialIndexRangeMap indices)
{
  contract_pre(frame.index() < frameCount());

  m_meshes[frame.index()] = std::make_unique<EntityModelMaterialMesh>(
    frame, std::move(vertices), std::move(indices));
}

void EntityModelSurface::setSkins(std::vector<gl::Material> skins)
{
  m_skins = std::make_unique<gl::MaterialCollection>(std::move(skins));
}

size_t EntityModelSurface::frameCount() const
{
  return m_meshes.size();
}

size_t EntityModelSurface::skinCount() const
{
  return m_skins->materialCount();
}

const gl::Material* EntityModelSurface::skin(const std::string& name) const
{
  return m_skins->materialByName(name);
}

const gl::Material* EntityModelSurface::skin(const size_t index) const
{
  return m_skins->materialByIndex(index);
}

std::unique_ptr<gl::MaterialIndexRangeRenderer> EntityModelSurface::buildRenderer(
  const size_t skinIndex, const size_t frameIndex) const
{
  const auto* mesh = frameIndex < frameCount() ? m_meshes[frameIndex].get() : nullptr;
  return mesh ? mesh->buildRenderer(skin(skinIndex)) : nullptr;
}

// EntityModelData

kdl_reflect_impl(EntityModelData);

EntityModelData::EntityModelData(const PitchType pitchType, const Orientation orientation)
  : m_pitchType{pitchType}
  , m_orientation{orientation}
{
}

PitchType EntityModelData::pitchType() const
{
  return m_pitchType;
}

Orientation EntityModelData::orientation() const
{
  return m_orientation;
}

std::unique_ptr<gl::MaterialRenderer> EntityModelData::buildRenderer(
  const size_t skinIndex, const size_t frameIndex, const uint32_t body) const
{
  auto renderers = std::vector<std::unique_ptr<gl::MaterialIndexRangeRenderer>>{};
  if (frameIndex >= frameCount())
  {
    return nullptr;
  }

  const auto* frame = this->frame(frameIndex, body);
  const auto actualSkinIndex = skinIndex + frame->skinOffset();
  const auto* selected =
    m_bodyParts.empty() ? nullptr : &m_bodySelections[bodySelectionIndex(body)].surfaces;
  for (size_t i = 0; i < m_surfaces.size(); ++i)
  {
    if (selected && std::ranges::find(*selected, i) == selected->end())
    {
      continue;
    }
    const auto& surface = m_surfaces[i];
    // If an out of range skin is requested, use the first skin as a fallback
    const auto correctedSkinIndex =
      actualSkinIndex < surface.skinCount() ? actualSkinIndex : 0;
    if (auto renderer = surface.buildRenderer(correctedSkinIndex, frameIndex))
    {
      renderers.push_back(std::move(renderer));
    }
  }
  return !renderers.empty() || !m_bodyParts.empty()
           ? std::make_unique<gl::MultiMaterialIndexRangeRenderer>(std::move(renderers))
           : nullptr;
}

vm::bbox3f EntityModelData::bounds(const size_t frameIndex, const uint32_t body) const
{
  const auto* selected = frame(frameIndex, body);
  return selected ? selected->bounds() : vm::bbox3f{8.0f};
}

void EntityModelData::upload(gl::Gl& gl)
{
  for (auto& surface : m_surfaces)
  {
    surface.upload(gl);
  }
}

void EntityModelData::drop(gl::Gl& gl)
{
  for (auto& surface : m_surfaces)
  {
    surface.drop(gl);
  }
}

EntityModelFrame& EntityModelData::addFrame(std::string name, const vm::bbox3f& bounds)
{
  return m_frames.emplace_back(frameCount(), std::move(name), bounds);
}

EntityModelSurface& EntityModelData::addSurface(std::string name, const size_t frameCount)
{
  return m_surfaces.emplace_back(std::move(name), frameCount);
}

size_t EntityModelData::frameCount() const
{
  return m_frames.size();
}

size_t EntityModelData::surfaceCount() const
{
  return m_surfaces.size();
}

const std::vector<EntityModelFrame>& EntityModelData::frames() const
{
  return m_frames;
}

std::vector<EntityModelFrame>& EntityModelData::frames()
{
  return m_frames;
}

const std::vector<EntityModelSurface>& EntityModelData::surfaces() const
{
  return m_surfaces;
}

const EntityModelFrame* EntityModelData::frame(const std::string& name) const
{
  const auto it = std::ranges::find_if(
    m_frames, [&](const auto& frame) { return frame.name() == name; });
  return it != m_frames.end() ? frame(it->index()) : nullptr;
}

const EntityModelFrame* EntityModelData::frame(
  const size_t index, const uint32_t body) const
{
  if (index >= frameCount())
  {
    return nullptr;
  }
  return m_bodyParts.empty() ? &m_frames[index]
                             : m_bodySelections[bodySelectionIndex(body)].frame.get();
}

void EntityModelData::setBodyParts(
  std::vector<EntityModelBodyPart> parts,
  std::vector<std::unique_ptr<EntityModelFrame>> surfaceFrames)
{
  contract_assert(m_bodyParts.empty());
  contract_assert(!parts.empty());
  contract_assert(frameCount() == 1);
  contract_assert(surfaceFrames.size() == surfaceCount());
  for (const auto& part : parts)
  {
    contract_assert(!part.models.empty());
    contract_assert(part.models.size() == 1 || part.base > 0);
    for (const auto& model : part.models)
    {
      for (const auto surface : model)
      {
        contract_assert(surface < surfaceCount());
      }
    }
  }
  m_bodyParts = std::move(parts);
  m_surfaceFrames = std::move(surfaceFrames);
  m_bodySelectionIndices.clear();
  m_bodySelections.clear();
  // Keep the public default frame list consistent too; only references and
  // bounds are copied, never the picking triangles or render geometry.
  const auto defaultIndex = bodySelectionIndex(0);
  auto frames = std::vector<const EntityModelFrame*>{};
  for (const auto surface : m_bodySelections[defaultIndex].surfaces)
  {
    frames.push_back(m_surfaceFrames[surface].get());
  }
  m_frames[0] =
    EntityModelFrame{m_bodySelections[defaultIndex].frame->bounds(), std::move(frames)};
}

bool EntityModelData::surfaceActive(const size_t surfaceIndex, const uint32_t body) const
{
  if (m_bodyParts.empty())
  {
    return surfaceIndex < surfaceCount();
  }
  const auto& surfaces = m_bodySelections[bodySelectionIndex(body)].surfaces;
  return std::ranges::find(surfaces, surfaceIndex) != surfaces.end();
}

size_t EntityModelData::bodySelectionIndex(const uint32_t body) const
{
  if (m_bodyParts.empty())
  {
    return 0;
  }
  auto models = std::vector<size_t>{};
  for (const auto& part : m_bodyParts)
  {
    models.push_back(
      part.models.size() == 1 ? 0 : (body / part.base) % part.models.size());
  }
  if (
    const auto it = m_bodySelectionIndices.find(models);
    it != m_bodySelectionIndices.end())
  {
    return it->second;
  }
  auto surfaces = std::vector<size_t>{};
  auto frames = std::vector<const EntityModelFrame*>{};
  auto bounds = std::optional<vm::bbox3f>{};
  for (size_t i = 0; i < models.size(); ++i)
  {
    for (const auto surface : m_bodyParts[i].models[models[i]])
    {
      surfaces.push_back(surface);
      const auto* frame = m_surfaceFrames[surface].get();
      frames.push_back(frame);
      bounds = bounds ? vm::merge(*bounds, frame->bounds()) : frame->bounds();
    }
  }
  const auto index = m_bodySelections.size();
  m_bodySelections.push_back(
    {std::move(surfaces),
     std::make_unique<EntityModelFrame>(
       bounds.value_or(vm::bbox3f{0.0f}), std::move(frames))});
  m_bodySelectionIndices.emplace(std::move(models), index);
  return index;
}

const EntityModelSurface& EntityModelData::surface(const size_t index) const
{
  if (index >= surfaceCount())
  {
    throw std::out_of_range{"Surface index is out of bounds"};
  }
  return m_surfaces[index];
}

EntityModelSurface& EntityModelData::surface(const size_t index)
{
  return KDL_CONST_OVERLOAD(surface(index));
}

const EntityModelSurface* EntityModelData::surface(const std::string& name) const
{
  const auto it = std::ranges::find_if(
    m_surfaces, [&](const auto& surface) { return surface.name() == name; });
  return it != m_surfaces.end() ? &*it : nullptr;
}

kdl_reflect_impl(EntityModel);

EntityModel::EntityModel(
  std::string name, std::shared_ptr<EntityModelDataResource> dataResource)
  : m_name{std::move(name)}
  , m_dataResource{std::move(dataResource)}
{
}

const std::string& EntityModel::name() const
{
  return m_name;
}

const EntityModelData* EntityModel::data() const
{
  return m_dataResource->get();
}

EntityModelData* EntityModel::data()
{
  return m_dataResource->get();
}

const EntityModelDataResource& EntityModel::dataResource() const
{
  return *m_dataResource;
}

} // namespace tb::mdl
