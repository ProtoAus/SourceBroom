/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/NativeCordons.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace tb::mdl
{
namespace
{
[[noreturn]] void reject(const std::string& reason)
{
  throw std::invalid_argument{"Native cordons: " + reason};
}
bool same(const NativeCordonSnapshot& a, const NativeCordonSnapshot& b)
{
  return a.form == b.form && a.active == b.active && a.records == b.records;
}
std::string decode(const VmfDocument& d, const VmfToken& t)
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
std::string quote(const std::string& text)
{
  auto result = std::string{"\""};
  for (const auto c : text)
  {
    if (c == '\\' || c == '"')
    {
      result += '\\';
    }
    result += c;
  }
  return result + '"';
}
void validBounds(const NativeCordonBox& box)
{
  for (size_t i = 0; i < 3; ++i)
  {
    if (
      !std::isfinite(box.mins[i]) || !std::isfinite(box.maxs[i])
      || !(box.maxs[i] > box.mins[i]) || !std::isfinite(box.maxs[i] - box.mins[i]))
    {
      reject("bounds require ordered finite coordinates and positive finite extents");
    }
  }
}
vm::vec3d point(const std::string& text)
{
  auto input = std::istringstream{text};
  input.imbue(std::locale::classic());
  input >> std::ws;
  const auto wrapped = input.peek() == '(';
  if (wrapped)
  {
    input.get();
  }
  auto result = vm::vec3d{};
  for (size_t i = 0; i < 3; ++i)
  {
    if (!(input >> result[i]) || !std::isfinite(result[i]))
    {
      reject("invalid bounds tuple");
    }
  }
  if (wrapped)
  {
    auto c = char{};
    if (!(input >> c) || c != ')')
    {
      reject("invalid bounds tuple");
    }
  }
  input >> std::ws;
  if (!input.eof())
  {
    reject("invalid bounds tuple");
  }
  return result;
}
std::string tuple(const vm::vec3d& p)
{
  auto out = std::ostringstream{};
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<double>::max_digits10) << '(' << p[0]
      << ' ' << p[1] << ' ' << p[2] << ')';
  return out.str();
}
std::string boxText(const NativeCordonBox& b)
{
  return "box\n{\n\"mins\" " + quote(tuple(b.mins)) + "\n\"maxs\" " + quote(tuple(b.maxs))
         + "\n}\n";
}
std::string optionalText(const std::string& key, const std::optional<std::string>& value)
{
  return value ? quote(key) + " " + quote(*value) + "\n" : "";
}
std::optional<std::string> flagText(const std::optional<bool> flag)
{
  return flag ? std::optional<std::string>{*flag ? "1" : "0"} : std::nullopt;
}
std::string recordText(const NativeCordonRecord& r)
{
  auto text = "cordon\n{\n" + optionalText("name", r.name)
              + optionalText("active", flagText(r.active));
  for (const auto& b : r.boxes)
  {
    text += boxText(b);
  }
  return text + "}\n";
}
template <typename T, typename Id>
const T* findId(const std::vector<T>& items, const Id& id)
{
  const auto it =
    std::find_if(items.begin(), items.end(), [&](const auto& x) { return x.id == id; });
  return it == items.end() ? nullptr : &*it;
}
} // namespace

struct PreparedNativeCordonChange::Data
{
  NativeCordonSnapshot before;
  NativeCordonSnapshot after;
  uint64_t beforeController = 0;
  uint64_t afterController = 0;
  uint64_t firstSerial = 0;
  uint64_t nextSerial = 0;
  // Only the first publication consumes the staged range. Redo restores it;
  // a different abandoned preparation must never reuse those identities.
  bool published = false;
};
PreparedNativeCordonChange::PreparedNativeCordonChange(std::shared_ptr<Data> data)
  : m_data{std::move(data)}
{
}
const NativeCordonSnapshot& PreparedNativeCordonChange::before() const
{
  if (!m_data)
  {
    reject("empty prepared change");
  }
  return m_data->before;
}
const NativeCordonSnapshot& PreparedNativeCordonChange::after() const
{
  if (!m_data)
  {
    reject("empty prepared change");
  }
  return m_data->after;
}

