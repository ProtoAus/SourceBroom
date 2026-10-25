/*
SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "mdl/SourceDisplacement.h"

#include "vm/intersection.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>

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
std::string numberText(const double value)
{
  auto out = std::ostringstream{};
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
  return out.str();
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
  if (const auto elevation = member(document, *block, "elevation", false))
  {
    result.m_elevation = numbers(value(document, *elevation), 1, *elevation)[0].value;
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
  const auto total = result.m_size * result.m_size;
  result.m_directions.assign(total, faceNormal);
  result.m_distances.assign(total, 0.0);
  result.m_offsets.assign(total, vm::vec3d{});
  result.m_alphas.assign(total, 0.0);
  result.m_distanceRows.resize(result.m_size);
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
  result.m_normalsBound =
    readRows("normals", 3, [&](const size_t y, auto, const auto&, const auto& values) {
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
      auto& binding = result.m_distanceRows[y];
      binding.entry = row;
      binding.text = text;
      for (size_t x = 0; x < result.m_size; ++x)
      {
        result.m_distances[y * result.m_size + x] = values[x].value;
        binding.spans.push_back({values[x].begin, values[x].end});
      }
    });
  readRows("offsets", 3, [&](const size_t y, auto, const auto&, const auto& values) {
    for (size_t x = 0; x < result.m_size; ++x)
    {
      result.m_offsets[y * result.m_size + x] = {
        values[3 * x].value, values[3 * x + 1].value, values[3 * x + 2].value};
    }
  });
  readRows("alphas", 1, [&](const size_t y, auto, const auto&, const auto& values) {
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

double SourceDisplacement::distance(const size_t row, const size_t column) const
{
  if (row >= m_size || column >= m_size)
  {
    throw std::out_of_range{"displacement sample"};
  }
  return m_distances[row * m_size + column];
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
  if (
    m_entry != before.m_entry || m_power != before.m_power || m_start != before.m_start
    || m_corners != before.m_corners || m_elevation != before.m_elevation
    || m_directions != before.m_directions || m_offsets != before.m_offsets
    || m_alphas != before.m_alphas || m_distanceRows != before.m_distanceRows)
  {
    invalid(m_entry, "unsupported displacement structure or provenance edit");
  }
  auto edits = std::vector<VmfValueEdit>{};
  for (size_t y = 0; y < m_size; ++y)
  {
    const auto& row = before.m_distanceRows[y];
    auto changed = false;
    for (size_t x = 0; x < m_size; ++x)
    {
      changed = changed || distance(y, x) != before.distance(y, x);
    }
    if (!changed)
    {
      continue;
    }
    if (!row.entry || row.spans.size() != m_size)
    {
      invalid(m_entry, "missing native distance row binding");
    }
    auto text = std::string{};
    size_t last = 0;
    for (size_t x = 0; x < m_size; ++x)
    {
      text += row.text.substr(last, row.spans[x].begin - last);
      text +=
        distance(y, x) == before.distance(y, x)
          ? row.text.substr(row.spans[x].begin, row.spans[x].end - row.spans[x].begin)
          : numberText(distance(y, x));
      last = row.spans[x].end;
    }
    text += row.text.substr(last);
    edits.push_back({*row.entry, "\"" + text + "\""});
  }
  return edits;
}
std::ostream& operator<<(std::ostream& stream, const SourceDisplacement& displacement)
{
  return stream << "SourceDisplacement{power=" << displacement.power()
                << ", vertices=" << displacement.positions().size() << "}";
}
} // namespace tb::mdl
