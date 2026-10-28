/*
SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "mdl/SourceDisplacement.h"

#include "vm/intersection.h"
#include "vm/mat_ext.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <numeric>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace tb::mdl
{
namespace
{
[[noreturn]] void invalid(const VmfEntryId id, const std::string& reason)
{
  throw std::invalid_argument{
    "VMF displacement entry " + std::to_string(id) + ": " + reason};
}
std::string decode(const VmfDocument& document, const VmfToken& token)
{
  const auto raw = document.tokenContents(token);
  auto result = std::string{};
  for (size_t i = 0; i < raw.size(); ++i)
  {
    if (raw[i] == '\\' && i + 1 < raw.size() && (raw[i + 1] == '\\' || raw[i + 1] == '"'))
    {
      ++i;
    }
    result += raw[i];
  }
  return result;
}
std::string key(const VmfDocument& document, const VmfEntryId id)
{
  return decode(document, document.entries()[id].key);
}
std::optional<VmfEntryId> member(
  const VmfDocument& document,
  const VmfEntryId owner,
  const std::string& name,
  const bool block)
{
  auto found = std::optional<VmfEntryId>{};
  for (const auto id : document.entries()[owner].children)
  {
    if (key(document, id) == name)
    {
      if (found || document.entries()[id].isBlock() != block)
      {
        invalid(owner, "ambiguous or wrong-shaped " + name);
      }
      found = id;
    }
  }
  return found;
}
bool hasAliasedMember(
  const VmfDocument& document, const VmfEntryId owner, const std::string& name)
{
  return std::any_of(
    document.entries()[owner].children.begin(),
    document.entries()[owner].children.end(),
    [&](const auto id) {
      const auto candidate = key(document, id);
      return candidate != name && candidate.size() == name.size()
             && std::equal(
               candidate.begin(),
               candidate.end(),
               name.begin(),
               [](const unsigned char a, const unsigned char b) {
                 return std::tolower(a) == std::tolower(b);
               });
    });
}
std::string value(const VmfDocument& document, const VmfEntryId id)
{
  return decode(document, *document.entries()[id].value);
}
struct ParsedNumber
{
  double value;
  size_t begin;
  size_t end;
};
std::vector<ParsedNumber> numbers(
  const std::string& text, const size_t count, const VmfEntryId id)
{
  auto result = std::vector<ParsedNumber>{};
  size_t offset = 0;
  while (offset < text.size())
  {
    if (std::isspace(static_cast<unsigned char>(text[offset])))
    {
      ++offset;
      continue;
    }
    const auto start = offset;
    while (offset < text.size()
           && !std::isspace(static_cast<unsigned char>(text[offset])))
    {
      ++offset;
    }
    auto token = std::string_view{text}.substr(start, offset - start);
    if (!token.empty() && token.front() == '+')
    {
      token.remove_prefix(1);
      if (token.empty() || token.front() == '+' || token.front() == '-')
      {
        invalid(id, "invalid numeric sign");
      }
    }
    auto number = double{};
    const auto parsed =
      std::from_chars(token.data(), token.data() + token.size(), number);
    if (
      parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()
      || !std::isfinite(number) || result.size() == count)
    {
      invalid(id, "invalid finite grid row");
    }
    result.push_back({number, start, offset});
  }
  if (result.size() != count)
  {
    invalid(id, "grid row width does not match displacement power");
  }
  return result;
}
bool properRigidTransform(const vm::mat4x4d& transform)
{
  for (size_t c = 0; c < 4; ++c)
  {
    for (size_t r = 0; r < 4; ++r)
    {
      if (!std::isfinite(transform[c][r]))
      {
        return false;
      }
    }
  }
  if (
    transform[0][3] != 0 || transform[1][3] != 0 || transform[2][3] != 0
    || transform[3][3] != 1)
  {
    return false;
  }
  const auto rotation = vm::slice<3, 3>(transform, 0, 0);
  for (size_t a = 0; a < 3; ++a)
  {
    for (size_t b = 0; b < 3; ++b)
    {
      if (std::abs(vm::dot(rotation[a], rotation[b]) - (a == b ? 1.0 : 0.0)) > 1e-12)
      {
        return false;
      }
    }
  }
  return vm::dot(vm::cross(rotation[0], rotation[1]), rotation[2]) > 0;
}
std::string numberText(const double value)
{
  if (!std::isfinite(value))
  {
    invalid(0, "cannot emit a nonfinite native scalar");
  }
  auto out = std::ostringstream{};
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  return out.str();
}
using SewEdge = std::vector<size_t>;
using SewPlan = std::array<SewEdge, 2>;
std::optional<SewPlan> planSew(
  const SourceDisplacement& first,
  const SourceDisplacement& second,
  bool* ambiguous = nullptr)
{
  if (ambiguous)
  {
    *ambiguous = false;
  }
  const auto size = first.size();
  if (
    &first == &second || size < 5
    || size > (size_t{1} << SourceDisplacement::MaxPower) + 1 || size != second.size()
    || first.power() != second.power() || first.positions().size() != size * size
    || second.positions().size() != size * size
    || first.basePositions().size() != size * size
    || second.basePositions().size() != size * size
    || !(first.canEditOffsets() || first.canInitializeOffsets())
    || !(second.canEditOffsets() || second.canInitializeOffsets()))
  {
    return std::nullopt;
  }
  auto edges = std::array<SewEdge, 4>{};
  for (size_t i = 0; i < size; ++i)
  {
    edges[0].push_back(i);
    edges[1].push_back(i * size + size - 1);
    edges[2].push_back((size - 1) * size + size - 1 - i);
    edges[3].push_back((size - 1 - i) * size);
  }
  auto result = std::optional<SewPlan>{};
  for (const auto& a : edges)
  {
    for (const auto& b : edges)
    {
      for (const auto reverse : {false, true})
      {
        auto candidate = b;
        if (reverse)
        {
          std::reverse(candidate.begin(), candidate.end());
        }
        auto matches = true;
        for (size_t i = 0; i < size; ++i)
        {
          if (
            vm::length(first.basePositions()[a[i]] - second.basePositions()[candidate[i]])
            > SourceDisplacement::TransformEpsilon)
          {
            matches = false;
            break;
          }
        }
        if (matches)
        {
          if (result)
          {
            if (ambiguous)
            {
              *ambiguous = true;
            }
            return std::nullopt;
          }
          result = SewPlan{a, std::move(candidate)};
        }
      }
    }
  }
  return result;
}
std::optional<std::vector<SewEdge>> planSewTogether(
  const std::vector<const SourceDisplacement*>& meshes)
{
  if (
    meshes.size() < 2 || meshes.size() > SourceDisplacement::MaxSewMeshes
    || !meshes.front())
  {
    return std::nullopt;
  }
  const auto size = meshes.front()->size();
  if (size < 5 || size > (size_t{1} << SourceDisplacement::MaxPower) + 1)
  {
    return std::nullopt;
  }
  const auto samples = size * size;
  for (size_t i = 0; i < meshes.size(); ++i)
  {
    const auto* mesh = meshes[i];
    if (
      !mesh || mesh->size() != size || mesh->power() != meshes.front()->power()
      || mesh->basePositions().size() != samples || mesh->positions().size() != samples
      || !(mesh->canEditOffsets() || mesh->canInitializeOffsets())
      || std::find(meshes.begin(), meshes.begin() + i, mesh) != meshes.begin() + i)
    {
      return std::nullopt;
    }
  }
  auto parents = std::vector<size_t>(meshes.size() * samples);
  std::iota(parents.begin(), parents.end(), size_t{0});
  const auto root = [&](size_t index) {
    while (parents[index] != index)
    {
      parents[index] = parents[parents[index]];
      index = parents[index];
    }
    return index;
  };
  auto matched = std::vector<bool>(parents.size(), false);
  auto links = std::vector<std::vector<size_t>>(meshes.size());
  auto usedEdges = std::set<std::tuple<size_t, size_t, size_t>>{};
  for (size_t a = 0; a < meshes.size(); ++a)
  {
    for (size_t b = a + 1; b < meshes.size(); ++b)
    {
      auto ambiguous = false;
      const auto plan = planSew(*meshes[a], *meshes[b], &ambiguous);
      if (ambiguous)
      {
        return std::nullopt;
      }
      if (!plan)
      {
        continue;
      }
      for (size_t side = 0; side < 2; ++side)
      {
        const auto& edge = (*plan)[side];
        if (!usedEdges
               .emplace(
                 side == 0 ? a : b,
                 std::min(edge.front(), edge.back()),
                 std::max(edge.front(), edge.back()))
               .second)
        {
          return std::nullopt;
        }
      }
      links[a].push_back(b);
      links[b].push_back(a);
      for (size_t i = 0; i < size; ++i)
      {
        const auto first = a * samples + (*plan)[0][i];
        const auto second = b * samples + (*plan)[1][i];
        parents[root(second)] = root(first);
        matched[first] = matched[second] = true;
      }
    }
  }
  auto visited = std::vector<bool>(meshes.size(), false);
  auto pending = std::vector<size_t>{0};
  while (!pending.empty())
  {
    const auto next = pending.back();
    pending.pop_back();
    if (!visited[next])
    {
      visited[next] = true;
      pending.insert(pending.end(), links[next].begin(), links[next].end());
    }
  }
  if (std::find(visited.begin(), visited.end(), false) != visited.end())
  {
    return std::nullopt;
  }
  auto groups = std::map<size_t, SewEdge>{};
  for (size_t i = 0; i < matched.size(); ++i)
  {
    if (matched[i])
    {
      groups[root(i)].push_back(i);
    }
  }
  auto result = std::vector<SewEdge>{};
  for (auto& [_, group] : groups)
  {
    for (const auto a : group)
    {
      if (!vm::is_finite(meshes[a / samples]->positions()[a % samples]))
      {
        return std::nullopt;
      }
      for (const auto b : group)
      {
        if (
          vm::length(
            meshes[a / samples]->basePositions()[a % samples]
            - meshes[b / samples]->basePositions()[b % samples])
          > SourceDisplacement::TransformEpsilon)
        {
          return std::nullopt; // Do not chain tolerance into a wider base-point group.
        }
      }
    }
    result.push_back(std::move(group));
  }
  return result;
}
double seamMean(std::vector<double> values)
{
  std::sort(values.begin(), values.end());
  if (values.size() == 2)
  {
    return std::midpoint(values[0], values[1]);
  }
  auto result = values.front();
  for (size_t i = 1; i < values.size(); ++i)
  {
    const auto count = double(i + 1);
    result = std::signbit(result) == std::signbit(values[i])
               ? result + (values[i] - result) / count
               : result * (double(i) / count) + values[i] / count;
  }
  return result;
}
} // namespace

std::optional<SourceDisplacement> SourceDisplacement::read(
  const VmfDocument& document,
  const VmfEntryId side,
  const std::vector<vm::vec3d>& corners,
  const vm::vec3d& faceNormal,
  const vm::bbox3d& worldBounds,
  const size_t maxVertices)
{
  const auto block = member(document, side, "dispinfo", true);
  if (!block)
  {
    return std::nullopt;
  }
  if (corners.size() != 4)
  {
    invalid(*block, "displacement requires a quadrilateral base face");
  }
  auto result = SourceDisplacement{};
  result.m_entry = *block;
  result.m_faceNormal = faceNormal;
  result.m_originalFaceNormal = faceNormal;
  result.m_worldBounds = worldBounds;
  const auto power = member(document, *block, "power", false);
  if (!power)
  {
    invalid(*block, "missing power");
  }
  const auto parsedPower = numbers(value(document, *power), 1, *power)[0].value;
  if (parsedPower < 2 || parsedPower > MaxPower || std::trunc(parsedPower) != parsedPower)
  {
    invalid(*power, "power outside supported bounded grid range 2..6");
  }
  result.m_power = static_cast<size_t>(parsedPower);
  result.m_size = (size_t{1} << result.m_power) + 1;
  if (result.m_size * result.m_size > maxVertices)
  {
    invalid(*block, "displacement vertex budget exceeded");
  }
  const auto start = member(document, *block, "startposition", false);
  if (!start)
  {
    invalid(*block, "missing startposition");
  }
  auto startText = value(document, *start);
  const auto first = startText.find_first_not_of(" \t\r\n");
  const auto last = startText.find_last_not_of(" \t\r\n");
  if (first == std::string::npos || startText[first] != '[' || startText[last] != ']')
  {
    invalid(*start, "invalid startposition tuple");
  }
  const auto startValues =
    numbers(startText.substr(first + 1, last - first - 1), 3, *start);
  result.m_start = {startValues[0].value, startValues[1].value, startValues[2].value};
  result.m_startRow.entry = *start;
  result.m_startRow.text = startText;
  for (const auto& component : startValues)
  {
    result.m_startRow.spans.push_back(
      {first + 1 + component.begin, first + 1 + component.end});
  }
  if (const auto elevation = member(document, *block, "elevation", false))
  {
    const auto text = value(document, *elevation);
    const auto scalar = numbers(text, 1, *elevation)[0];
    result.m_elevation = scalar.value;
    result.m_elevationRow = Row{*elevation, text, {{scalar.begin, scalar.end}}};
  }
  auto clockwise = corners;
  std::reverse(clockwise.begin(), clockwise.end());
  const auto nearest = std::min_element(
    clockwise.begin(), clockwise.end(), [&](const auto& a, const auto& b) {
      return vm::dot(a - result.m_start, a - result.m_start)
             < vm::dot(b - result.m_start, b - result.m_start);
    });
  const auto diagonal = corners[0] - corners[2];
  const auto tolerance =
    std::max(0.001, std::sqrt(vm::dot(diagonal, diagonal)) * 0.00001);
  if (
    vm::dot(*nearest - result.m_start, *nearest - result.m_start) > tolerance * tolerance)
  {
    invalid(*start, "startposition is not a base-face corner");
  }
  std::rotate(clockwise.begin(), nearest, clockwise.end());
  std::copy(clockwise.begin(), clockwise.end(), result.m_corners.begin());
  result.m_originalCorners = result.m_corners;
  const auto total = result.m_size * result.m_size;
  result.m_directions.assign(total, faceNormal);
  result.m_distances.assign(total, 0.0);
  result.m_offsets.assign(total, vm::vec3d{});
  result.m_offsetNormals.assign(total, vm::vec3d{});
  result.m_alphas.assign(total, 0.0);
  result.m_directionRows.resize(result.m_size);
  result.m_distanceRows.resize(result.m_size);
  result.m_alphaRows.resize(result.m_size);
  result.m_offsetRows.resize(result.m_size);
  result.m_offsetNormalRows.resize(result.m_size);
  const auto bindRow =
    [](auto& binding, const VmfEntryId row, const auto& text, const auto& values) {
      binding.entry = row;
      binding.text = text;
      for (const auto& component : values)
      {
        binding.spans.push_back({component.begin, component.end});
      }
    };
  const auto readRows =
    [&](const std::string& name, const size_t components, auto&& apply) {
      auto seen = std::vector<bool>(result.m_size, false);
      if (const auto rows = member(document, *block, name, true))
      {
        for (const auto row : document.entries()[*rows].children)
        {
          const auto nameText = key(document, row);
          if (document.entries()[row].isBlock() || !nameText.starts_with("row"))
          {
            invalid(row, "invalid grid row name or shape");
          }
          const auto suffix = std::string_view{nameText}.substr(3);
          auto index = size_t{};
          const auto parsed =
            std::from_chars(suffix.data(), suffix.data() + suffix.size(), index);
          if (
            parsed.ec != std::errc{} || parsed.ptr != suffix.data() + suffix.size()
            || index >= result.m_size || seen[index])
          {
            invalid(row, "duplicate or out-of-range grid row");
          }
          seen[index] = true;
          const auto text = value(document, row);
          const auto values = numbers(text, result.m_size * components, row);
          apply(index, row, text, values);
        }
      }
      return std::all_of(
        seen.begin(), seen.end(), [](const auto present) { return present; });
    };
  result.m_normalsBound = readRows(
    "normals",
    3,
    [&](const size_t y, const auto row, const auto& text, const auto& values) {
      bindRow(result.m_directionRows[y], row, text, values);
      for (size_t x = 0; x < result.m_size; ++x)
      {
        result.m_directions[y * result.m_size + x] = {
          values[3 * x].value, values[3 * x + 1].value, values[3 * x + 2].value};
      }
    });
  readRows(
    "distances",
    1,
    [&](const size_t y, const VmfEntryId row, const auto& text, const auto& values) {
      bindRow(result.m_distanceRows[y], row, text, values);
      for (size_t x = 0; x < result.m_size; ++x)
      {
        result.m_distances[y * result.m_size + x] = values[x].value;
      }
    });
  result.m_offsetAuthoringSupported = !hasAliasedMember(document, *block, "offsets");
  result.m_offsetTableMissing = !member(document, *block, "offsets", true);
  readRows(
    "offsets",
    3,
    [&](const size_t y, const auto row, const auto& text, const auto& values) {
      bindRow(result.m_offsetRows[y], row, text, values);
      for (size_t x = 0; x < result.m_size; ++x)
      {
        result.m_offsets[y * result.m_size + x] = {
          values[3 * x].value, values[3 * x + 1].value, values[3 * x + 2].value};
      }
    });
  try
  {
    readRows(
      "offset_normals",
      3,
      [&](const size_t y, const auto row, const auto& text, const auto& values) {
        bindRow(result.m_offsetNormalRows[y], row, text, values);
        for (size_t x = 0; x < result.m_size; ++x)
        {
          result.m_offsetNormals[y * result.m_size + x] = {
            values[3 * x].value, values[3 * x + 1].value, values[3 * x + 2].value};
        }
      });
  }
  catch (const std::invalid_argument&)
  {
    // This previously ignored metadata must not reduce unchanged-load coverage.
    // Translation preserves it; rotation requires typed vector provenance.
    result.m_rotationSupported = false;
    result.m_offsetNormals.assign(total, vm::vec3d{});
    result.m_offsetNormalRows.assign(result.m_size, Row{});
  }
  result.m_alphaAuthoringSupported = !hasAliasedMember(document, *block, "alphas");
  result.m_alphaTableMissing = !member(document, *block, "alphas", true);
  readRows(
    "alphas",
    1,
    [&](const size_t y, const auto row, const auto& text, const auto& values) {
      bindRow(result.m_alphaRows[y], row, text, values);
      for (size_t x = 0; x < result.m_size; ++x)
      {
        if (values[x].value < 0 || values[x].value > 255)
        {
          invalid(*block, "alpha sample outside 0..255");
        }
        result.m_alphas[y * result.m_size + x] = values[x].value;
      }
    });
  if (
    !result.m_normalsBound
    && std::any_of(
      result.m_distances.begin(), result.m_distances.end(), [](const auto v) {
        return v != 0;
      }))
  {
    invalid(*block, "nonzero distances require complete normal rows");
  }
  result.rebuild();
  return result;
}

void SourceDisplacement::rebuild()
{
  const auto total = m_size * m_size;
  m_positions.resize(total);
  m_basePositions.resize(total);
  for (size_t y = 0; y < m_size; ++y)
  {
    const auto v = static_cast<double>(y) / static_cast<double>(m_size - 1);
    for (size_t x = 0; x < m_size; ++x)
    {
      const auto u = static_cast<double>(x) / static_cast<double>(m_size - 1);
      const auto a = m_corners[0] * (1 - u) + m_corners[3] * u;
      const auto b = m_corners[1] * (1 - u) + m_corners[2] * u;
      const auto index = y * m_size + x;
      const auto base = a * (1 - v) + b * v;
      const auto point = base + m_directions[index] * m_distances[index]
                         + m_offsets[index] + m_faceNormal * m_elevation;
      if (!vm::is_finite(point) || !m_worldBounds.contains(point))
      {
        invalid(m_entry, "sample outside finite editor world bounds");
      }
      m_basePositions[index] = base;
      m_positions[index] = point;
    }
  }
  m_bounds = *vm::bbox3d::build(m_positions);
  m_indices.clear();
  for (size_t y = 0; y + 1 < m_size; ++y)
  {
    for (size_t x = 0; x + 1 < m_size; ++x)
    {
      const auto tl = static_cast<uint32_t>(y * m_size + x);
      const auto tr = tl + 1;
      const auto bl = tl + static_cast<uint32_t>(m_size);
      const auto br = bl + 1;
      const auto cell = (x + y) % 2 ? std::array{tl, bl, tr, tr, bl, br}
                                    : std::array{tl, bl, br, tl, br, tr};
      m_indices.insert(m_indices.end(), cell.begin(), cell.end());
    }
  }
  m_normals.assign(total, vm::vec3d{});
  for (size_t i = 0; i < m_indices.size(); i += 3)
  {
    const auto a = m_indices[i], b = m_indices[i + 1], c = m_indices[i + 2];
    const auto normal =
      -vm::cross(m_positions[b] - m_positions[a], m_positions[c] - m_positions[a]);
    m_normals[a] = m_normals[a] + normal;
    m_normals[b] = m_normals[b] + normal;
    m_normals[c] = m_normals[c] + normal;
  }
  for (auto& normal : m_normals)
  {
    const auto length = std::sqrt(vm::dot(normal, normal));
    normal = length > 0.0000001 ? normal / length : m_faceNormal;
  }
}

void SourceDisplacement::validateTransformedSamples(
  const SourceDisplacement& before, const vm::mat4x4d& transform) const
{
  for (size_t i = 0; i < m_positions.size(); ++i)
  {
    const auto expected = transform * before.m_positions[i];
    const auto expectedBase = transform * before.m_basePositions[i];
    if (
      !vm::is_finite(expected) || !vm::is_finite(expectedBase)
      || !vm::is_equal(m_positions[i], expected, TransformEpsilon)
      || !vm::is_equal(m_basePositions[i], expectedBase, TransformEpsilon))
    {
      invalid(m_entry, "base motion loses native sample precision");
    }
  }
}

double SourceDisplacement::distance(const size_t row, const size_t column) const
{
  if (row >= m_size || column >= m_size)
  {
    throw std::out_of_range{"displacement sample"};
  }
  return m_distances[row * m_size + column];
}
double SourceDisplacement::alpha(const size_t row, const size_t column) const
{
  if (row >= m_size || column >= m_size)
  {
    throw std::out_of_range{"displacement sample"};
  }
  return m_alphas[row * m_size + column];
}
vm::vec3d SourceDisplacement::offset(const size_t row, const size_t column) const
{
  if (row >= m_size || column >= m_size)
  {
    throw std::out_of_range{"displacement sample"};
  }
  return m_offsets[row * m_size + column];
}
bool SourceDisplacement::hasUnboundOffsetRows() const
{
  return m_size > 0 && m_offsetRows.size() == m_size
         && m_offsets.size() == m_size * m_size
         && std::all_of(m_offsetRows.begin(), m_offsetRows.end(), [](const auto& row) {
              return !row.entry && row.text.empty() && row.spans.empty();
            });
}
bool SourceDisplacement::canInitializeOffsets() const
{
  return m_offsetAuthoringSupported && m_offsetTableMissing && !m_offsetRowsInitialized
         && hasUnboundOffsetRows()
         && std::all_of(m_offsets.begin(), m_offsets.end(), [](const auto& offset) {
              return offset == vm::vec3d{};
            });
}
void SourceDisplacement::initializeOffsets()
{
  if (!canInitializeOffsets())
  {
    invalid(m_entry, "offset initialization requires a genuinely absent native table");
  }
  m_offsetRowsInitialized = true;
}
bool SourceDisplacement::canEditOffsets() const
{
  if (!m_offsetAuthoringSupported)
  {
    return false;
  }
  if (m_offsetRowsInitialized)
  {
    return m_offsetTableMissing && hasUnboundOffsetRows();
  }
  return m_offsetRows.size() == m_size && m_offsets.size() == m_size * m_size
         && std::all_of(
           m_offsetRows.begin(), m_offsetRows.end(), [this](const auto& row) {
             return row.entry && row.spans.size() == 3 * m_size;
           });
}
bool SourceDisplacement::hasUnboundAlphaRows() const
{
  return m_size > 0 && m_alphaRows.size() == m_size && m_alphas.size() == m_size * m_size
         && std::all_of(m_alphaRows.begin(), m_alphaRows.end(), [](const auto& row) {
              return !row.entry && row.text.empty() && row.spans.empty();
            });
}
bool SourceDisplacement::canInitializeAlphas() const
{
  return m_alphaAuthoringSupported && m_alphaTableMissing && !m_alphaRowsInitialized
         && hasUnboundAlphaRows()
         && std::all_of(m_alphas.begin(), m_alphas.end(), [](const double value) {
              return value == 0;
            });
}
void SourceDisplacement::initializeAlphas()
{
  if (!canInitializeAlphas())
  {
    invalid(m_entry, "alpha initialization requires a genuinely absent native table");
  }
  m_alphaRowsInitialized = true;
}
bool SourceDisplacement::canEditAlphas() const
{
  if (!m_alphaAuthoringSupported)
  {
    return false;
  }
  if (m_alphaRowsInitialized)
  {
    return m_alphaTableMissing && hasUnboundAlphaRows();
  }
  return m_alphaRows.size() == m_size && m_alphas.size() == m_size * m_size
         && std::all_of(m_alphaRows.begin(), m_alphaRows.end(), [this](const auto& row) {
              return row.entry && row.spans.size() == m_size;
            });
}
bool SourceDisplacement::canEditDistances() const
{
  return m_normalsBound
         && std::all_of(
           m_directions.begin(),
           m_directions.end(),
           [](const auto& direction) {
             const auto length2 = vm::dot(direction, direction);
             return std::isfinite(length2) && length2 > 1e-12;
           })
         && std::all_of(
           m_distanceRows.begin(), m_distanceRows.end(), [](const auto& row) {
             return row.entry.has_value();
           });
}
void SourceDisplacement::setDistance(
  const size_t row, const size_t column, const double distance)
{
  if (!canEditDistances())
  {
    invalid(m_entry, "distance editing requires complete native normal/distance rows");
  }
  if (!std::isfinite(distance))
  {
    invalid(m_entry, "non-finite distance edit");
  }
  const auto previous = this->distance(row, column);
  m_distances[row * m_size + column] = distance;
  try
  {
    rebuild();
  }
  catch (...)
  {
    m_distances[row * m_size + column] = previous;
    rebuild();
    throw;
  }
}
void SourceDisplacement::setAlpha(
  const size_t row, const size_t column, const double value)
{
  if (!canEditAlphas() || !std::isfinite(value) || value < 0 || value > 255)
  {
    invalid(m_entry, "alpha samples require bound rows and finite values in 0..255");
  }
  (void)alpha(row, column);
  // Blend weight only: geometry, normals and bounds remain unchanged.
  m_alphas[row * m_size + column] = value;
}
void SourceDisplacement::setOffset(
  const size_t row, const size_t column, const vm::vec3d& value)
{
  if (!canEditOffsets() || !vm::is_finite(value))
  {
    invalid(m_entry, "offset editing requires complete native rows and finite vectors");
  }
  const auto previous = offset(row, column);
  m_offsets[row * m_size + column] = value;
  try
  {
    rebuild();
  }
  catch (...)
  {
    m_offsets[row * m_size + column] = previous;
    rebuild();
    throw;
  }
}
bool SourceDisplacement::canSewWith(const SourceDisplacement& other) const
{
  return bool(planSew(*this, other));
}
bool SourceDisplacement::sewWith(SourceDisplacement& other)
{
  const auto plan = planSew(*this, other);
  if (!plan)
  {
    invalid(
      m_entry,
      "sew requires two supported same-power meshes with one full shared base edge");
  }
  auto first = *this;
  auto second = other;
  auto changed = false;
  for (size_t i = 0; i < m_size; ++i)
  {
    const auto a = (*plan)[0][i];
    const auto b = (*plan)[1][i];
    const auto& p = m_positions[a];
    const auto& q = other.m_positions[b];
    if (vm::length(p - q) <= TransformEpsilon)
    {
      continue;
    }
    if (!changed)
    {
      if (first.canInitializeOffsets())
      {
        first.initializeOffsets();
      }
      if (second.canInitializeOffsets())
      {
        second.initializeOffsets();
      }
    }
    const auto midpoint = vm::vec3d{
      std::midpoint(p.x(), q.x()),
      std::midpoint(p.y(), q.y()),
      std::midpoint(p.z(), q.z())};
    first.m_offsets[a] = first.m_offsets[a] + (midpoint - p);
    second.m_offsets[b] = second.m_offsets[b] + (midpoint - q);
    if (!vm::is_finite(first.m_offsets[a]) || !vm::is_finite(second.m_offsets[b]))
    {
      invalid(m_entry, "sewn native offsets are not finite");
    }
    changed = true;
  }
  if (!changed)
  {
    return false;
  }
  first.rebuild();
  second.rebuild();
  for (size_t i = 0; i < m_size; ++i)
  {
    if (
      vm::length(first.m_positions[(*plan)[0][i]] - second.m_positions[(*plan)[1][i]])
      > TransformEpsilon)
    {
      invalid(m_entry, "seam cannot be represented with native offsets");
    }
  }
  static_assert(std::is_nothrow_move_assignable_v<SourceDisplacement>);
  *this = std::move(first);
  other = std::move(second);
  return true;
}
bool SourceDisplacement::canSewTogether(
  const std::vector<const SourceDisplacement*>& meshes)
{
  return planSewTogether(meshes).has_value();
}
bool SourceDisplacement::sewTogether(const std::vector<SourceDisplacement*>& meshes)
{
  const auto originals =
    std::vector<const SourceDisplacement*>{meshes.begin(), meshes.end()};
  const auto plan = planSewTogether(originals);
  if (!plan)
  {
    throw std::invalid_argument{"Unsupported connected displacement position seams"};
  }
  if (meshes.size() == 2)
  {
    return meshes[0]->sewWith(*meshes[1]);
  }
  auto staged = std::vector<SourceDisplacement>{};
  staged.reserve(meshes.size());
  for (const auto* mesh : meshes)
  {
    staged.push_back(*mesh);
  }
  auto changed = std::vector<bool>(meshes.size(), false);
  auto targets = std::vector<std::pair<SewEdge, vm::vec3d>>{};
  const auto samples = meshes.front()->size() * meshes.front()->size();
  for (const auto& group : *plan)
  {
    auto needsChange = false;
    for (const auto a : group)
    {
      for (const auto b : group)
      {
        needsChange |= vm::length(
                         meshes[a / samples]->m_positions[a % samples]
                         - meshes[b / samples]->m_positions[b % samples])
                       > TransformEpsilon;
      }
    }
    if (!needsChange)
    {
      continue;
    }
    auto target = vm::vec3d{};
    for (size_t axis = 0; axis < 3; ++axis)
    {
      auto values = std::vector<double>{};
      for (const auto sample : group)
      {
        values.push_back(meshes[sample / samples]->m_positions[sample % samples][axis]);
      }
      target[axis] = seamMean(std::move(values));
    }
    if (!vm::is_finite(target))
    {
      invalid(meshes.front()->m_entry, "displacement seam average overflowed");
    }
    targets.emplace_back(group, target);
    for (const auto sample : group)
    {
      const auto mesh = sample / samples;
      const auto vertex = sample % samples;
      const auto offset =
        meshes[mesh]->m_offsets[vertex] + (target - meshes[mesh]->m_positions[vertex]);
      if (!vm::is_finite(offset))
      {
        invalid(meshes[mesh]->m_entry, "displacement seam offset overflowed");
      }
      if (offset == meshes[mesh]->m_offsets[vertex])
      {
        continue;
      }
      if (staged[mesh].canInitializeOffsets())
      {
        staged[mesh].initializeOffsets();
      }
      staged[mesh].m_offsets[vertex] = offset;
      changed[mesh] = true;
    }
  }
  if (targets.empty())
  {
    return false;
  }
  for (size_t i = 0; i < staged.size(); ++i)
  {
    if (changed[i])
    {
      staged[i].rebuild();
    }
  }
  for (const auto& [group, target] : targets)
  {
    for (const auto a : group)
    {
      const auto& actual = staged[a / samples].m_positions[a % samples];
      if (vm::length(actual - target) > TransformEpsilon)
      {
        invalid(
          meshes[a / samples]->m_entry, "displacement seam target is not representable");
      }
      for (const auto b : group)
      {
        if (
          vm::length(actual - staged[b / samples].m_positions[b % samples])
          > TransformEpsilon)
        {
          invalid(meshes[a / samples]->m_entry, "displacement seam is not representable");
        }
      }
    }
  }
  static_assert(std::is_nothrow_move_assignable_v<SourceDisplacement>);
  for (size_t i = 0; i < meshes.size(); ++i)
  {
    if (changed[i])
    {
      *meshes[i] = std::move(staged[i]);
    }
  }
  return true;
}
SourceDisplacement SourceDisplacement::translated(const vm::vec3d& delta) const
{
  if (!vm::is_finite(delta))
  {
    invalid(m_entry, "nonfinite translation");
  }
  auto result = *this;
  result.m_start = result.m_start + delta;
  for (auto& corner : result.m_corners)
  {
    corner = corner + delta;
  }
  const auto transform = vm::translation_matrix(delta);
  result.m_baseTransform = transform * m_baseTransform;
  result.rebuild();
  result.validateTransformedSamples(*this, transform);
  return result;
}

SourceDisplacement SourceDisplacement::rigidlyTransformed(
  const vm::mat4x4d& transform) const
{
  if (!properRigidTransform(transform))
  {
    invalid(m_entry, "displacement rotation requires a finite proper rigid transform");
  }
  const auto delta = transform * vm::vec3d{};
  if (transform == vm::translation_matrix(delta))
  {
    return translated(delta);
  }
  if (!m_rotationSupported)
  {
    invalid(m_entry, "rotation requires typed native offset_normals rows");
  }
  return similarityTransformed(transform, 1);
}

SourceDisplacement SourceDisplacement::uniformlyScaled(const vm::mat4x4d& transform) const
{
  const auto scale = transform[0][0];
  const auto delta = transform * vm::vec3d{};
  if (
    !std::isfinite(scale) || scale <= 0 || !vm::is_finite(delta)
    || transform
         != vm::translation_matrix(delta)
              * vm::scaling_matrix(vm::vec3d{scale, scale, scale}))
  {
    invalid(m_entry, "native scale requires an exact finite positive uniform matrix");
  }
  if (scale == 1)
  {
    return translated(delta);
  }
  if (!m_rotationSupported)
  {
    invalid(m_entry, "scale requires typed native offset_normals rows");
  }
  return similarityTransformed(transform, scale);
}

SourceDisplacement SourceDisplacement::similarityTransformed(
  const vm::mat4x4d& transform, const double scale) const
{
  auto result = *this;
  const auto linear = vm::slice<3, 3>(transform, 0, 0);
  const auto rotation = linear / scale;
  const auto scaleScalar = [&](const double value) {
    const auto scaled = value * scale;
    if (!std::isfinite(scaled) || (value != 0 && scaled == 0))
    {
      invalid(m_entry, "native scalar scaling is not representable");
    }
    return scaled;
  };
  result.m_start = transform * m_start;
  for (auto& corner : result.m_corners)
  {
    corner = transform * corner;
  }
  const auto rotateVector = [&](vm::vec3d& vector) {
    vector = rotation * vector;
    if (!vm::is_finite(vector))
    {
      invalid(m_entry, "native vector rotation overflows");
    }
  };
  rotateVector(result.m_faceNormal);
  for (auto& direction : result.m_directions)
  {
    rotateVector(direction);
  }
  for (auto& offset : result.m_offsets)
  {
    if (scale == 1)
    {
      offset = linear * offset;
    }
    else
    {
      for (size_t c = 0; c < 3; ++c)
      {
        offset[c] = scaleScalar(offset[c]);
      }
    }
    if (!vm::is_finite(offset))
    {
      invalid(m_entry, "native offset scaling overflows");
    }
  }
  for (auto& normal : result.m_offsetNormals)
  {
    rotateVector(normal);
  }
  result.m_baseScale = m_baseScale * scale;
  result.m_elevation = scaleScalar(m_elevation);
  for (auto& distance : result.m_distances)
  {
    distance = scaleScalar(distance);
  }
  result.m_baseTransform = transform * m_baseTransform;
  if (
    !std::isfinite(result.m_baseScale) || result.m_baseScale <= 0
    || !std::isfinite(result.m_elevation)
    || !properRigidTransform(
      result.m_baseTransform
      * vm::scaling_matrix(vm::vec3d{1, 1, 1} / result.m_baseScale)))
  {
    invalid(m_entry, "native scale frame or elevation is not representable");
  }
  result.rebuild();
  result.validateTransformedSamples(*this, transform);
  return result;
}

std::optional<vm::mat4x4d> SourceDisplacement::baseTransformFrom(
  const SourceDisplacement& before) const
{
  if (
    m_originalCorners != before.m_originalCorners
    || m_originalFaceNormal != before.m_originalFaceNormal)
  {
    return std::nullopt;
  }
  const auto inverse = vm::invert(before.m_baseTransform);
  if (!inverse)
  {
    return std::nullopt;
  }
  // Equal stored frames mean no movement, not a noisy M * inverse(M).
  const auto transform = m_baseTransform == before.m_baseTransform
                           ? vm::mat4x4d::identity()
                           : m_baseTransform * *inverse;
  const auto scale = m_baseScale / before.m_baseScale;
  const auto rigid = transform * vm::scaling_matrix(vm::vec3d{1, 1, 1} / scale);
  if (!std::isfinite(scale) || scale <= 0 || !properRigidTransform(rigid))
  {
    return std::nullopt;
  }
  for (size_t i = 0; i < m_corners.size(); ++i)
  {
    if (!vm::is_equal(m_corners[i], transform * before.m_corners[i], TransformEpsilon))
    {
      return std::nullopt;
    }
  }
  if (
    !vm::is_equal(m_start, transform * before.m_start, TransformEpsilon)
    || !vm::is_equal(
      m_faceNormal, vm::slice<3, 3>(rigid, 0, 0) * before.m_faceNormal, TransformEpsilon))
  {
    return std::nullopt;
  }
  return transform;
}

std::optional<vm::vec3d> SourceDisplacement::baseTranslationFrom(
  const SourceDisplacement& before) const
{
  if (
    vm::slice<3, 3>(m_baseTransform, 0, 0)
      != vm::slice<3, 3>(before.m_baseTransform, 0, 0)
    || !baseTransformFrom(before))
  {
    return std::nullopt;
  }
  // Translation leaves the stored linear frame exact, even after an oblique turn.
  return m_start - before.m_start;
}

std::optional<double> SourceDisplacement::intersect(const vm::ray3d& ray) const
{
  auto result = std::optional<double>{};
  for (size_t i = 0; i < m_indices.size(); i += 3)
  {
    const auto hit = vm::intersect_ray_triangle(
      ray,
      m_positions[m_indices[i]],
      m_positions[m_indices[i + 1]],
      m_positions[m_indices[i + 2]]);
    if (hit && (!result || *hit < *result))
    {
      result = hit;
    }
  }
  return result;
}
std::vector<VmfValueEdit> SourceDisplacement::editsFrom(
  const SourceDisplacement& before) const
{
  auto structure = std::vector<VmfStructuralEdit>{};
  auto values = editsFrom(before, structure);
  if (!structure.empty())
  {
    invalid(m_entry, "native table initialization requires the structural emitter");
  }
  return values;
}
std::vector<VmfValueEdit> SourceDisplacement::editsFrom(
  const SourceDisplacement& before, std::vector<VmfStructuralEdit>& structure) const
{
  const auto transform = baseTransformFrom(before);
  if (
    !transform || m_entry != before.m_entry || m_power != before.m_power
    || m_size != before.m_size || m_elevationRow != before.m_elevationRow
    || m_startRow != before.m_startRow || m_worldBounds != before.m_worldBounds
    || m_normalsBound != before.m_normalsBound
    || m_alphaAuthoringSupported != before.m_alphaAuthoringSupported
    || m_offsetAuthoringSupported != before.m_offsetAuthoringSupported
    || m_alphaTableMissing != before.m_alphaTableMissing
    || (m_alphaRowsInitialized && !m_alphaTableMissing)
    || (m_alphaTableMissing && !m_alphaRowsInitialized
        && std::any_of(m_alphas.begin(), m_alphas.end(), [](const double value) { return value != 0; }))
    || m_offsetTableMissing != before.m_offsetTableMissing
    || (m_offsetRowsInitialized && !m_offsetTableMissing)
    || (m_offsetTableMissing && !m_offsetRowsInitialized
        && std::any_of(m_offsets.begin(), m_offsets.end(), [](const auto& offset) {
             return offset != vm::vec3d{};
           }))
    || m_rotationSupported != before.m_rotationSupported
    || m_alphas.size() != before.m_alphas.size()
    || m_offsets.size() != before.m_offsets.size()
    || m_directionRows != before.m_directionRows
    || m_distanceRows != before.m_distanceRows || m_alphaRows != before.m_alphaRows
    || m_offsetRows != before.m_offsetRows
    || m_offsetNormalRows != before.m_offsetNormalRows)
  {
    invalid(m_entry, "unsupported displacement structure or provenance edit");
  }
  if (
    m_alphas != before.m_alphas
    || m_alphaRowsInitialized != before.m_alphaRowsInitialized)
  {
    if (
      !(canEditAlphas() || canInitializeAlphas())
      || !(before.canEditAlphas() || before.canInitializeAlphas())
      || std::any_of(m_alphas.begin(), m_alphas.end(), [](const double value) {
           return !std::isfinite(value) || value < 0 || value > 255;
         }))
    {
      invalid(m_entry, "alpha changes require complete bound rows and values in 0..255");
    }
  }
  const auto scale = m_baseScale / before.m_baseScale;
  if (
    !std::isfinite(m_elevation)
    || std::abs(m_elevation - before.m_elevation * scale) > TransformEpsilon)
  {
    invalid(m_entry, "native elevation must follow the base scale");
  }
  const auto linear = vm::slice<3, 3>(*transform, 0, 0);
  const auto rotation = linear / scale;
  const auto followsLinearFrame =
    [&](const auto& oldValues, const auto& newValues, const auto& matrix) {
      if (oldValues.size() != newValues.size())
      {
        return false;
      }
      for (size_t i = 0; i < oldValues.size(); ++i)
      {
        const auto expected = matrix * oldValues[i];
        if (
          !vm::is_finite(newValues[i]) || !vm::is_finite(expected)
          || !vm::is_equal(newValues[i], expected, TransformEpsilon))
        {
          return false;
        }
      }
      return true;
    };
  if (
    !followsLinearFrame(before.m_directions, m_directions, rotation)
    || (!followsLinearFrame(before.m_offsets, m_offsets, linear)
        && !(canEditOffsets()
             && (before.canEditOffsets() || before.canInitializeOffsets())
             && std::all_of(m_offsets.begin(), m_offsets.end(), [](const auto& offset) {
                  return vm::is_finite(offset);
                })))
    || !followsLinearFrame(before.m_offsetNormals, m_offsetNormals, rotation))
  {
    invalid(m_entry, "displacement vectors do not follow the base frame");
  }
  auto edits = std::vector<VmfValueEdit>{};
  const auto editRow = [&](const Row& row, const auto& oldValues, const auto& newValues) {
    auto text = std::string{};
    size_t offset = 0;
    auto changed = false;
    for (size_t i = 0; i < newValues.size(); ++i)
    {
      if (newValues[i] != oldValues[i])
      {
        if (!row.entry || row.spans.size() != newValues.size())
        {
          invalid(m_entry, "missing native numeric binding");
        }
        text += row.text.substr(offset, row.spans[i].begin - offset);
        text += numberText(newValues[i]);
        offset = row.spans[i].end;
        changed = true;
      }
    }
    if (changed)
    {
      text += row.text.substr(offset);
      edits.push_back({*row.entry, "\"" + text + "\""});
    }
  };
  editRow(before.m_elevationRow, std::array{before.m_elevation}, std::array{m_elevation});
  editRow(
    before.m_startRow,
    std::array{before.m_start.x(), before.m_start.y(), before.m_start.z()},
    std::array{m_start.x(), m_start.y(), m_start.z()});
  for (size_t y = 0; y < m_size; ++y)
  {
    editRow(
      before.m_distanceRows[y],
      std::span{before.m_distances}.subspan(y * m_size, m_size),
      std::span{m_distances}.subspan(y * m_size, m_size));
    if (!m_alphaTableMissing)
    {
      editRow(
        before.m_alphaRows[y],
        std::span{before.m_alphas}.subspan(y * m_size, m_size),
        std::span{m_alphas}.subspan(y * m_size, m_size));
    }
  }
  const auto editVectors =
    [&](const auto& rows, const auto& oldValues, const auto& newValues) {
      auto oldRow = std::vector<double>(3 * m_size);
      auto newRow = std::vector<double>(3 * m_size);
      for (size_t y = 0; y < m_size; ++y)
      {
        // Absent normal rows follow the face normal; absent offset rows stay zero.
        if (!rows[y].entry)
        {
          continue;
        }
        for (size_t x = 0; x < m_size; ++x)
        {
          for (size_t c = 0; c < 3; ++c)
          {
            oldRow[3 * x + c] = oldValues[y * m_size + x][c];
            newRow[3 * x + c] = newValues[y * m_size + x][c];
          }
        }
        editRow(rows[y], oldRow, newRow);
      }
    };
  editVectors(before.m_directionRows, before.m_directions, m_directions);
  editVectors(before.m_offsetRows, before.m_offsets, m_offsets);
  editVectors(before.m_offsetNormalRows, before.m_offsetNormals, m_offsetNormals);
  // The preserving emitter admits one append per parent, including combined tables.
  auto newTables = std::string{};
  if (m_alphaRowsInitialized)
  {
    auto text = std::string{"\nalphas\n{\n"};
    for (size_t y = 0; y < m_size; ++y)
    {
      text += "\"row" + std::to_string(y) + "\" \"";
      for (size_t x = 0; x < m_size; ++x)
      {
        if (x != 0)
        {
          text += " ";
        }
        text += numberText(m_alphas[y * m_size + x]);
      }
      text += "\"\n";
    }
    text += "}\n";
    newTables += text;
  }
  if (m_offsetRowsInitialized)
  {
    auto text = std::string{"\noffsets\n{\n"};
    for (size_t y = 0; y < m_size; ++y)
    {
      text += "\"row" + std::to_string(y) + "\" \"";
      for (size_t x = 0; x < m_size; ++x)
      {
        for (size_t component = 0; component < 3; ++component)
        {
          if (x != 0 || component != 0)
          {
            text += " ";
          }
          text += numberText(m_offsets[y * m_size + x][component]);
        }
      }
      text += "\"\n";
    }
    text += "}\n";
    newTables += text;
  }
  if (!newTables.empty())
  {
    structure.push_back({VmfStructuralEdit::Type::Append, m_entry, std::move(newTables)});
  }
  return edits;
}
std::ostream& operator<<(std::ostream& stream, const SourceDisplacement& displacement)
{
  return stream << "SourceDisplacement{power=" << displacement.power()
                << ", vertices=" << displacement.positions().size() << "}";
}
} // namespace tb::mdl