struct NativeCordons::Impl
{
  struct Provenance
  {
    VmfEntryId entry;
    std::map<std::string, VmfEntryId> scalars;
  };
  NativeCordonSnapshot base;
  NativeCordonSnapshot live;
  std::optional<Provenance> root;
  std::map<uint64_t, Provenance> records;
  std::map<uint64_t, Provenance> boxes;
  uint64_t controller = 0;
  uint64_t baseController = 0;
  uint64_t next = 1;

  Provenance shape(
    const VmfDocument& d,
    const VmfEntryId entry,
    const std::vector<std::string>& scalarNames,
    const std::string& blockName)
  {
    if (!d.entries()[entry].isBlock())
    {
      reject("controller/record must be a block");
    }
    auto result = Provenance{entry, {}};
    for (const auto child : d.entries()[entry].children)
    {
      const auto name = decode(d, d.entries()[child].key);
      if (!blockName.empty() && name == blockName && d.entries()[child].isBlock())
      {
        continue;
      }
      if (
        d.entries()[child].isBlock()
        || std::find(scalarNames.begin(), scalarNames.end(), name) == scalarNames.end()
        || !result.scalars.emplace(name, child).second)
      {
        reject("unknown, repeated or mis-shaped controller field");
      }
    }
    return result;
  }
  std::optional<std::string> scalar(
    const VmfDocument& d, const Provenance& p, const std::string& name)
  {
    const auto it = p.scalars.find(name);
    if (it != p.scalars.end() && d.entries()[it->second].value->length > MaxTextBytes)
    {
      reject("scalar text limit exceeded");
    }
    return it == p.scalars.end()
             ? std::nullopt
             : std::optional<std::string>{decode(d, *d.entries()[it->second].value)};
  }
  std::optional<bool> flag(const VmfDocument& d, const Provenance& p)
  {
    const auto value = scalar(d, p, "active");
    if (!value)
    {
      return std::nullopt;
    }
    if (*value != "0" && *value != "1")
    {
      reject("activation must be 0 or 1");
    }
    return *value == "1";
  }
  NativeCordonBox readBox(const VmfDocument& d, const Provenance& p)
  {
    if (boxes.size() >= MaxBoxes)
    {
      reject("box limit exceeded");
    }
    const auto lo = scalar(d, p, "mins");
    const auto hi = scalar(d, p, "maxs");
    if (!lo || !hi)
    {
      reject("partial bounds");
    }
    auto box =
      NativeCordonBox{NativeCordonBoxId{base.m_scope, next++}, point(*lo), point(*hi)};
    validBounds(box);
    boxes.emplace(box.id.m_serial, p);
    return box;
  }
  NativeCordonRecord readRecord(
    const VmfDocument& d, const VmfEntryId entry, const bool legacy)
  {
    const auto p = shape(
      d,
      entry,
      legacy ? std::vector<std::string>{"name", "active", "mins", "maxs"}
             : std::vector<std::string>{"name", "active"},
      legacy ? "" : "box");
    auto r = NativeCordonRecord{
      NativeCordonId{base.m_scope, next++}, scalar(d, p, "name"), flag(d, p), {}};
    if (r.name && r.name->size() > MaxNameBytes)
    {
      reject("name byte limit exceeded");
    }
    records.emplace(r.id.m_serial, p);
    if (legacy)
    {
      r.boxes.push_back(readBox(d, p));
    }
    else
    {
      for (const auto child : d.entries()[entry].children)
      {
        if (d.entries()[child].isBlock())
        {
          r.boxes.push_back(readBox(d, shape(d, child, {"mins", "maxs"}, "")));
        }
      }
    }
    if (r.boxes.empty())
    {
      reject("cordon requires at least one box");
    }
    return r;
  }
  void limits(const NativeCordonSnapshot& s) const
  {
    if (s.records.size() > MaxRecords)
    {
      reject("record limit exceeded");
    }
    size_t count = 0;
    size_t text = 0;
    for (const auto& r : s.records)
    {
      if (r.name)
      {
        if (r.name->size() > MaxNameBytes)
        {
          reject("name byte limit exceeded");
        }
        for (const auto c : *r.name)
        {
          if (static_cast<unsigned char>(c) < 32 || c == 127)
          {
            reject("control character in name");
          }
        }
        text += r.name->size();
      }
      count += r.boxes.size();
      if (count > MaxBoxes || count + s.records.size() > MaxBatchItems)
      {
        reject("box/batch limit exceeded");
      }
      if (text > MaxTextBytes)
      {
        reject("text limit exceeded");
      }
      for (const auto& b : r.boxes)
      {
        validBounds(b);
      }
    }
  }
};

