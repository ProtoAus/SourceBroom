/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/NativeVisgroups.h"

#include "mdl/SourceEditorMetadata.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>

namespace tb::mdl
{
namespace
{
[[noreturn]] void reject(const std::string& reason)
{
  throw std::invalid_argument{"Native visgroups: " + reason};
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
std::string quote(const std::string& value)
{
  auto out = std::string{"\""};
  for (const auto c : value)
  {
    if (c == '\\' || c == '"')
    {
      out += '\\';
    }
    out += c;
  }
  return out + '"';
}
std::optional<NativeVisgroupColor> parseColor(const std::string_view text)
{
  const auto space = [](const char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  };
  auto result = NativeVisgroupColor{};
  size_t position = 0;
  for (auto& channel : result)
  {
    while (position < text.size() && space(text[position]))
    {
      ++position;
    }
    const auto start = position;
    while (position < text.size() && text[position] >= '0' && text[position] <= '9')
    {
      channel = channel * 10 + text[position++] - '0';
      if (channel > 255)
      {
        return std::nullopt;
      }
    }
    if (position == start)
    {
      return std::nullopt;
    }
  }
  while (position < text.size() && space(text[position]))
  {
    ++position;
  }
  return position == text.size() ? std::optional{result} : std::nullopt;
}
std::string colorText(const NativeVisgroupColor& color)
{
  return std::to_string(color[0]) + " " + std::to_string(color[1]) + " "
         + std::to_string(color[2]);
}
std::optional<std::string> decimal(const std::string& value)
{
  if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
  {
    return std::nullopt;
  }
  const auto first = value.find_first_not_of('0');
  return first == std::string::npos ? "0" : value.substr(first);
}
std::string increment(std::string value)
{
  for (size_t i = value.size(); i > 0; --i)
  {
    if (value[i - 1] != '9')
    {
      ++value[i - 1];
      return value;
    }
    value[i - 1] = '0';
  }
  return "1" + value;
}
template <typename T, typename Id>
const T* find(const std::vector<T>& rows, const Id& id)
{
  const auto it =
    std::find_if(rows.begin(), rows.end(), [&](const auto& r) { return r.id == id; });
  return it == rows.end() ? nullptr : &*it;
}
bool sameContext(const NativeVisgroupContext& a, const NativeVisgroupContext& b)
{
  // Generation is a first-publication freshness token, not replay semantics.
  return a.members == b.members && a.editable == b.editable
         && a.refusalReason == b.refusalReason;
}
bool sameAuthored(const NativeVisgroupSnapshot& a, const NativeVisgroupSnapshot& b)
{
  if (
    a.records != b.records || a.refusalReason != b.refusalReason
    || a.members.size() != b.members.size())
  {
    return false;
  }
  for (size_t i = 0; i < a.members.size(); ++i)
  {
    const auto& x = a.members[i];
    const auto& y = b.members[i];
    if (
      x.id != y.id || x.source != y.source || x.editor != y.editor
      || x.nativeId != y.nativeId || x.memberships != y.memberships
      || x.manualShown != y.manualShown || x.autoShown != y.autoShown
      || x.wrapperHidden != y.wrapperHidden || x.refusalReason != y.refusalReason)
    {
      return false;
    }
  }
  return true;
}
void validName(const std::string& name)
{
  if (name.size() > NativeVisgroups::MaxNameBytes)
  {
    reject("name byte limit exceeded");
  }
  for (const auto c : name)
  {
    if (static_cast<unsigned char>(c) < 32 || c == 127)
    {
      reject("control character in name");
    }
  }
}
} // namespace

NativeVisgroupRequest::NativeVisgroupRequest(const NativeVisgroupSnapshot& s)
  : expected{s}
{
  for (const auto& r : s.records)
  {
    records.push_back(
      {r.id,
       r.parent ? std::optional<NativeVisgroupTarget>{*r.parent} : std::nullopt,
       r.name});
  }
}
struct PreparedNativeVisgroupChange::Data
{
  NativeVisgroupSnapshot before;
  NativeVisgroupSnapshot after;
  NativeVisgroupContext context;
  uint64_t firstSerial = 0;
  uint64_t nextSerial = 0;
  std::string nextNative;
  std::set<std::string> allocated;
  bool published = false;
};
PreparedNativeVisgroupChange::PreparedNativeVisgroupChange(std::shared_ptr<Data> data)
  : m_data{std::move(data)}
{
}
const NativeVisgroupSnapshot& PreparedNativeVisgroupChange::before() const
{
  return m_data->before;
}
const NativeVisgroupSnapshot& PreparedNativeVisgroupChange::after() const
{
  return m_data->after;
}

struct NativeVisgroups::Impl
{
  struct TreeSource
  {
    VmfEntryId entry;
    std::optional<VmfEntryId> name;
    std::optional<VmfEntryId> color;
  };
  struct MemberSource
  {
    VmfEntryId entry;
    std::vector<std::pair<VmfEntryId, std::string>> refs;
    std::optional<VmfEntryId> manual;
    size_t row;
  };
  NativeVisgroupSnapshot base;
  NativeVisgroupSnapshot live;
  std::string source;
  std::optional<VmfEntryId> controller;
  std::map<uint64_t, TreeSource> trees;
  std::map<uint64_t, MemberSource> members;
  // Raw occurrences not represented by known member editor fields.
  std::map<std::string, std::vector<std::optional<uint64_t>>> barriers;
  std::map<uint64_t, std::vector<std::string>> memberBarriers;
  size_t barrierCount = 0;
  size_t barrierBytes = 0;
  std::set<std::string> reserved;
  std::string allocationReason;
  std::string nextNative = "1";
  uint64_t next = 1;

