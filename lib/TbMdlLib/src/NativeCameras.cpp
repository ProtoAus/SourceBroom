/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/NativeCameras.h"

#include "mdl/SourceEditorMetadata.h"

#include <algorithm>
#include <charconv>
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
  throw std::invalid_argument{"Native cameras: " + reason};
}
std::string decode(const VmfDocument& d, const VmfToken& token)
{
  const auto raw = d.tokenContents(token);
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
vm::vec3d point(const std::string& text)
{
  auto input = std::istringstream{text};
  input.imbue(std::locale::classic());
  input >> std::ws;
  const auto wrapped = input.peek() == '[';
  if (wrapped)
  {
    input.get();
  }
  auto result = vm::vec3d{};
  for (size_t i = 0; i < 3; ++i)
  {
    if (!(input >> result[i]) || !std::isfinite(result[i]))
    {
      reject("invalid position/look tuple");
    }
  }
  if (wrapped)
  {
    auto close = char{};
    if (!(input >> close) || close != ']')
    {
      reject("invalid position/look tuple");
    }
  }
  input >> std::ws;
  if (!input.eof())
  {
    reject("invalid position/look tuple");
  }
  return result;
}
void validPose(const NativeCameraRecord& record)
{
  if (!SourceEditorCamera{VmfEntryId{}, record.eye, record.look}.navigable())
  {
    reject("position/look require a finite nonzero readable direction");
  }
}
bool same(const NativeCameraSnapshot& a, const NativeCameraSnapshot& b)
{
  return a.present == b.present && a.active == b.active && a.records == b.records;
}
template <typename T>
const T* findId(const std::vector<T>& records, const NativeCameraId& id)
{
  const auto it = std::find_if(
    records.begin(), records.end(), [&](const auto& record) { return record.id == id; });
  return it == records.end() ? nullptr : &*it;
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
std::string tuple(const vm::vec3d& point)
{
  auto stream = std::ostringstream{};
  stream.imbue(std::locale::classic());
  stream << std::setprecision(std::numeric_limits<double>::max_digits10) << '['
         << point.x() << ' ' << point.y() << ' ' << point.z() << ']';
  return stream.str();
}
std::string recordText(const NativeCameraRecord& record)
{
  return "camera\n{\n\"position\" " + quote(tuple(record.eye)) + "\n\"look\" "
         + quote(tuple(record.look)) + "\n}\n";
}
std::optional<std::string> activeText(const NativeCameraSnapshot& snapshot)
{
  switch (snapshot.active.kind)
  {
  case NativeCameraActiveKind::Absent:
    return std::nullopt;
  case NativeCameraActiveKind::None:
    return "-1";
  case NativeCameraActiveKind::Camera:
    for (size_t i = 0; i < snapshot.records.size(); ++i)
    {
      if (snapshot.records[i].id == snapshot.active.camera)
      {
        return std::to_string(i);
      }
    }
    reject("active camera is not a retained record");
  }
  reject("invalid active camera kind");
}
} // namespace

struct PreparedNativeCameraChange::Data
{
  NativeCameraSnapshot before;
  NativeCameraSnapshot after;
  uint64_t beforeController = 0;
  uint64_t afterController = 0;
  uint64_t firstSerial = 0;
  uint64_t nextSerial = 0;
  bool published = false;
};
PreparedNativeCameraChange::PreparedNativeCameraChange(std::shared_ptr<Data> data)
  : m_data{std::move(data)}
{
}
const NativeCameraSnapshot& PreparedNativeCameraChange::before() const
{
  if (!m_data)
  {
    reject("empty prepared change");
  }
  return m_data->before;
}
const NativeCameraSnapshot& PreparedNativeCameraChange::after() const
{
  if (!m_data)
  {
    reject("empty prepared change");
  }
  return m_data->after;
}
struct NativeCameras::Impl
{
  struct Provenance
  {
    VmfEntryId entry;
    std::map<std::string, VmfEntryId> scalars;
  };
  NativeCameraSnapshot base;
  NativeCameraSnapshot live;
  std::optional<Provenance> root;
  std::map<uint64_t, Provenance> records;
  uint64_t controller = 0;
  uint64_t baseController = 0;
  uint64_t next = 1;

  Provenance shape(const VmfDocument& d, VmfEntryId entry, bool controllerBlock)
  {
    if (!d.entries()[entry].isBlock())
    {
      reject("controller/record must be a block");
    }
    auto result = Provenance{entry, {}};
    for (const auto child : d.entries()[entry].children)
    {
      const auto name = decode(d, d.entries()[child].key);
      if (controllerBlock && name == "camera" && d.entries()[child].isBlock())
      {
        continue;
      }
      if (
        d.entries()[child].isBlock()
        || (controllerBlock ? name != "activecamera" : (name != "position" && name != "look"))
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
    if (it == p.scalars.end())
    {
      return std::nullopt;
    }
    if (d.entries()[it->second].value->length > MaxTextBytes)
    {
      reject("scalar text limit exceeded");
    }
    return decode(d, *d.entries()[it->second].value);
  }
};
NativeCameras::NativeCameras(const VmfDocument& d)
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
      std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : char(c);
      });
      if (lower != "cameras")
      {
        continue;
      }
      if (name != lower || s.root)
      {
        reject("duplicate or case-variant controller");
      }
      s.root = s.shape(d, entry, true);
      s.base.present = true;
      s.baseController = s.controller = s.next++;
      for (const auto child : d.entries()[entry].children)
      {
        if (!d.entries()[child].isBlock())
        {
          continue;
        }
        if (s.base.records.size() >= MaxRecords)
        {
          reject("record limit exceeded");
        }
        const auto p = s.shape(d, child, false);
        const auto eye = s.scalar(d, p, "position");
        const auto look = s.scalar(d, p, "look");
        if (!eye || !look)
        {
          reject("partial camera pose");
        }
        auto record = NativeCameraRecord{
          NativeCameraId{s.base.m_scope, s.next++}, point(*eye), point(*look)};
        validPose(record);
        s.records.emplace(record.id.m_serial, p);
        s.base.records.push_back(std::move(record));
      }
      if (const auto active = s.scalar(d, *s.root, "activecamera"))
      {
        if (*active == "-1")
        {
          s.base.active.kind = NativeCameraActiveKind::None;
        }
        else
        {
          auto index = size_t{};
          const auto parsed =
            std::from_chars(active->data(), active->data() + active->size(), index);
          if (
            active->empty() || parsed.ec != std::errc{}
            || parsed.ptr != active->data() + active->size()
            || index >= s.base.records.size())
          {
            reject("active camera index is invalid/out of range");
          }
          s.base.active = {NativeCameraActiveKind::Camera, s.base.records[index].id};
        }
      }
    }
  }
  catch (const std::invalid_argument& e)
  {
    s.base.present = false;
    s.base.active = {};
    s.base.records.clear();
    s.base.refusalReason = e.what();
  }
  s.live = s.base;
}
NativeCameras::NativeCameras(NativeCameras&&) noexcept = default;
NativeCameras& NativeCameras::operator=(NativeCameras&&) noexcept = default;
NativeCameras::~NativeCameras() = default;
NativeCameraSnapshot NativeCameras::snapshot() const
{
  return m_impl->live;
}
std::optional<PreparedNativeCameraChange> NativeCameras::prepare(
  const NativeCameraRequest& request) const
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
  for (const auto& record : request.records)
  {
    validPose(record);
  }
  auto desired = s.live;
  desired.present = request.present;
  desired.active = request.active;
  desired.records = request.records;
  if (
    desired.active.kind != NativeCameraActiveKind::Absent
    && desired.active.kind != NativeCameraActiveKind::None
    && desired.active.kind != NativeCameraActiveKind::Camera)
  {
    reject("invalid active camera kind");
  }
  if (
    desired.active.kind != NativeCameraActiveKind::Camera
    && !desired.active.camera.isNew())
  {
    reject("inactive field carries a camera handle");
  }
  if (
    !desired.present
    && (!desired.records.empty() || desired.active.kind != NativeCameraActiveKind::Absent))
  {
    reject("removed controller has fields");
  }
  if (
    desired.active.kind == NativeCameraActiveKind::Camera
    && (desired.active.camera.isNew() || !findId(desired.records, desired.active.camera)))
  {
    reject("active camera requires a retained existing record");
  }
  auto data = std::make_shared<PreparedNativeCameraChange::Data>();
  data->firstSerial = s.next;
  auto next = s.next;
  const auto allocate = [&]() {
    if (next == std::numeric_limits<uint64_t>::max())
    {
      reject("handle limit exceeded");
    }
    return next++;
  };
  auto seen = std::vector<NativeCameraId>{};
  for (auto& record : desired.records)
  {
    if (!record.id.isNew())
    {
      if (!findId(s.live.records, record.id))
      {
        reject("stale or foreign camera handle");
      }
      if (std::find(seen.begin(), seen.end(), record.id) != seen.end())
      {
        reject("repeated camera handle");
      }
      seen.push_back(record.id);
    }
    else
    {
      record.id = NativeCameraId{s.live.m_scope, allocate()};
    }
  }
  size_t pos = 0;
  bool added = false;
  for (const auto& record : desired.records)
  {
    if (!findId(s.live.records, record.id))
    {
      added = true;
      continue;
    }
    if (added)
    {
      reject("new cameras must append after retained records");
    }
    while (pos < s.live.records.size() && s.live.records[pos].id != record.id)
    {
      ++pos;
    }
    if (pos == s.live.records.size())
    {
      reject("reordering existing cameras is unsupported");
    }
    ++pos;
  }
  if (same(desired, s.live))
  {
    return std::nullopt;
  }
  data->before = s.live;
  data->after = std::move(desired);
  data->beforeController = s.controller;
  data->afterController =
    !data->after.present ? 0 : (s.controller ? s.controller : allocate());
  data->nextSerial = next;
  return PreparedNativeCameraChange{std::move(data)};
}
void NativeCameras::apply(
  PreparedNativeCameraChange& change, NativeCameraDirection direction)
{
  auto& s = *m_impl;
  if (!change.m_data)
  {
    reject("empty prepared change");
  }
  auto& c = *change.m_data;
  const auto forward = direction == NativeCameraDirection::Forward;
  if (!forward && direction != NativeCameraDirection::Backward)
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
void NativeCameras::emitEdits(
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
  const auto remove = [&](VmfEntryId entry) {
    structure.push_back({VmfStructuralEdit::Type::Remove, entry, {}});
  };
  const auto optionalText = [](const std::optional<std::string>& active) {
    return active ? "\"activecamera\" " + quote(*active) + "\n" : std::string{};
  };
  if (s.controller != s.baseController)
  {
    if (s.root)
    {
      remove(s.root->entry);
    }
    if (s.live.present)
    {
      auto text = std::string{"cameras\n{\n"} + optionalText(activeText(s.live));
      for (const auto& record : s.live.records)
      {
        text += recordText(record);
      }
      roots += text + "}\n";
    }
    return;
  }
  if (!s.root)
  {
    return;
  }
  auto appended = std::string{};
  const auto beforeActive = activeText(s.base);
  const auto afterActive = activeText(s.live);
  if (beforeActive != afterActive)
  {
    const auto it = s.root->scalars.find("activecamera");
    if (it == s.root->scalars.end())
    {
      appended += optionalText(afterActive);
    }
    else if (!afterActive)
    {
      remove(it->second);
    }
    else
    {
      values.push_back({it->second, quote(*afterActive)});
    }
  }
  for (const auto& original : s.base.records)
  {
    const auto* record = findId(s.live.records, original.id);
    const auto& p = s.records.at(original.id.m_serial);
    if (!record)
    {
      remove(p.entry);
      continue;
    }
    if (original.eye != record->eye)
    {
      values.push_back({p.scalars.at("position"), quote(tuple(record->eye))});
    }
    if (original.look != record->look)
    {
      values.push_back({p.scalars.at("look"), quote(tuple(record->look))});
    }
  }
  for (const auto& record : s.live.records)
  {
    if (!findId(s.base.records, record.id))
    {
      appended += recordText(record);
    }
  }
  if (!appended.empty())
  {
    structure.push_back(
      {VmfStructuralEdit::Type::Append, s.root->entry, std::move(appended)});
  }
}
} // namespace tb::mdl