NativeCordons::NativeCordons(const VmfDocument& d)
  : m_impl{std::make_unique<Impl>()}
{
  auto& s = *m_impl;
  s.base.m_scope = std::make_shared<const int>(0);
  try
  {
    for (const auto entry : d.roots())
    {
      const auto name = decode(d, d.entries()[entry].key);
      auto lower = name;
      std::transform(
        lower.begin(), lower.end(), lower.begin(), [](const unsigned char c) {
          return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : char(c);
        });
      if (lower != "cordon" && lower != "cordons")
      {
        continue;
      }
      if (name != lower || s.root)
      {
        reject("competing, duplicate or case-variant controller");
      }
      const auto legacy = name == "cordon";
      s.root = s.shape(
        d,
        entry,
        legacy ? std::vector<std::string>{"name", "active", "mins", "maxs"}
               : std::vector<std::string>{"active"},
        legacy ? "" : "cordon");
      s.base.form = legacy ? NativeCordonForm::Legacy : NativeCordonForm::Modern;
      s.baseController = s.controller = s.next++;
      if (legacy)
      {
        s.base.records.push_back(s.readRecord(d, entry, true));
      }
      else
      {
        s.base.active = s.flag(d, *s.root);
        for (const auto child : d.entries()[entry].children)
        {
          if (d.entries()[child].isBlock())
          {
            s.base.records.push_back(s.readRecord(d, child, false));
          }
          if (s.base.records.size() > MaxRecords)
          {
            reject("record limit exceeded");
          }
        }
      }
    }
    s.limits(s.base);
  }
  catch (const std::invalid_argument& e)
  {
    s.base.form = NativeCordonForm::None;
    s.base.active.reset();
    s.base.records.clear();
    s.base.refusalReason = e.what();
  }
  s.live = s.base;
}
NativeCordons::NativeCordons(NativeCordons&&) noexcept = default;
NativeCordons& NativeCordons::operator=(NativeCordons&&) noexcept = default;
NativeCordons::~NativeCordons() = default;
NativeCordonSnapshot NativeCordons::snapshot() const
{
  return m_impl->live;
}