  void document(const VmfDocument& d) const
  {
    if (source != d.source())
    {
      reject("emission document differs from immutable provenance");
    }
  }
  void context(const NativeVisgroupContext& c) const
  {
    if (c.members.size() > MaxMembers)
    {
      reject("context member limit exceeded");
    }
    auto seen = std::set<uint64_t>{};
    for (const auto& m : c.members)
    {
      if (
        (m.id.m_scope != live.m_scope || !members.contains(m.id.m_serial))
        || !seen.insert(m.id.m_serial).second)
      {
        reject("foreign or repeated context member");
      }
      if (
        m.owner
        && (m.owner->m_scope != live.m_scope || !members.contains(m.owner->m_serial) || *m.owner == m.id))
      {
        reject("foreign or self context owner");
      }
    }
    auto parents = std::map<uint64_t, uint64_t>{};
    for (const auto& m : c.members)
    {
      if (m.owner)
      {
        parents[m.id.m_serial] = m.owner->m_serial;
      }
    }
    for (const auto& m : c.members)
    {
      auto seenParents = std::set<uint64_t>{};
      auto current = m.id.m_serial;
      while (parents.contains(current))
      {
        if (!seenParents.insert(current).second || seenParents.size() > MaxDepth)
        {
          reject("cyclic or excessive member ownership");
        }
        current = parents.at(current);
      }
    }
  }
  NativeVisgroupSnapshot contextual(const NativeVisgroupContext& c) const
  {
    context(c);
    auto out = live;
    out.m_contextGeneration = c.generation;
    auto rows = std::map<uint64_t, const NativeVisgroupMemberContext*>{};
    for (const auto& row : c.members)
    {
      rows.emplace(row.id.m_serial, &row);
    }
    for (auto& m : out.members)
    {
      m.contextRefusalReason = "unbound member";
      const auto row = rows.find(m.id.m_serial);
      if (row != rows.end())
      {
        const auto* it = row->second;
        m.owner = it->owner;
        m.active = it->active;
        m.projected = it->projected;
        m.editable = c.editable && it->active && it->projected && it->editable
                     && m.refusalReason.empty();
        m.contextRefusalReason = !c.editable ? c.refusalReason : it->refusalReason;
        m.kind = it->kind;
      }
    }
    return out;
  }
};

NativeVisgroups::NativeVisgroups(const VmfDocument& d)
  : m_impl{std::make_unique<Impl>()}
{
  auto& s = *m_impl;
  s.source = d.source();
  s.base.m_scope = std::make_shared<const int>(0);
  const auto metadata = SourceEditorMetadata::read(d);
  const auto key = [&](const VmfEntryId e) { return decode(d, d.entries()[e].key); };
  auto definitions = std::set<VmfEntryId>{};
  auto modeledRefs = std::set<VmfEntryId>{};
  auto ids = std::map<VmfEntryId, NativeVisgroupId>{};
  auto treeRows = std::map<VmfEntryId, size_t>{};
  auto memberRows = std::map<VmfEntryId, size_t>{};
  for (const auto& row : metadata.visgroups)
  {
    ids.emplace(row.entry, NativeVisgroupId{s.base.m_scope, s.next++});
  }
  for (const auto& row : metadata.visgroups)
  {
    auto r = NativeVisgroupRecord{
      ids.at(row.entry), {}, row.nativeId.value_or(""), row.name, {}, {}, {}};
    if (row.parent)
    {
      r.parent = ids.at(*row.parent);
    }
    auto p = Impl::TreeSource{row.entry, {}, {}};
    auto idCount = size_t{0};
    for (const auto child : d.entries()[row.entry].children)
    {
      const auto k = key(child);
      if (k == "visgroupid" && d.entries()[child].value)
      {
        definitions.insert(child);
        ++idCount;
      }
      else if (k == "name" && d.entries()[child].value && !p.name)
      {
        p.name = child;
      }
      else if (k == "visgroup" && d.entries()[child].isBlock())
      {
      }
      else if (k == "color" && d.entries()[child].value)
      {
        if (p.color)
        {
          r.color.reset();
          r.colorRefusalReason = "repeated color fields are preserved, not overwritten";
        }
        else
        {
          p.color = child;
          r.color = parseColor(d.tokenContents(*d.entries()[child].value));
          if (!r.color)
          {
            r.colorRefusalReason = "color is not three decimal RGB channels in 0..255";
          }
        }
      }
      else
      {
        r.refusalReason = "unknown, repeated or malformed visgroup field";
      }
    }
    if (!row.nativeId || idCount != 1)
    {
      r.refusalReason = "invalid visgroup identity";
    }
    s.trees.emplace(r.id.m_serial, p);
    treeRows.emplace(row.entry, s.base.records.size());
    s.base.records.push_back(std::move(r));
  }
  for (const auto root : d.roots())
  {
    if (key(root) == "visgroups")
    {
      if (s.controller || !d.entries()[root].isBlock())
      {
        s.base.refusalReason = "ambiguous visgroups controller";
      }
      else
      {
        s.controller = root;
      }
      for (const auto child : d.entries()[root].children)
      {
        if (key(child) != "visgroup" || !d.entries()[child].isBlock())
        {
          s.base.refusalReason = "unsupported visgroups controller field";
        }
      }
    }
  }
  for (const auto& row : metadata.memberships)
  {
    if (key(row.entry) == "world")
    {
      continue;
    }
    auto m = NativeVisgroupMember{};
    m.id = NativeVisgroupMemberId{s.base.m_scope, s.next++};
    m.source = row.entry;
    m.nativeId = row.nativeId.value_or("");
    m.memberships = row.visgroupIds;
    m.manualShown = row.visgroupShown;
    m.autoShown = row.visgroupAutoShown;
    m.wrapperHidden = row.hiddenByContainer;
    const auto k = key(row.entry);
    if (k == "group")
    {
      m.kind = NativeVisgroupMemberKind::Group;
    }
    else if (k == "solid")
    {
      auto parent = d.entries()[row.entry].parent;
      while (parent && key(*parent) == "hidden")
      {
        parent = d.entries()[*parent].parent;
      }
      m.kind = parent && key(*parent) == "entity"
                 ? NativeVisgroupMemberKind::InternalSolid
                 : NativeVisgroupMemberKind::WorldSolid;
    }
    else
    {
      m.kind = NativeVisgroupMemberKind::PointEntity;
      for (const auto child : d.entries()[row.entry].children)
      {
        if (key(child) == "solid" || key(child) == "hidden")
        {
          m.kind = NativeVisgroupMemberKind::BrushEntity;
        }
      }
    }
    auto p = Impl::MemberSource{row.entry, {}, {}, s.base.members.size()};
    size_t editors = 0;
    for (const auto child : d.entries()[row.entry].children)
    {
      if (key(child) != "editor")
      {
        continue;
      }
      ++editors;
      if (!d.entries()[child].isBlock())
      {
        m.refusalReason = "malformed editor";
        continue;
      }
      m.editor = child;
      for (const auto field : d.entries()[child].children)
      {
        const auto f = key(field);
        if (f == "visgroupid" && d.entries()[field].value)
        {
          if (const auto id = decimal(decode(d, *d.entries()[field].value)))
          {
            p.refs.emplace_back(field, *id);
            modeledRefs.insert(field);
          }
        }
        if (f == "visgroupshown")
        {
          p.manual = field;
        }
      }
    }
    if (editors > 1)
    {
      m.refusalReason = "repeated editor blocks";
      // The reader intentionally withholds ambiguous refs. Retain all candidate
      // identities for protection/deletion, never as editable membership.
      m.memberships.clear();
      for (const auto& [entry, ref] : p.refs)
      {
        m.memberships.push_back(ref);
      }
    }
    s.members.emplace(m.id.m_serial, std::move(p));
    memberRows.emplace(row.entry, s.base.members.size());
    s.base.members.push_back(std::move(m));
  }
  // Diagnostics are local to the nearest modeled tree/member owner.
  for (const auto& diagnostic : metadata.diagnostics)
  {
    auto entry = std::optional<VmfEntryId>{diagnostic.entry};
    while (entry)
    {
      if (const auto row = treeRows.find(*entry); row != treeRows.end())
      {
        s.base.records[row->second].refusalReason = diagnostic.message;
        break;
      }
      if (const auto row = memberRows.find(*entry); row != memberRows.end())
      {
        s.base.members[row->second].refusalReason = diagnostic.message;
        break;
      }
      entry = d.entries()[*entry].parent;
    }
  }
  size_t reservedBytes = 0;
  for (VmfEntryId e = 0; e < d.entries().size(); ++e)
  {
    if (key(e) != "visgroupid" || !d.entries()[e].value)
    {
      continue;
    }
    const auto raw = decode(d, *d.entries()[e].value);
    const auto id = decimal(raw);
    if (!id)
    {
      continue;
    }
    if (s.reserved.insert(*id).second)
    {
      reservedBytes += id->size();
    }
    if (raw.size() > MaxIdBytes || reservedBytes > MaxReservedBytes)
    {
      s.allocationReason = "reserved decimal namespace exceeds allocation budget";
    }
    if (!definitions.contains(e) && !modeledRefs.contains(e))
    {
      auto ancestor = d.entries()[e].parent;
      auto owner = std::optional<uint64_t>{};
      while (ancestor)
      {
        if (const auto row = memberRows.find(*ancestor); row != memberRows.end())
        {
          owner = s.base.members[row->second].id.m_serial;
          break;
        }
        ancestor = d.entries()[*ancestor].parent;
      }
      s.barriers[*id].push_back(owner);
      if (owner)
      {
        s.memberBarriers[*owner].push_back(*id);
      }
      ++s.barrierCount;
      s.barrierBytes += id->size();
    }
  }
  size_t refs = 0;
  size_t bytes = 0;
  for (const auto& r : s.base.records)
  {
    bytes += r.name.size() + r.nativeId.size();
  }
  for (const auto& m : s.base.members)
  {
    refs += m.memberships.size();
    for (const auto& ref : m.memberships)
    {
      bytes += ref.size();
    }
  }
  if (
    s.base.records.size() > MaxRecords || s.base.members.size() > MaxMembers
    || refs > MaxMemberships || bytes > MaxTextBytes)
  {
    s.base.refusalReason =
      "loaded visgroup model exceeds authoring budget; preserved read-only";
  }
  // An unfamiliar branch cannot be moved/deleted through a supported ancestor.
  for (auto& r : s.base.records)
  {
    if (r.refusalReason.empty())
    {
      continue;
    }
    auto parent = r.parent;
    while (parent)
    {
      auto it =
        std::find_if(s.base.records.begin(), s.base.records.end(), [&](const auto& x) {
          return x.id == *parent;
        });
      it->refusalReason = "protected descendant branch";
      parent = it->parent;
    }
  }
  s.live = s.base;
}
NativeVisgroups::NativeVisgroups(NativeVisgroups&&) noexcept = default;
NativeVisgroups& NativeVisgroups::operator=(NativeVisgroups&&) noexcept = default;
NativeVisgroups::~NativeVisgroups() = default;
NativeVisgroupSnapshot NativeVisgroups::snapshot() const
{
  return m_impl->contextual({});
}
NativeVisgroupSnapshot NativeVisgroups::snapshot(
  const NativeVisgroupContext& context) const
{
  return m_impl->contextual(context);
}

std::optional<PreparedNativeVisgroupChange> NativeVisgroups::prepare(
  const NativeVisgroupRequest& request, const NativeVisgroupContext& context) const
{
  const auto& s = *m_impl;
  if (!s.live.refusalReason.empty())
  {
    reject(s.live.refusalReason);
  }
  if (
    request.expected.records.size() > MaxRecords
    || request.expected.members.size() > MaxMembers || request.records.size() > MaxRecords
    || request.memberships.size() > MaxWorkItems)
  {
    reject("request record/member/work limit exceeded");
  }
  const auto current = s.contextual(context);
  if (
    request.expected.m_scope != s.live.m_scope
    || request.expected.revision != s.live.revision
    || request.expected.m_contextGeneration != context.generation
    || !sameAuthored(request.expected, s.live)
    || request.expected.members != current.members)
  {
    reject("stale, foreign or context-mismatched expected snapshot");
  }
  if (!context.editable)
  {
    reject("context is read-only: " + context.refusalReason);
  }
  if (request.records.size() > MaxRecords || request.memberships.size() > MaxWorkItems)
  {
    reject("request record/work limit exceeded");
  }
  size_t bytes = 0;
  size_t work = request.records.size();
  for (const auto& r : request.records)
  {
    bytes += r.name.size();
    if (bytes > MaxTextBytes)
    {
      reject("request text limit exceeded");
    }
  }
  for (const auto& action : request.memberships)
  {
    if (action.roots.size() > MaxWorkItems - work)
    {
      reject("request work limit exceeded");
    }
    work += action.roots.size();
  }
  if (request.manual && request.manual->roots.size() > MaxWorkItems - work)
  {
    reject("request work limit exceeded");
  }
  const auto spend = [&]() {
    if (++work > MaxWorkItems)
    {
      reject("request work limit exceeded");
    }
  };
  auto data = std::make_shared<PreparedNativeVisgroupChange::Data>();
  data->before = s.live;
  data->after = s.live;
  data->context = context;
  data->firstSerial = s.next;
  data->nextSerial = s.next;
  data->nextNative = s.nextNative;
  auto& after = data->after;
  after.records.clear();
  auto targets = std::map<uint64_t, NativeVisgroupId>{};
  auto seen = std::set<uint64_t>{};
  for (const auto& row : request.records)
  {
    auto r = NativeVisgroupRecord{};
    if (const auto* id = std::get_if<NativeVisgroupId>(&row.key))
    {
      const auto* old = find(s.live.records, *id);
      if (!old || !seen.insert(id->m_serial).second)
      {
        reject("foreign, stale or repeated visgroup handle");
      }
      r = *old;
      if (row.name != old->name)
      {
        validName(row.name);
      }
    }
    else
    {
      const auto draft = std::get<NativeVisgroupDraftKey>(row.key).value;
      if (!draft || targets.contains(draft))
      {
        reject("invalid or repeated draft key");
      }
      validName(row.name);
      if (!s.allocationReason.empty())
      {
        reject(s.allocationReason);
      }
      if (data->nextSerial == std::numeric_limits<uint64_t>::max())
      {
        reject("handle budget exhausted");
      }
      r.id = NativeVisgroupId{s.live.m_scope, data->nextSerial++};
      while (s.reserved.contains(data->nextNative)
             || data->allocated.contains(data->nextNative))
      {
        spend();
        data->nextNative = increment(data->nextNative);
      }
      if (data->nextNative.size() > MaxIdBytes)
      {
        reject("native ID allocation limit exceeded");
      }
      r.nativeId = data->nextNative;
      data->allocated.insert(r.nativeId);
      data->nextNative = increment(data->nextNative);
      targets.emplace(draft, r.id);
    }
    r.name = row.name;
    after.records.push_back(std::move(r));
  }
  const auto resolve = [&](const NativeVisgroupTarget& target) {
    if (const auto* id = std::get_if<NativeVisgroupId>(&target))
    {
      if (!find(after.records, *id))
      {
        reject("foreign or deleted target handle");
      }
      return *id;
    }
    const auto draft = std::get<NativeVisgroupDraftKey>(target).value;
    if (!targets.contains(draft))
    {
      reject("unresolved draft target");
    }
    return targets.at(draft);
  };
  for (size_t i = 0; i < request.records.size(); ++i)
  {
    auto& r = after.records[i];
    r.parent = request.records[i].parent
                 ? std::optional<NativeVisgroupId>{resolve(*request.records[i].parent)}
                 : std::nullopt;
    if (
      const auto* old = find(s.live.records, r.id);
      old && !old->refusalReason.empty() && r != *old)
    {
      reject("protected tree record: " + old->refusalReason);
    }
  }
  auto treeParents = std::map<uint64_t, uint64_t>{};
  for (const auto& r : after.records)
  {
    if (r.parent)
    {
      treeParents[r.id.m_serial] = r.parent->m_serial;
    }
  }
  for (const auto& r : after.records)
  {
    auto ancestors = std::set<uint64_t>{};
    auto node = r.id.m_serial;
    while (treeParents.contains(node))
    {
      spend();
      if (!ancestors.insert(node).second || ancestors.size() > MaxDepth)
      {
        reject("cyclic or excessive tree ancestry");
      }
      node = treeParents.at(node);
    }
  }
  // Source siblings (including roots) retain order. New/moved rows append.
  auto siblingOrder = std::map<uint64_t, std::pair<size_t, bool>>{};
  for (const auto& child : after.records)
  {
    const auto parent = child.parent ? child.parent->m_serial : 0;
    auto& [last, appended] = siblingOrder[parent];
    const auto* old = find(s.live.records, child.id);
    if (!old || old->parent != child.parent)
    {
      appended = true;
      continue;
    }
    const auto pos = static_cast<size_t>(old - s.live.records.data()) + 1;
    if (appended || pos < last)
    {
      reject("retained siblings must keep order; new/moved siblings append");
    }
    last = pos;
  }
  if (request.color)
  {
    spend();
    auto& target = *std::ranges::find(
      after.records, resolve(request.color->visgroup), &NativeVisgroupRecord::id);
    if (!target.refusalReason.empty() || !target.colorRefusalReason.empty())
    {
      reject("protected RGB target: " + target.refusalReason + target.colorRefusalReason);
    }
    if (std::ranges::any_of(request.color->color, [](const int channel) {
          return channel < 0 || channel > 255;
        }))
    {
      reject("RGB channels must be in 0..255");
    }
    target.color = request.color->color;
  }
  auto memberIndex = std::map<uint64_t, size_t>{};
  auto contexts = std::map<uint64_t, const NativeVisgroupMemberContext*>{};
  auto children = std::map<uint64_t, std::vector<uint64_t>>{};
  for (size_t i = 0; i < after.members.size(); ++i)
  {
    memberIndex[after.members[i].id.m_serial] = i;
  }
  for (const auto& c : context.members)
  {
    contexts[c.id.m_serial] = &c;
    if (c.owner)
    {
      children[c.owner->m_serial].push_back(c.id.m_serial);
    }
  }
  const auto activeBarrier = [&](const std::string& native) {
    const auto it = s.barriers.find(native);
    if (it == s.barriers.end())
    {
      return false;
    }
    for (const auto owner : it->second)
    {
      spend();
      if (!owner || !contexts.contains(*owner) || contexts.at(*owner)->active)
      {
        return true;
      }
    }
    return false;
  };
  auto deleted = std::set<std::string>{};
  for (const auto& old : s.live.records)
  {
    if (find(after.records, old.id))
    {
      if (old.parent && !find(after.records, *old.parent))
      {
        reject("delete requires complete subtree");
      }
      continue;
    }
    if (!old.refusalReason.empty())
    {
      reject("protected deleted tree record");
    }
    deleted.insert(old.nativeId);
    if (activeBarrier(old.nativeId))
    {
      reject("unmodeled raw reference prevents subtree deletion");
    }
  }
  const auto requireMember = [&](const uint64_t serial) {
    const auto& m = current.members.at(memberIndex.at(serial));
    if (!m.editable)
    {
      reject(
        "required member is unbound, inactive or protected: " + m.refusalReason + " "
        + m.contextRefusalReason);
    }
  };
  // Missing owners in context may hide required children: require a complete
  // context before aggregate closure. Direct leaf edits remain independently local.
  const auto closure = [&](const std::vector<NativeVisgroupMemberId>& roots) {
    auto result = std::set<uint64_t>{};
    auto pending = std::vector<uint64_t>{};
    for (const auto& id : roots)
    {
      if (id.m_scope != s.live.m_scope || !memberIndex.contains(id.m_serial))
      {
        reject("foreign or stale member root");
      }
      pending.push_back(id.m_serial);
    }
    while (!pending.empty())
    {
      spend();
      const auto serial = pending.back();
      pending.pop_back();
      if (!result.insert(serial).second)
      {
        continue;
      }
      requireMember(serial);
      const auto kind = current.members[memberIndex.at(serial)].kind;
      if (
        kind == NativeVisgroupMemberKind::BrushEntity
        || kind == NativeVisgroupMemberKind::Group)
      {
        if (context.members.size() != after.members.size())
        {
          reject("aggregate closure requires complete member context");
        }
      }
      for (const auto child : children[serial])
      {
        // A parked child is not a currently owned child, even if the context
        // retains its former owner as inspection information.
        if (contexts.at(child)->active)
        {
          pending.push_back(child);
        }
      }
    }
    return result;
  };
  for (auto& m : after.members)
  {
    auto affected = false;
    for (const auto& ref : m.memberships)
    {
      affected = affected || deleted.contains(ref);
    }
    if (!affected)
    {
      continue;
    }
    const auto c = contexts.find(m.id.m_serial);
    if (c != contexts.end() && !c->second->active)
    {
      continue;
    }
    requireMember(m.id.m_serial);
    std::erase_if(m.memberships, [&](const auto& ref) { return deleted.contains(ref); });
  }
  auto intent = std::map<std::pair<uint64_t, std::string>, bool>{};
  for (const auto& action : request.memberships)
  {
    const auto id = resolve(action.visgroup);
    const auto* group = find(after.records, id);
    if (!group->refusalReason.empty())
    {
      reject("protected membership target");
    }
    for (const auto serial : closure(action.roots))
    {
      const auto pair = std::pair{serial, group->nativeId};
      if (intent.contains(pair) && intent.at(pair) != action.assign)
      {
        reject("conflicting membership intents");
      }
      intent[pair] = action.assign;
    }
  }
  for (const auto& [pair, assign] : intent)
  {
    auto& refs = after.members[memberIndex.at(pair.first)].memberships;
    const auto it = std::find(refs.begin(), refs.end(), pair.second);
    if (assign && it == refs.end())
    {
      refs.push_back(pair.second);
    }
    else if (!assign && it != refs.end())
    {
      refs.erase(it);
    }
  }
  if (request.manual)
  {
    auto groupClosure = std::set<uint64_t>{};
    for (const auto& root : request.manual->roots)
    {
      groupClosure.insert(resolve(root).m_serial);
    }
    for (const auto& r : after.records)
    {
      auto ancestor = std::optional<NativeVisgroupId>{r.id};
      while (ancestor)
      {
        spend();
        if (groupClosure.contains(ancestor->m_serial))
        {
          groupClosure.insert(r.id.m_serial);
          break;
        }
        ancestor = find(after.records, *ancestor)->parent;
      }
    }
    auto native = std::set<std::string>{};
    for (const auto& r : after.records)
    {
      if (groupClosure.contains(r.id.m_serial))
      {
        if (!r.refusalReason.empty())
        {
          reject("protected manual tree closure");
        }
        native.insert(r.nativeId);
      }
    }
    for (const auto& id : native)
    {
      if (activeBarrier(id))
      {
        reject("unmodeled raw member prevents manual closure");
      }
    }
    auto roots = std::vector<NativeVisgroupMemberId>{};
    for (const auto& m : after.members)
    {
      if (!std::any_of(m.memberships.begin(), m.memberships.end(), [&](const auto& ref) {
            return native.contains(ref);
          }))
      {
        continue;
      }
      const auto c = contexts.find(m.id.m_serial);
      if (c != contexts.end() && !c->second->active)
      {
        continue;
      }
      roots.push_back(m.id);
    }
    for (const auto serial : closure(roots))
    {
      after.members[memberIndex.at(serial)].manualShown = request.manual->shown;
    }
  }
  size_t refs = 0;
  bytes = 0;
  for (const auto& r : after.records)
  {
    bytes += r.name.size() + r.nativeId.size();
  }
  for (const auto& m : after.members)
  {
    refs += m.memberships.size();
    for (const auto& ref : m.memberships)
    {
      bytes += ref.size();
    }
    if (refs > MaxMemberships || bytes > MaxTextBytes)
    {
      reject("result membership/text budget exceeded");
    }
  }
  if (sameAuthored(s.live, after))
  {
    return std::nullopt;
  }
  return PreparedNativeVisgroupChange{std::move(data)};
}

void NativeVisgroups::apply(
  PreparedNativeVisgroupChange& change,
  const NativeVisgroupDirection direction,
  const NativeVisgroupContext& context)
{
  auto& s = *m_impl;
  auto& c = *change.m_data;
  if (
    direction != NativeVisgroupDirection::Forward
    && direction != NativeVisgroupDirection::Backward)
  {
    reject("invalid direction");
  }
  s.context(context);
  const auto forward = direction == NativeVisgroupDirection::Forward;
  const auto& expected = forward ? c.before : c.after;
  const auto& desired = forward ? c.after : c.before;
  auto compared = s.live;
  auto comparedContext = context;
  if (compared.members.size() > expected.members.size())
  {
    auto additional = std::set<uint64_t>{};
    for (size_t i = expected.members.size(); i < compared.members.size(); ++i)
    {
      additional.insert(compared.members[i].id.m_serial);
    }
    auto filtered = std::vector<NativeVisgroupMemberContext>{};
    filtered.reserve(comparedContext.members.size());
    size_t parked = 0;
    for (const auto& row : comparedContext.members)
    {
      if (additional.contains(row.id.m_serial))
      {
        if (row.active)
        {
          reject("additional live member during replay");
        }
        ++parked;
      }
      else
      {
        filtered.push_back(row);
      }
    }
    if (parked != additional.size())
    {
      reject("additional unbound member during replay");
    }
    comparedContext.members.swap(filtered);
    compared.members.resize(expected.members.size());
  }
  if (
    expected.m_scope != s.live.m_scope || !sameAuthored(compared, expected)
    || !sameContext(c.context, comparedContext))
  {
    reject("replay semantic state/context mismatch");
  }
  if (!c.published && (!forward || s.live.revision != c.before.revision || s.next != c.firstSerial || context.generation != c.context.generation))
  {
    reject("stale first dispatch");
  }
  if (s.live.revision == std::numeric_limits<uint64_t>::max())
  {
    reject("revision exhausted");
  }
  auto staged = desired;
  staged.members.insert(
    staged.members.end(),
    s.live.members.begin() + desired.members.size(),
    s.live.members.end());
  staged.revision = s.live.revision + 1;
  s.live = std::move(staged);
  if (!c.published)
  {
    s.next = c.nextSerial;
    s.nextNative = c.nextNative;
    s.reserved.insert(c.allocated.begin(), c.allocated.end());
    c.published = true;
  }
}

uint64_t NativeVisgroups::revision() const
{
  return m_impl->live.revision;
}
std::set<std::string> NativeVisgroups::liveRecordIds() const
{
  auto result = std::set<std::string>{};
  for (const auto& record : m_impl->live.records)
  {
    result.insert(record.nativeId);
  }
  return result;
}

const std::vector<std::string>& NativeVisgroups::rawReferences(
  const NativeVisgroupMemberId id) const
{
  static_cast<void>(member(id));
  const auto found = m_impl->memberBarriers.find(id.m_serial);
  static const auto empty = std::vector<std::string>{};
  return found == m_impl->memberBarriers.end() ? empty : found->second;
}

std::vector<NativeVisgroupMemberId> NativeVisgroups::pendingMemberIds(
  const size_t count) const
{
  const auto& s = *m_impl;
  if (
    count > MaxMembers - std::min(MaxMembers, s.live.members.size())
    || count > std::numeric_limits<uint64_t>::max() - s.next)
  {
    reject("lifecycle member budget exhausted");
  }
  auto ids = std::vector<NativeVisgroupMemberId>{};
  ids.reserve(count);
  for (size_t i = 0; i < count; ++i)
  {
    ids.push_back(NativeVisgroupMemberId{s.live.m_scope, s.next + i});
  }
  return ids;
}

const NativeVisgroupMember& NativeVisgroups::member(const NativeVisgroupMemberId id) const
{
  const auto& s = *m_impl;
  if (id.m_scope != s.live.m_scope || !s.members.contains(id.m_serial))
  {
    reject("foreign lifecycle member");
  }
  return s.live.members[s.members.at(id.m_serial).row];
}

std::vector<NativeVisgroupMemberId> NativeVisgroups::registerMembers(
  const std::vector<Registration>& registrations)
{
  auto& s = *m_impl;
  if (
    registrations.size() > MaxMembers - std::min(MaxMembers, s.live.members.size())
    || registrations.size() > std::numeric_limits<uint64_t>::max() - s.next
    || s.barrierCount > MaxMemberships || s.barrierBytes > MaxTextBytes
    || s.live.revision == std::numeric_limits<uint64_t>::max())
  {
    reject("lifecycle member budget exhausted");
  }
  auto base = s.base.members;
  auto live = s.live.members;
  auto sources = s.members;
  auto barriers = s.barriers;
  auto memberBarriers = s.memberBarriers;
  auto barrierCount = s.barrierCount;
  auto barrierBytes = s.barrierBytes;
  auto next = s.next;
  auto result = std::vector<NativeVisgroupMemberId>{};
  size_t memberships = 0;
  size_t bytes = 0;
  for (const auto& r : s.live.records)
  {
    bytes += r.name.size() + r.nativeId.size();
  }
  for (const auto& m : live)
  {
    memberships += m.memberships.size();
    for (const auto& ref : m.memberships)
    {
      bytes += ref.size();
    }
  }
  for (const auto& r : registrations)
  {
    auto b = NativeVisgroupMember{};
    auto m = NativeVisgroupMember{};
    auto provenance = Impl::MemberSource{0, {}, {}, base.size()};
    if (r.copy)
    {
      if (r.copy->m_scope != s.live.m_scope || !s.members.contains(r.copy->m_serial))
      {
        reject("foreign lifecycle copy");
      }
      provenance = s.members.at(r.copy->m_serial);
      b = s.base.members[provenance.row];
      m = s.live.members[provenance.row];
      provenance.row = base.size();
    }
    b.id = m.id = NativeVisgroupMemberId{s.live.m_scope, next++};
    b.nativeId = m.nativeId = r.nativeId;
    b.kind = m.kind = r.kind;
    memberships += m.memberships.size();
    for (const auto& ref : m.memberships)
    {
      bytes += ref.size();
    }
    if (memberships > MaxMemberships || bytes > MaxTextBytes)
    {
      reject("lifecycle membership/text budget exhausted");
    }
    if (r.copy)
    {
      if (
        const auto found = s.memberBarriers.find(r.copy->m_serial);
        found != s.memberBarriers.end())
      {
        for (const auto& ref : found->second)
        {
          if (
            barrierCount >= MaxMemberships
            || ref.size() > MaxTextBytes - std::min(MaxTextBytes, barrierBytes))
          {
            reject("lifecycle raw-reference budget exhausted");
          }
          ++barrierCount;
          barrierBytes += ref.size();
          barriers[ref].push_back(m.id.m_serial);
          memberBarriers[m.id.m_serial].push_back(ref);
        }
      }
    }
    if (
      barrierCount > MaxMemberships - std::min(MaxMemberships, memberships)
      || barrierBytes > MaxTextBytes - std::min(MaxTextBytes, bytes))
    {
      reject("lifecycle aggregate reference/text budget exhausted");
    }
    result.push_back(m.id);
    sources.emplace(m.id.m_serial, std::move(provenance));
    base.push_back(std::move(b));
    live.push_back(std::move(m));
  }
  if (!registrations.empty())
  {
    s.base.members.swap(base);
    s.live.members.swap(live);
    s.members.swap(sources);
    s.barriers.swap(barriers);
    s.memberBarriers.swap(memberBarriers);
    s.barrierCount = barrierCount;
    s.barrierBytes = barrierBytes;
    s.next = next;
    ++s.live.revision;
  }
  return result;
}

NativeVisgroupEmissionSession NativeVisgroups::beginEmission(const VmfDocument& d) const
{
  const auto& s = *m_impl;
  s.document(d);
  return NativeVisgroupEmissionSession{s.live.m_scope, s.live.revision};
}

void NativeVisgroups::emitMemberEdits(
  const NativeVisgroupEmissionSession& session,
  const NativeVisgroupMemberId id,
  std::vector<VmfValueEdit>& values,
  std::vector<VmfStructuralEdit>& structure,
  std::string& appended) const
{
  const auto& s = *m_impl;
  if (session.m_scope != s.live.m_scope || session.m_revision != s.live.revision)
  {
    reject("stale or foreign emission session");
  }
  if (id.m_scope != s.live.m_scope || !s.members.contains(id.m_serial))
  {
    reject("foreign member emission handle");
  }
  const auto& p = s.members.at(id.m_serial);
  const auto* before = &s.base.members[p.row];
  const auto* after = &s.live.members[p.row];
  if (after->memberships != before->memberships)
  {
    // Appends follow preserved fields: keep only the live prefix in source order.
    auto retained = size_t{0};
    for (const auto& [entry, ref] : p.refs)
    {
      if (retained < after->memberships.size() && after->memberships[retained] == ref)
      {
        ++retained;
      }
      else
      {
        structure.push_back({VmfStructuralEdit::Type::Remove, entry, {}});
      }
    }
    for (auto i = retained; i < after->memberships.size(); ++i)
    {
      appended += "\"visgroupid\" " + quote(after->memberships[i]) + "\n";
    }
  }
  if (before->manualShown != after->manualShown)
  {
    if (p.manual)
    {
      if (after->manualShown)
      {
        values.push_back({*p.manual, *after->manualShown ? "\"1\"" : "\"0\""});
      }
      else
      {
        structure.push_back({VmfStructuralEdit::Type::Remove, *p.manual, {}});
      }
    }
    else if (after->manualShown)
    {
      appended +=
        std::string{"\"visgroupshown\" "} + (*after->manualShown ? "\"1\"\n" : "\"0\"\n");
    }
  }
}

void NativeVisgroups::emitTreeEdits(
  const VmfDocument& d,
  std::vector<VmfValueEdit>& values,
  std::vector<VmfStructuralEdit>& structure,
  std::string& roots) const
{
  const auto& s = *m_impl;
  s.document(d);
  auto localValues = std::vector<VmfValueEdit>{};
  auto localStructure = std::vector<VmfStructuralEdit>{};
  auto append = std::map<VmfEntryId, std::string>{};
  auto removed = std::set<uint64_t>{};
  // Parent equality alone is insufficient after several live moves: a loaded
  // row moved away and back must still append behind its retained siblings.
  auto siblingOrder = std::map<uint64_t, std::pair<size_t, bool>>{};
  for (const auto& now : s.live.records)
  {
    auto& [last, appended] = siblingOrder[now.parent ? now.parent->m_serial : 0];
    const auto* old = find(s.base.records, now.id);
    const auto position = old ? static_cast<size_t>(old - s.base.records.data()) + 1 : 0;
    if (old && old->parent == now.parent && !appended && position > last)
    {
      last = position;
    }
    else
    {
      appended = true;
      if (old)
      {
        removed.insert(old->id.m_serial);
      }
    }
  }
  for (const auto& old : s.base.records)
  {
    const auto* now = find(s.live.records, old.id);
    if (!now || now->parent != old.parent)
    {
      removed.insert(old.id.m_serial);
    }
    if (now && now->name != old.name)
    {
      const auto& p = s.trees.at(old.id.m_serial);
      if (p.name)
      {
        localValues.push_back({*p.name, quote(now->name)});
      }
      else
      {
        append[p.entry] += "\"name\" " + quote(now->name) + "\n";
      }
    }
    if (now && now->color != old.color)
    {
      if (!now->color)
      {
        reject("RGB removal is not an authored operation");
      }
      const auto& p = s.trees.at(old.id.m_serial);
      if (p.color)
      {
        localValues.push_back({*p.color, quote(colorText(*now->color))});
      }
      else
      {
        append[p.entry] += "\"color\" " + quote(colorText(*now->color)) + "\n";
      }
    }
  }
  // A moved source root uses subtree-local surgical edits. Retained children
  // stay in place, even when their values change; no remove/reappend reordering.
  const auto text = [&](auto&& self, const NativeVisgroupRecord& r) -> std::string {
    const auto* old = find(s.base.records, r.id);
    if (!old)
    {
      auto result = "visgroup\n{\n\"name\" " + quote(r.name) + "\n\"visgroupid\" "
                    + quote(r.nativeId) + "\n";
      if (r.color)
      {
        result += "\"color\" " + quote(colorText(*r.color)) + "\n";
      }
      for (const auto& child : s.live.records)
      {
        if (child.parent == std::optional<NativeVisgroupId>{r.id})
        {
          result += self(self, child) + "\n";
        }
      }
      return result + "}\n";
    }
    auto v = std::vector<VmfValueEdit>{};
    auto st = std::vector<VmfStructuralEdit>{};
    const auto collect = [&](auto&& visit, const NativeVisgroupRecord& row) -> void {
      const auto* previous = find(s.base.records, row.id);
      const auto& p = s.trees.at(row.id.m_serial);
      auto added = std::string{};
      if (previous->name != row.name)
      {
        if (p.name)
        {
          v.push_back({*p.name, quote(row.name)});
        }
        else
        {
          added += "\"name\" " + quote(row.name) + "\n";
        }
      }
      if (previous->color != row.color)
      {
        if (!row.color)
        {
          reject("RGB removal is not an authored operation");
        }
        if (p.color)
        {
          v.push_back({*p.color, quote(colorText(*row.color))});
        }
        else
        {
          added += "\"color\" " + quote(colorText(*row.color)) + "\n";
        }
      }
      for (const auto& child : s.base.records)
      {
        if (child.parent != std::optional<NativeVisgroupId>{row.id})
        {
          continue;
        }
        const auto* now = find(s.live.records, child.id);
        if (!now || removed.contains(child.id.m_serial))
        {
          st.push_back(
            {VmfStructuralEdit::Type::Remove, s.trees.at(child.id.m_serial).entry, {}});
        }
        else
        {
          visit(visit, *now);
        }
      }
      for (const auto& child : s.live.records)
      {
        if (child.parent != std::optional<NativeVisgroupId>{row.id})
        {
          continue;
        }
        const auto* prior = find(s.base.records, child.id);
        if (!prior || removed.contains(child.id.m_serial))
        {
          added += self(self, child) + "\n";
        }
      }
      if (!added.empty())
      {
        st.push_back({VmfStructuralEdit::Type::Append, p.entry, added});
      }
    };
    collect(collect, r);
    return d.serializeEntry(s.trees.at(r.id.m_serial).entry, v, st);
  };
  const auto suppressed = [&](const NativeVisgroupRecord& r) {
    auto parent = r.parent;
    while (parent)
    {
      if (removed.contains(parent->m_serial))
      {
        return true;
      }
      parent = find(s.base.records, *parent)->parent;
    }
    return false;
  };
  for (const auto& old : s.base.records)
  {
    if (removed.contains(old.id.m_serial) && !suppressed(old))
    {
      localStructure.push_back(
        {VmfStructuralEdit::Type::Remove, s.trees.at(old.id.m_serial).entry, {}});
    }
  }
  auto rootAppend = std::string{};
  for (const auto& now : s.live.records)
  {
    const auto* old = find(s.base.records, now.id);
    if (old && !removed.contains(now.id.m_serial))
    {
      continue;
    }
    // If its live ancestor is itself emitted, that ancestor owns the whole text.
    auto parent = now.parent;
    bool included = false;
    while (parent)
    {
      if (!find(s.base.records, *parent) || removed.contains(parent->m_serial))
      {
        included = true;
        break;
      }
      parent = find(s.live.records, *parent)->parent;
    }
    if (included)
    {
      continue;
    }
    const auto subtree = text(text, now);
    if (now.parent)
    {
      append[s.trees.at(now.parent->m_serial).entry] += subtree + "\n";
    }
    else
    {
      rootAppend += subtree + "\n";
    }
  }
  // Suppress patches under every source subtree already removed/moved.
  const auto underRemoved = [&](VmfEntryId e) {
    auto entry = std::optional<VmfEntryId>{e};
    while (entry)
    {
      for (const auto serial : removed)
      {
        if (s.trees.at(serial).entry == *entry)
        {
          return true;
        }
      }
      entry = d.entries()[*entry].parent;
    }
    return false;
  };
  for (const auto& v : localValues)
  {
    if (!underRemoved(v.entry))
    {
      values.push_back(v);
    }
  }
  structure.insert(structure.end(), localStructure.begin(), localStructure.end());
  for (const auto& [entry, fields] : append)
  {
    if (!underRemoved(entry))
    {
      structure.push_back({VmfStructuralEdit::Type::Append, entry, fields});
    }
  }
  if (!rootAppend.empty())
  {
    if (s.controller)
    {
      structure.push_back({VmfStructuralEdit::Type::Append, *s.controller, rootAppend});
    }
    else
    {
      roots += "visgroups\n{\n" + rootAppend + "}\n";
    }
  }
}
} // namespace tb::mdl
