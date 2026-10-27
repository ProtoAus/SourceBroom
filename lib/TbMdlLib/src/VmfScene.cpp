/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/VmfScene.h"

#include "base/Uuid.h"
#include "mdl/Brush.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/EntityNode.h"
#include "mdl/GroupNode.h"
#include "mdl/LayerNode.h"
#include "mdl/MapFormat.h"
#include "mdl/NodeContents.h"
#include "mdl/SourceDisplacement.h"
#include "mdl/SourceEditorMetadata.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace tb::mdl
{
namespace
{
[[noreturn]] void refuse(const VmfEntryId id, const std::string& reason)
{
  throw std::invalid_argument{"VMF entry " + std::to_string(id) + ": " + reason};
}
std::string text(const VmfDocument& d, const VmfToken& t)
{
  const auto raw = d.tokenContents(t);
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
std::string key(const VmfDocument& d, VmfEntryId id)
{
  return text(d, d.entries()[id].key);
}
std::optional<VmfEntryId> property(
  const VmfDocument& d, VmfEntryId block, const std::string& name)
{
  auto result = std::optional<VmfEntryId>{};
  for (const auto id : d.entries()[block].children)
  {
    if (!d.entries()[id].isBlock() && key(d, id) == name)
    {
      if (result)
      {
        refuse(block, "ambiguous repeated property " + name);
      }
      result = id;
    }
  }
  return result;
}
std::string value(const VmfDocument& d, VmfEntryId block, const std::string& name)
{
  const auto id = property(d, block, name);
  if (!id)
  {
    refuse(block, "missing " + name);
  }
  return text(d, *d.entries()[*id].value);
}
std::string quote(const std::string& s)
{
  auto out = std::string{"\""};
  for (const auto c : s)
  {
    if (c == '\\' || c == '"')
    {
      out += '\\';
    }
    out += c;
  }
  return out + '"';
}
std::string number(double n)
{
  if (!std::isfinite(n))
  {
    throw std::invalid_argument{"VMF non-finite edit"};
  }
  auto s = std::ostringstream{};
  s.imbue(std::locale::classic());
  s << std::setprecision(std::numeric_limits<double>::max_digits10) << n;
  return s.str();
}
std::vector<double> numbers(std::string s, const std::string& pattern, VmfEntryId id)
{
  auto input = std::istringstream{s};
  input.imbue(std::locale::classic());
  auto out = std::vector<double>{};
  for (const auto c : pattern)
  {
    if (c == 'n')
    {
      double n;
      if (!(input >> n) || !std::isfinite(n))
      {
        refuse(id, "invalid finite number");
      }
      out.push_back(n);
    }
    else
    {
      char actual;
      if (!(input >> actual) || actual != c)
      {
        refuse(id, "invalid numeric tuple");
      }
    }
  }
  input >> std::ws;
  if (!input.eof())
  {
    refuse(id, "trailing numeric data");
  }
  return out;
}
float checkedFloat(double n, VmfEntryId id)
{
  const auto f = static_cast<float>(n);
  if (!std::isfinite(f) || (n != 0 && f == 0))
  {
    refuse(id, "UV attribute outside scene float range");
  }
  return f;
}
Entity entity(const VmfDocument& d, VmfEntryId id)
{
  auto props = std::vector<EntityProperty>{};
  auto seen = std::set<std::string>{};
  for (const auto child : d.entries()[id].children)
  {
    if (!d.entries()[child].isBlock() && key(d, child) != "id")
    {
      const auto name = key(d, child);
      // Repeated native properties stay authoritative in the document. The scene
      // shows the first occurrence and serialization refuses ambiguous edits.
      if (seen.insert(name).second)
      {
        props.emplace_back(name, text(d, *d.entries()[child].value));
      }
    }
  }
  auto result = Entity{std::move(props)};
  auto connections = std::vector<SourceEntityConnection>{};
  for (const auto child : d.entries()[id].children)
  {
    if (d.entries()[child].isBlock() && key(d, child) == "connections")
    {
      for (const auto row : d.entries()[child].children)
      {
        if (!d.entries()[row].isBlock())
        {
          connections.push_back({key(d, row), text(d, *d.entries()[row].value), row});
        }
      }
    }
  }
  result.setSourceConnections(std::move(connections));
  return result;
}
bool unsafeClass(const std::string_view name)
{
  return name == "info_overlay" || name == "info_overlay_transition"
         || name == "func_instance" || name == "func_instance_parms";
}
bool referencesGeometry(const VmfDocument& d, VmfEntryId id)
{
  if (key(d, id) == "entity")
  {
    const auto classname = property(d, id, "classname");
    if (classname)
    {
      const auto name = text(d, *d.entries()[*classname].value);
      if (unsafeClass(name))
      {
        return true;
      }
    }
  }
  return false;
}
bool supportedConnections(
  const VmfDocument& d, const VmfEntryId owner, const VmfEntryId block)
{
  if (key(d, owner) != "entity" || key(d, block) != "connections")
  {
    return false;
  }
  for (const auto row : d.entries()[block].children)
  {
    const auto& entry = d.entries()[row];
    if (
      entry.isBlock()
      || !validSourceConnection({key(d, row), text(d, *entry.value), row}))
    {
      return false;
    }
  }
  return true;
}

// Native face polygon caches are geometry, not inert metadata. Content edits and
// closed ownership gates use the same immutable-to-current polygon validation.
std::set<VmfEntryId> polygonCacheEdits(
  const VmfDocument& d,
  const Brush& before,
  const Brush& after,
  std::vector<VmfValueEdit>& edits,
  const bool validateUnchanged = false)
{
  auto admitted = std::set<VmfEntryId>{};
  if (!validateUnchanged && before == after)
  {
    return admitted;
  }
  const auto integer = [](const std::string& token, const size_t bound, VmfEntryId id) {
    auto result = size_t{0};
    if (token.empty())
    {
      refuse(id, "invalid point_data integer");
    }
    for (const auto c : token)
    {
      if (c < '0' || c > '9' || result > bound / 10)
      {
        refuse(id, "invalid point_data integer");
      }
      result = result * 10 + static_cast<size_t>(c - '0');
      if (result > bound)
      {
        refuse(id, "point_data integer outside polygon bounds");
      }
    }
    return result;
  };
  struct PolygonCorrespondence
  {
    std::vector<vm::vec3d> before;
    std::vector<vm::vec3d> after;
    std::vector<size_t> indices;
  };
  auto correspondence = std::map<size_t, PolygonCorrespondence>{};
  const auto positionKey = [](const vm::vec3d& position) {
    return std::array<double, 3>{position.x(), position.y(), position.z()};
  };
  const auto prepareCorrespondence = [&](const VmfEntryId block) {
    using Incidence = std::map<std::array<double, 3>, std::set<size_t>>;
    auto oldIncidence = Incidence{};
    auto newIncidence = Incidence{};
    const auto collect =
      [&](const Brush& brush, Incidence& incidence, const bool original) {
        for (const auto& face : brush.faces())
        {
          auto ring = face.vertexPositions();
          for (const auto& position : ring)
          {
            if (
              !std::isfinite(position.x()) || !std::isfinite(position.y())
              || !std::isfinite(position.z()))
            {
              refuse(block, "non-finite native polygon cache geometry ring");
            }
            incidence[positionKey(position)].insert(face.lineNumber());
          }
          auto& polygon = correspondence[face.lineNumber()];
          (original ? polygon.before : polygon.after) = std::move(ring);
        }
      };
    collect(before, oldIncidence, true);
    collect(after, newIncidence, false);
    auto oldIdentities = std::set<std::set<size_t>>{};
    auto newIdentities = std::set<std::set<size_t>>{};
    for (const auto& [position, identity] : oldIncidence)
    {
      if (!oldIdentities.insert(identity).second)
      {
        refuse(block, "ambiguous native polygon cache vertex incidence");
      }
    }
    for (const auto& [position, identity] : newIncidence)
    {
      if (!newIdentities.insert(identity).second)
      {
        refuse(block, "ambiguous native polygon cache vertex incidence");
      }
    }
    if (oldIdentities != newIdentities)
    {
      refuse(block, "native polygon cache topology/incidence change unsupported");
    }
    // Check the entire brush once, including faces without cached polygons.
    for (auto& [binding, polygon] : correspondence)
    {
      if (polygon.before.empty() || polygon.before.size() != polygon.after.size())
      {
        refuse(block, "native polygon cache topology/cardinality change unsupported");
      }
      auto targetIndices = std::map<std::set<size_t>, size_t>{};
      for (size_t i = 0; i < polygon.after.size(); ++i)
      {
        targetIndices.emplace(newIncidence.at(positionKey(polygon.after[i])), i);
      }
      for (const auto& position : polygon.before)
      {
        const auto match = targetIndices.find(oldIncidence.at(positionKey(position)));
        if (match == targetIndices.end())
        {
          refuse(block, "native polygon cache topology/incidence change unsupported");
        }
        polygon.indices.push_back(match->second);
      }
      for (size_t i = 0; i < polygon.indices.size(); ++i)
      {
        if (
          polygon.indices[(i + 1) % polygon.indices.size()]
          != (polygon.indices[i] + 1) % polygon.indices.size())
        {
          refuse(block, "native polygon cache winding reversal unsupported");
        }
      }
    }
  };
  for (const auto& face : before.faces())
  {
    const auto side = face.lineNumber() - 1;
    auto block = std::optional<VmfEntryId>{};
    for (const auto child : d.entries()[side].children)
    {
      if (key(d, child) == "point_data" || key(d, child) == "vertices_plus")
      {
        if (block || !d.entries()[child].isBlock())
        {
          refuse(side, "duplicate, mixed or non-block native polygon cache");
        }
        block = child;
      }
    }
    if (!block)
    {
      continue;
    }
    if (correspondence.empty())
    {
      prepareCorrespondence(*block);
    }
    const auto indexed = key(d, *block) == "point_data";
    const auto& polygon = correspondence.at(face.lineNumber());
    const auto& oldRing = polygon.before;
    const auto& newRing = polygon.after;
    const auto& newForOld = polygon.indices;
    auto rows = std::vector<VmfEntryId>{};
    for (const auto row : d.entries()[*block].children)
    {
      const auto name = key(d, row);
      if (
        d.entries()[row].isBlock()
        || (indexed ? (name != "numpts" && name != "point") : name != "v"))
      {
        refuse(*block, "unknown or nested native polygon cache field");
      }
      if (name == (indexed ? "point" : "v"))
      {
        rows.push_back(row);
      }
    }
    const auto count =
      indexed ? integer(value(d, *block, "numpts"), rows.size(), *block) : rows.size();
    if (count < 3 || count != rows.size() || count != oldRing.size())
    {
      refuse(*block, "native polygon cache count does not match brush polygon");
    }
    auto oldForIndex = std::vector<size_t>(count, count);
    auto usedOld = std::set<size_t>{};
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex)
    {
      const auto row = rows[rowIndex];
      const auto raw = text(d, *d.entries()[row].value);
      auto input = std::istringstream{raw};
      auto tokens = std::vector<std::string>(indexed ? 4 : 3);
      for (auto& token : tokens)
      {
        if (!(input >> token))
        {
          refuse(row, "invalid native polygon cache row");
        }
      }
      input >> std::ws;
      if (!input.eof())
      {
        refuse(row, "trailing native polygon cache row data");
      }
      const auto offset = indexed ? size_t{1} : size_t{0};
      const auto index = indexed ? integer(tokens[0], count - 1, row) : rowIndex;
      const auto xyz = numbers(
        tokens[offset] + " " + tokens[offset + 1] + " " + tokens[offset + 2], "nnn", row);
      const auto position = vm::vec3d{xyz[0], xyz[1], xyz[2]};
      auto match = std::optional<size_t>{};
      for (size_t i = 0; i < count; ++i)
      {
        if (vm::is_equal(position, oldRing[i], SourceDisplacement::TransformEpsilon))
        {
          if (match)
          {
            refuse(row, "ambiguous native polygon cache polygon vertex");
          }
          match = i;
        }
      }
      if (!match || oldForIndex[index] != count || !usedOld.insert(*match).second)
      {
        refuse(row, "duplicate or unmatched native polygon cache index/vertex");
      }
      oldForIndex[index] = *match;
      const auto& replacement = newRing[newForOld[*match]];
      auto updated = raw;
      auto cursor = size_t{0};
      // Patch backwards so unchanged coordinate/index lexemes and row spacing stay raw.
      auto spans = std::vector<std::pair<size_t, size_t>>(tokens.size());
      for (size_t i = 0; i < tokens.size(); ++i)
      {
        cursor = raw.find(tokens[i], cursor);
        spans[i] = {cursor, tokens[i].size()};
        cursor += tokens[i].size();
      }
      for (size_t axis = 3; axis > 0; --axis)
      {
        if (oldRing[*match][axis - 1] != replacement[axis - 1])
        {
          updated.replace(
            spans[offset + axis - 1].first,
            spans[offset + axis - 1].second,
            number(replacement[axis - 1]));
        }
      }
      if (updated != raw)
      {
        edits.push_back({row, quote(updated)});
      }
    }
    const auto step = (oldForIndex[1] + count - oldForIndex[0]) % count;
    if (step != 1 && step != count - 1)
    {
      refuse(*block, "native polygon cache indices do not follow polygon winding");
    }
    for (size_t i = 0; i < count; ++i)
    {
      if (oldForIndex[(i + 1) % count] != (oldForIndex[i] + step) % count)
      {
        refuse(*block, "native polygon cache indices do not form a cyclic polygon");
      }
    }
    admitted.insert(*block);
  }
  return admitted;
}

bool opaque(
  const VmfDocument& d,
  VmfEntryId id,
  const std::set<VmfEntryId>* displacementSides = nullptr,
  const std::set<VmfEntryId>* polygonBlocks = nullptr,
  const bool ownOnly = false)
{
  if (referencesGeometry(d, id))
  {
    return true;
  }
  for (const auto child : d.entries()[id].children)
  {
    if (!d.entries()[child].isBlock())
    {
      const auto owner = key(d, id);
      const auto name = key(d, child);
      if (owner == "solid" && name != "id")
      {
        return true;
      }
      if (
        owner == "side" && name != "id" && name != "plane" && name != "material"
        && name != "uaxis" && name != "vaxis" && name != "rotation"
        && name != "lightmapscale" && name != "smoothing_groups")
      {
        return true;
      }
    }
    else
    {
      const auto name = key(d, child);
      if (ownOnly && name == "solid")
      {
        continue;
      }
      if (name == "solid" || name == "side" || name == "hidden")
      {
        if (opaque(d, child, displacementSides, polygonBlocks, ownOnly))
        {
          return true;
        }
      }
      else if (
        (name == "point_data" || name == "vertices_plus") && polygonBlocks
        && polygonBlocks->contains(child))
      {
        // This exact side block was checked against both complete geometry rings.
      }
      else if (name == "dispinfo" && displacementSides && displacementSides->contains(id))
      {
        const auto fields = std::set<std::string>{
          "power",
          "startposition",
          "elevation",
          "subdiv",
          "flags",
          "normals",
          "distances",
          "offsets",
          "offset_normals",
          "alphas",
          "triangle_tags",
          "allowed_verts"};
        for (const auto field : d.entries()[child].children)
        {
          if (!fields.contains(key(d, field)))
          {
            return true;
          }
        }
      }
      else if (name != "editor" && !supportedConnections(d, id, child))
      {
        return true;
      }
    }
  }
  return false;
}
using NodeMap = std::map<size_t, const Node*>;
void collect(const Node& node, NodeMap& map)
{
  if (dynamic_cast<const LayerNode*>(&node) == nullptr)
  {
    if (node.lineNumber() == 0 || !map.emplace(node.lineNumber(), &node).second)
    {
      refuse(0, "missing or duplicate native binding; creation is not supported");
    }
  }
  for (const auto* child : node.children())
  {
    collect(*child, map);
  }
}
// Generated identities are not document indices. Geometry correspondence uses
// immutable source-side identities; publication still uses the registered IDs.
template <typename Resolve>
Brush canonicalPolygonBrush(
  const VmfDocument& d, const Brush& brush, const VmfEntryId solid, Resolve resolve)
{
  if (!d.entries()[solid].isBlock() || key(d, solid) != "solid")
  {
    refuse(solid, "polygon cache source is not a native solid");
  }
  auto result = brush;
  auto sides = std::set<VmfEntryId>{};
  for (size_t i = 0; i < result.faceCount(); ++i)
  {
    auto& face = result.face(i);
    if (const auto side = resolve(face.lineNumber()))
    {
      if (
        !d.entries()[*side].isBlock() || key(d, *side) != "side"
        || d.entries()[*side].parent != solid || !sides.insert(*side).second)
      {
        refuse(solid, "foreign or duplicate polygon cache side prototype");
      }
      face.setFilePosition(*side + 1, 1);
    }
  }
  return result;
}

// Ownership validates even unchanged caches. The full raw source subtree is
// still checked by opaque; no child approval exempts an unknown sibling.
template <typename Resolve, typename Validate>
std::set<VmfEntryId> ownershipPolygonBlocks(
  const VmfDocument& d,
  const NodeMap& original,
  const Node& root,
  Resolve resolve,
  Validate validate)
{
  auto result = std::set<VmfEntryId>{};
  auto covered = std::set<size_t>{};
  const auto checkLayout = [&](
                             const auto& self,
                             const VmfEntryId entry,
                             const std::set<VmfEntryId>& blocks) -> void {
    const auto name = key(d, entry);
    if ((name == "point_data" || name == "vertices_plus") && !blocks.contains(entry))
    {
      refuse(entry, "misplaced or unvalidated polygon cache prevents ownership");
    }
    for (const auto child : d.entries()[entry].children)
    {
      self(self, child, blocks);
    }
  };
  const auto checkBrush = [&](const BrushNode& node, const bool live) {
    if (live)
    {
      validate(node);
    }
    const auto source = resolve(node.lineNumber());
    if (!source)
    {
      return;
    }
    const auto found = original.find(*source + 1);
    const auto* before =
      found == original.end() ? nullptr : dynamic_cast<const BrushNode*>(found->second);
    if (!before)
    {
      refuse(*source, "polygon cache ownership source is not a projected solid");
    }
    auto edits = std::vector<VmfValueEdit>{};
    const auto normalized = canonicalPolygonBrush(d, node.brush(), *source, resolve);
    const auto blocks = polygonCacheEdits(d, before->brush(), normalized, edits, true);
    checkLayout(checkLayout, *source, blocks);
    covered.insert(*source + 1);
    if (blocks.empty())
    {
      return;
    }
    for (const auto* subject : {before, &node})
    {
      for (const auto& face : subject->brush().faces())
      {
        if (face.sourceDisplacement())
        {
          refuse(*source, "polygon cache displacement ownership unsupported");
        }
      }
      for (const Node* ancestor = subject->parent(); ancestor;
           ancestor = ancestor->parent())
      {
        if (dynamic_cast<const GroupNode*>(ancestor))
        {
          refuse(*source, "polygon cache native group ownership unsupported");
        }
      }
    }
    result.insert(blocks.begin(), blocks.end());
  };
  const auto current = [&](const auto& self, const Node& node) -> void {
    if (const auto* brush = dynamic_cast<const BrushNode*>(&node))
    {
      checkBrush(*brush, true);
    }
    for (const auto* child : node.children())
    {
      self(self, *child);
    }
  };
  current(current, root);
  // An entity source retains raw solids already removed from its live closure.
  // Validate those exact projected originals too, rather than whitelist them.
  if (dynamic_cast<const EntityNode*>(&root))
  {
    if (const auto source = resolve(root.lineNumber()))
    {
      const auto found = original.find(*source + 1);
      if (found == original.end() || !dynamic_cast<const EntityNode*>(found->second))
      {
        refuse(*source, "polygon cache owner is not a projected entity");
      }
      const auto raw = [&](const auto& self, const Node& node) -> void {
        if (
          const auto* brush = dynamic_cast<const BrushNode*>(&node);
          brush && !covered.contains(brush->lineNumber()))
        {
          // Archived originals retain pre-edit side IDs, not live side-state
          // certificates. Their raw cache geometry and layout still validate.
          checkBrush(*brush, false);
        }
        for (const auto* child : node.children())
        {
          self(self, *child);
        }
      };
      raw(raw, *found->second);
      checkLayout(checkLayout, *source, result);
    }
  }
  return result;
}

std::string planeText(const BrushFace& face)
{
  auto out = std::string{};
  for (const auto& p : face.points())
  {
    if (!out.empty())
    {
      out += ' ';
    }
    out += '(' + number(p.x()) + ' ' + number(p.y()) + ' ' + number(p.z()) + ')';
  }
  return out;
}
std::string axisText(const vm::vec3d& axis, float offset, float scale)
{
  return '[' + number(axis.x()) + ' ' + number(axis.y()) + ' ' + number(axis.z()) + ' '
         + number(offset) + "] " + number(scale);
}
void requireEditableVersion(const VmfDocument& document)
{
  for (const auto root : document.roots())
  {
    if (document.entries()[root].isBlock() && key(document, root) == "versioninfo")
    {
      const auto format = property(document, root, "formatversion");
      if (!format || text(document, *document.entries()[*format].value) != "100")
      {
        refuse(root, "native edits require recognized VMF formatversion 100");
      }
    }
  }
}
std::string freshSideText(const BrushFace& face, const std::string& id)
{
  const auto uv = face.uvAttributes();
  const auto axes = face.takeUvCoordSystemSnapshot();
  if (
    !axes || uv.scale.x() == 0.0f || uv.scale.y() == 0.0f
    || face.surfaceAttributes() != SurfaceAttributes{})
  {
    refuse(0, "new native sides require Valve UVs and default surface attributes");
  }
  return "side\n{\n\"id\" " + quote(id) + "\n\"plane\" " + quote(planeText(face))
         + "\n\"material\" " + quote(face.materialName()) + "\n\"uaxis\" "
         + quote(axisText(axes->uAxis, uv.offset.x(), uv.scale.x())) + "\n\"vaxis\" "
         + quote(axisText(axes->vAxis, uv.offset.y(), uv.scale.y())) + "\n\"rotation\" "
         + quote(number(uv.rotation))
         + "\n\"lightmapscale\" \"16\"\n\"smoothing_groups\" \"0\"\n}\n";
}
std::string freshEntityPrefix(const Entity& entity, const std::string& id)
{
  if (
    !entity.hasProperty("classname") || entity.classname().empty()
    || unsafeClass(entity.classname()))
  {
    refuse(
      0, "new native entities require a supported non-instance/non-overlay classname");
  }
  auto result = "entity\n{\n\"id\" " + quote(id) + "\n";
  auto names = std::set<std::string>{};
  for (const auto& property : entity.properties())
  {
    if (property.key() == "id" || !names.insert(property.key()).second)
    {
      refuse(0, "reserved native id or duplicate entity property");
    }
    result += quote(property.key()) + " " + quote(property.value()) + "\n";
  }
  if (!entity.sourceConnections().empty())
  {
    result += "connections\n{\n";
    for (const auto& row : entity.sourceConnections())
    {
      if (row.sourceEntry || !validSourceConnection(row))
      {
        refuse(0, "new native output has invalid or foreign source provenance");
      }
      result += quote(row.event) + " " + quote(row.value) + "\n";
    }
    result += "}\n";
  }
  return result;
}
void validateFreshData(const Node& node)
{
  if (const auto* entity = dynamic_cast<const EntityNode*>(&node))
  {
    static_cast<void>(
      VmfDocument::parse(freshEntityPrefix(entity->entity(), "1") + "}\n"));
  }
  else if (const auto* brush = dynamic_cast<const BrushNode*>(&node))
  {
    if (!brush->brush().closed() || !brush->brush().fullySpecified())
    {
      refuse(0, "new native brush is not a closed, fully specified convex solid");
    }
    for (const auto& face : brush->brush().faces())
    {
      static_cast<void>(VmfDocument::parse(freshSideText(face, "1")));
    }
  }
  else
  {
    refuse(0, "native creation only supports plain entities and convex brushes");
  }
}
std::string normalizedId(std::string id)
{
  if (id.empty() || id.find_first_not_of("0123456789") != std::string::npos)
  {
    return {};
  }
  const auto first = id.find_first_not_of('0');
  return first == std::string::npos ? "0" : id.substr(first);
}
std::string incrementId(std::string id)
{
  for (auto i = id.size(); i > 0; --i)
  {
    if (id[i - 1] != '9')
    {
      ++id[i - 1];
      return id;
    }
    id[i - 1] = '0';
  }
  return '1' + id;
}
// Raw metadata recognition grants no projection or edit authority. Unsupported
// membership stays isolated without turning an inert record into a global
// geometry reference. Unknown/nested attachments still fail closed.
bool membershipOnlyGroupRecord(const VmfDocument& d, const VmfEntryId group)
{
  if (opaque(d, group))
  {
    return false;
  }
  for (const auto child : d.entries()[group].children)
  {
    if (key(d, child) == "editor")
    {
      if (!d.entries()[child].isBlock())
      {
        return false;
      }
      for (const auto field : d.entries()[child].children)
      {
        if (d.entries()[field].isBlock())
        {
          return false;
        }
      }
    }
    else if (d.entries()[child].isBlock())
    {
      return false;
    }
  }
  return true;
}
bool hammerPlusSettingsName(const std::string& name)
{
  return name == "palette_plus" || name == "colorcorrection_plus" || name == "light_plus"
         || name == "postprocess_plus" || name == "bgimages_plus";
}

// These observed scalar preview settings carry no geometry bindings. Preserve
// them verbatim; future fields, resource strings and background images are opaque.
bool inertHammerPlusSettings(const VmfDocument& d, const VmfEntryId root)
{
  if (!d.entries()[root].isBlock())
  {
    return false;
  }
  const auto name = key(d, root);
  auto seen = std::set<std::string>{};
  auto colors = std::set<std::string>{};
  auto lutNames = std::set<std::string>{};
  auto lutWeights = std::set<std::string>{};
  for (size_t i = 0; i < 16; ++i)
  {
    colors.insert("color" + std::to_string(i));
  }
  for (size_t i = 0; i < 4; ++i)
  {
    lutNames.insert("name" + std::to_string(i));
    lutWeights.insert("weight" + std::to_string(i));
  }
  const auto counts = std::set<std::string>{
    "samples_sun", "samples_ambient", "samples_vis", "bounced", "supersample", "cubemap"};
  const auto lightToggles = std::set<std::string>{"incremental", "bleed_hack", "debug"};
  const auto lightScalars = std::set<std::string>{
    "incremental_delay",
    "bake_dist",
    "radius_scale",
    "brightness_scale",
    "ao_scale",
    "soften_cosine"};
  const auto postToggles =
    std::set<std::string>{"enable", "copied_from_controller", "vignette_enable"};
  const auto postScalars = std::set<std::string>{
    "bloom_scale",
    "bloom_exponent",
    "bloom_saturation",
    "auto_exposure_min",
    "auto_exposure_max",
    "tonemap_percent_target",
    "tonemap_percent_bright_pixels",
    "tonemap_min_avg_luminance",
    "tonemap_rate",
    "vignette_start",
    "vignette_end",
    "vignette_blur"};
  for (const auto field : d.entries()[root].children)
  {
    const auto& entry = d.entries()[field];
    const auto fieldName = key(d, field);
    if (entry.isBlock() || !seen.insert(fieldName).second)
    {
      return false;
    }
    const auto raw = text(d, *entry.value);
    if (
      (name == "colorcorrection_plus" && lutNames.contains(fieldName))
      || (name == "light_plus" && fieldName == "texlight"))
    {
      if (!raw.empty())
      {
        return false;
      }
      continue;
    }
    const auto rgb = name == "palette_plus" && colors.contains(fieldName);
    const auto count = name == "light_plus" && counts.contains(fieldName);
    const auto toggle =
      (name == "light_plus" && lightToggles.contains(fieldName))
      || (name == "postprocess_plus" && postToggles.contains(fieldName));
    const auto scalar =
      (name == "light_plus" && lightScalars.contains(fieldName))
      || (name == "postprocess_plus" && postScalars.contains(fieldName))
      || (name == "colorcorrection_plus" && lutWeights.contains(fieldName));
    if (!rgb && !count && !toggle && !scalar)
    {
      return false;
    }
    try
    {
      const auto parsed = numbers(raw, rgb ? "nnn" : "n", field);
      for (const auto n : parsed)
      {
        if (
          (rgb && (n < 0 || n > 255 || std::trunc(n) != n))
          || (count && (n < 0 || n > std::numeric_limits<int32_t>::max() || std::trunc(n) != n))
          || (toggle && n != 0 && n != 1))
        {
          return false;
        }
      }
    }
    catch (const std::invalid_argument&)
    {
      return false;
    }
  }
  if (name == "colorcorrection_plus")
  {
    for (size_t i = 0; i < 4; ++i)
    {
      if (
        seen.contains("name" + std::to_string(i))
        != seen.contains("weight" + std::to_string(i)))
      {
        return false;
      }
    }
  }
  return true;
}
std::optional<std::string> geometryProblem(const VmfDocument& d, const WorldNode& world)
{
  for (const auto child : d.entries()[world.lineNumber() - 1].children)
  {
    if (d.entries()[child].isBlock())
    {
      const auto name = key(d, child);
      if (
        name != "solid" && name != "hidden" && name != "editor"
        && !(name == "group" && membershipOnlyGroupRecord(d, child)))
      {
        return "unknown world attachment prevents safe brush edit/removal";
      }
    }
  }
  auto nodes = NodeMap{};
  collect(world, nodes);
  for (const auto& [binding, node] : nodes)
  {
    if (dynamic_cast<const EntityNode*>(node))
    {
      const auto id = binding - 1;
      if (referencesGeometry(d, id))
      {
        return "overlay/instance references prevent safe brush edit/removal";
      }
      for (const auto child : d.entries()[id].children)
      {
        if (d.entries()[child].isBlock())
        {
          const auto name = key(d, child);
          if (name != "solid" && name != "editor" && !supportedConnections(d, id, child))
          {
            return "unknown entity attachment may reference this brush";
          }
        }
      }
    }
  }
  auto settings = std::set<std::string>{};
  for (const auto root : d.roots())
  {
    const auto rootName = key(d, root);
    if (hammerPlusSettingsName(rootName))
    {
      if (!settings.insert(rootName).second || !inertHammerPlusSettings(d, root))
      {
        return "unsupported Hammer++ root settings prevent safe brush edit/removal";
      }
      continue;
    }
    if (d.entries()[root].isBlock())
    {
      const auto name = key(d, root);
      if (
        name != "world" && name != "entity" && name != "hidden" && name != "versioninfo"
        && name != "visgroups" && name != "viewsettings" && name != "cameras"
        && name != "cordons")
      {
        return "unknown root block prevents safe brush edit/removal";
      }
    }
  }
  return std::nullopt;
}
} // namespace

const Node* VmfScene::effectiveParent(const Node& node) const
{
  const auto* parent = node.parent();
  while (const auto* group = dynamic_cast<const GroupNode*>(parent))
  {
    const auto* root = static_cast<const Node*>(group);
    while (root->parent())
    {
      root = root->parent();
    }
    const auto admitted = m_groupNodes.find(group->lineNumber());
    const auto link = m_groupLinks.find(group->lineNumber());
    if (
      link == m_groupLinks.end() || link->second != group->linkId()
      || !m_nativeGroups.contains(group->lineNumber())
      || (root != m_original.get() && (admitted == m_groupNodes.end() || admitted->second != group)))
    {
      refuse(0, "unsupported native group ancestry");
    }
    parent = group->parent();
  }
  return parent;
}

size_t VmfScene::parentBinding(const Node& node) const
{
  const auto* parent = effectiveParent(node);
  if (dynamic_cast<const LayerNode*>(parent))
  {
    parent = parent->parent();
  }
  return parent ? parent->lineNumber() : 0;
}

void VmfScene::projectGroups()
{
  const auto metadata = SourceEditorMetadata::read(m_document);
  auto nodes = NodeMap{};
  collect(*m_world, nodes);
  auto bad = std::set<VmfEntryId>{};
  auto members = std::map<VmfEntryId, const SourceEditorMembership*>{};
  auto groupEntries = std::map<std::string, std::vector<VmfEntryId>>{};
  for (const auto& group : metadata.groups)
  {
    if (group.nativeId)
    {
      groupEntries[*group.nativeId].push_back(group.entry);
    }
  }
  for (const auto& member : metadata.memberships)
  {
    members.emplace(member.entry, &member);
  }
  for (const auto& diagnostic : metadata.diagnostics)
  {
    auto entry = std::optional<VmfEntryId>{diagnostic.entry};
    while (entry && !members.contains(*entry))
    {
      entry = m_document.entries()[*entry].parent;
    }
    if (entry)
    {
      bad.insert(*entry);
    }
  }
  for (const auto& member : metadata.memberships)
  {
    const auto group = key(m_document, member.entry) == "group";
    if (!group && !member.groupId)
    {
      continue;
    }
    const auto found = nodes.find(member.entry + 1);
    const auto lexicalParent = m_document.entries()[member.entry].parent;
    const auto validLexical =
      group
        ? lexicalParent == std::optional<VmfEntryId>{m_world->lineNumber() - 1}
        : (found != nodes.end() && found->second->parent() == m_world->defaultLayer());
    if (!validLexical || (member.groupId && !member.group)
        || (group ? opaque(m_document, member.entry)
                  : (found == nodes.end() || opaque(m_document, member.entry))))
    {
      bad.insert(member.entry);
    }
  }
  // A component is indivisible: foreign/unprojected members must never be lost
  // by dissolving an apparently empty display group.
  auto changed = true;
  while (changed)
  {
    changed = false;
    for (const auto& member : metadata.memberships)
    {
      for (const auto& candidate : member.candidateGroupIds)
      {
        const auto found = groupEntries.find(candidate);
        if (found == groupEntries.end())
        {
          continue;
        }
        for (const auto target : found->second)
        {
          if (bad.contains(member.entry) || bad.contains(target))
          {
            changed = bad.insert(member.entry).second || changed;
            changed = bad.insert(target).second || changed;
          }
        }
      }
    }
  }
  for (const auto entry : bad)
  {
    m_unsupportedGroupObjects.insert(entry + 1);
    m_diagnostics.push_back(
      {entry, "Unsupported native editor-group component preserved without projection"});
  }
  auto groups = std::map<VmfEntryId, GroupNode*>{};
  for (const auto& group : metadata.groups)
  {
    if (bad.contains(group.entry) || !group.nativeId)
    {
      continue;
    }
    auto node =
      std::make_unique<GroupNode>(Group{"Group " + value(m_document, group.entry, "id")});
    node->setFilePosition(group.entry + 1, 1);
    groups.emplace(group.entry, node.get());
    m_nativeGroups.insert(group.entry + 1);
    m_groupNodes.emplace(group.entry + 1, node.get());
    m_groupLinks.emplace(group.entry + 1, node->linkId());
    m_world->defaultLayer()->addChild(node.release());
  }
  for (const auto& member : metadata.memberships)
  {
    if (bad.contains(member.entry) || !member.group || !groups.contains(*member.group))
    {
      continue;
    }
    auto* node = groups.contains(member.entry)
                   ? static_cast<Node*>(groups.at(member.entry))
                   : const_cast<Node*>(nodes.at(member.entry + 1));
    node->parent()->removeChild(node);
    groups.at(*member.group)->addChild(node);
  }
  auto sourceMasks = std::map<VmfEntryId, NativeVisibility>{};
  const auto own = [&](const VmfEntryId entry) {
    const auto& row = *members.at(entry);
    return NativeVisibility{
      row.hiddenByContainer, row.visgroupShown == false, row.visgroupAutoShown == false};
  };
  const auto merge = [](NativeVisibility& mask, const NativeVisibility other) {
    mask.wrapper = mask.wrapper || other.wrapper;
    mask.manual = mask.manual || other.manual;
    mask.automatic = mask.automatic || other.automatic;
  };
  for (const auto& group : metadata.groups)
  {
    if (sourceMasks.contains(group.entry))
    {
      continue;
    }
    auto path = std::vector<VmfEntryId>{};
    auto positions = std::map<VmfEntryId, size_t>{};
    auto cursor = group.entry;
    auto mask = NativeVisibility{};
    while (true)
    {
      if (const auto cached = sourceMasks.find(cursor); cached != sourceMasks.end())
      {
        mask = cached->second;
        break;
      }
      if (const auto cycle = positions.find(cursor); cycle != positions.end())
      {
        for (size_t p = cycle->second; p < path.size(); ++p)
        {
          merge(mask, own(path[p]));
        }
        break;
      }
      const auto row = members.find(cursor);
      if (row == members.end())
      {
        break;
      }
      positions.emplace(cursor, path.size());
      path.push_back(cursor);
      if (!row->second->group)
      {
        break;
      }
      cursor = *row->second->group;
    }
    for (auto it = path.rbegin(); it != path.rend(); ++it)
    {
      merge(mask, own(*it));
      sourceMasks.emplace(*it, mask);
    }
  }
  for (const auto& member : metadata.memberships)
  {
    if (
      member.group && !groups.contains(*member.group)
      && sourceMasks.contains(*member.group))
    {
      m_sourceNativeMasks.emplace(member.entry + 1, sourceMasks.at(*member.group));
    }
  }
}

void VmfScene::validateRegisteredGroup(const GroupNode& group) const
{
  const auto binding = group.lineNumber();
  const auto admitted = m_groupNodes.find(binding);
  const auto link = m_groupLinks.find(binding);
  const auto states = m_groupStates.find(binding);
  if (
    !binding || !m_nativeGroups.contains(binding) || admitted == m_groupNodes.end()
    || admitted->second != &group || link == m_groupLinks.end()
    || link->second != group.linkId() || states == m_groupStates.end()
    || states->second.empty() || m_unsupportedGroupObjects.contains(binding))
  {
    refuse(0, "unregistered or unsupported native group container");
  }
}

void VmfScene::validateRegisteredDetachedGroup(const GroupNode& group) const
{
  requireEditing();
  requireEditableVersion(m_document);
  if (group.parent() || group.visibilityState() != VisibilityState::Inherited)
  {
    refuse(
      0,
      "registered parked native group must be detached with inherited session "
      "visibility");
  }
  validateRegisteredGroup(group);
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  if (std::ranges::any_of(live, [&](const auto& item) {
        const auto* other = dynamic_cast<const GroupNode*>(item.second);
        return other && other->linkId() == group.linkId();
      }))
  {
    refuse(0, "parked native group has a live linked/foreign container");
  }
}

void VmfScene::validateGroupClosure(
  const std::vector<Node*>& roots, const bool creation) const
{
  requireEditing();
  requireEditableVersion(m_document);
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  if (creation && (m_nextBinding == std::numeric_limits<size_t>::max()
      || m_generated.size() >= 1'000'000 || m_nextObjectId.size() > 4096
      || m_nextObjectId.size() > 16 * 1024 * 1024 - m_generatedIdBytes))
  {
    refuse(0, "native group identity budget exhausted");
  }
  const auto check = [&](auto&& self, const Node& node) -> void {
    validateVisgroupProjection(node);
    if (
      !live.contains(node.lineNumber()) || live.at(node.lineNumber()) != &node
      || m_unsupportedGroupObjects.contains(node.lineNumber())
      || node.visibilityState() != VisibilityState::Inherited || !node.visible())
    {
      refuse(0, "native group closure is not uniquely live and supported");
    }
    static_cast<void>(parentBinding(node));
    if (const auto* group = dynamic_cast<const GroupNode*>(&node))
    {
      const auto linked = std::ranges::count_if(live, [&](const auto& item) {
        const auto* other = dynamic_cast<const GroupNode*>(item.second);
        return other && other->linkId() == group->linkId();
      });
      validateRegisteredGroup(*group);
      if (linked != 1)
      {
        refuse(0, "foreign group container");
      }
    }
    else if (
      !dynamic_cast<const EntityNode*>(&node) && !dynamic_cast<const BrushNode*>(&node))
    {
      refuse(0, "unsupported native group member");
    }
    const auto* parentGroup = dynamic_cast<const GroupNode*>(node.parent());
    const auto membership = parentGroup ? parentGroup->lineNumber() : 0;
    const auto admitted = m_groupStates.find(node.lineNumber());
    if (admitted == m_groupStates.end() || !admitted->second.contains(membership))
    {
      refuse(0, "unadmitted native membership in group closure");
    }
    const auto source = prototype(node.lineNumber());
    if (source && opaque(m_document, *source))
    {
      refuse(*source, "opaque native group member");
    }
    if (dynamic_cast<const BrushNode*>(&node))
    {
      checkGeometryReferences(source.value_or(0));
    }
    for (const auto* child : node.children())
    {
      self(self, *child);
    }
  };
  for (const auto* root : roots)
  {
    check(check, *root);
  }
}

void VmfScene::registerGroupChanges(
  Node* created, const std::vector<std::pair<Node*, Node*>>& parents)
{
  requireEditing();
  if (
    created
    && (created->lineNumber() || !dynamic_cast<GroupNode*>(created) || created->parent()))
  {
    refuse(0, "new native group must be a detached unbound container");
  }
  auto closure = std::vector<Node*>{};
  for (const auto& [node, parent] : parents)
  {
    if (node != created)
    {
      closure.push_back(node);
    }
    if (
      const auto* group = dynamic_cast<const GroupNode*>(parent);
      group && group != created)
    {
      const auto admitted = m_groupNodes.find(group->lineNumber());
      const auto link = m_groupLinks.find(group->lineNumber());
      if (
        admitted == m_groupNodes.end() || admitted->second != group
        || link == m_groupLinks.end() || link->second != group->linkId()
        || !group->visible() || group->visibilityState() != VisibilityState::Inherited)
      {
        refuse(0, "unsupported native membership destination");
      }
    }
  }
  validateGroupClosure(closure, created != nullptr);
  auto generated = m_generated;
  auto groups = m_nativeGroups;
  auto groupNodes = m_groupNodes;
  auto groupLinks = m_groupLinks;
  auto states = m_groupStates;
  const auto binding = m_nextBinding;
  const auto id = m_nextObjectId;
  if (
    created
    && (binding == std::numeric_limits<size_t>::max() || generated.size() >= 1'000'000 || id.size() > 4096 || id.size() > 16 * 1024 * 1024 - m_generatedIdBytes))
  {
    refuse(0, "native group identity budget exhausted");
  }
  auto next = incrementId(id);
  auto display = Group{"Group " + id};
  if (created)
  {
    generated.emplace(
      binding, GeneratedBinding{GeneratedKind::Group, id, std::nullopt, 0});
    groups.insert(binding);
    groupNodes.emplace(binding, static_cast<GroupNode*>(created));
    groupLinks.emplace(binding, static_cast<GroupNode*>(created)->linkId());
  }
  for (const auto& [node, parent] : parents)
  {
    if (node != created && m_unsupportedGroupObjects.contains(node->lineNumber()))
    {
      refuse(0, "unsupported native membership publication");
    }
    const auto* group = dynamic_cast<const GroupNode*>(parent);
    const auto membership = group && group == created ? binding
                            : group                   ? group->lineNumber()
                                                      : 0;
    if (group && !groups.contains(membership))
    {
      refuse(0, "foreign native membership target");
    }
    states[node == created ? binding : node->lineNumber()].insert(membership);
  }
  size_t count = 0;
  for (const auto& [entry, memberships] : states)
  {
    if (memberships.size() > 1'000'000 - count)
    {
      refuse(0, "native group membership budget exhausted");
    }
    count += memberships.size();
  }
  if (created)
  {
    registerVisgroupMembers({{created, binding, id}}, false);
  }
  m_generated.swap(generated);
  m_nativeGroups.swap(groups);
  m_groupNodes.swap(groupNodes);
  m_groupLinks.swap(groupLinks);
  m_groupStates.swap(states);
  if (created)
  {
    m_nextBinding = binding + 1;
    m_nextObjectId = std::move(next);
    m_generatedIdBytes += id.size();
    created->setFilePosition(binding, 1);
    static_cast<GroupNode&>(*created).setGroup(std::move(display));
  }
}

VmfScene::VmfScene(VmfDocument document)
  : m_document{std::move(document)}
  , m_cameras{m_document}
  , m_cordons{m_document}
  , m_visgroups{m_document}
  , m_nextBinding{m_document.entries().size() + 1}
{
  // Reserve IDs even in unprojected groups/opaque blocks. Object and side IDs
  // have separate Source namespaces; never narrow loaded decimal IDs to C++ ints.
  for (VmfEntryId id = 0; id < m_document.entries().size(); ++id)
  {
    if (!m_document.entries()[id].isBlock())
    {
      continue;
    }
    auto& next = key(m_document, id) == "side" ? m_nextSideId : m_nextObjectId;
    for (const auto child : m_document.entries()[id].children)
    {
      const auto& entry = m_document.entries()[child];
      if (entry.value && key(m_document, child) == "id")
      {
        const auto used = normalizedId(text(m_document, *entry.value));
        if (
          !used.empty()
          && (used.size() > next.size() || (used.size() == next.size() && used >= next)))
        {
          next = incrementId(used);
        }
      }
    }
  }
}
VmfScene::VmfScene(VmfScene&&) noexcept = default;
VmfScene& VmfScene::operator=(VmfScene&&) noexcept = default;
VmfScene::~VmfScene() = default;

VmfScene VmfScene::read(
  std::string source, const vm::bbox3d& bounds, EntityPropertyConfig config)
{
  return readImpl(std::move(source), bounds, std::move(config), false);
}

VmfScene VmfScene::readReadOnlyPreview(
  std::string source, const vm::bbox3d& bounds, EntityPropertyConfig config)
{
  return readImpl(std::move(source), bounds, std::move(config), true);
}

VmfScene VmfScene::readImpl(
  std::string source,
  const vm::bbox3d& bounds,
  EntityPropertyConfig config,
  const bool readOnlyPreview)
{
  auto scene = VmfScene{VmfDocument::parse(std::move(source))};
  scene.m_readOnlyPreview = readOnlyPreview;
  const auto& d = scene.m_document;
  auto worldId = std::optional<VmfEntryId>{};
  // Hammer allocates face IDs independently of world/entity/solid/group IDs.
  auto objectIds = std::set<std::string>{};
  auto sideIds = std::set<std::string>{};
  for (VmfEntryId id = 0; id < d.entries().size(); ++id)
  {
    if (!d.entries()[id].isBlock())
    {
      continue;
    }
    const auto name = key(d, id);
    if (name == "world" && !d.entries()[id].parent)
    {
      if (worldId)
      {
        refuse(id, "multiple worlds");
      }
      worldId = id;
    }
    if (name == "world" || name == "entity" || name == "solid" || name == "side")
    {
      auto native = value(d, id, "id");
      if (native.empty() || native.find_first_not_of("0123456789") != std::string::npos)
      {
        refuse(id, "invalid native ID");
      }
      const auto firstDigit = native.find_first_not_of('0');
      native = firstDigit == std::string::npos ? "0" : native.substr(firstDigit);
      auto& ids = name == "side" ? sideIds : objectIds;
      if (!ids.insert(native).second)
      {
        if (!readOnlyPreview)
        {
          refuse(id, "duplicate native ID within object/face namespace");
        }
        scene.m_diagnostics.push_back(
          {id, "Duplicate native ID preserved unchanged; read-only preview"});
      }
    }
    const auto editableConnections =
      name == "connections" && d.entries()[id].parent
      && supportedConnections(d, *d.entries()[id].parent, id);
    if (
      name != "world" && name != "entity" && name != "solid" && name != "side"
      && !editableConnections)
    {
      scene.m_diagnostics.push_back(
        {id,
         "Preserved native block: " + name + "; not interpreted as editable scene data"});
    }
  }
  if (!worldId)
  {
    refuse(0, "missing world");
  }
  scene.m_diagnostics.push_back(
    {*worldId,
     "Basic native brush/point authoring and bounded existing-object edits; "
     "opaque geometry, hidden-state authoring and unsupported attachments remain gated. "
     "UV axes parsed as doubles; offsets/scales/rotation use the scene float projection. "
     "Unchanged native numeric tokens remain authoritative."});
  for (const auto root : d.roots())
  {
    if (d.entries()[root].isBlock() && key(d, root) == "versioninfo")
    {
      const auto format = property(d, root, "formatversion");
      if (!format || text(d, *d.entries()[*format].value) != "100")
      {
        scene.m_diagnostics.push_back(
          {root, "Unrecognized VMF format version: preview only; native edits disabled"});
      }
    }
  }
  scene.m_world = std::make_unique<WorldNode>(
    std::move(config), entity(d, *worldId), MapFormat::SourceVMF);
  scene.m_world->setFilePosition(*worldId + 1, 1);

  size_t displacementVertices = 0;
  constexpr size_t maxDisplacementVertices = 1048576;
  const auto load = [&](auto&& self, VmfEntryId id, Node& parent, bool hidden) -> void {
    const auto name = key(d, id);
    if (name == "hidden")
    {
      for (const auto child : d.entries()[id].children)
      {
        if (d.entries()[child].isBlock())
        {
          self(self, child, parent, true);
        }
      }
      return;
    }
    if (name == "entity")
    {
      auto node = std::make_unique<EntityNode>(entity(d, id));
      node->setFilePosition(id + 1, 1);
      if (hidden)
      {
        node->setNativeVisibility({true, false, false});
      }
      const auto omissionsBefore = scene.m_unprojectedSolidCount;
      for (const auto child : d.entries()[id].children)
      {
        if (d.entries()[child].isBlock())
        {
          self(self, child, *node, hidden);
        }
      }
      if (node->childCount() == 0 && scene.m_unprojectedSolidCount > omissionsBefore)
      {
        scene.m_diagnostics.push_back(
          {id,
           "Brush entity not projected: all native solids failed; not a point entity"});
        return;
      }
      parent.addChild(node.release());
    }
    else if (name == "solid")
    {
      try
      {
        auto faces = std::vector<BrushFace>{};
        for (const auto side : d.entries()[id].children)
        {
          if (!d.entries()[side].isBlock() || key(d, side) != "side")
          {
            continue;
          }
          const auto p = numbers(value(d, side, "plane"), "(nnn)(nnn)(nnn)", side);
          const auto u = numbers(value(d, side, "uaxis"), "[nnnn]n", side);
          const auto v = numbers(value(d, side, "vaxis"), "[nnnn]n", side);
          const auto r = numbers(value(d, side, "rotation"), "n", side);
          const auto uv = UvAttributes{
            {checkedFloat(u[3], side), checkedFloat(v[3], side)},
            {checkedFloat(u[4], side), checkedFloat(v[4], side)},
            checkedFloat(r[0], side)};
          if (!uv.valid())
          {
            refuse(side, "invalid UV scale");
          }
          const auto ua = vm::vec3d{u[0], u[1], u[2]};
          const auto va = vm::vec3d{v[0], v[1], v[2]};
          if (
            vm::squared_length(ua) == 0 || vm::squared_length(va) == 0
            || !std::isfinite(vm::squared_length(ua))
            || !std::isfinite(vm::squared_length(va))
            || !std::isfinite(vm::squared_length(vm::cross(ua, va)))
            || vm::squared_length(vm::cross(ua, va)) == 0)
          {
            refuse(side, "degenerate UV axes");
          }
          // Source and TB Valve faces both use clockwise points viewed from outside.
          auto result = BrushFace::createFromValve(
            {p[0], p[1], p[2]},
            {p[3], p[4], p[5]},
            {p[6], p[7], p[8]},
            value(d, side, "material"),
            uv,
            {},
            ua,
            va,
            MapFormat::Valve,
            true);
          if (result.is_error())
          {
            refuse(side, "invalid Source face plane");
          }
          auto face = std::move(result).value();
          face.setFilePosition(side + 1, 1);
          faces.push_back(std::move(face));
        }
        const auto count = faces.size();
        auto result = Brush::create(bounds, std::move(faces));
        if (result.is_error())
        {
          refuse(
            id,
            "invalid or unbounded Source solid: " + std::get<Error>(result.error()).msg);
        }
        auto brush = std::move(result).value();
        if (brush.faceCount() != count || !brush.closed() || !brush.fullySpecified())
        {
          refuse(id, "solid construction discarded or clipped a native side");
        }
        size_t addedVertices = 0;
        for (size_t index = 0; index < brush.faceCount(); ++index)
        {
          auto& face = brush.face(index);
          auto displacement = SourceDisplacement::read(
            d,
            face.lineNumber() - 1,
            face.vertexPositions(),
            face.normal(),
            bounds,
            maxDisplacementVertices - displacementVertices - addedVertices);
          if (displacement)
          {
            addedVertices += displacement->positions().size();
          }
          face.setSourceDisplacement(std::move(displacement));
        }
        displacementVertices += addedVertices;
        auto node = std::make_unique<BrushNode>(std::move(brush));
        node->setFilePosition(id + 1, 1);
        if (hidden)
        {
          node->setNativeVisibility({true, false, false});
        }
        parent.addChild(node.release());
      }
      catch (const std::invalid_argument& e)
      {
        if (!readOnlyPreview)
        {
          throw;
        }
        auto sample = std::optional<vm::vec3d>{};
        for (const auto side : d.entries()[id].children)
        {
          if (d.entries()[side].isBlock() && key(d, side) == "side")
          {
            try
            {
              const auto p = numbers(value(d, side, "plane"), "(nnn)(nnn)(nnn)", side);
              const auto point = vm::vec3d{p[0], p[1], p[2]};
              if (bounds.contains(point))
              {
                sample = point;
              }
            }
            catch (const std::invalid_argument&)
            {
              // No reliable plane sample: this omission remains explicitly unlocated.
            }
            break;
          }
        }
        ++scene.m_unprojectedSolidCount;
        scene.m_diagnostics.push_back(
          {id, std::string{"Native solid not projected: "} + e.what(), sample, true});
      }
    }
  };
  for (const auto child : d.entries()[*worldId].children)
  {
    if (d.entries()[child].isBlock())
    {
      load(load, child, *scene.m_world->defaultLayer(), false);
    }
  }
  for (const auto root : d.roots())
  {
    if (root != *worldId && d.entries()[root].isBlock())
    {
      load(load, root, *scene.m_world->defaultLayer(), false);
    }
  }
  scene.projectGroups();
  auto projected = NodeMap{};
  collect(*scene.m_world, projected);
  for (const auto& [binding, node] : projected)
  {
    const auto* group = dynamic_cast<const GroupNode*>(node->parent());
    scene.m_groupStates[binding].insert(group ? group->lineNumber() : 0);
  }
  scene.m_activeWorld = scene.m_world.get();
  for (const auto& member : scene.m_visgroups.snapshot().members)
  {
    if (member.source)
    {
      const auto binding = *member.source + 1;
      scene.m_memberIds.emplace(binding, member.id);
      scene.m_memberBindings.emplace(member.id.m_serial, binding);
      if (const auto it = projected.find(binding); it != projected.end())
      {
        scene.m_memberNodes.emplace(binding, it->second);
      }
    }
  }
  scene.projectVisgroups();
  scene.m_original.reset(
    static_cast<WorldNode*>(scene.m_world->cloneRecursively(bounds)));
  try
  {
    scene.m_geometryReferenceProblem = geometryProblem(d, *scene.m_original);
  }
  catch (const std::invalid_argument& e)
  {
    // Ambiguous native metadata is preservable on no-edit loads, but cannot
    // justify geometry/ownership edits. Cache a closed guard, not a load error.
    scene.m_geometryReferenceProblem =
      "ambiguous native references prevent safe geometry edits: " + std::string{e.what()};
    scene.m_diagnostics.push_back({*worldId, *scene.m_geometryReferenceProblem});
  }
  return scene;
}
void VmfScene::requireEditing() const
{
  if (m_readOnlyPreview)
  {
    refuse(0, "read-only compatibility preview cannot author native data");
  }
}

NativeVisgroupContext VmfScene::visgroupContext() const
{
  auto result = NativeVisgroupContext{};
  try
  {
    requireEditing();
    requireEditableVersion(m_document);
  }
  catch (const std::invalid_argument& e)
  {
    result.editable = false;
    result.refusalReason = e.what();
  }
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  auto original = NodeMap{};
  if (m_original)
  {
    collect(*m_original, original);
  }
  auto ancestorAdmission = std::map<size_t, std::string>{};
  const auto requireOwner = [&](const Node& owner) {
    const auto binding = owner.lineNumber();
    auto [admission, inserted] = ancestorAdmission.try_emplace(binding);
    if (inserted)
    {
      try
      {
        if (m_unsupportedGroupObjects.contains(binding))
        {
          refuse(0, "unsupported native owning component");
        }
        if (
          const auto* entity = dynamic_cast<const EntityNode*>(&owner);
          entity && unsafeClass(entity->entity().classname()))
        {
          refuse(0, "instance/overlay native owning component");
        }
        if (
          const auto found = m_memberIds.find(binding);
          found != m_memberIds.end()
          && !m_visgroups.member(found->second).refusalReason.empty())
        {
          refuse(0, "protected native owner metadata");
        }
        auto blocks = std::set<VmfEntryId>{};
        if (m_original && dynamic_cast<const EntityNode*>(&owner))
        {
          try
          {
            blocks = ownershipPolygonBlocks(
              m_document,
              original,
              owner,
              [&](size_t b) { return prototype(b); },
              [&](const BrushNode& b) { validateSideState(b); });
          }
          catch (const std::invalid_argument&)
          {
            // A protected sibling cannot certify an owner cache allowance, but
            // must not itself lock an ordinary owner's independent direct child.
          }
        }
        if (
          const auto source = prototype(binding);
          source && opaque(m_document, *source, nullptr, &blocks, true))
        {
          refuse(*source, "protected native owner attachment");
        }
      }
      catch (const std::invalid_argument& e)
      {
        admission->second = e.what();
      }
    }
    if (!admission->second.empty())
    {
      throw std::invalid_argument{admission->second};
    }
  };
  const auto snapshot = m_visgroups.snapshot();
  for (const auto& member : snapshot.members)
  {
    auto row = NativeVisgroupMemberContext{};
    row.id = member.id;
    row.kind = member.kind;
    const auto binding = m_memberBindings.at(member.id.m_serial);
    const auto expected = m_memberNodes.find(binding);
    const auto found = live.find(binding);
    if (
      expected != m_memberNodes.end() && found != live.end()
      && expected->second != found->second)
    {
      refuse(0, "native member replaced without registration");
    }
    if (m_removals.contains(binding) && found != live.end())
    {
      refuse(0, "parked/disposed native member appeared without activation");
    }
    if (
      expected != m_memberNodes.end() && found != live.end()
      && expected->second == found->second)
    {
      m_pendingMembers.erase(binding);
    }
    if (
      expected != m_memberNodes.end() && found == live.end()
      && !m_removals.contains(binding) && !m_pendingMembers.contains(binding))
    {
      refuse(0, "native member detached without registered parking");
    }
    row.active = !m_removals.contains(binding) && (expected == m_memberNodes.end()
      || (found != live.end() && found->second == expected->second));
    row.projected = expected != m_memberNodes.end() && row.active;
    row.editable = row.projected;
    if (!row.projected)
    {
      row.refusalReason =
        row.active ? "native member not projected" : "parked native member";
    }
    else
    {
      const auto& node = *found->second;
      const auto parent = node.parent();
      if (parent && m_memberIds.contains(parent->lineNumber()))
      {
        row.owner = m_memberIds.at(parent->lineNumber());
      }
      if (dynamic_cast<const BrushNode*>(&node))
      {
        row.kind = dynamic_cast<const EntityNode*>(parent)
                     ? NativeVisgroupMemberKind::InternalSolid
                     : NativeVisgroupMemberKind::WorldSolid;
      }
      else if (dynamic_cast<const EntityNode*>(&node))
      {
        row.kind = node.hasChildren() ? NativeVisgroupMemberKind::BrushEntity
                   : member.kind == NativeVisgroupMemberKind::BrushEntity
                     ? member.kind
                     : NativeVisgroupMemberKind::PointEntity;
      }
      try
      {
        validateVisgroupProjection(node);
        for (const auto* owner = node.parent(); owner; owner = owner->parent())
        {
          if (
            dynamic_cast<const EntityNode*>(owner)
            || dynamic_cast<const GroupNode*>(owner))
          {
            requireOwner(*owner);
          }
        }
        if (const auto* brush = dynamic_cast<const BrushNode*>(&node))
        {
          validateSideState(*brush);
        }
        if (m_unsupportedGroupObjects.contains(binding))
        {
          refuse(0, "unsupported native group component");
        }
        static_cast<void>(parentBinding(node));
        auto blocks = std::set<VmfEntryId>{};
        if (m_original && !dynamic_cast<const GroupNode*>(&node))
        {
          blocks = ownershipPolygonBlocks(
            m_document,
            original,
            node,
            [&](size_t b) { return prototype(b); },
            [&](const BrushNode& b) { validateSideState(b); });
        }
        if (
          const auto source = prototype(binding);
          source && opaque(m_document, *source, nullptr, &blocks))
        {
          refuse(*source, "protected native metadata component");
        }
        if (
          const auto* entity = dynamic_cast<const EntityNode*>(&node);
          entity && unsafeClass(entity->entity().classname()))
        {
          refuse(0, "instance/overlay native metadata component");
        }
      }
      catch (const std::invalid_argument& e)
      {
        row.editable = false;
        row.refusalReason = e.what();
      }
    }
    if (!member.refusalReason.empty())
    {
      row.editable = false;
      row.refusalReason = member.refusalReason;
    }
    result.members.push_back(std::move(row));
  }
  result.generation = m_lastVisgroupContext.generation;
  if (
    result.members != m_lastVisgroupContext.members
    || result.editable != m_lastVisgroupContext.editable
    || result.refusalReason != m_lastVisgroupContext.refusalReason)
  {
    if (result.generation == std::numeric_limits<uint64_t>::max())
    {
      refuse(0, "native member context generation exhausted");
    }
    ++result.generation;
    m_lastVisgroupContext = result;
  }
  return result;
}

void VmfScene::registerVisgroupMembers(
  const std::vector<std::tuple<Node*, size_t, std::string>>& members, const bool copy)
{
  auto registrations = std::vector<NativeVisgroups::Registration>{};
  for (const auto& [node, binding, nativeId] : members)
  {
    if (m_memberIds.contains(binding))
    {
      refuse(0, "repeated native member registration");
    }
    const auto kind = dynamic_cast<GroupNode*>(node) ? NativeVisgroupMemberKind::Group
                      : dynamic_cast<EntityNode*>(node)
                        ? (node->hasChildren() ? NativeVisgroupMemberKind::BrushEntity
                                               : NativeVisgroupMemberKind::PointEntity)
                        : NativeVisgroupMemberKind::WorldSolid;
    auto source = std::optional<NativeVisgroupMemberId>{};
    if (copy)
    {
      const auto found = m_memberIds.find(node->lineNumber());
      if (found == m_memberIds.end())
      {
        refuse(0, "unregistered native member copy");
      }
      source = found->second;
    }
    registrations.push_back({nativeId, kind, source});
  }
  const auto ids = m_visgroups.pendingMemberIds(members.size());
  auto memberIds = m_memberIds;
  auto bindings = m_memberBindings;
  auto nodes = m_memberNodes;
  auto pending = m_pendingMembers;
  auto masks = std::vector<NativeVisibility>{};
  masks.reserve(members.size());
  for (size_t i = 0; i < members.size(); ++i)
  {
    const auto& [node, binding, nativeId] = members[i];
    memberIds.emplace(binding, ids[i]);
    bindings.emplace(ids[i].m_serial, binding);
    nodes.emplace(binding, node);
    pending.insert(binding);
    auto mask = NativeVisibility{};
    if (registrations[i].copy)
    {
      const auto& m = m_visgroups.member(*registrations[i].copy);
      mask = {m.wrapperHidden, m.manualShown == false, m.autoShown == false};
    }
    masks.push_back(mask);
  }
  static_cast<void>(m_visgroups.registerMembers(registrations));
  m_memberIds.swap(memberIds);
  m_memberBindings.swap(bindings);
  m_memberNodes.swap(nodes);
  m_pendingMembers.swap(pending);
  for (size_t i = 0; i < members.size(); ++i)
  {
    std::get<0>(members[i])->setNativeVisibility(masks[i]);
  }
}

NativeVisibility VmfScene::memberVisibility(const NativeVisgroupMember& member) const
{
  auto result = NativeVisibility{
    member.wrapperHidden, member.manualShown == false, member.autoShown == false};
  if (
    const auto found = m_sourceNativeMasks.find(m_memberBindings.at(member.id.m_serial));
    found != m_sourceNativeMasks.end())
  {
    result.wrapper = result.wrapper || found->second.wrapper;
    result.manual = result.manual || found->second.manual;
    result.automatic = result.automatic || found->second.automatic;
  }
  return result;
}

void VmfScene::validateVisgroupProjection(const Node& node) const
{
  for (auto* ancestor = &node; ancestor; ancestor = ancestor->parent())
  {
    if (
      const auto found = m_memberIds.find(ancestor->lineNumber());
      found != m_memberIds.end())
    {
      const auto& m = m_visgroups.member(found->second);
      if (ancestor->nativeVisibility() != memberVisibility(m))
      {
        refuse(0, "native viewport projection differs from authoritative metadata");
      }
    }
  }
}

void VmfScene::projectVisgroups()
{
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  for (const auto& m : m_visgroups.snapshot().members)
  {
    const auto binding = m_memberBindings.at(m.id.m_serial);
    const auto found = live.find(binding);
    const auto expected = m_memberNodes.find(binding);
    if (
      found != live.end() && expected != m_memberNodes.end()
      && found->second == expected->second)
    {
      const_cast<Node*>(found->second)->setNativeVisibility(memberVisibility(m));
    }
  }
}

NativeVisgroupSnapshot VmfScene::visgroups() const
{
  return m_visgroups.snapshot(visgroupContext());
}
std::optional<PreparedNativeVisgroupChange> VmfScene::prepareVisgroupChange(
  const NativeVisgroupRequest& request) const
{
  requireEditing();
  requireEditableVersion(m_document);
  return m_visgroups.prepare(request, visgroupContext());
}
std::vector<std::pair<Node*, NativeVisibility>> VmfScene::visgroupVisibility(
  const PreparedNativeVisgroupChange& change,
  const NativeVisgroupDirection direction) const
{
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  auto masks = std::map<size_t, NativeVisibility>{};
  const auto& desired =
    direction == NativeVisgroupDirection::Forward ? change.after() : change.before();
  for (const auto& member : desired.members)
  {
    static_cast<void>(m_visgroups.member(member.id));
    masks.emplace(m_memberBindings.at(member.id.m_serial), memberVisibility(member));
  }
  auto result = std::vector<std::pair<Node*, NativeVisibility>>{};
  for (const auto& [binding, node] : live)
  {
    auto effective = NativeVisibility{};
    for (auto* ancestor = node; ancestor; ancestor = ancestor->parent())
    {
      const auto found = masks.find(ancestor->lineNumber());
      const auto own =
        found == masks.end() ? ancestor->nativeVisibility() : found->second;
      effective.wrapper |= own.wrapper;
      effective.manual |= own.manual;
      effective.automatic |= own.automatic;
    }
    result.emplace_back(const_cast<Node*>(node), effective);
  }
  return result;
}

std::vector<Node*> VmfScene::applyVisgroupChange(
  PreparedNativeVisgroupChange& change, const NativeVisgroupDirection direction)
{
  requireEditing();
  requireEditableVersion(m_document);
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  auto masks = std::map<size_t, NativeVisibility>{};
  const auto& desired =
    direction == NativeVisgroupDirection::Forward ? change.after() : change.before();
  for (const auto& m : desired.members)
  {
    static_cast<void>(m_visgroups.member(m.id));
    masks.emplace(m_memberBindings.at(m.id.m_serial), memberVisibility(m));
  }
  const auto effective = [&](const Node& node) {
    auto result = NativeVisibility{};
    for (const auto* ancestor = &node; ancestor; ancestor = ancestor->parent())
    {
      const auto found = masks.find(ancestor->lineNumber());
      const auto own =
        found == masks.end() ? ancestor->nativeVisibility() : found->second;
      result.wrapper = result.wrapper || own.wrapper;
      result.manual = result.manual || own.manual;
      result.automatic = result.automatic || own.automatic;
    }
    return result;
  };
  auto changed = std::set<Node*>{};
  for (const auto& [binding, node] : live)
  {
    if (effective(*node) != node->effectiveNativeVisibility())
    {
      for (auto* affected = const_cast<Node*>(node); affected;
           affected = affected->parent())
      {
        if (!dynamic_cast<WorldNode*>(affected) && !dynamic_cast<LayerNode*>(affected))
        {
          changed.insert(affected);
        }
      }
    }
  }
  auto closure = std::vector<Node*>{changed.begin(), changed.end()};
  m_visgroups.apply(change, direction, visgroupContext());
  for (const auto& [binding, mask] : masks)
  {
    const auto found = live.find(binding);
    if (
      found != live.end() && m_memberNodes.contains(binding)
      && m_memberNodes.at(binding) == found->second)
    {
      const_cast<Node*>(found->second)->setNativeVisibility(mask);
    }
  }
  return closure;
}

NativeCameraSnapshot VmfScene::cameras() const
{
  auto snapshot = m_cameras.snapshot();
  try
  {
    requireEditing();
    requireEditableVersion(m_document);
  }
  catch (const std::invalid_argument& e)
  {
    snapshot.refusalReason = e.what();
  }
  return snapshot;
}
std::optional<PreparedNativeCameraChange> VmfScene::prepareCameraChange(
  const NativeCameraRequest& request) const
{
  requireEditing();
  requireEditableVersion(m_document);
  return m_cameras.prepare(request);
}
void VmfScene::applyCameraChange(
  PreparedNativeCameraChange& change, NativeCameraDirection direction)
{
  requireEditing();
  requireEditableVersion(m_document);
  m_cameras.apply(change, direction);
}

NativeCordonSnapshot VmfScene::cordons() const
{
  auto snapshot = m_cordons.snapshot();
  try
  {
    requireEditing();
    requireEditableVersion(m_document);
  }
  catch (const std::invalid_argument& e)
  {
    snapshot.refusalReason = e.what();
  }
  return snapshot;
}
std::optional<PreparedNativeCordonChange> VmfScene::prepareCordonChange(
  const NativeCordonRequest& request) const
{
  requireEditing();
  requireEditableVersion(m_document);
  return m_cordons.prepare(request);
}
void VmfScene::applyCordonChange(
  PreparedNativeCordonChange& change, const NativeCordonDirection direction)
{
  requireEditing();
  requireEditableVersion(m_document);
  m_cordons.apply(change, direction);
}

WorldNode& VmfScene::world()
{
  return *m_world;
}
const WorldNode& VmfScene::world() const
{
  return *m_world;
}
std::unique_ptr<WorldNode> VmfScene::takeWorld()
{
  return std::move(m_world);
}
const VmfDocument& VmfScene::document() const
{
  return m_document;
}
const std::vector<VmfSceneDiagnostic>& VmfScene::diagnostics() const
{
  return m_diagnostics;
}
std::optional<VmfEntryId> VmfScene::prototype(const size_t binding) const
{
  if (binding && binding <= m_document.entries().size())
  {
    return binding - 1;
  }
  const auto it = m_generated.find(binding);
  if (it == m_generated.end())
  {
    refuse(0, "missing native binding");
  }
  return it->second.prototype;
}
std::string VmfScene::nativeId(const Node& node) const
{
  if (const auto it = m_generated.find(node.lineNumber()); it != m_generated.end())
  {
    return it->second.nativeId;
  }
  const auto source = prototype(node.lineNumber());
  if (!source)
  {
    refuse(0, "missing native ID");
  }
  return value(m_document, *source, "id");
}
void VmfScene::checkGeometryReferences(const VmfEntryId object) const
{
  auto entry = std::optional<VmfEntryId>{object};
  while (entry)
  {
    if (m_unsupportedGroupObjects.contains(*entry + 1))
    {
      refuse(object, "unsupported native group component geometry owner");
    }
    entry = m_document.entries()[*entry].parent;
  }
  if (m_geometryReferenceProblem)
  {
    refuse(object, *m_geometryReferenceProblem);
  }
}
void VmfScene::validateDuplication(const std::vector<Node*>& nodes) const
{
  const auto session = m_visgroups.beginEmission(m_document);
  auto liveSources = NodeMap{};
  collect(*m_activeWorld, liveSources);
  requireEditing();
  if (!nodes.empty())
  {
    requireEditableVersion(m_document);
  }
  auto original = NodeMap{};
  collect(*m_original, original);
  const auto check =
    [&](
      auto&& self, const Node& node, const std::set<VmfEntryId>& polygonBlocks) -> void {
    if (
      (!dynamic_cast<const EntityNode*>(&node) && !dynamic_cast<const BrushNode*>(&node)
       && !(
         dynamic_cast<const GroupNode*>(&node)
         && m_nativeGroups.contains(node.lineNumber())))
      || m_unsupportedGroupObjects.contains(node.lineNumber())
      || node.visibilityState() != VisibilityState::Inherited || !node.visible())
    {
      refuse(0, "native duplication does not support groups/layers/hidden objects");
    }
    const auto admitted = m_memberNodes.find(node.lineNumber());
    const auto liveSource = liveSources.find(node.lineNumber());
    if (
      admitted == m_memberNodes.end() || liveSource == liveSources.end()
      || admitted->second != liveSource->second || !liveSource->second->visible())
    {
      refuse(0, "native duplicate requires its admitted live visible source");
    }
    validateVisgroupProjection(node);
    validateVisgroupProjection(*liveSource->second);
    const auto source = prototype(node.lineNumber());
    auto owner = source;
    while (owner)
    {
      if (m_unsupportedGroupObjects.contains(*owner + 1))
      {
        refuse(*owner, "unsupported native group component duplication owner");
      }
      owner = m_document.entries()[*owner].parent;
    }
    if (source && opaque(m_document, *source, nullptr, &polygonBlocks))
    {
      refuse(
        *source, "opaque/displacement/instance attachments prevent native duplication");
    }
    if (const auto* group = dynamic_cast<const GroupNode*>(&node))
    {
      const auto link = m_groupLinks.find(node.lineNumber());
      if (link == m_groupLinks.end() || link->second != group->linkId())
      {
        refuse(0, "native linked/foreign group duplication unsupported");
      }
    }
    const auto* root = &node;
    while (root->parent())
    {
      root = root->parent();
    }
    if (root == m_activeWorld)
    {
      static_cast<void>(parentBinding(node));
    }
    static_cast<void>(emitNode(node, original, session));
    for (const auto* child : node.children())
    {
      self(self, *child, polygonBlocks);
    }
  };
  for (const auto* node : nodes)
  {
    const auto polygonBlocks = ownershipPolygonBlocks(
      m_document,
      original,
      *node,
      [&](const size_t binding) { return prototype(binding); },
      [&](const BrushNode& brush) { validateSideState(brush); });
    check(check, *node, polygonBlocks);
  }
}
void VmfScene::validateAdditions(
  const Node& parent, const std::vector<Node*>& nodes, const bool duplication) const
{
  if (!nodes.empty())
  {
    requireEditing();
    requireEditableVersion(m_document);
  }
  validateVisgroupProjection(parent);
  if (!parent.visible() || parent.visibilityState() != VisibilityState::Inherited)
  {
    refuse(0, "native additions require a visible parent");
  }
  const auto* owner = &parent;
  if (const auto* group = dynamic_cast<const GroupNode*>(owner))
  {
    const auto admitted = m_groupNodes.find(group->lineNumber());
    const auto link = m_groupLinks.find(group->lineNumber());
    if (
      !m_nativeGroups.contains(owner->lineNumber()) || admitted == m_groupNodes.end()
      || admitted->second != group || link == m_groupLinks.end()
      || link->second != group->linkId() || !group->visible()
      || group->visibilityState() != VisibilityState::Inherited)
    {
      refuse(0, "foreign native group parent");
    }
    owner = effectiveParent(*owner);
  }
  if (const auto* layer = dynamic_cast<const LayerNode*>(owner))
  {
    const auto* world = dynamic_cast<const WorldNode*>(layer->parent());
    if (!world || layer != world->defaultLayer())
    {
      refuse(0, "native groups/layers are not authorable");
    }
    owner = world;
  }
  if (!dynamic_cast<const WorldNode*>(owner) && !dynamic_cast<const EntityNode*>(owner))
  {
    refuse(0, "unsupported native addition parent");
  }
  const auto source = prototype(owner->lineNumber());
  if (dynamic_cast<const EntityNode*>(owner))
  {
    auto original = NodeMap{};
    collect(*m_original, original);
    const auto polygonBlocks = ownershipPolygonBlocks(
      m_document,
      original,
      *owner,
      [&](const size_t binding) { return prototype(binding); },
      [&](const BrushNode& brush) { validateSideState(brush); });
    if (
      (source && opaque(m_document, *source, nullptr, &polygonBlocks))
      || (!m_generated.contains(owner->lineNumber()) && owner->childCount() == 0))
    {
      refuse(0, "opaque or point entity cannot acquire native brush ownership");
    }
  }
  for (const auto* node : nodes)
  {
    if (node->parent())
    {
      refuse(0, "native additions must transfer detached roots");
    }
    if (dynamic_cast<const EntityNode*>(node) && !dynamic_cast<const WorldNode*>(owner))
    {
      refuse(0, "native entities must be added at world level");
    }
  }
  if (duplication)
  {
    validateDuplication(nodes);
    auto groupedDestination = false;
    for (const Node* ancestor = &parent; ancestor; ancestor = ancestor->parent())
    {
      groupedDestination = groupedDestination || dynamic_cast<const GroupNode*>(ancestor);
    }
    if (groupedDestination)
    {
      auto original = NodeMap{};
      collect(*m_original, original);
      for (const auto* node : nodes)
      {
        if (!ownershipPolygonBlocks(
               m_document,
               original,
               *node,
               [&](const size_t binding) { return prototype(binding); },
               [&](const BrushNode& brush) { validateSideState(brush); })
               .empty())
        {
          refuse(0, "polygon cache native group additions unsupported");
        }
      }
    }
    return;
  }
  const auto check = [&](auto&& self, const Node& node) -> void {
    if (
      node.lineNumber() || node.visibilityState() != VisibilityState::Inherited
      || node.nativeVisibility().hidden())
    {
      refuse(0, "fresh native nodes carry foreign provenance or hidden state");
    }
    validateFreshData(node);
    if (const auto* brush = dynamic_cast<const BrushNode*>(&node))
    {
      for (const auto& face : brush->brush().faces())
      {
        if (face.lineNumber())
        {
          refuse(
            0,
            "fresh native side carries foreign provenance; topology/paste unsupported");
        }
      }
    }
    for (const auto* child : node.children())
    {
      if (
        !dynamic_cast<const BrushNode*>(child) || !dynamic_cast<const EntityNode*>(&node))
      {
        refuse(0, "unsupported fresh native hierarchy");
      }
      self(self, *child);
    }
  };
  for (const auto* node : nodes)
  {
    check(check, *node);
  }
}
void VmfScene::registerAdditions(
  const std::vector<Node*>& nodes, const bool duplication, const Node* parent)
{
  registerAdditionBatch({{parent, nodes}}, duplication);
}

void VmfScene::registerAdditionBatch(
  const std::vector<std::pair<const Node*, std::vector<Node*>>>& batches,
  const bool duplication)
{
  requireEditing();
  auto nodes = std::vector<Node*>{};
  for (const auto& [parent, children] : batches)
  {
    if (parent)
    {
      validateAdditions(*parent, children, duplication);
    }
    nodes.insert(nodes.end(), children.begin(), children.end());
  }
  if (duplication)
  {
    validateDuplication(nodes);
  }
  struct Assignment
  {
    Node* node;
    const BrushFace* face;
    size_t binding;
  };
  auto additions = std::map<size_t, GeneratedBinding>{};
  auto groups = m_nativeGroups;
  auto groupNodes = m_groupNodes;
  auto groupLinks = m_groupLinks;
  auto groupStates = m_groupStates;
  auto states = m_clipStates;
  auto memberships = m_sideStateMemberships;
  auto assignments = std::vector<Assignment>{};
  auto groupDisplays = std::vector<std::tuple<GroupNode*, Group, std::string>>{};
  auto leafLinks = std::vector<std::pair<Node*, std::string>>{};
  auto binding = m_nextBinding;
  auto objectId = m_nextObjectId;
  auto sideId = m_nextSideId;
  auto idBytes = m_generatedIdBytes;
  const auto allocate =
    [&](const GeneratedKind kind, std::optional<VmfEntryId> source, const size_t owner) {
      auto& id = kind == GeneratedKind::Side ? sideId : objectId;
      if (
        binding == std::numeric_limits<size_t>::max()
        || m_generated.size() + additions.size() >= 1'000'000 || id.size() > 4096
        || id.size() > 16 * 1024 * 1024 - idBytes)
      {
        refuse(
          0, "native identity budget exhausted; save/reopen before further creation");
      }
      const auto allocated = binding++;
      idBytes += id.size();
      additions.emplace(allocated, GeneratedBinding{kind, id, source, owner});
      id = incrementId(id);
      return allocated;
    };
  const auto registerNode = [&](auto&& self, Node& node, size_t groupOwner) -> void {
    const auto source = duplication ? prototype(node.lineNumber()) : std::nullopt;
    const auto kind = dynamic_cast<EntityNode*>(&node)  ? GeneratedKind::Entity
                      : dynamic_cast<GroupNode*>(&node) ? GeneratedKind::Group
                                                        : GeneratedKind::Solid;
    const auto allocated = allocate(kind, source, 0);
    if (kind == GeneratedKind::Group)
    {
      groups.insert(allocated);
      groupNodes.emplace(allocated, static_cast<GroupNode*>(&node));
      auto linkId = generateUuid();
      groupLinks.emplace(allocated, linkId);
      groupDisplays.emplace_back(
        static_cast<GroupNode*>(&node),
        Group{"Group " + additions.at(allocated).nativeId},
        std::move(linkId));
    }
    if (duplication && kind != GeneratedKind::Group)
    {
      leafLinks.emplace_back(&node, generateUuid());
    }
    assignments.push_back({&node, nullptr, allocated});
    groupStates[allocated].insert(groupOwner);
    if (const auto* brush = dynamic_cast<const BrushNode*>(&node))
    {
      if (duplication)
      {
        validateSideState(*brush);
      }
      auto sides = std::set<size_t>{};
      for (const auto& face : brush->brush().faces())
      {
        const auto originalSide =
          duplication ? prototype(face.lineNumber()) : std::nullopt;
        if (
          originalSide
          && (!source || m_document.entries()[*originalSide].parent != source))
        {
          refuse(
            source.value_or(0),
            "duplicate side provenance does not belong to native solid");
        }
        const auto side = allocate(GeneratedKind::Side, originalSide, allocated);
        sides.insert(side);
        assignments.push_back({nullptr, &face, side});
      }
      if (sides.size() > 1'000'000 - memberships)
      {
        refuse(0, "native admitted side-state budget exhausted");
      }
      memberships += sides.size();
      states[allocated].push_back(std::move(sides));
    }
    for (auto* child : node.children())
    {
      self(self, *child, kind == GeneratedKind::Group ? allocated : 0);
    }
  };
  for (const auto& [parent, children] : batches)
  {
    for (auto* node : children)
    {
      registerNode(
        registerNode,
        *node,
        dynamic_cast<const GroupNode*>(parent) ? parent->lineNumber() : 0);
    }
  }
  size_t groupMemberships = 0;
  for (const auto& [entry, admittedGroups] : groupStates)
  {
    if (admittedGroups.size() > 1'000'000 - groupMemberships)
    {
      refuse(0, "native group membership budget exhausted");
    }
    groupMemberships += admittedGroups.size();
  }
  auto memberRegistrations = std::vector<std::tuple<Node*, size_t, std::string>>{};
  for (const auto& assignment : assignments)
  {
    if (assignment.node)
    {
      memberRegistrations.emplace_back(
        assignment.node, assignment.binding, additions.at(assignment.binding).nativeId);
    }
  }
  registerVisgroupMembers(memberRegistrations, duplication);
  // All fallible validation/allocation precedes identity publication. Detached
  // command-owned nodes keep these bindings when undo/redo parks/restores them.
  m_generated.merge(additions);
  m_groupStates.swap(groupStates);
  m_nativeGroups.swap(groups);
  m_groupNodes.swap(groupNodes);
  m_groupLinks.swap(groupLinks);
  m_clipStates.swap(states);
  m_sideStateMemberships = memberships;
  m_nextBinding = binding;
  m_nextObjectId = std::move(objectId);
  m_nextSideId = std::move(sideId);
  m_generatedIdBytes = idBytes;
  for (auto& [node, linkId] : leafLinks)
  {
    if (auto* entity = dynamic_cast<EntityNode*>(node))
    {
      entity->setLinkId(std::move(linkId));
    }
    else
    {
      static_cast<BrushNode*>(node)->setLinkId(std::move(linkId));
    }
  }
  for (auto& [group, display, linkId] : groupDisplays)
  {
    group->setGroup(std::move(display));
    group->setLinkId(std::move(linkId));
    group->resetPersistentId();
  }
  for (const auto& assignment : assignments)
  {
    if (assignment.node)
    {
      assignment.node->setFilePosition(assignment.binding, 1);
    }
    else
    {
      assignment.face->setFilePosition(assignment.binding, 1);
    }
  }
}
void VmfScene::validateRemoval(const std::vector<Node*>& removalClosure) const
{
  if (!removalClosure.empty())
  {
    requireEditing();
  }
  if (!removalClosure.empty())
  {
    requireEditableVersion(m_document);
  }
  auto original = NodeMap{};
  collect(*m_original, original);
  const auto check =
    [&](
      auto&& self, const Node& node, const std::set<VmfEntryId>& polygonBlocks) -> void {
    if (
      (!dynamic_cast<const EntityNode*>(&node) && !dynamic_cast<const BrushNode*>(&node)
       && !(
         dynamic_cast<const GroupNode*>(&node)
         && m_nativeGroups.contains(node.lineNumber())))
      || m_unsupportedGroupObjects.contains(node.lineNumber()))
    {
      refuse(0, "unsupported native removal closure");
    }
    validateVisgroupProjection(node);
    if (node.visibilityState() != VisibilityState::Inherited || !node.visible())
    {
      refuse(0, "native removal requires a visible inherited-visibility closure");
    }
    static_cast<void>(parentBinding(node));
    if (const auto* group = dynamic_cast<const GroupNode*>(&node))
    {
      const auto link = m_groupLinks.find(node.lineNumber());
      if (link == m_groupLinks.end() || link->second != group->linkId())
      {
        refuse(0, "native linked/foreign group removal unsupported");
      }
    }
    const auto source = prototype(node.lineNumber());
    if (source && opaque(m_document, *source, nullptr, &polygonBlocks))
    {
      refuse(*source, "opaque/displacement/instance attachments prevent native removal");
    }
    if (dynamic_cast<const BrushNode*>(&node) && !m_generated.contains(node.lineNumber()))
    {
      checkGeometryReferences(*source);
    }
    for (const auto* child : node.children())
    {
      self(self, *child, polygonBlocks);
    }
  };
  for (const auto* node : removalClosure)
  {
    const auto polygonBlocks = ownershipPolygonBlocks(
      m_document,
      original,
      *node,
      [&](const size_t binding) { return prototype(binding); },
      [&](const BrushNode& brush) { validateSideState(brush); });
    check(check, *node, polygonBlocks);
  }
}
void VmfScene::validateReparent(const Node& parent, const std::vector<Node*>& nodes) const
{
  validateReparentImpl(parent, nodes, true);
}
void VmfScene::validateReparentImpl(
  const Node& parent, const std::vector<Node*>& nodes, const bool operation) const
{
  requireEditing();
  if (!nodes.empty())
  {
    requireEditableVersion(m_document);
  }
  validateVisgroupProjection(parent);
  const auto target = m_generated.find(parent.lineNumber());
  if (
    !dynamic_cast<const EntityNode*>(&parent) || target == m_generated.end()
    || target->second.kind != GeneratedKind::Entity)
  {
    refuse(
      0, "native reparenting only supports wrapping plain brushes in a new brush entity");
  }
  if (
    operation
    && (!parent.visible() || parent.visibilityState() != VisibilityState::Inherited))
  {
    refuse(0, "native wrapping requires a visible owner");
  }
  if (target->second.prototype && opaque(m_document, *target->second.prototype))
  {
    refuse(*target->second.prototype, "opaque new owner prevents native wrapping");
  }
  auto original = NodeMap{};
  collect(*m_original, original);
  for (const auto* node : nodes)
  {
    validateVisgroupProjection(*node);
    if (
      !dynamic_cast<const BrushNode*>(node)
      || (operation && (node->visibilityState() != VisibilityState::Inherited || !node->visible())))
    {
      refuse(0, "native wrapping only supports visible plain brushes");
    }
    const auto source = prototype(node->lineNumber());
    if (source && opaque(m_document, *source))
    {
      refuse(*source, "opaque/displacement attachment prevents native wrapping");
    }
    if (!m_generated.contains(node->lineNumber()))
    {
      const auto found = original.find(node->lineNumber());
      if (
        found == original.end()
        || parentBinding(*found->second) != m_original->lineNumber())
      {
        refuse(0, "native wrapping currently requires world-owned brushes");
      }
      checkGeometryReferences(*source);
    }
  }
}
namespace
{
std::set<size_t> sideBindings(const Brush& brush)
{
  auto result = std::set<size_t>{};
  for (const auto& face : brush.faces())
  {
    if (!face.lineNumber() || !result.insert(face.lineNumber()).second)
    {
      refuse(0, "missing or duplicate native side binding");
    }
  }
  return result;
}

bool sameClipFaceData(const BrushFace& a, const BrushFace& b)
{
  const auto au = a.takeUvCoordSystemSnapshot();
  const auto bu = b.takeUvCoordSystemSnapshot();
  return au && bu && a.points() == b.points() && a.materialName() == b.materialName()
         && a.uvAttributes() == b.uvAttributes()
         && a.surfaceAttributes() == b.surfaceAttributes() && au->uAxis == bu->uAxis
         && au->vAxis == bu->vAxis && !a.sourceDisplacement() && !b.sourceDisplacement();
}
} // namespace

struct PreparedVmfRemoval::Data
{
  struct Row
  {
    const Node* address;
    size_t parent;
    std::set<size_t> sides;
    NativeVisibility causes;
    std::set<std::string> rawDefinitions;
  };
  std::shared_ptr<const void> scope;
  bool published = false;
  bool retired = false;
  bool groupContainersOnly = false;
  uint64_t revision;
  std::map<size_t, NativeVisgroupMember> members;
  std::vector<Node*> roots;
  std::map<size_t, Row> rows;
};

PreparedVmfRemoval VmfScene::prepareRemoval(const std::vector<Node*>& roots) const
{
  for (const auto* root : roots)
  {
    if (!root)
    {
      refuse(0, "null removal root");
    }
  }
  validateRemoval(roots);
  auto proof = std::make_shared<PreparedVmfRemoval::Data>();
  proof->scope = m_lifecycleScope;
  proof->revision = m_visgroups.revision();
  proof->roots = roots;
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  const auto definitions = m_visgroups.liveRecordIds();
  const auto capture = [&](auto&& self, const Node& node) -> void {
    const auto binding = node.lineNumber();
    if (
      !live.contains(binding) || live.at(binding) != &node
      || !m_memberNodes.contains(binding) || m_memberNodes.at(binding) != &node
      || proof->rows.contains(binding))
    {
      refuse(0, "removal proof requires an exact disjoint live closure");
    }
    auto sides = std::set<size_t>{};
    if (const auto* brush = dynamic_cast<const BrushNode*>(&node))
    {
      validateSideState(*brush);
      sides = sideBindings(brush->brush());
    }
    auto rawDefinitions = std::set<std::string>{};
    for (const auto& ref : m_visgroups.rawReferences(m_memberIds.at(binding)))
    {
      if (definitions.contains(ref))
      {
        rawDefinitions.insert(ref);
      }
    }
    proof->members.emplace(binding, m_visgroups.member(m_memberIds.at(binding)));
    proof->rows.emplace(
      binding,
      PreparedVmfRemoval::Data::Row{
        &node,
        node.parent()->lineNumber(),
        std::move(sides),
        node.nativeVisibility(),
        std::move(rawDefinitions)});
    for (const auto* child : node.children())
    {
      self(self, *child);
    }
  };
  for (const auto* root : roots)
  {
    if (!root)
    {
      refuse(0, "null removal root");
    }
    capture(capture, *root);
  }
  return PreparedVmfRemoval{std::move(proof)};
}

PreparedVmfRemoval VmfScene::prepareGroupRemoval(const std::vector<Node*>& groups) const
{
  auto proof = prepareRemoval(groups);
  proof.m_data->groupContainersOnly = true;
  std::set<size_t> bindings;
  for (const auto* group : groups)
  {
    if (!dynamic_cast<const GroupNode*>(group))
      refuse(0, "group-container ticket requires groups");
    bindings.insert(group->lineNumber());
  }
  std::erase_if(
    proof.m_data->rows, [&](const auto& row) { return !bindings.contains(row.first); });
  // NativeGroupCommand proves the full movement and expected EMPTY detached closure.
  return proof;
}

bool VmfScene::hasVisgroupMember(const Node& node) const
{
  const auto binding = node.lineNumber();
  const auto member = m_memberIds.find(binding);
  const auto address = m_memberNodes.find(binding);
  return member != m_memberIds.end() && address != m_memberNodes.end()
         && address->second == &node
         && m_memberBindings.at(member->second.m_serial) == binding;
}

std::optional<NativeVisgroupMemberId> VmfScene::registeredVisgroupMember(
  const Node& node) const
{
  if (!hasVisgroupMember(node))
  {
    return std::nullopt;
  }
  return m_memberIds.at(node.lineNumber());
}

void VmfScene::validateVisgroupReplay(
  const NativeVisgroupSnapshot& expected, const std::vector<size_t>& bindings) const
{
  const auto current = visgroups();
  if (current.refusalReason != expected.refusalReason)
    refuse(0, "native structural replay scene admission differs");
  const auto relevant = std::set<size_t>{bindings.begin(), bindings.end()};
  for (const auto& before : expected.members)
  {
    if (!relevant.contains(m_memberBindings.at(before.id.m_serial)))
      continue;
    const auto found =
      std::ranges::find(current.members, before.id, &NativeVisgroupMember::id);
    if (found == current.members.end() || *found != before)
      refuse(0, "native structural replay member metadata/context differs");
    for (const auto& ref : before.memberships)
    {
      const auto original =
        std::ranges::find(expected.records, ref, &NativeVisgroupRecord::nativeId);
      const auto live =
        std::ranges::find(current.records, ref, &NativeVisgroupRecord::nativeId);
      if (
        original != expected.records.end()
        && (live == current.records.end() || *original != *live))
        refuse(0, "native structural replay referenced definition differs");
    }
  }
}

void VmfScene::publishRemoval(
  const PreparedVmfRemoval& proof, const std::vector<Node*>& parkedRoots)
{
  if (!proof.m_data)
  {
    refuse(0, "empty or moved-from native removal ticket");
  }
  requireEditing();
  requireEditableVersion(m_document);
  const auto& p = *proof.m_data;
  if (
    p.scope != m_lifecycleScope || p.published || p.retired || p.roots != parkedRoots
    || p.revision != m_visgroups.revision())
  {
    refuse(0, "foreign or stale native removal proof");
  }
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  auto parked = NodeMap{};
  for (const auto* root : parkedRoots)
  {
    if (!root || root->parent())
    {
      refuse(0, "removal publication requires parked owned roots");
    }
    collect(*root, parked);
  }
  if (parked.size() != p.rows.size())
  {
    refuse(0, "partial native removal publication");
  }
  for (const auto& [binding, row] : p.rows)
  {
    const auto found = parked.find(binding);
    if (
      live.contains(binding) || found == parked.end() || found->second != row.address
      || found->second->nativeVisibility() != row.causes)
    {
      refuse(0, "native removal closure no longer matches its proof");
    }
    if (
      const auto* brush = dynamic_cast<const BrushNode*>(found->second);
      brush && sideBindings(brush->brush()) != row.sides)
    {
      refuse(0, "native removal side-state changed");
    }
    if (found->second->parent() && found->second->parent()->lineNumber() != row.parent)
    {
      refuse(0, "native removal internal owner changed");
    }
  }
  auto removals = m_removals;
  auto tickets = m_parkedTickets;
  for (const auto& [binding, row] : p.rows)
  {
    removals[binding] = p.members.at(binding);
    tickets[binding] = proof.m_data;
  }
  m_removals.swap(removals);
  m_parkedTickets.swap(tickets);
  proof.m_data->published = true;
}

void VmfScene::validateActivation(
  const PreparedVmfRemoval& proof,
  const std::vector<std::pair<Node*, Node*>>& placements) const
{
  requireEditing();
  requireEditableVersion(m_document);
  if (
    !proof.m_data || proof.m_data->scope != m_lifecycleScope || !proof.m_data->published
    || proof.m_data->retired)
  {
    refuse(0, "foreign/terminal native activation ticket");
  }
  const auto& p = *proof.m_data;
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  auto parked = NodeMap{};
  auto parents = std::map<const Node*, const Node*>{};
  auto roots = std::vector<Node*>{};
  for (const auto& [root, parent] : placements)
  {
    if (
      !root || !parent || root->parent()
      || !(
        parent == m_activeWorld->defaultLayer()
        || std::ranges::any_of(
          live, [&](const auto& row) { return row.second == parent; }))
      || !parents.emplace(root, parent).second)
    {
      refuse(0, "activation requires owned detached roots and live unique parents");
    }
    roots.push_back(root);
    collect(*root, parked);
  }
  if (roots != p.roots || parked.size() != p.rows.size())
  {
    refuse(0, "partial or reordered native activation closure");
  }
  const auto definitions = m_visgroups.liveRecordIds();
  for (const auto& [binding, row] : p.rows)
  {
    const auto found = parked.find(binding);
    if (
      live.contains(binding) || found == parked.end() || found->second != row.address
      || !m_parkedTickets.contains(binding)
      || m_parkedTickets.at(binding) != proof.m_data)
    {
      refuse(0, "activation closure identity/ticket changed");
    }
    const auto& node = *found->second;
    const auto* parent = node.parent() ? node.parent() : parents.at(&node);
    const auto& member = m_visgroups.member(m_memberIds.at(binding));
    if (
      parent->lineNumber() != row.parent || node.nativeVisibility() != row.causes
      || m_removals.at(binding) != member)
    {
      refuse(0, "activation semantic metadata/owner differs");
    }
    for (const auto& ref : row.rawDefinitions)
    {
      if (!definitions.contains(ref))
        refuse(0, "restore raw-reference definitions before activation");
    }
    for (const auto& ref : member.memberships)
    {
      if (!definitions.contains(ref))
        refuse(0, "restore visgroup definitions before activation");
    }
    if (
      const auto* brush = dynamic_cast<const BrushNode*>(&node);
      brush && sideBindings(brush->brush()) != row.sides)
    {
      refuse(0, "activation side-state differs");
    }
  }
}

void VmfScene::activateRemoval(
  const PreparedVmfRemoval& proof, const std::vector<Node*>& restoredRoots)
{
  if (!proof.m_data)
  {
    refuse(0, "empty or moved-from native removal ticket");
  }
  requireEditing();
  requireEditableVersion(m_document);
  auto& p = *proof.m_data;
  if (
    p.scope != m_lifecycleScope || !p.published || p.retired || p.roots != restoredRoots)
  {
    refuse(0, "foreign/terminal native activation ticket");
  }
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  // Verify current live ownership before dereferencing caller-provided addresses.
  for (const auto& [binding, row] : p.rows)
  {
    if (
      !live.contains(binding) || live.at(binding) != row.address
      || !m_parkedTickets.contains(binding)
      || m_parkedTickets.at(binding) != proof.m_data)
    {
      refuse(0, "native activation lacks its exact live closure/ticket");
    }
  }
  auto restored = NodeMap{};
  for (const auto* root : restoredRoots)
  {
    if (p.groupContainersOnly)
      restored.emplace(root->lineNumber(), root);
    else
      collect(*root, restored);
  }
  if (restored.size() != p.rows.size())
  {
    refuse(0, "partial native activation closure");
  }
  const auto definitions = m_visgroups.liveRecordIds();
  for (const auto& [binding, row] : p.rows)
  {
    const auto& node = *restored.at(binding);
    const auto* m = &m_visgroups.member(m_memberIds.at(binding));
    if (
      !node.parent() || node.parent()->lineNumber() != row.parent
      || node.nativeVisibility() != row.causes || m_removals.at(binding) != *m)
    {
      refuse(0, "native activation semantic metadata/owner differs");
    }
    for (const auto& ref : row.rawDefinitions)
    {
      if (!definitions.contains(ref))
      {
        refuse(0, "restore raw-reference definitions before object activation");
      }
    }
    for (const auto& ref : m->memberships)
    {
      if (!definitions.contains(ref))
      {
        refuse(0, "restore visgroup definitions before object activation");
      }
    }
    if (
      const auto* brush = dynamic_cast<const BrushNode*>(&node);
      brush && sideBindings(brush->brush()) != row.sides)
    {
      refuse(0, "native activation side-state differs");
    }
  }
  for (const auto& [binding, row] : p.rows)
  {
    m_removals.erase(binding);
    m_parkedTickets.erase(binding);
  }
  p.retired = true;
  p.roots.clear();
  p.rows.clear();
  p.members.clear();
  projectVisgroups();
}

void VmfScene::disposeRemoval(const PreparedVmfRemoval& proof)
{
  if (!proof.m_data)
  {
    refuse(0, "empty or moved-from native removal ticket");
  }
  auto& p = *proof.m_data;
  if (p.scope != m_lifecycleScope || !p.published || p.retired)
  {
    refuse(0, "foreign/terminal native disposal ticket");
  }
  auto live = NodeMap{};
  collect(*m_activeWorld, live);
  for (const auto& [binding, row] : p.rows)
  {
    if (
      live.contains(binding) || !m_parkedTickets.contains(binding)
      || m_parkedTickets.at(binding) != proof.m_data)
    {
      refuse(0, "native disposal requires its exact parked ticket");
    }
  }
  for (const auto& [binding, row] : p.rows)
  {
    m_parkedTickets.erase(binding);
    m_memberNodes.erase(binding);
  }
  p.retired = true;
  p.roots.clear();
  p.rows.clear();
  p.members.clear();
}

void VmfScene::validateSideState(const BrushNode& node) const
{
  const auto binding = node.lineNumber();
  const auto source = prototype(binding);
  const auto generated = m_generated.find(binding);
  if (generated != m_generated.end() && generated->second.kind != GeneratedKind::Solid)
  {
    refuse(0, "native solid kind mismatch");
  }
  const auto bindings = sideBindings(node.brush());
  auto prototypes = std::set<VmfEntryId>{};
  for (const auto& face : node.brush().faces())
  {
    const auto record = m_generated.find(face.lineNumber());
    const auto side = prototype(face.lineNumber());
    if (record != m_generated.end())
    {
      if (
        record->second.kind != GeneratedKind::Side || record->second.owner != binding
        || face.sourceDisplacement())
      {
        refuse(0, "foreign generated side owner/kind or displacement");
      }
    }
    else if (generated != m_generated.end() || !side)
    {
      refuse(0, "generated solid acquired unregistered side");
    }
    if (side && (!source || m_document.entries()[*side].parent != source
                 || key(m_document, *side) != "side" || !prototypes.insert(*side).second))
    {
      refuse(0, "side prototype does not uniquely belong to native solid");
    }
  }
  if (
    const auto states = m_clipStates.find(binding);
    states != m_clipStates.end()
    && std::ranges::find(states->second, bindings) != states->second.end())
  {
    return;
  }
  if (generated == m_generated.end() && source)
  {
    auto baseline = std::set<size_t>{};
    for (const auto child : m_document.entries()[*source].children)
    {
      if (m_document.entries()[child].isBlock() && key(m_document, child) == "side")
      {
        baseline.insert(child + 1);
      }
    }
    if (bindings == baseline)
    {
      return;
    }
  }
  refuse(0, "unregistered native brush side set");
}

void VmfScene::validateClipOwner(
  const BrushNode& node, const bool clipping, const bool operation) const
{
  validateVisgroupProjection(node);
  const Node* owner = effectiveParent(node);
  const auto* entity = dynamic_cast<const EntityNode*>(owner);
  auto polygonBlocks = std::set<VmfEntryId>{};
  if (!clipping)
  {
    auto original = NodeMap{};
    collect(*m_original, original);
    polygonBlocks = ownershipPolygonBlocks(
      m_document,
      original,
      entity ? static_cast<const Node&>(*entity) : static_cast<const Node&>(node),
      [&](const size_t binding) { return prototype(binding); },
      [&](const BrushNode& brush) { validateSideState(brush); });
  }
  if (entity)
  {
    const auto record = m_generated.find(entity->lineNumber());
    const auto source = prototype(entity->lineNumber());
    if (
      (record != m_generated.end() && record->second.kind != GeneratedKind::Entity)
      || (source && (key(m_document, *source) != "entity" || opaque(m_document, *source, nullptr, &polygonBlocks)
                     || m_unsupportedGroupObjects.contains(*source + 1)))
      || unsafeClass(entity->entity().classname())
      || (operation && entity->visibilityState() != VisibilityState::Inherited))
    {
      refuse(0, "unsupported native clip entity owner");
    }
    if (record == m_generated.end())
    {
      auto original = NodeMap{};
      collect(*m_original, original);
      const auto found = original.find(entity->lineNumber());
      if (
        found == original.end() || !dynamic_cast<const EntityNode*>(found->second)
        || found->second->childCount() == 0)
      {
        refuse(0, "point or unprojected entity cannot acquire clip ownership");
      }
    }
    if (!source)
    {
      validateFreshData(*entity);
    }
    owner = effectiveParent(*entity);
  }
  const auto* layer = dynamic_cast<const LayerNode*>(owner);
  const auto* world = layer ? dynamic_cast<const WorldNode*>(layer->parent()) : nullptr;
  if (
    !world || world != m_activeWorld || world->lineNumber() != m_original->lineNumber()
    || layer != world->defaultLayer()
    || (operation && (layer->visibilityState() != VisibilityState::Inherited || !node.visible() || node.visibilityState() != VisibilityState::Inherited)))
  {
    refuse(0, "native clip requires live visible ordinary ownership");
  }
  auto live = NodeMap{};
  collect(*world, live);
  if (!live.contains(node.lineNumber()) || live.at(node.lineNumber()) != &node)
  {
    refuse(0, "native clip target is not a unique live solid");
  }
  const auto source = prototype(node.lineNumber());
  if (
    source
    && (key(m_document, *source) != "solid" || opaque(m_document, *source, nullptr, &polygonBlocks)))
  {
    refuse(*source, "opaque/displacement source prevents native clipping");
  }
  if (source)
  {
    for (auto parent = m_document.entries()[*source].parent; parent;
         parent = m_document.entries()[*parent].parent)
    {
      if (
        key(m_document, *parent) == "entity"
        && opaque(m_document, *parent, nullptr, &polygonBlocks))
      {
        refuse(*parent, "opaque source owner prevents native clipping");
      }
    }
  }
  if (!m_generated.contains(node.lineNumber()))
  {
    auto original = NodeMap{};
    collect(*m_original, original);
    const auto found = original.find(node.lineNumber());
    if (found == original.end())
    {
      refuse(0, "native clip solid is not projected");
    }
    if (parentBinding(*found->second) != parentBinding(node))
    {
      if (!entity)
      {
        refuse(0, "native clip owner provenance changed");
      }
      validateReparentImpl(*entity, {const_cast<BrushNode*>(&node)}, operation);
    }
  }
  const auto states = m_clipStates.find(node.lineNumber());
  if (clipping || (states != m_clipStates.end() && states->second.size() > 1))
  {
    checkGeometryReferences(source.value_or(0));
  }
  for (const auto& face : node.brush().faces())
  {
    if (face.sourceDisplacement())
    {
      refuse(0, "native displacement clipping unsupported");
    }
  }
  validateSideState(node);
}

void VmfScene::registerClips(
  std::vector<std::pair<Node*, NodeContents>>& nodes, const vm::bbox3d& worldBounds)
{
  requireEditing();
  requireEditableVersion(m_document);
  if (nodes.empty())
  {
    refuse(0, "empty native clip batch");
  }
  auto generated = m_generated;
  auto states = m_clipStates;
  auto memberships = m_sideStateMemberships;
  auto binding = m_nextBinding;
  auto sideId = m_nextSideId;
  auto idBytes = m_generatedIdBytes;
  auto assignments = std::vector<std::pair<const BrushFace*, size_t>>{};
  auto seen = std::set<const Node*>{};
  for (auto& [node, contents] : nodes)
  {
    if (!node)
    {
      refuse(0, "missing native clip target");
    }
    const auto* before = dynamic_cast<const BrushNode*>(node);
    const auto* after = std::get_if<Brush>(&contents.get());
    if (!seen.insert(node).second || !before || !after)
    {
      refuse(0, "invalid or repeated native clip target");
    }
    validateClipOwner(*before);
    const auto source = prototype(node->lineNumber()).value_or(0);
    validateContents(*node, NodeContents{before->brush()}, worldBounds);
    const auto oldSides = sideBindings(before->brush());
    auto newSides = std::set<size_t>{};
    const BrushFace* cut = nullptr;
    for (const auto& face : before->brush().faces())
    {
      if (face.sourceDisplacement())
      {
        refuse(source, "displacement brush clipping unsupported");
      }
    }
    for (const auto& face : after->faces())
    {
      if (face.sourceDisplacement())
      {
        refuse(source, "displacement brush clipping unsupported");
      }
      if (!face.lineNumber())
      {
        if (cut)
        {
          refuse(source, "native clip requires exactly one new cut face");
        }
        cut = &face;
      }
      else if (
        !oldSides.contains(face.lineNumber())
        || !newSides.insert(face.lineNumber()).second)
      {
        refuse(source, "foreign or duplicate clip survivor");
      }
    }
    if (!cut || !after->closed() || !after->fullySpecified())
    {
      refuse(source, "empty or no-op native clip");
    }
    if (
      binding == std::numeric_limits<size_t>::max() || generated.size() >= 1'000'000
      || sideId.size() > 4096 || sideId.size() > 16 * 1024 * 1024 - idBytes)
    {
      refuse(0, "native identity budget exhausted; save/reopen before further creation");
    }
    static_cast<void>(VmfDocument::parse(freshSideText(*cut, sideId)));
    // Reproduce Brush::clip's actual face preservation, never infer provenance
    // by matching floating-point planes. This also rejects forged survivors.
    auto expected = before->brush();
    if (
      !expected.clip(worldBounds, *cut).is_success()
      || expected.faceCount() != after->faceCount())
    {
      refuse(source, "candidate is not an ordinary one-plane clip");
    }
    for (const auto& face : expected.faces())
    {
      const auto found = std::ranges::find_if(after->faces(), [&](const auto& other) {
        return face.lineNumber() == other.lineNumber();
      });
      if (found == after->faces().end() || !sameClipFaceData(face, *found))
      {
        refuse(source, "clip changed surviving face provenance or data");
      }
    }
    auto oldVertices = before->brush().vertexPositions();
    auto newVertices = after->vertexPositions();
    std::sort(oldVertices.begin(), oldVertices.end());
    std::sort(newVertices.begin(), newVertices.end());
    if (oldVertices == newVertices)
    {
      refuse(source, "no-op native clip");
    }
    const auto allocated = binding++;
    generated.emplace(
      allocated,
      GeneratedBinding{GeneratedKind::Side, sideId, std::nullopt, node->lineNumber()});
    idBytes += sideId.size();
    sideId = incrementId(sideId);
    newSides.insert(allocated);
    auto& admitted = states[node->lineNumber()];
    if (std::ranges::find(admitted, newSides) == admitted.end())
    {
      if (newSides.size() > 1'000'000 - memberships)
      {
        refuse(0, "native admitted side-state budget exhausted");
      }
      memberships += newSides.size();
      admitted.push_back(std::move(newSides));
    }
    assignments.emplace_back(cut, allocated);
  }
  // No live node, identity counter or command-owned contents change on failure.
  m_generated.swap(generated);
  m_clipStates.swap(states);
  m_sideStateMemberships = memberships;
  m_nextBinding = binding;
  m_nextSideId.swap(sideId);
  m_generatedIdBytes = idBytes;
  for (const auto& [face, allocated] : assignments)
  {
    face->setFilePosition(allocated, 1);
  }
}

void VmfScene::registerBothClips(
  const std::vector<BrushNode*>& subjects,
  std::vector<std::pair<Node*, NodeContents>>& keepers,
  const std::vector<BrushNode*>& siblings,
  const vm::bbox3d& worldBounds)
{
  // No-op subjects participate in admission too, before any staged allocation.
  validateBothClipReplay(subjects, {}, {}, worldBounds);
  if (keepers.empty() || keepers.size() != siblings.size())
  {
    refuse(0, "invalid native Both batch");
  }
  auto generated = m_generated;
  auto groupStates = m_groupStates;
  auto states = m_clipStates;
  auto memberships = m_sideStateMemberships;
  auto binding = m_nextBinding;
  auto sideId = m_nextSideId;
  auto objectId = m_nextObjectId;
  auto idBytes = m_generatedIdBytes;
  auto faceAssignments = std::vector<std::pair<const BrushFace*, size_t>>{};
  auto nodeAssignments = std::vector<std::pair<BrushNode*, size_t>>{};
  auto seen = std::set<const Node*>{};
  auto seenSiblings = std::set<const Node*>{};
  const auto allocate = [&](
                          const GeneratedKind kind,
                          const std::optional<VmfEntryId> source,
                          const size_t owner) {
    auto& id = kind == GeneratedKind::Side ? sideId : objectId;
    if (
      binding == std::numeric_limits<size_t>::max() || generated.size() >= 1'000'000
      || id.size() > 4096 || id.size() > 16 * 1024 * 1024 - idBytes)
    {
      refuse(0, "native identity budget exhausted; save/reopen before further creation");
    }
    const auto result = binding++;
    generated.emplace(result, GeneratedBinding{kind, id, source, owner});
    idBytes += id.size();
    id = incrementId(id);
    return result;
  };
  const auto admit = [&](const size_t owner, std::set<size_t> sides) {
    auto& admitted = states[owner];
    if (std::ranges::find(admitted, sides) == admitted.end())
    {
      if (sides.size() > 1'000'000 - memberships)
      {
        refuse(0, "native admitted side-state budget exhausted");
      }
      memberships += sides.size();
      admitted.push_back(std::move(sides));
    }
  };
  for (size_t i = 0; i < keepers.size(); ++i)
  {
    auto* before = dynamic_cast<BrushNode*>(keepers[i].first);
    auto* front = std::get_if<Brush>(&keepers[i].second.get());
    auto* sibling = siblings[i];
    if (
      !before || !front || !sibling || sibling->parent() || sibling->hasChildren()
      || before->hasChildren() || !seen.insert(before).second
      || !seenSiblings.insert(sibling).second
      || std::ranges::find(subjects, before) == subjects.end()
      || sibling->lineNumber() != before->lineNumber()
      || sibling->visibilityState() != VisibilityState::Inherited
      || !before->parent()->canAddChild(*sibling))
    {
      refuse(0, "invalid native Both keeper/sibling ownership");
    }
    const auto oldSides = sideBindings(before->brush());
    const auto checkHalf = [&](const Brush& half) -> const BrushFace* {
      const BrushFace* cut = nullptr;
      auto survivors = std::set<size_t>{};
      for (const auto& face : half.faces())
      {
        if (face.sourceDisplacement())
        {
          refuse(0, "native displacement clipping unsupported");
        }
        if (!face.lineNumber())
        {
          if (cut)
          {
            refuse(0, "native Both requires one cut face per half");
          }
          cut = &face;
        }
        else if (
          !oldSides.contains(face.lineNumber())
          || !survivors.insert(face.lineNumber()).second)
        {
          refuse(0, "foreign or duplicate native Both survivor");
        }
      }
      if (!cut || !half.closed() || !half.fullySpecified())
      {
        refuse(0, "empty or no-op native Both half");
      }
      static_cast<void>(VmfDocument::parse(freshSideText(*cut, sideId)));
      auto expected = before->brush();
      if (
        !expected.clip(worldBounds, *cut).is_success()
        || expected.faceCount() != half.faceCount())
      {
        refuse(0, "native Both half is not an ordinary clip");
      }
      for (const auto& face : expected.faces())
      {
        const auto found = std::ranges::find_if(half.faces(), [&](const auto& other) {
          return face.lineNumber() == other.lineNumber();
        });
        if (found == half.faces().end() || !sameClipFaceData(face, *found))
        {
          refuse(0, "native Both changed survivor provenance or data");
        }
      }
      return cut;
    };
    const auto* frontCut = checkHalf(*front);
    const auto* backCut = checkHalf(sibling->brush());
    const auto& fp = frontCut->points();
    const auto& bp = backCut->points();
    if (fp[0] != bp[0] || fp[1] != bp[2] || fp[2] != bp[1])
    {
      refuse(0, "native Both halves do not share the opposite cut");
    }
    auto frontSides = std::set<size_t>{};
    for (const auto& face : front->faces())
    {
      if (face.lineNumber())
      {
        frontSides.insert(face.lineNumber());
      }
    }
    const auto frontBinding =
      allocate(GeneratedKind::Side, std::nullopt, before->lineNumber());
    frontSides.insert(frontBinding);
    faceAssignments.emplace_back(frontCut, frontBinding);
    admit(before->lineNumber(), std::move(frontSides));

    const auto solid = allocate(GeneratedKind::Solid, prototype(before->lineNumber()), 0);
    nodeAssignments.emplace_back(sibling, solid);
    const auto* parentGroup = dynamic_cast<const GroupNode*>(before->parent());
    groupStates[solid].insert(parentGroup ? parentGroup->lineNumber() : 0);
    auto backSides = std::set<size_t>{};
    for (const auto& face : sibling->brush().faces())
    {
      const auto source = face.lineNumber() ? prototype(face.lineNumber()) : std::nullopt;
      const auto side = allocate(GeneratedKind::Side, source, solid);
      faceAssignments.emplace_back(&face, side);
      backSides.insert(side);
    }
    admit(solid, std::move(backSides));
  }
  size_t groupMemberships = 0;
  for (const auto& [entry, admittedGroups] : groupStates)
  {
    if (admittedGroups.size() > 1'000'000 - groupMemberships)
    {
      refuse(0, "native group membership budget exhausted");
    }
    groupMemberships += admittedGroups.size();
  }
  auto memberRegistrations = std::vector<std::tuple<Node*, size_t, std::string>>{};
  for (const auto& [node, allocated] : nodeAssignments)
  {
    memberRegistrations.emplace_back(node, allocated, generated.at(allocated).nativeId);
  }
  registerVisgroupMembers(memberRegistrations, true);
  // One publication for the entire selected batch, including both namespaces,
  // exact side states and detached-node provenance. No registerAdditions/Clips calls.
  m_groupStates.swap(groupStates);
  m_generated.swap(generated);
  m_clipStates.swap(states);
  m_sideStateMemberships = memberships;
  m_nextBinding = binding;
  m_nextSideId.swap(sideId);
  m_nextObjectId.swap(objectId);
  m_generatedIdBytes = idBytes;
  for (const auto& [face, allocated] : faceAssignments)
  {
    face->setFilePosition(allocated, 1);
  }
  for (const auto& [node, allocated] : nodeAssignments)
  {
    node->setFilePosition(allocated, 1);
  }
}

void VmfScene::validateBothClipReplay(
  const std::vector<BrushNode*>& subjects,
  const std::vector<std::pair<Node*, NodeContents>>& keepers,
  const std::vector<BrushNode*>& siblings,
  const vm::bbox3d& worldBounds) const
{
  const auto session = m_visgroups.beginEmission(m_document);
  requireEditing();
  requireEditableVersion(m_document);
  auto seen = std::set<const Node*>{};
  for (const auto* subject : subjects)
  {
    if (!subject || subject->hasChildren() || !seen.insert(subject).second)
    {
      refuse(0, "invalid or repeated native Both subject");
    }
    validateClipOwner(*subject);
    validateContents(*subject, NodeContents{subject->brush()}, worldBounds);
  }
  if (keepers.size() != siblings.size())
  {
    refuse(0, "invalid native Both replay batch");
  }
  auto original = NodeMap{};
  collect(*m_original, original);
  for (size_t i = 0; i < keepers.size(); ++i)
  {
    const auto& [node, contents] = keepers[i];
    const auto* sibling = siblings[i];
    if (
      !node || !seen.contains(node) || !sibling || sibling->parent()
      || sibling->hasChildren()
      || sibling->visibilityState() != VisibilityState::Inherited
      || !node->parent()->canAddChild(*sibling))
    {
      refuse(0, "invalid parked native Both ownership");
    }
    validateClipContents(*node, contents, worldBounds);
    validateSideState(*sibling);
    const auto record = m_generated.find(sibling->lineNumber());
    if (
      record == m_generated.end() || record->second.kind != GeneratedKind::Solid
      || !sibling->brush().closed() || !sibling->brush().fullySpecified()
      || !worldBounds.contains(sibling->brush().bounds()))
    {
      refuse(0, "invalid parked native Both solid");
    }
    // Detached validation deliberately does not call the live-owner validator.
    static_cast<void>(emitNode(*sibling, original, session));
  }
}

void VmfScene::validateContents(
  const Node& node, const NodeContents& contents, const vm::bbox3d& worldBounds) const
{
  if (const auto* before = dynamic_cast<const BrushNode*>(&node))
  {
    const auto* after = std::get_if<Brush>(&contents.get());
    if (!after || sideBindings(before->brush()) != sideBindings(*after))
    {
      refuse(0, "generic content edits cannot change native side identities");
    }
  }
  validateContentsImpl(node, contents, worldBounds);
}

void VmfScene::validateClipContents(
  const Node& node, const NodeContents& contents, const vm::bbox3d& worldBounds) const
{
  requireEditing();
  if (const auto* brush = dynamic_cast<const BrushNode*>(&node))
  {
    validateClipOwner(*brush);
  }
  validateContentsImpl(node, contents, worldBounds);
}

void VmfScene::validateContentsImpl(
  const Node& node, const NodeContents& contents, const vm::bbox3d& worldBounds) const
{
  requireEditing();
  requireEditableVersion(m_document);
  validateVisgroupProjection(node);
  auto changed = true;
  if (const auto* entity = std::get_if<Entity>(&contents.get()))
  {
    if (const auto* current = dynamic_cast<const EntityNodeBase*>(&node))
    {
      changed = current->entity() != *entity;
    }
  }
  else if (const auto* brush = std::get_if<Brush>(&contents.get()))
  {
    if (const auto* current = dynamic_cast<const BrushNode*>(&node))
    {
      changed = current->brush() != *brush;
    }
  }
  else if (const auto* group = std::get_if<Group>(&contents.get()))
  {
    if (const auto* current = dynamic_cast<const GroupNode*>(&node))
    {
      changed = current->group() != *group;
    }
  }
  const auto native = node.effectiveNativeVisibility();
  if (changed && (native.manual || native.automatic))
  {
    refuse(0, "native manual/automatic hidden content edits require explicit Show");
  }
  if (m_unsupportedGroupObjects.contains(node.lineNumber()))
  {
    refuse(0, "unsupported native group component content edit");
  }
  static_cast<void>(parentBinding(node));
  if (
    const auto* brush = dynamic_cast<const BrushNode*>(&node);
    brush
    && (m_generated.contains(node.lineNumber()) || m_clipStates.contains(node.lineNumber())))
  {
    validateClipOwner(*brush, false);
  }
  auto copy = std::unique_ptr<Node>{node.clone(worldBounds)};
  if (const auto* entity = std::get_if<Entity>(&contents.get()))
  {
    auto* target = dynamic_cast<EntityNodeBase*>(copy.get());
    if (!target)
    {
      refuse(0, "native content type changed");
    }
    static_cast<void>(target->setEntity(*entity));
  }
  else if (const auto* brush = std::get_if<Brush>(&contents.get()))
  {
    auto* target = dynamic_cast<BrushNode*>(copy.get());
    if (!target)
    {
      refuse(0, "native content type changed");
    }
    static_cast<void>(target->setBrush(*brush));
  }
  else if (const auto* group = std::get_if<Group>(&contents.get()))
  {
    const auto* before = dynamic_cast<const GroupNode*>(&node);
    const auto link = m_groupLinks.find(node.lineNumber());
    if (
      !before || !m_nativeGroups.contains(node.lineNumber()) || link == m_groupLinks.end()
      || link->second != before->linkId() || group->name() != before->group().name())
    {
      refuse(0, "native group rename/content editing unsupported");
    }
    return;
  }
  else
  {
    refuse(0, "native group/layer/patch content editing unsupported");
  }
  auto original = NodeMap{};
  collect(*m_original, original);
  const auto source = prototype(node.lineNumber());
  if (source)
  {
    const auto found = original.find(*source + 1);
    if (found == original.end())
    {
      refuse(*source, "native content provenance is not a projected object");
    }
    auto values = std::vector<VmfValueEdit>{};
    auto structure = std::vector<VmfStructuralEdit>{};
    auto appended = std::map<VmfEntryId, std::string>{};
    nodeEdits(
      *found->second,
      *copy,
      values,
      structure,
      appended,
      !m_generated.contains(node.lineNumber()));
  }
  else
  {
    static_cast<void>(emitNode(*copy, original, m_visgroups.beginEmission(m_document)));
  }
}
void VmfScene::nodeEdits(
  const Node& beforeNode,
  const Node& afterNode,
  std::vector<VmfValueEdit>& edits,
  std::vector<VmfStructuralEdit>& structural,
  std::map<VmfEntryId, std::string>& appended,
  const bool protectExistingGeometry) const
{
  const auto& d = m_document;
  const auto* before = &beforeNode;
  const auto* after = &afterNode;
  const auto id = before->lineNumber() - 1;
  const auto edit =
    [&](VmfEntryId block, const std::string& name, std::string replacement) {
      const auto id = property(d, block, name);
      if (!id)
      {
        refuse(block, "adding property " + name + " unsupported");
      }
      edits.push_back({*id, quote(replacement)});
    };
  if (const auto* eb = dynamic_cast<const EntityNodeBase*>(before))
  {
    const auto* ea = dynamic_cast<const EntityNodeBase*>(after);
    if (!ea)
    {
      refuse(id, "object type changed");
    }
    if (
      eb->entity().classname() != ea->entity().classname()
      && unsafeClass(ea->entity().classname()))
    {
      refuse(id, "creating overlay/instance semantics is unsupported");
    }
    const auto& bp = eb->entity().properties();
    const auto& ap = ea->entity().properties();
    auto names = std::set<std::string>{};
    for (const auto& p : ap)
    {
      if (p.key() == "id" || !names.insert(p.key()).second)
      {
        refuse(id, "reserved native id or duplicate entity property");
      }
    }
    for (const auto& p : bp)
    {
      const auto* v = ea->entity().property(p.key());
      if (!v || *v != p.value())
      {
        if (opaque(d, id))
        {
          refuse(id, "entity has opaque attachments; edit refused");
        }
        if (!v)
        {
          const auto entry = property(d, id, p.key());
          if (!entry)
          {
            refuse(id, "stale property binding");
          }
          structural.push_back({VmfStructuralEdit::Type::Remove, *entry, {}});
        }
        else
        {
          edit(id, p.key(), *v);
        }
      }
    }
    for (const auto& p : ap)
    {
      if (!eb->entity().hasProperty(p.key()))
      {
        if (opaque(d, id) || property(d, id, p.key()))
        {
          refuse(id, "opaque attachment or stale property addition");
        }
        appended[id] += quote(p.key()) + " " + quote(p.value()) + "\n";
      }
    }
    const auto& bc = eb->entity().sourceConnections();
    const auto& ac = ea->entity().sourceConnections();
    if (bc != ac)
    {
      auto blocks = std::vector<VmfEntryId>{};
      for (const auto child : d.entries()[id].children)
      {
        if (d.entries()[child].isBlock() && key(d, child) == "connections")
        {
          blocks.push_back(child);
        }
      }
      if (blocks.size() > 1 || key(d, id) != "entity")
      {
        refuse(id, "ambiguous connections block or non-entity outputs");
      }
      auto retained = std::set<VmfEntryId>{};
      auto last = std::optional<VmfEntryId>{};
      auto adding = false;
      auto newRows = std::string{};
      for (const auto& row : ac)
      {
        if (row.sourceEntry)
        {
          const auto it = std::find_if(bc.begin(), bc.end(), [&](const auto& old) {
            return old.sourceEntry == row.sourceEntry;
          });
          if (
            it == bc.end() || !retained.insert(*row.sourceEntry).second
            || (last && *row.sourceEntry <= *last) || adding)
          {
            refuse(id, "stale, duplicate or reordered connection binding");
          }
          last = row.sourceEntry;
          if (it->event != row.event || it->value != row.value)
          {
            if (!validSourceConnection(row))
            {
              refuse(id, "invalid edited connection");
            }
            if (it->event != row.event)
            {
              structural.push_back(
                {VmfStructuralEdit::Type::Key, *row.sourceEntry, quote(row.event)});
            }
            if (it->value != row.value)
            {
              edits.push_back({*row.sourceEntry, quote(row.value)});
            }
          }
        }
        else
        {
          adding = true;
          if (!validSourceConnection(row))
          {
            refuse(id, "invalid new connection");
          }
          newRows += quote(row.event) + " " + quote(row.value) + "\n";
        }
      }
      for (const auto& row : bc)
      {
        if (!retained.contains(*row.sourceEntry))
        {
          if (!validSourceConnection(row))
          {
            refuse(id, "malformed native connection cannot be removed");
          }
          structural.push_back({VmfStructuralEdit::Type::Remove, *row.sourceEntry, {}});
        }
      }
      if (!newRows.empty())
      {
        if (blocks.empty())
        {
          appended[id] += "connections\n{\n" + newRows + "}\n";
        }
        else
        {
          appended[blocks.front()] += newRows;
        }
      }
    }
  }
  else if (const auto* bb = dynamic_cast<const BrushNode*>(before))
  {
    const auto* ba = dynamic_cast<const BrushNode*>(after);
    if (!ba)
    {
      refuse(id, "object type changed");
    }
    const auto& b = bb->brush();
    const auto& a = ba->brush();
    validateSideState(*ba);
    const auto bindings = sideBindings(a);
    const auto originalBindings = sideBindings(b);
    auto templateBindings = std::set<size_t>{};
    for (const auto& face : a.faces())
    {
      if (const auto side = prototype(face.lineNumber()))
      {
        templateBindings.insert(*side + 1);
      }
    }
    const auto topologyChanged =
      templateBindings != originalBindings || templateBindings.size() != bindings.size();
    if (topologyChanged)
    {
      if (opaque(d, id))
      {
        refuse(id, "opaque/displacement attachment prevents native clipping");
      }
      checkGeometryReferences(id);
    }
    auto displacementSides = std::set<VmfEntryId>{};
    for (const auto& face : b.faces())
    {
      if (face.sourceDisplacement())
      {
        displacementSides.insert(face.lineNumber() - 1);
      }
    }
    const auto polygonEditStart = edits.size();
    auto polygonBlocks = std::set<VmfEntryId>{};
    if (b != a)
    {
      const auto polygonBrush = canonicalPolygonBrush(
        d, a, id, [&](const size_t binding) { return prototype(binding); });
      polygonBlocks = polygonCacheEdits(d, b, polygonBrush, edits);
    }
    if (edits.size() != polygonEditStart)
    {
      if (opaque(d, id, &displacementSides, &polygonBlocks))
      {
        refuse(id, "opaque attachment prevents native polygon cache edit");
      }
      if (protectExistingGeometry)
      {
        checkGeometryReferences(id);
      }
    }
    auto sides = std::map<size_t, const BrushFace*>{};
    for (const auto& f : a.faces())
    {
      const auto source = prototype(f.lineNumber());
      const auto generatedSide = m_generated.find(f.lineNumber());
      if (!source && generatedSide != m_generated.end())
      {
        if (f.sourceDisplacement())
        {
          refuse(id, "generated displacement side unsupported");
        }
        const auto text = freshSideText(f, generatedSide->second.nativeId);
        static_cast<void>(VmfDocument::parse(text));
        appended[id] += text;
        continue;
      }
      if (!source || !sides.emplace(*source + 1, &f).second)
      {
        refuse(id, "duplicate face binding");
      }
    }
    if (!displacementSides.empty())
    {
      auto baseTransform = std::optional<vm::mat4x4d>{};
      auto baseChanged = false;
      for (const auto& face : b.faces())
      {
        const auto found = sides.find(face.lineNumber());
        if (found == sides.end())
        {
          refuse(id, "face topology/binding changed");
        }
        const auto& current = *found->second;
        baseChanged = baseChanged || face.points() != current.points();
        if (const auto& mesh = face.sourceDisplacement())
        {
          if (!current.sourceDisplacement())
          {
            refuse(id, "displacement structure changed");
          }
          const auto transform = current.sourceDisplacement()->baseTransformFrom(*mesh);
          if (!transform)
          {
            refuse(id, "displacement bases require shared proper similarity motion");
          }
          if (baseTransform)
          {
            // Stored frames encode intent exactly; epsilon is for points only.
            if (*baseTransform != *transform)
            {
              refuse(id, "displacement sides must share one exact base frame");
            }
          }
          else
          {
            baseTransform = transform;
          }
        }
      }
      if (!baseTransform)
      {
        refuse(id, "missing displacement base");
      }
      if (baseChanged != (*baseTransform != vm::mat4x4d::identity()))
      {
        refuse(id, "displacement mesh and base must move together");
      }
      for (const auto& face : b.faces())
      {
        const auto& current = *sides.at(face.lineNumber());
        for (size_t i = 0; i < face.points().size(); ++i)
        {
          if (!vm::is_equal(
                current.points()[i],
                *baseTransform * face.points()[i],
                SourceDisplacement::TransformEpsilon))
          {
            refuse(id, "all displacement base planes must follow one similarity frame");
          }
        }
      }
    }
    for (const auto& f : b.faces())
    {
      const auto it = sides.find(f.lineNumber());
      if (it == sides.end())
      {
        if (!topologyChanged)
        {
          refuse(id, "face topology/binding changed");
        }
        structural.push_back({VmfStructuralEdit::Type::Remove, f.lineNumber() - 1, {}});
        continue;
      }
      const auto& g = *it->second;
      const auto side = f.lineNumber() - 1;
      const auto fu = f.takeUvCoordSystemSnapshot();
      const auto gu = g.takeUvCoordSystemSnapshot();
      if (!fu || !gu)
      {
        refuse(side, "non-Valve UV projection");
      }
      const auto uv = f.uvAttributes();
      const auto gv = g.uvAttributes();
      if (gv.scale.x() == 0.0f || gv.scale.y() == 0.0f)
      {
        refuse(side, "zero native UV scale");
      }
      if (f.surfaceAttributes() != g.surfaceAttributes())
      {
        refuse(side, "surface attribute editing unsupported");
      }
      const auto displacementChanged = f.sourceDisplacement() != g.sourceDisplacement();
      const auto changed =
        f.points() != g.points() || f.materialName() != g.materialName() || uv != gv
        || fu->uAxis != gu->uAxis || fu->vAxis != gu->vAxis || displacementChanged;
      if (!changed)
      {
        continue;
      }
      if (opaque(d, id, &displacementSides, &polygonBlocks))
      {
        refuse(id, "opaque attachment prevents safe brush edit");
      }
      if (protectExistingGeometry)
      {
        checkGeometryReferences(id);
      }
      if (displacementChanged)
      {
        if (!f.sourceDisplacement() || !g.sourceDisplacement())
        {
          refuse(side, "displacement structure changed");
        }
        const auto displacementEdits =
          g.sourceDisplacement()->editsFrom(*f.sourceDisplacement(), structural);
        edits.insert(edits.end(), displacementEdits.begin(), displacementEdits.end());
      }
      if (f.points() != g.points())
      {
        edit(side, "plane", planeText(g));
      }
      if (f.materialName() != g.materialName())
      {
        edit(side, "material", g.materialName());
      }
      if (
        uv.offset.x() != gv.offset.x() || uv.scale.x() != gv.scale.x()
        || fu->uAxis != gu->uAxis)
      {
        edit(side, "uaxis", axisText(gu->uAxis, gv.offset.x(), gv.scale.x()));
      }
      if (
        uv.offset.y() != gv.offset.y() || uv.scale.y() != gv.scale.y()
        || fu->vAxis != gu->vAxis)
      {
        edit(side, "vaxis", axisText(gu->vAxis, gv.offset.y(), gv.scale.y()));
      }
      if (uv.rotation != gv.rotation)
      {
        edit(side, "rotation", number(gv.rotation));
      }
    }
  }
  else if (const auto* group = dynamic_cast<const GroupNode*>(before))
  {
    const auto* target = dynamic_cast<const GroupNode*>(after);
    if (
      !target || !m_nativeGroups.contains(before->lineNumber())
      || target->group().name() != group->group().name())
    {
      refuse(id, "native group rename unsupported");
    }
  }
  else
  {
    refuse(id, "unsupported scene object");
  }
}

std::string VmfScene::emitNode(
  const Node& node,
  const NodeMap& original,
  const NativeVisgroupEmissionSession& session) const
{
  const auto& d = m_document;
  const auto source = prototype(node.lineNumber());
  const auto generated = m_generated.find(node.lineNumber());
  const auto* entity = dynamic_cast<const EntityNode*>(&node);
  const auto* brush = dynamic_cast<const BrushNode*>(&node);
  const auto* group = dynamic_cast<const GroupNode*>(&node);
  const auto freshEditor = [&]() {
    auto fields = std::string{};
    if (
      const auto member = m_memberIds.find(node.lineNumber());
      member != m_memberIds.end())
    {
      auto values = std::vector<VmfValueEdit>{};
      auto structure = std::vector<VmfStructuralEdit>{};
      m_visgroups.emitMemberEdits(session, member->second, values, structure, fields);
      if (!values.empty() || !structure.empty())
      {
        refuse(0, "fresh member has source edits");
      }
    }
    if (const auto* parent = dynamic_cast<const GroupNode*>(node.parent()))
    {
      fields += "\"groupid\" " + quote(nativeId(*parent)) + "\n";
    }
    return fields.empty() ? std::string{} : "editor\n{\n" + fields + "}\n";
  };
  if (group)
  {
    if (!m_nativeGroups.contains(node.lineNumber()))
    {
      refuse(0, "foreign emitted group");
    }
    if (!source)
    {
      auto result = "group\n{\n\"id\" " + quote(nativeId(node)) + "\n";
      result += freshEditor();
      return result + "}\n";
    }
    auto values = std::vector<VmfValueEdit>{};
    auto structure = std::vector<VmfStructuralEdit>{};
    auto appended = std::map<VmfEntryId, std::string>{};
    if (generated != m_generated.end())
    {
      values.push_back({*property(d, *source, "id"), quote(nativeId(node))});
    }
    membershipEdits(*source, node, session, values, structure, appended);
    for (const auto& [block, entries] : appended)
    {
      structure.push_back({VmfStructuralEdit::Type::Append, block, entries});
    }
    return d.serializeEntry(*source, values, structure);
  }
  if (!entity && !brush)
  {
    refuse(0, "unsupported emitted native node");
  }
  if (
    generated != m_generated.end()
    && generated->second.kind != (entity ? GeneratedKind::Entity : GeneratedKind::Solid))
  {
    refuse(0, "generated native node kind mismatch");
  }
  const auto sideId = [&](const BrushFace& face) {
    const auto record = m_generated.find(face.lineNumber());
    if (generated != m_generated.end())
    {
      if (
        record == m_generated.end() || record->second.kind != GeneratedKind::Side
        || record->second.owner != node.lineNumber())
      {
        refuse(0, "foreign or missing generated side identity");
      }
      return record->second.nativeId;
    }
    if (record != m_generated.end())
    {
      if (
        record->second.kind != GeneratedKind::Side
        || record->second.owner != node.lineNumber() || record->second.prototype)
      {
        refuse(0, "foreign generated cut side identity");
      }
      return record->second.nativeId;
    }
    const auto side = prototype(face.lineNumber());
    if (!side || !source || d.entries()[*side].parent != source)
    {
      refuse(0, "native side provenance does not belong to emitted solid");
    }
    return value(d, *side, "id");
  };
  if (!source)
  {
    auto result = std::string{};
    if (entity)
    {
      result = freshEntityPrefix(entity->entity(), nativeId(node));
      for (const auto* child : node.children())
      {
        if (!dynamic_cast<const BrushNode*>(child))
        {
          refuse(0, "native entity contains unsupported child");
        }
        result += emitNode(*child, original, session);
      }
    }
    else
    {
      validateFreshData(node);
      validateSideState(*brush);
      auto sides = std::set<size_t>{};
      result = "solid\n{\n\"id\" " + quote(nativeId(node)) + "\n";
      for (const auto& face : brush->brush().faces())
      {
        if (!sides.insert(face.lineNumber()).second)
        {
          refuse(0, "duplicate fresh native side identity");
        }
        result += freshSideText(face, sideId(face));
      }
    }
    result += freshEditor();
    result += "}\n";
    static_cast<void>(VmfDocument::parse(result));
    return result;
  }
  const auto polygonBlocks = ownershipPolygonBlocks(
    m_document,
    original,
    node,
    [&](const size_t binding) { return prototype(binding); },
    [&](const BrushNode& brush) { validateSideState(brush); });
  if (opaque(d, *source, nullptr, &polygonBlocks))
  {
    refuse(*source, "opaque native source template cannot be duplicated/detached");
  }
  const auto before = original.find(*source + 1);
  if (before == original.end())
  {
    refuse(*source, "native template is not a projected object");
  }
  auto values = std::vector<VmfValueEdit>{};
  auto structure = std::vector<VmfStructuralEdit>{};
  auto appended = std::map<VmfEntryId, std::string>{};
  nodeEdits(
    *before->second, node, values, structure, appended, generated == m_generated.end());
  if (generated != m_generated.end())
  {
    values.push_back({*property(d, *source, "id"), quote(generated->second.nativeId)});
  }
  if (entity)
  {
    // A partial brush-entity duplicate keeps the native owner payload, not its
    // unselected solids. Hidden wrappers remain lossless but lose copied solids.
    const auto removeOldSolids = [&](auto&& self, const VmfEntryId block) -> void {
      for (const auto child : d.entries()[block].children)
      {
        if (d.entries()[child].isBlock())
        {
          const auto name = key(d, child);
          if (name == "solid")
          {
            structure.push_back({VmfStructuralEdit::Type::Remove, child, {}});
          }
          else if (name == "hidden")
          {
            self(self, child);
          }
        }
      }
    };
    removeOldSolids(removeOldSolids, *source);
    for (const auto* child : node.children())
    {
      if (!dynamic_cast<const BrushNode*>(child))
      {
        refuse(*source, "unsupported duplicated entity child");
      }
      appended[*source] += emitNode(*child, original, session) + "\n";
    }
  }
  else
  {
    for (const auto& face : brush->brush().faces())
    {
      const auto id = sideId(face);
      if (generated != m_generated.end())
      {
        const auto side = prototype(face.lineNumber());
        if (side)
        {
          values.push_back({*property(d, *side, "id"), quote(id)});
        }
      }
    }
  }
  membershipEdits(*source, node, session, values, structure, appended);
  for (const auto& [block, entries] : appended)
  {
    if (!entries.empty())
    {
      structure.push_back({VmfStructuralEdit::Type::Append, block, entries});
    }
  }
  return d.serializeEntry(*source, values, structure);
}

void VmfScene::membershipEdits(
  const VmfEntryId source,
  const Node& node,
  const NativeVisgroupEmissionSession& session,
  std::vector<VmfValueEdit>& values,
  std::vector<VmfStructuralEdit>& structure,
  std::map<VmfEntryId, std::string>& appended) const
{
  if (m_unsupportedGroupObjects.contains(source + 1))
  {
    refuse(source, "unresolved native group membership cannot be changed");
  }
  const auto* parent = dynamic_cast<const GroupNode*>(node.parent());
  const auto desired = parent ? nativeId(*parent) : std::string{};
  if (parent && !m_nativeGroups.contains(parent->lineNumber()))
  {
    refuse(source, "foreign native group membership");
  }
  auto editors = std::vector<VmfEntryId>{};
  for (const auto child : m_document.entries()[source].children)
  {
    if (m_document.entries()[child].isBlock() && key(m_document, child) == "editor")
    {
      editors.push_back(child);
    }
  }
  if (editors.size() > 1)
  {
    refuse(source, "ambiguous native editor membership");
  }
  auto fields = std::vector<VmfEntryId>{};
  if (!editors.empty())
  {
    for (const auto child : m_document.entries()[editors.front()].children)
    {
      if (key(m_document, child) == "groupid")
      {
        fields.push_back(child);
      }
    }
  }
  if (fields.size() > 1)
  {
    refuse(source, "ambiguous native group membership");
  }
  auto added = std::string{};
  if (
    const auto member = m_memberIds.find(node.lineNumber()); member != m_memberIds.end())
  {
    m_visgroups.emitMemberEdits(session, member->second, values, structure, added);
  }
  if (!fields.empty())
  {
    const auto field = fields.front();
    if (!m_document.entries()[field].value)
    {
      refuse(source, "invalid native membership");
    }
    const auto old = text(m_document, *m_document.entries()[field].value);
    if (desired.empty())
    {
      structure.push_back({VmfStructuralEdit::Type::Remove, field, {}});
    }
    else if (normalizedId(old) != normalizedId(desired))
    {
      values.push_back({field, quote(desired)});
    }
  }
  else if (!desired.empty())
  {
    added += "\"groupid\" " + quote(desired) + "\n";
  }
  if (!added.empty())
  {
    if (editors.empty())
    {
      appended[source] += "editor\n{\n" + added + "}\n";
    }
    else
    {
      appended[editors.front()] += added;
    }
  }
}

std::string VmfScene::serialize(const WorldNode& world) const
{
  const auto& d = m_document;
  const auto session = m_visgroups.beginEmission(d);
  auto original = NodeMap{};
  auto current = NodeMap{};
  collect(*m_original, original);
  collect(world, current);
  for (const auto& [binding, expected] : m_memberNodes)
  {
    if (const auto found = current.find(binding); found != current.end())
    {
      if (found->second != expected)
      {
        refuse(0, "native member replaced without registration");
      }
      m_pendingMembers.erase(binding);
    }
    else if (
      m_generated.contains(binding) && !m_pendingMembers.contains(binding)
      && !m_removals.contains(binding))
    {
      refuse(0, "generated native member detached without registered removal");
    }
  }
  if (
    world.lineNumber() != m_original->lineNumber() || world.childCount() != 1
    || world.children().front() != world.defaultLayer())
  {
    refuse(0, "native world/layer structure changed");
  }
  for (const auto& [binding, node] : current)
  {
    if (const auto* container = dynamic_cast<const GroupNode*>(node))
    {
      const auto admitted = m_groupNodes.find(binding);
      const auto link = m_groupLinks.find(binding);
      if (
        admitted == m_groupNodes.end() || admitted->second != container
        || link == m_groupLinks.end() || link->second != container->linkId())
      {
        refuse(0, "foreign native group container identity");
      }
    }
    validateVisgroupProjection(*node);
    if (m_removals.contains(binding))
    {
      refuse(0, "removed native member requires explicit activation");
    }
    const auto* group = dynamic_cast<const GroupNode*>(node->parent());
    const auto membership = group ? group->lineNumber() : 0;
    const auto admitted = m_groupStates.find(binding);
    if (admitted == m_groupStates.end() || !admitted->second.contains(membership))
    {
      refuse(0, "unadmitted native editor membership");
    }
    if (
      const auto* brush = dynamic_cast<const BrushNode*>(node);
      brush && (m_generated.contains(binding) || m_clipStates.contains(binding)))
    {
      validateClipOwner(*brush, false, false);
    }
  }
  auto edits = std::vector<VmfValueEdit>{};
  auto structural = std::vector<VmfStructuralEdit>{};
  auto appended = std::map<VmfEntryId, std::string>{};
  auto removed = std::set<VmfEntryId>{};
  auto removalClosure = std::vector<Node*>{};
  const auto hidden = [&](const size_t binding) {
    const auto member = m_memberIds.find(binding);
    if (member == m_memberIds.end())
    {
      return false;
    }
    const auto* m = &m_visgroups.member(member->second);
    return m->wrapperHidden || m->manualShown == false || m->autoShown == false;
  };
  for (const auto& [binding, before] : original)
  {
    if (!current.contains(binding))
    {
      removed.insert(binding - 1);
      const auto witness = m_removals.find(binding);
      if (witness != m_removals.end())
      {
        if (witness->second != m_visgroups.member(m_memberIds.at(binding)))
        {
          refuse(0, "removed native member differs from admitted live metadata");
        }
      }
      else
      {
        for (auto* ancestor = before; ancestor; ancestor = ancestor->parent())
        {
          if (hidden(ancestor->lineNumber()))
          {
            refuse(0, "unwitnessed hidden native detach");
          }
        }
        if (
          const auto states = m_groupStates.find(binding); states != m_groupStates.end())
        {
          for (const auto group : states->second)
          {
            if (hidden(group))
            {
              refuse(0, "unwitnessed hidden native group detach");
            }
          }
        }
        removalClosure.push_back(const_cast<Node*>(before));
      }
    }
  }
  validateRemoval(removalClosure);
  for (const auto id : removed)
  {
    auto ancestorRemoved = false;
    for (auto parent = d.entries()[id].parent; parent;
         parent = d.entries()[*parent].parent)
    {
      ancestorRemoved = ancestorRemoved || removed.contains(*parent);
    }
    if (!ancestorRemoved)
    {
      structural.push_back({VmfStructuralEdit::Type::Remove, id, {}});
    }
  }
  for (const auto& [binding, before] : original)
  {
    const auto found = current.find(binding);
    if (found == current.end())
    {
      continue;
    }
    const auto* after = found->second;
    const auto id = binding - 1;
    if (parentBinding(*before) != parentBinding(*after))
    {
      if (!after->parent())
      {
        refuse(id, "missing native parent");
      }
      validateReparentImpl(*after->parent(), {const_cast<Node*>(after)}, false);
      structural.push_back({VmfStructuralEdit::Type::Remove, id, {}});
      // The new generated entity emits this same native solid (same side IDs)
      // from its source template. Do not leave overlapping edits at the old span.
      continue;
    }
    nodeEdits(*before, *after, edits, structural, appended, true);
    const auto* oldGroup = dynamic_cast<const GroupNode*>(before->parent());
    const auto* newGroup = dynamic_cast<const GroupNode*>(after->parent());
    if (m_memberIds.contains(binding) && (!m_unsupportedGroupObjects.contains(binding)
      || (oldGroup ? oldGroup->lineNumber() : 0) != (newGroup ? newGroup->lineNumber() : 0)))
    {
      membershipEdits(id, *after, session, edits, structural, appended);
    }
  }
  auto roots = std::string{};
  for (const auto& [binding, node] : current)
  {
    if (original.contains(binding))
    {
      continue;
    }
    const auto record = m_generated.find(binding);
    if (record == m_generated.end() || record->second.kind == GeneratedKind::Side)
    {
      refuse(0, "unregistered native object");
    }
    const auto parent = parentBinding(*node);
    if (!current.contains(parent))
    {
      refuse(0, "unknown generated native parent");
    }
    if (m_generated.contains(parent))
    {
      if (
        !dynamic_cast<const EntityNode*>(current.at(parent))
        || !dynamic_cast<const BrushNode*>(node))
      {
        refuse(0, "unsupported generated hierarchy");
      }
      continue;
    }
    const auto text = emitNode(*node, original, session);
    if (dynamic_cast<const EntityNode*>(node))
    {
      if (parent != world.lineNumber())
      {
        refuse(0, "native entities must remain at world level");
      }
      roots += text + "\n";
    }
    else if (dynamic_cast<const GroupNode*>(node))
    {
      if (!m_nativeGroups.contains(binding) || parent != world.lineNumber())
      {
        refuse(0, "invalid generated native group owner");
      }
      appended[world.lineNumber() - 1] += text + "\n";
    }
    else if (dynamic_cast<const BrushNode*>(node))
    {
      const auto* owner = current.at(parent);
      if (
        !dynamic_cast<const WorldNode*>(owner) && !dynamic_cast<const EntityNode*>(owner))
      {
        refuse(0, "unsupported native brush owner");
      }
      if (dynamic_cast<const EntityNode*>(owner))
      {
        const auto polygonBlocks = ownershipPolygonBlocks(
          d,
          original,
          *owner,
          [&](const size_t binding) { return prototype(binding); },
          [&](const BrushNode& brush) { validateSideState(brush); });
        if (opaque(d, parent - 1, nullptr, &polygonBlocks))
        {
          refuse(parent - 1, "opaque brush-entity attachment prevents addition");
        }
      }
      appended[parent - 1] += text + "\n";
    }
    else
    {
      refuse(0, "unsupported generated object type");
    }
  }
  m_visgroups.emitTreeEdits(d, edits, structural, roots);
  m_cordons.emitEdits(edits, structural, roots);
  m_cameras.emitEdits(edits, structural, roots);
  if (!roots.empty())
  {
    structural.push_back(
      {VmfStructuralEdit::Type::AppendRoot, VmfStructuralEdit::Root, roots});
  }
  for (const auto& [block, entries] : appended)
  {
    structural.push_back({VmfStructuralEdit::Type::Append, block, entries});
  }
  if (!edits.empty() || !structural.empty())
  {
    requireEditing();
    requireEditableVersion(d);
  }
  return d.serialize(edits, structural);
}
} // namespace tb::mdl