std::optional<PreparedNativeCordonChange> NativeCordons::prepare(
  const NativeCordonRequest& request) const
{
  const auto& s = *m_impl;
  if (!s.live.refusalReason.empty())
  {
    reject(s.live.refusalReason);
  }
  if (
    request.expected.m_scope != s.live.m_scope
    || request.expected.revision != s.live.revision || !same(request.expected, s.live))
  {
    reject("stale or foreign expected snapshot");
  }
  if (request.records.size() > MaxRecords)
  {
    reject("record limit exceeded");
  }
  // Validate bounded input before copying the requested local state.
  size_t boxes = 0;
  size_t bytes = 0;
  for (const auto& r : request.records)
  {
    if (r.name && r.name->size() > MaxNameBytes)
    {
      reject("name byte limit exceeded");
    }
    bytes += r.name ? r.name->size() : 0;
    if (r.boxes.size() > MaxBoxes - boxes)
    {
      reject("box limit exceeded");
    }
    boxes += r.boxes.size();
  }
  if (bytes > MaxTextBytes || boxes + request.records.size() > MaxBatchItems)
  {
    reject("text/batch limit exceeded");
  }
  auto desired = s.live;
  desired.form = request.form;
  desired.active = request.active;
  desired.records = request.records;
  s.limits(desired);
  if (
    desired.form != NativeCordonForm::None && desired.form != NativeCordonForm::Modern
    && desired.form != NativeCordonForm::Legacy)
  {
    reject("invalid controller form");
  }
  if (desired.form == NativeCordonForm::None)
  {
    if (!desired.records.empty() || desired.active)
    {
      reject("removed controller has fields");
    }
  }
  else if (s.live.form == NativeCordonForm::Legacy)
  {
    if (
      desired.form != NativeCordonForm::Legacy || desired.active
      || desired.records.size() != 1 || desired.records[0].id != s.live.records[0].id
      || desired.records[0].name != s.live.records[0].name
      || desired.records[0].boxes.size() != 1
      || desired.records[0].boxes[0].id != s.live.records[0].boxes[0].id)
    {
      reject("legacy supports only in-place bounds/activation/removal");
    }
  }
  else if (desired.form != NativeCordonForm::Modern)
  {
    reject("cannot create legacy controller");
  }
  if (
    desired.form == NativeCordonForm::Modern && desired.records.empty()
    && !(s.live.form == NativeCordonForm::Modern && s.live.records.empty()))
  {
    reject("remove the controller when removing its last record");
  }

  auto data = std::make_shared<PreparedNativeCordonChange::Data>();
  data->firstSerial = s.next;
  auto next = s.next;
  const auto allocate = [&]() {
    if (next == std::numeric_limits<uint64_t>::max())
    {
      reject("handle limit exceeded");
    }
    return next++;
  };
  auto seenRecords = std::vector<NativeCordonId>{};
  auto seenBoxes = std::vector<NativeCordonBoxId>{};
  for (auto& r : desired.records)
  {
    if (r.boxes.empty())
    {
      reject("remove a record rather than its last box");
    }
    const auto* old = findId(s.live.records, r.id);
    if (!r.id.isNew() && !old)
    {
      reject("stale or foreign cordon handle");
    }
    if (
      std::find(seenRecords.begin(), seenRecords.end(), r.id) != seenRecords.end()
      && !r.id.isNew())
    {
      reject("repeated cordon handle");
    }
    seenRecords.push_back(r.id);
    if (r.id.isNew())
    {
      r.id = NativeCordonId{s.live.m_scope, allocate()};
    }
    for (auto& b : r.boxes)
    {
      if (!b.id.isNew() && (!old || !findId(old->boxes, b.id)))
      {
        reject("stale, moved or foreign box handle");
      }
      if (
        std::find(seenBoxes.begin(), seenBoxes.end(), b.id) != seenBoxes.end()
        && !b.id.isNew())
      {
        reject("repeated box handle");
      }
      seenBoxes.push_back(b.id);
      if (b.id.isNew())
      {
        b.id = NativeCordonBoxId{s.live.m_scope, allocate()};
      }
    }
  }
  // Reordering loaded children has no native surgical representation in this API.
  const auto ordered = [](const auto& before, const auto& after) {
    size_t pos = 0;
    for (const auto& item : after)
    {
      if (!findId(before, item.id))
      {
        continue;
      }
      while (pos < before.size() && before[pos].id != item.id)
      {
        ++pos;
      }
      if (pos == before.size())
      {
        reject("reordering existing entries is unsupported");
      }
      ++pos;
    }
    bool added = false;
    for (const auto& item : after)
    {
      if (!findId(before, item.id))
      {
        added = true;
      }
      else if (added)
      {
        reject("new entries must append");
      }
    }
  };
  ordered(s.live.records, desired.records);
  for (const auto& r : desired.records)
  {
    if (const auto* old = findId(s.live.records, r.id))
    {
      ordered(old->boxes, r.boxes);
    }
  }
  if (same(desired, s.live))
  {
    return std::nullopt;
  }
  data->before = s.live;
  data->after = std::move(desired);
  data->beforeController = s.controller;
  data->afterController = data->after.form == NativeCordonForm::None
                            ? 0
                            : (s.controller ? s.controller : allocate());
  data->nextSerial = next;
  return PreparedNativeCordonChange{std::move(data)};
}

