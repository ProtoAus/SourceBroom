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
bool referencesGeometry(const VmfDocument& d, VmfEntryId id)
{
  if (key(d, id) == "entity")
  {
    const auto classname = property(d, id, "classname");
    if (classname)
    {
      const auto name = text(d, *d.entries()[*classname].value);
      if (
        name == "info_overlay" || name == "info_overlay_transition"
        || name == "func_instance" || name == "func_instance_parms")
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

bool opaque(const VmfDocument& d, VmfEntryId id)
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
        if (opaque(d, child))
        {
          return true;
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
} // namespace

VmfScene::VmfScene(VmfDocument document)
  : m_document{std::move(document)}
{
}
VmfScene::VmfScene(VmfScene&&) noexcept = default;
VmfScene& VmfScene::operator=(VmfScene&&) noexcept = default;
VmfScene::~VmfScene() = default;

VmfScene VmfScene::read(
  std::string source, const vm::bbox3d& bounds, EntityPropertyConfig config)
{
  auto scene = VmfScene{VmfDocument::parse(std::move(source))};
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
        refuse(id, "duplicate native ID within object/face namespace");
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
     "Restricted native editing: existing plain brush values, entity properties and "
     "ordered outputs; no object creation/deletion, reparenting, hidden-state editing, "
     "or opaque transforms. "
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
      for (const auto child : d.entries()[id].children)
      {
        if (d.entries()[child].isBlock())
        {
          self(self, child, *node, hidden);
        }
      }
      parent.addChild(node.release());
    }
    else if (name == "solid")
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
      auto node = std::make_unique<BrushNode>(std::move(brush));
      node->setFilePosition(id + 1, 1);
      if (hidden)
      {
        node->setVisibilityState(VisibilityState::Hidden);
      }
      parent.addChild(node.release());
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
  return scene;
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
std::string VmfScene::nativeId(const Node& node) const
{
  if (!node.lineNumber() || node.lineNumber() > m_document.entries().size())
  {
    refuse(0, "missing native binding");
  }
  return value(m_document, node.lineNumber() - 1, "id");
}

std::string VmfScene::serialize(const WorldNode& world) const
{
  const auto& d = m_document;
  auto original = NodeMap{};
  auto current = NodeMap{};
  collect(*m_original, original);
  collect(world, current);
  if (original.size() != current.size())
  {
    refuse(0, "object addition/deletion unsupported");
  }
  auto edits = std::vector<VmfValueEdit>{};
  auto structural = std::vector<VmfStructuralEdit>{};
  auto appended = std::map<VmfEntryId, std::string>{};
  const auto edit =
    [&](VmfEntryId block, const std::string& name, std::string replacement) {
      const auto id = property(d, block, name);
      if (!id)
      {
        refuse(block, "adding property " + name + " unsupported");
      }
      edits.push_back({*id, quote(replacement)});
    };
  for (const auto& [binding, before] : original)
  {
    const auto found = current.find(binding);
    if (found == current.end())
    {
      refuse(binding - 1, "lost native binding");
    }
    const auto* after = found->second;
    const auto id = binding - 1;
    if (parentBinding(*before) != parentBinding(*after))
    {
      refuse(id, "reparenting unsupported");
    }
    if (before->visibilityState() != after->visibilityState())
    {
      refuse(id, "changing native hidden state unsupported");
    }
    if (const auto* eb = dynamic_cast<const EntityNodeBase*>(before))
    {
      const auto* ea = dynamic_cast<const EntityNodeBase*>(after);
      if (!ea)
      {
        refuse(id, "object type changed");
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
      auto sides = std::map<size_t, const BrushFace*>{};
      for (const auto& f : a.faces())
      {
        if (!sides.emplace(f.lineNumber(), &f).second)
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
        if (f.surfaceAttributes() != g.surfaceAttributes())
        {
          refuse(side, "surface attribute editing unsupported");
        }
        const auto changed = f.points() != g.points()
                             || f.materialName() != g.materialName() || uv != gv
                             || fu->uAxis != gu->uAxis || fu->vAxis != gu->vAxis;
        if (!changed)
        {
          continue;
        }
        if (opaque(d, id))
        {
          refuse(id, "opaque/displacement attachment prevents safe brush edit");
        }
        // Other solids' displacement/vertices data and standard entity I/O do
        // not move with this brush. IDs/target names remain unchanged. Unknown
        // global attachments still fail closed because their references are unknown.
        for (const auto child : d.entries()[m_original->lineNumber() - 1].children)
        {
          if (d.entries()[child].isBlock())
          {
            const auto name = key(d, child);
            if (name != "solid" && name != "hidden" && name != "editor")
            {
              refuse(id, "unknown world attachment prevents safe brush edit");
            }
          }
        }
        for (const auto& [otherBinding, other] : original)
        {
          if (dynamic_cast<const EntityNode*>(other))
          {
            if (referencesGeometry(d, otherBinding - 1))
            {
              refuse(id, "overlay/instance references prevent safe brush edit");
            }
            for (const auto child : d.entries()[otherBinding - 1].children)
            {
              if (d.entries()[child].isBlock())
              {
                const auto name = key(d, child);
                if (
                  name != "solid" && name != "editor"
                  && !supportedConnections(d, otherBinding - 1, child))
                {
                  refuse(
                    id,
                    "unknown entity attachment may reference this brush; edit refused");
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
              name != "world" && name != "entity" && name != "hidden"
              && name != "versioninfo" && name != "visgroups" && name != "viewsettings"
              && name != "cameras" && name != "cordons")
            {
              refuse(id, "unknown root block prevents safe brush edit");
            }
          }
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
    else
    {
      refuse(id, "unsupported scene object");
    }
  }
  for (const auto& [block, entries] : appended)
  {
    structural.push_back({VmfStructuralEdit::Type::Append, block, entries});
  }
  if (!edits.empty() || !structural.empty())
  {
    for (const auto root : d.roots())
    {
      if (d.entries()[root].isBlock() && key(d, root) == "versioninfo")
      {
        const auto format = property(d, root, "formatversion");
        if (!format || text(d, *d.entries()[*format].value) != "100")
        {
          refuse(root, "native edits require recognized VMF formatversion 100");
        }
      }
    }
  }
  return d.serialize(edits, structural);
}
} // namespace tb::mdl
