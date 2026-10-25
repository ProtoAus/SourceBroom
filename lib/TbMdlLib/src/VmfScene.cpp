/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/VmfScene.h"

#include "mdl/Brush.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/EntityNode.h"
#include "mdl/LayerNode.h"
#include "mdl/MapFormat.h"
#include "mdl/NodeContents.h"
#include "mdl/SourceDisplacement.h"

#include <algorithm>
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

bool opaque(
  const VmfDocument& d,
  VmfEntryId id,
  const std::set<VmfEntryId>* displacementSides = nullptr)
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
      if (name == "solid" || name == "side" || name == "hidden")
      {
        if (opaque(d, child, displacementSides))
        {
          return true;
        }
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
size_t parentBinding(const Node& node)
{
  const auto* p = node.parent();
  while (p && dynamic_cast<const LayerNode*>(p))
  {
    p = p->parent();
  }
  return p ? p->lineNumber() : 0;
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
std::optional<std::string> geometryProblem(const VmfDocument& d, const WorldNode& world)
{
  for (const auto child : d.entries()[world.lineNumber() - 1].children)
  {
    if (d.entries()[child].isBlock())
    {
      const auto name = key(d, child);
      if (name != "solid" && name != "hidden" && name != "editor")
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
  for (const auto root : d.roots())
  {
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

VmfScene::VmfScene(VmfDocument document)
  : m_document{std::move(document)}
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
    if (
      name == "world" || name == "entity" || name == "solid" || name == "side"
      || name == "group")
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
        node->setVisibilityState(VisibilityState::Hidden);
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
          node->setVisibilityState(VisibilityState::Hidden);
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
  if (m_geometryReferenceProblem)
  {
    refuse(object, *m_geometryReferenceProblem);
  }
}
void VmfScene::validateDuplication(const std::vector<Node*>& nodes) const
{
  requireEditing();
  if (!nodes.empty())
  {
    requireEditableVersion(m_document);
  }
  auto original = NodeMap{};
  collect(*m_original, original);
  const auto check = [&](auto&& self, const Node& node) -> void {
    if (
      (!dynamic_cast<const EntityNode*>(&node) && !dynamic_cast<const BrushNode*>(&node))
      || node.visibilityState() != VisibilityState::Inherited)
    {
      refuse(0, "native duplication does not support groups/layers/hidden objects");
    }
    const auto source = prototype(node.lineNumber());
    if (source && opaque(m_document, *source))
    {
      refuse(
        *source, "opaque/displacement/instance attachments prevent native duplication");
    }
    static_cast<void>(emitNode(node, original));
    for (const auto* child : node.children())
    {
      self(self, *child);
    }
  };
  for (const auto* node : nodes)
  {
    check(check, *node);
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
  const auto* owner = &parent;
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
    if (
      (source && opaque(m_document, *source))
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
    return;
  }
  const auto check = [&](auto&& self, const Node& node) -> void {
    if (node.lineNumber() || node.visibilityState() != VisibilityState::Inherited)
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
void VmfScene::registerAdditions(const std::vector<Node*>& nodes, const bool duplication)
{
  requireEditing();
  struct Assignment
  {
    Node* node;
    const BrushFace* face;
    size_t binding;
  };
  auto additions = std::map<size_t, GeneratedBinding>{};
  auto assignments = std::vector<Assignment>{};
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
  const auto registerNode = [&](auto&& self, Node& node) -> void {
    const auto source = duplication ? prototype(node.lineNumber()) : std::nullopt;
    const auto kind =
      dynamic_cast<EntityNode*>(&node) ? GeneratedKind::Entity : GeneratedKind::Solid;
    const auto allocated = allocate(kind, source, 0);
    assignments.push_back({&node, nullptr, allocated});
    if (const auto* brush = dynamic_cast<const BrushNode*>(&node))
    {
      additions.at(allocated).sideCount = brush->brush().faceCount();
      for (const auto& face : brush->brush().faces())
      {
        const auto originalSide =
          duplication ? prototype(face.lineNumber()) : std::nullopt;
        if (
          source
          && (!originalSide || m_document.entries()[*originalSide].parent != source))
        {
          refuse(*source, "duplicate side provenance does not belong to native solid");
        }
        const auto side = allocate(GeneratedKind::Side, originalSide, allocated);
        assignments.push_back({nullptr, &face, side});
      }
    }
    for (auto* child : node.children())
    {
      self(self, *child);
    }
  };
  for (auto* node : nodes)
  {
    registerNode(registerNode, *node);
  }
  // All fallible validation/allocation precedes identity publication. Detached
  // command-owned nodes keep these bindings when undo/redo parks/restores them.
  m_generated.merge(additions);
  m_nextBinding = binding;
  m_nextObjectId = std::move(objectId);
  m_nextSideId = std::move(sideId);
  m_generatedIdBytes = idBytes;
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
  const auto check = [&](auto&& self, const Node& node) -> void {
    if (!dynamic_cast<const EntityNode*>(&node) && !dynamic_cast<const BrushNode*>(&node))
    {
      refuse(0, "native world/groups/layers cannot be removed");
    }
    const auto source = prototype(node.lineNumber());
    if (source && opaque(m_document, *source))
    {
      refuse(*source, "opaque/displacement/instance attachments prevent native removal");
    }
    if (dynamic_cast<const BrushNode*>(&node) && !m_generated.contains(node.lineNumber()))
    {
      checkGeometryReferences(*source);
    }
    for (const auto* child : node.children())
    {
      self(self, *child);
    }
  };
  for (const auto* node : removalClosure)
  {
    check(check, *node);
  }
}
void VmfScene::validateReparent(const Node& parent, const std::vector<Node*>& nodes) const
{
  requireEditing();
  if (!nodes.empty())
  {
    requireEditableVersion(m_document);
  }
  const auto target = m_generated.find(parent.lineNumber());
  if (
    !dynamic_cast<const EntityNode*>(&parent) || target == m_generated.end()
    || target->second.kind != GeneratedKind::Entity)
  {
    refuse(
      0, "native reparenting only supports wrapping plain brushes in a new brush entity");
  }
  if (target->second.prototype && opaque(m_document, *target->second.prototype))
  {
    refuse(*target->second.prototype, "opaque new owner prevents native wrapping");
  }
  auto original = NodeMap{};
  collect(*m_original, original);
  for (const auto* node : nodes)
  {
    if (
      !dynamic_cast<const BrushNode*>(node)
      || node->visibilityState() != VisibilityState::Inherited)
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
void VmfScene::validateContents(
  const Node& node, const NodeContents& contents, const vm::bbox3d& worldBounds) const
{
  requireEditing();
  requireEditableVersion(m_document);
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
    static_cast<void>(emitNode(*copy, original));
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
    if (b.faceCount() != a.faceCount())
    {
      refuse(id, "brush topology changed");
    }
    auto displacementSides = std::set<VmfEntryId>{};
    for (const auto& face : b.faces())
    {
      if (face.sourceDisplacement())
      {
        displacementSides.insert(face.lineNumber() - 1);
      }
    }
    auto sides = std::map<size_t, const BrushFace*>{};
    for (const auto& f : a.faces())
    {
      const auto source = prototype(f.lineNumber());
      const auto generatedNode = m_generated.find(after->lineNumber());
      const auto generatedSide = m_generated.find(f.lineNumber());
      if (generatedNode != m_generated.end())
      {
        if (
          generatedSide == m_generated.end()
          || generatedSide->second.kind != GeneratedKind::Side
          || generatedSide->second.owner != after->lineNumber())
        {
          refuse(id, "foreign generated side identity");
        }
      }
      else if (generatedSide != m_generated.end())
      {
        refuse(id, "existing native solid acquired generated side identity");
      }
      if (!source || !sides.emplace(*source + 1, &f).second)
      {
        refuse(id, "duplicate face binding");
      }
    }
    for (const auto& f : b.faces())
    {
      const auto it = sides.find(f.lineNumber());
      if (it == sides.end())
      {
        refuse(id, "face topology/binding changed");
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
      if (opaque(d, id, &displacementSides))
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
          g.sourceDisplacement()->editsFrom(*f.sourceDisplacement());
        edits.insert(edits.end(), displacementEdits.begin(), displacementEdits.end());
      }
      if (f.points() != g.points())
      {
        if (std::any_of(b.faces().begin(), b.faces().end(), [](const auto& face) {
              return bool(face.sourceDisplacement());
            }))
        {
          refuse(side, "displacement base transform is not yet supported");
        }
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
  else
  {
    refuse(id, "unsupported scene object");
  }
}

std::string VmfScene::emitNode(const Node& node, const NodeMap& original) const
{
  const auto& d = m_document;
  const auto source = prototype(node.lineNumber());
  const auto generated = m_generated.find(node.lineNumber());
  const auto* entity = dynamic_cast<const EntityNode*>(&node);
  const auto* brush = dynamic_cast<const BrushNode*>(&node);
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
      refuse(0, "existing native solid acquired generated side identity");
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
        result += emitNode(*child, original);
      }
    }
    else
    {
      validateFreshData(node);
      if (
        generated == m_generated.end()
        || brush->brush().faceCount() != generated->second.sideCount)
      {
        refuse(0, "fresh native brush topology changed");
      }
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
    result += "}\n";
    static_cast<void>(VmfDocument::parse(result));
    return result;
  }
  if (opaque(d, *source))
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
      appended[*source] += emitNode(*child, original) + "\n";
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
        if (!side || d.entries()[*side].parent != source)
        {
          refuse(*source, "generated side lost its native template");
        }
        values.push_back({*property(d, *side, "id"), quote(id)});
      }
    }
  }
  for (const auto& [block, entries] : appended)
  {
    if (!entries.empty())
    {
      structure.push_back({VmfStructuralEdit::Type::Append, block, entries});
    }
  }
  return d.serializeEntry(*source, values, structure);
}

std::string VmfScene::serialize(const WorldNode& world) const
{
  const auto& d = m_document;
  auto original = NodeMap{};
  auto current = NodeMap{};
  collect(*m_original, original);
  collect(world, current);
  if (
    world.lineNumber() != m_original->lineNumber() || world.childCount() != 1
    || world.children().front() != world.defaultLayer()
    || world.defaultLayer()->visibilityState()
         != m_original->defaultLayer()->visibilityState())
  {
    refuse(0, "native world/layer structure changed");
  }
  auto edits = std::vector<VmfValueEdit>{};
  auto structural = std::vector<VmfStructuralEdit>{};
  auto appended = std::map<VmfEntryId, std::string>{};
  auto removed = std::set<VmfEntryId>{};
  auto removalClosure = std::vector<Node*>{};
  for (const auto& [binding, before] : original)
  {
    if (!current.contains(binding))
    {
      removed.insert(binding - 1);
      removalClosure.push_back(const_cast<Node*>(before));
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
    if (before->visibilityState() != after->visibilityState())
    {
      refuse(id, "changing native hidden state unsupported");
    }
    if (parentBinding(*before) != parentBinding(*after))
    {
      if (!after->parent())
      {
        refuse(id, "missing native parent");
      }
      validateReparent(*after->parent(), {const_cast<Node*>(after)});
      structural.push_back({VmfStructuralEdit::Type::Remove, id, {}});
      // The new generated entity emits this same native solid (same side IDs)
      // from its source template. Do not leave overlapping edits at the old span.
      continue;
    }
    nodeEdits(*before, *after, edits, structural, appended, true);
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
    if (node->visibilityState() != VisibilityState::Inherited)
    {
      refuse(0, "authoring native hidden state unsupported");
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
    const auto text = emitNode(*node, original);
    if (dynamic_cast<const EntityNode*>(node))
    {
      if (parent != world.lineNumber())
      {
        refuse(0, "native entities must remain at world level");
      }
      roots += text + "\n";
    }
    else if (dynamic_cast<const BrushNode*>(node))
    {
      const auto* owner = current.at(parent);
      if (
        !dynamic_cast<const WorldNode*>(owner) && !dynamic_cast<const EntityNode*>(owner))
      {
        refuse(0, "unsupported native brush owner");
      }
      if (dynamic_cast<const EntityNode*>(owner) && opaque(d, parent - 1))
      {
        refuse(parent - 1, "opaque brush-entity attachment prevents addition");
      }
      appended[parent - 1] += text + "\n";
    }
    else
    {
      refuse(0, "unsupported generated object type");
    }
  }
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