void NativeCordons::apply(
  PreparedNativeCordonChange& change, const NativeCordonDirection direction)
{
  auto& s = *m_impl;
  if (!change.m_data)
  {
    reject("empty prepared change");
  }
  auto& c = *change.m_data;
  const auto forward = direction == NativeCordonDirection::Forward;
  if (!forward && direction != NativeCordonDirection::Backward)
  {
    reject("invalid replay direction");
  }
  const auto& expected = forward ? c.before : c.after;
  const auto& desired = forward ? c.after : c.before;
  if (
    !s.live.refusalReason.empty() || expected.m_scope != s.live.m_scope
    || !same(expected, s.live)
    || s.controller != (forward ? c.beforeController : c.afterController))
  {
    reject("replay expected state mismatch");
  }
  if (
    !c.published
    && (!forward || s.next != c.firstSerial || s.live.revision != c.before.revision))
  {
    reject("stale staged handle allocation");
  }
  if (s.live.revision == std::numeric_limits<uint64_t>::max())
  {
    reject("revision limit exceeded");
  }
  auto staged = desired;
  staged.revision = s.live.revision + 1;
  s.live = std::move(staged);
  s.controller = forward ? c.afterController : c.beforeController;
  s.next = std::max(s.next, c.nextSerial);
  c.published = true;
}

void NativeCordons::emitEdits(
  std::vector<VmfValueEdit>& values,
  std::vector<VmfStructuralEdit>& structure,
  std::string& roots) const
{
  const auto& s = *m_impl;
  if (
    !s.live.refusalReason.empty()
    || (same(s.base, s.live) && s.controller == s.baseController))
  {
    return;
  }
  const auto remove = [&](const VmfEntryId entry) {
    structure.push_back({VmfStructuralEdit::Type::Remove, entry, {}});
  };
  const auto append = [&](const VmfEntryId entry, std::string text) {
    if (!text.empty())
    {
      structure.push_back({VmfStructuralEdit::Type::Append, entry, std::move(text)});
    }
  };
  if (s.controller != s.baseController)
  {
    if (s.root)
    {
      remove(s.root->entry);
    }
    if (s.live.form != NativeCordonForm::None)
    {
      auto text = "cordons\n{\n" + optionalText("active", flagText(s.live.active));
      for (const auto& r : s.live.records)
      {
        text += recordText(r);
      }
      roots += text + "}\n";
    }
    return;
  }
  if (!s.root)
  {
    return;
  }
  const auto scalar = [&](
                        const Impl::Provenance& p,
                        const std::string& key,
                        const std::optional<std::string>& before,
                        const std::optional<std::string>& after,
                        std::string& appended) {
    if (before == after)
    {
      return;
    }
    const auto it = p.scalars.find(key);
    if (it == p.scalars.end())
    {
      appended += optionalText(key, after);
    }
    else if (!after)
    {
      remove(it->second);
    }
    else
    {
      values.push_back({it->second, quote(*after)});
    }
  };
  auto rootAppend = std::string{};
  scalar(*s.root, "active", flagText(s.base.active), flagText(s.live.active), rootAppend);
  for (const auto& old : s.base.records)
  {
    const auto* now = findId(s.live.records, old.id);
    const auto& p = s.records.at(old.id.m_serial);
    if (!now)
    {
      remove(p.entry);
      continue;
    }
    auto recordAppend = std::string{};
    scalar(p, "name", old.name, now->name, recordAppend);
    scalar(p, "active", flagText(old.active), flagText(now->active), recordAppend);
    for (const auto& oldBox : old.boxes)
    {
      const auto* box = findId(now->boxes, oldBox.id);
      const auto& bp = s.boxes.at(oldBox.id.m_serial);
      if (!box)
      {
        remove(bp.entry);
        continue;
      }
      auto boxAppend = std::string{};
      if (oldBox.mins != box->mins)
      {
        scalar(bp, "mins", tuple(oldBox.mins), tuple(box->mins), boxAppend);
      }
      if (oldBox.maxs != box->maxs)
      {
        scalar(bp, "maxs", tuple(oldBox.maxs), tuple(box->maxs), boxAppend);
      }
      append(bp.entry, std::move(boxAppend));
    }
    for (const auto& box : now->boxes)
    {
      if (!findId(old.boxes, box.id))
      {
        recordAppend += boxText(box);
      }
    }
    append(p.entry, std::move(recordAppend));
  }
  for (const auto& r : s.live.records)
  {
    if (!findId(s.base.records, r.id))
    {
      rootAppend += recordText(r);
    }
  }
  append(s.root->entry, std::move(rootAppend));
}
} // namespace tb::mdl
