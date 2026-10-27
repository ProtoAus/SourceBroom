/*
SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "mdl/VmfDocument.h"

#include "vm/bbox.h"
#include "vm/mat.h"
#include "vm/ray.h"
#include "vm/vec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace tb::mdl
{
/** Native displacement samples and their exact document-row provenance. */
class SourceDisplacement
{
private:
  struct NumberSpan
  {
    size_t begin;
    size_t end;
    bool operator==(const NumberSpan&) const = default;
  };
  struct Row
  {
    std::optional<VmfEntryId> entry;
    std::string text;
    std::vector<NumberSpan> spans;
    bool operator==(const Row&) const = default;
  };

  VmfEntryId m_entry = 0;
  size_t m_power = 2;
  size_t m_size = 5;
  vm::vec3d m_start;
  Row m_startRow;
  double m_elevation = 0;
  Row m_elevationRow;
  std::array<vm::vec3d, 4> m_corners;
  std::array<vm::vec3d, 4> m_originalCorners;
  vm::vec3d m_originalFaceNormal;
  vm::mat4x4d m_baseTransform = vm::mat4x4d::identity();
  // Keep accepted scale independent of approximate rotation-matrix arithmetic.
  double m_baseScale = 1;
  vm::vec3d m_faceNormal;
  vm::bbox3d m_worldBounds;
  std::vector<vm::vec3d> m_directions;
  std::vector<double> m_distances;
  std::vector<vm::vec3d> m_offsets;
  std::vector<vm::vec3d> m_offsetNormals;
  std::vector<double> m_alphas;
  std::vector<Row> m_directionRows;
  std::vector<Row> m_distanceRows;
  std::vector<Row> m_alphaRows;
  std::vector<Row> m_offsetRows;
  std::vector<Row> m_offsetNormalRows;
  bool m_normalsBound = false;
  bool m_rotationSupported = true;
  std::vector<vm::vec3d> m_positions;
  std::vector<vm::vec3d> m_basePositions;
  std::vector<vm::vec3d> m_normals;
  std::vector<uint32_t> m_indices;
  vm::bbox3d m_bounds;

  void rebuild();
  SourceDisplacement similarityTransformed(
    const vm::mat4x4d& transform, double scale) const;
  void validateTransformedSamples(
    const SourceDisplacement& before, const vm::mat4x4d& transform) const;

public:
  static constexpr size_t MaxPower = 6;
  static constexpr double TranslationEpsilon = 0.000001;
  static constexpr double TransformEpsilon = TranslationEpsilon;

  /** Corners are the underlying BrushFace's counter-clockwise vertex positions. */
  static std::optional<SourceDisplacement> read(
    const VmfDocument& document,
    VmfEntryId side,
    const std::vector<vm::vec3d>& corners,
    const vm::vec3d& faceNormal,
    const vm::bbox3d& worldBounds,
    size_t maxVertices = 1048576);

  size_t power() const { return m_power; }
  size_t size() const { return m_size; }
  const std::vector<vm::vec3d>& positions() const { return m_positions; }
  const std::vector<vm::vec3d>& basePositions() const { return m_basePositions; }
  const std::vector<vm::vec3d>& normals() const { return m_normals; }
  const std::vector<uint32_t>& indices() const { return m_indices; }
  const std::vector<double>& alphas() const { return m_alphas; }
  const vm::bbox3d& bounds() const { return m_bounds; }
  double distance(size_t row, size_t column) const;
  double alpha(size_t row, size_t column) const;
  bool canEditDistances() const;
  bool canEditAlphas() const;
  void setDistance(size_t row, size_t column, double distance);
  void setAlpha(size_t row, size_t column, double alpha);
  SourceDisplacement translated(const vm::vec3d& delta) const;
  /** Explicit rotation path only; not permission for generic affine commands. */
  SourceDisplacement rigidlyTransformed(const vm::mat4x4d& transform) const;
  /** Explicit positive uniform scale path; the linear matrix must be exactly s*I. */
  SourceDisplacement uniformlyScaled(const vm::mat4x4d& transform) const;
  std::optional<vm::mat4x4d> baseTransformFrom(const SourceDisplacement& before) const;
  /** Only a uniform base translation, within plane-intersection roundoff. */
  std::optional<vm::vec3d> baseTranslationFrom(const SourceDisplacement& before) const;
  std::optional<double> intersect(const vm::ray3d& ray) const;

  /** Changed samples replace only their numeric tokens, not untouched row spelling. */
  std::vector<VmfValueEdit> editsFrom(const SourceDisplacement& before) const;
  bool operator==(const SourceDisplacement&) const = default;
  friend std::ostream& operator<<(std::ostream&, const SourceDisplacement&);
};
} // namespace tb::mdl
