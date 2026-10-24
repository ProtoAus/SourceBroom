/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/SourceEditorMetadata.h"

#include <cmath>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace tb::mdl
{
namespace
{
// Match the native scene's decoding: only escaped quotes and backslashes decode.
std::string text(const VmfDocument& document, const VmfToken& token)
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

class Reader
{
private:
  const VmfDocument& m_document;
  SourceEditorMetadata m_result;
  std::map<std::string, std::vector<VmfEntryId>> m_objectIds;
  std::map<std::string, std::vector<VmfEntryId>> m_groupIds;
  std::map<std::string, std::vector<VmfEntryId>> m_visgroupIds;

  std::string key(const VmfEntryId entry) const
  {
    return text(m_document, m_document.entries()[entry].key);
  }

  void issue(const VmfEntryId entry, const SourceMetadataIssue code, std::string message)
  {
    m_result.diagnostics.push_back({entry, code, std::move(message)});
  }

  std::vector<VmfEntryId> children(
    const VmfEntryId entry, const std::string& name, const bool blocks) const
  {
    auto result = std::vector<VmfEntryId>{};
    for (const auto child : m_document.entries()[entry].children)
    {
      if (m_document.entries()[child].isBlock() == blocks && key(child) == name)
      {
        result.push_back(child);
      }
    }
    return result;
  }

  std::optional<std::string> property(
    const VmfEntryId entry, const std::string& name, const bool required = false)
  {
    const auto matches = children(entry, name, false);
    if (matches.empty())
    {
      if (required)
      {
        issue(entry, SourceMetadataIssue::MissingProperty, "Missing " + name);
      }
      return std::nullopt;
    }
    if (matches.size() != 1)
    {
      issue(entry, SourceMetadataIssue::DuplicateProperty, "Ambiguous repeated " + name);
      return std::nullopt;
    }
    return text(m_document, *m_document.entries()[matches.front()].value);
  }

  std::optional<std::string> decimal(
    const VmfEntryId entry, const std::string& name, const std::string& value)
  {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
    {
      issue(entry, SourceMetadataIssue::InvalidValue, "Invalid decimal ID for " + name);
      return std::nullopt;
    }
    const auto first = value.find_first_not_of('0');
    return first == std::string::npos ? "0" : value.substr(first);
  }

  std::optional<std::string> id(
    const VmfEntryId entry, const std::string& name, const bool required = false)
  {
    const auto value = property(entry, name, required);
    return value ? decimal(entry, name, *value) : std::nullopt;
  }

  std::optional<bool> flag(const VmfEntryId entry, const std::string& name)
  {
    const auto value = property(entry, name);
    if (!value)
    {
      return std::nullopt;
    }
    if (*value == "0" || *value == "1")
    {
      return *value == "1";
    }
    issue(entry, SourceMetadataIssue::InvalidValue, "Expected 0 or 1 for " + name);
    return std::nullopt;
  }

  std::optional<vm::vec3d> point(
    const VmfEntryId entry, const std::string& name, const bool brackets)
  {
    const auto value = property(entry, name, true);
    if (!value)
    {
      return std::nullopt;
    }
    auto input = std::istringstream{*value};
    input.imbue(std::locale::classic());
    auto point = vm::vec3d{};
    auto delimiter = char{};
    auto closing = brackets ? ']' : '\0';
    input >> std::ws;
    auto valid = true;
    if (brackets)
    {
      valid = bool(input >> delimiter) && delimiter == '[';
    }
    else if (input.peek() == '(')
    {
      input.get();
      closing = ')';
    }
    for (size_t i = 0; i < 3 && valid; ++i)
    {
      valid = bool(input >> point[i]) && std::isfinite(point[i]);
    }
    if (valid && closing)
    {
      valid = bool(input >> delimiter) && delimiter == closing;
    }
    input >> std::ws;
    if (!valid || !input.eof())
    {
      issue(
        entry, SourceMetadataIssue::InvalidValue, "Invalid finite vector for " + name);
      return std::nullopt;
    }
    return point;
  }

  void box(const VmfEntryId entry, SourceEditorCordon& cordon)
  {
    const auto mins = point(entry, "mins", false);
    const auto maxs = point(entry, "maxs", false);
    if (!mins || !maxs)
    {
      return;
    }
    for (size_t i = 0; i < 3; ++i)
    {
      if ((*mins)[i] >= (*maxs)[i] || !std::isfinite((*maxs)[i] - (*mins)[i]))
      {
        issue(
          entry,
          SourceMetadataIssue::InvalidValue,
          "Cordon bounds must have finite positive extent");
        return;
      }
    }
    cordon.boxes.emplace_back(*mins, *maxs);
  }

  void object(const VmfEntryId entry, const bool hidden)
  {
    const auto name = key(entry);
    if (name == "hidden")
    {
      for (const auto child : m_document.entries()[entry].children)
      {
        if (m_document.entries()[child].isBlock())
        {
          object(child, true);
        }
      }
      return;
    }
    if (name != "world" && name != "entity" && name != "solid" && name != "group")
    {
      return;
    }
    auto member = SourceEditorMembership{};
    member.entry = entry;
    member.nativeId = id(entry, "id", true);
    member.hiddenByContainer = hidden;
    if (member.nativeId)
    {
      m_objectIds[*member.nativeId].push_back(entry);
    }
    const auto editors = children(entry, "editor", true);
    if (editors.size() > 1)
    {
      issue(
        entry, SourceMetadataIssue::DuplicateProperty, "Ambiguous repeated editor block");
    }
    else if (!editors.empty())
    {
      const auto editor = editors.front();
      member.groupId = id(editor, "groupid");
      member.visgroupShown = flag(editor, "visgroupshown");
      member.visgroupAutoShown = flag(editor, "visgroupautoshown");
      auto seen = std::set<std::string>{};
      for (const auto ref : children(editor, "visgroupid", false))
      {
        const auto native =
          decimal(ref, "visgroupid", text(m_document, *m_document.entries()[ref].value));
        if (native)
        {
          if (!seen.insert(*native).second)
          {
            issue(
              ref,
              SourceMetadataIssue::DuplicateProperty,
              "Repeated visgroup membership " + *native);
          }
          member.visgroupIds.push_back(*native);
        }
      }
    }
    if (name == "group")
    {
      m_result.groups.push_back({entry, member.nativeId, member.groupId, std::nullopt});
      if (member.nativeId)
      {
        m_groupIds[*member.nativeId].push_back(entry);
      }
    }
    m_result.memberships.push_back(std::move(member));
    for (const auto child : m_document.entries()[entry].children)
    {
      if (m_document.entries()[child].isBlock())
      {
        object(child, hidden);
      }
    }
  }

  void visgroup(const VmfEntryId entry, const std::optional<VmfEntryId> parent)
  {
    const auto native = id(entry, "visgroupid", true);
    const auto name = property(entry, "name");
    m_result.visgroups.push_back({entry, native, parent, name.value_or("")});
    if (native)
    {
      m_visgroupIds[*native].push_back(entry);
    }
    for (const auto child : children(entry, "visgroup", true))
    {
      visgroup(child, entry);
    }
  }

  void duplicates(const std::map<std::string, std::vector<VmfEntryId>>& ids)
  {
    for (const auto& [native, entries] : ids)
    {
      if (entries.size() > 1)
      {
        for (const auto entry : entries)
        {
          issue(entry, SourceMetadataIssue::DuplicateId, "Duplicate native ID " + native);
        }
      }
    }
  }

  std::optional<VmfEntryId> resolve(
    const VmfEntryId owner,
    const std::string& native,
    const std::map<std::string, std::vector<VmfEntryId>>& ids,
    const std::string& kind)
  {
    const auto found = ids.find(native);
    if (found == ids.end())
    {
      issue(
        owner, SourceMetadataIssue::DanglingReference, "Missing " + kind + " " + native);
      return std::nullopt;
    }
    if (found->second.size() != 1 || (kind == "group" && m_objectIds[native].size() != 1))
    {
      issue(
        owner,
        SourceMetadataIssue::AmbiguousReference,
        "Ambiguous " + kind + " " + native);
      return std::nullopt;
    }
    return found->second.front();
  }

  void memberships()
  {
    duplicates(m_objectIds);
    duplicates(m_visgroupIds);
    auto parents = std::map<VmfEntryId, VmfEntryId>{};
    for (auto& member : m_result.memberships)
    {
      if (member.groupId)
      {
        member.group = resolve(member.entry, *member.groupId, m_groupIds, "group");
        if (member.group)
        {
          parents.emplace(member.entry, *member.group);
        }
      }
      for (const auto& native : member.visgroupIds)
      {
        if (const auto target = resolve(member.entry, native, m_visgroupIds, "visgroup"))
        {
          member.visgroups.push_back(*target);
        }
      }
    }
    for (auto& group : m_result.groups)
    {
      const auto parent = parents.find(group.entry);
      if (parent != parents.end())
      {
        group.parent = parent->second;
      }
    }
    // Cyclic ancestry is never exposed as a resolved parent/membership.
    auto invalid = std::set<VmfEntryId>{};
    auto complete = std::set<VmfEntryId>{};
    for (const auto& group : m_result.groups)
    {
      auto path = std::vector<VmfEntryId>{};
      auto visited = std::set<VmfEntryId>{};
      auto current = group.entry;
      while (true)
      {
        if (!visited.insert(current).second || invalid.count(current))
        {
          for (const auto entry : path)
          {
            invalid.insert(entry);
          }
          break;
        }
        if (complete.count(current))
        {
          break;
        }
        path.push_back(current);
        const auto parent = parents.find(current);
        if (parent == parents.end())
        {
          break;
        }
        current = parent->second;
      }
      complete.insert(path.begin(), path.end());
    }
    for (auto& group : m_result.groups)
    {
      if (invalid.count(group.entry))
      {
        issue(group.entry, SourceMetadataIssue::Cycle, "Group ancestry reaches a cycle");
        group.parent.reset();
      }
    }
    for (auto& member : m_result.memberships)
    {
      if (member.group && invalid.count(*member.group))
      {
        member.group.reset();
      }
    }
  }

  void cameras(const VmfEntryId root)
  {
    const auto begin = m_result.cameras.size();
    for (const auto entry : children(root, "camera", true))
    {
      auto camera = SourceEditorCamera{
        entry, point(entry, "position", true), point(entry, "look", true)};
      if (camera.eye && camera.look && !camera.navigable())
      {
        issue(
          entry,
          SourceMetadataIssue::InvalidValue,
          "Camera requires a finite nonzero navigation direction");
      }
      m_result.cameras.push_back(std::move(camera));
    }
    const auto active = property(root, "activecamera");
    if (!active || *active == "-1")
    {
      return;
    }
    const auto index = decimal(root, "activecamera", *active);
    if (!index)
    {
      return;
    }
    // Compare decimal strings first, avoiding overflow even for hostile IDs.
    const auto count = std::to_string(m_result.cameras.size() - begin);
    if (
      index->size() > count.size() || (index->size() == count.size() && *index >= count))
    {
      issue(
        root,
        SourceMetadataIssue::DanglingReference,
        "Active camera index is out of range");
      return;
    }
    auto offset = size_t{0};
    for (const auto digit : *index)
    {
      offset = offset * 10 + size_t(digit - '0');
    }
    const auto& camera = m_result.cameras[begin + offset];
    if (camera.navigable())
    {
      m_result.activeCamera = camera.entry;
    }
    else
    {
      issue(root, SourceMetadataIssue::InvalidValue, "Active camera is not navigable");
    }
  }

  void cordon(const VmfEntryId entry, const bool legacy)
  {
    auto cordon = SourceEditorCordon{
      entry, property(entry, "name").value_or(""), flag(entry, "active"), {}};
    if (legacy)
    {
      box(entry, cordon);
      cordon.boundsValid = cordon.boxes.size() == 1;
    }
    else
    {
      const auto boxes = children(entry, "box", true);
      if (boxes.empty())
      {
        issue(entry, SourceMetadataIssue::MissingProperty, "Cordon has no boxes");
      }
      for (const auto child : boxes)
      {
        box(child, cordon);
      }
      cordon.boundsValid = !boxes.empty() && cordon.boxes.size() == boxes.size();
    }
    m_result.cordons.push_back(std::move(cordon));
  }

public:
  explicit Reader(const VmfDocument& document)
    : m_document{document}
  {
  }

  SourceEditorMetadata read()
  {
    auto cameraRoots = std::vector<VmfEntryId>{};
    auto cordonRoots = std::vector<VmfEntryId>{};
    for (const auto root : m_document.roots())
    {
      if (!m_document.entries()[root].isBlock())
      {
        continue;
      }
      const auto name = key(root);
      if (name == "world" || name == "entity" || name == "hidden")
      {
        object(root, false);
      }
      else if (name == "visgroups")
      {
        for (const auto child : children(root, "visgroup", true))
        {
          visgroup(child, std::nullopt);
        }
      }
      else if (name == "cameras")
      {
        cameraRoots.push_back(root);
        cameras(root);
      }
      else if (name == "cordon")
      {
        cordon(root, true);
      }
      else if (name == "cordons")
      {
        cordonRoots.push_back(root);
        m_result.cordonsActive = flag(root, "active");
        for (const auto child : children(root, "cordon", true))
        {
          cordon(child, false);
        }
      }
    }
    if (cameraRoots.size() > 1)
    {
      m_result.activeCamera.reset();
      issue(
        cameraRoots.back(),
        SourceMetadataIssue::DuplicateProperty,
        "Ambiguous cameras blocks");
    }
    if (cordonRoots.size() > 1)
    {
      m_result.cordonsActive.reset();
      issue(
        cordonRoots.back(),
        SourceMetadataIssue::DuplicateProperty,
        "Ambiguous cordons blocks");
    }
    memberships();
    return std::move(m_result);
  }
};
} // namespace

bool SourceEditorCamera::navigable() const
{
  if (!eye || !look)
  {
    return false;
  }
  const auto direction = *look - *eye;
  auto lengthSquared = 0.0;
  for (size_t i = 0; i < 3; ++i)
  {
    if (!std::isfinite((*eye)[i]) || !std::isfinite((*look)[i]))
    {
      return false;
    }
    lengthSquared += direction[i] * direction[i];
  }
  return std::isfinite(lengthSquared) && lengthSquared > 0.0;
}

SourceEditorMetadata SourceEditorMetadata::read(const VmfDocument& document)
{
  return Reader{document}.read();
}
} // namespace tb::mdl
