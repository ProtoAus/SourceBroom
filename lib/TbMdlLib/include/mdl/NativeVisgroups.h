/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/VmfDocument.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tb::mdl
{
class NativeVisgroups;
template <typename Tag>
class NativeVisgroupHandle
{
private:
  std::shared_ptr<const void> m_scope;
  uint64_t m_serial = 0;
  NativeVisgroupHandle(std::shared_ptr<const void> scope, uint64_t serial)
    : m_scope{std::move(scope)}
    , m_serial{serial}
  {
  }
  friend class NativeVisgroups;
  friend class VmfScene;

public:
  NativeVisgroupHandle() = default;
  bool operator==(const NativeVisgroupHandle&) const = default;
  // Incidental process-local index ordering, not native/tree order or authority.
  bool operator<(const NativeVisgroupHandle& other) const
  {
    if (m_scope.get() != other.m_scope.get())
      return std::less<const void*>{}(m_scope.get(), other.m_scope.get());
    return m_serial < other.m_serial;
  }
};
using NativeVisgroupId = NativeVisgroupHandle<struct NativeVisgroupRecordTag>;
using NativeVisgroupMemberId = NativeVisgroupHandle<struct NativeVisgroupMemberTag>;
struct NativeVisgroupDraftKey
{
  uint64_t value = 0;
  bool operator==(const NativeVisgroupDraftKey&) const = default;
};
using NativeVisgroupTarget = std::variant<NativeVisgroupId, NativeVisgroupDraftKey>;
enum class NativeVisgroupMemberKind
{
  WorldSolid,
  PointEntity,
  BrushEntity,
  InternalSolid,
  Group,
  Unsupported
};
using NativeVisgroupColor = std::array<int, 3>;
struct NativeVisgroupRecord
{
  NativeVisgroupId id;
  std::optional<NativeVisgroupId> parent;
  std::string nativeId;
  std::string name;
  std::string refusalReason;
  std::optional<NativeVisgroupColor> color;
  // Color-only protection; unrelated edits still preserve unfamiliar scalar bytes.
  std::string colorRefusalReason;
  bool operator==(const NativeVisgroupRecord&) const = default;
};
struct NativeVisgroupMember
{
  NativeVisgroupMemberId id;
  std::optional<VmfEntryId> source;
  std::optional<VmfEntryId> editor;
  std::string nativeId;
  NativeVisgroupMemberKind kind = NativeVisgroupMemberKind::Unsupported;
  std::vector<std::string> memberships;
  std::optional<bool> manualShown;
  std::optional<bool> autoShown;
  bool wrapperHidden = false;
  std::string refusalReason;
  std::optional<NativeVisgroupMemberId> owner;
  bool active = false;
  bool projected = false;
  bool editable = false;
  std::string contextRefusalReason;
  bool operator==(const NativeVisgroupMember&) const = default;
};
/** Scene supplies structural METADATA admission only, after proving current
 * identity/liveness/ownership. Native/session visibility is NOT admission here.
 * Missing context is unbound, never proof of inactivity. No Node pointers. */
struct NativeVisgroupMemberContext
{
  NativeVisgroupMemberId id;
  std::optional<NativeVisgroupMemberId> owner;
  NativeVisgroupMemberKind kind = NativeVisgroupMemberKind::Unsupported;
  bool active = true;
  bool projected = false;
  bool editable = false;
  std::string refusalReason;
  bool operator==(const NativeVisgroupMemberContext&) const = default;
};
struct NativeVisgroupContext
{
  std::vector<NativeVisgroupMemberContext> members;
  uint64_t generation = 0;
  bool editable = true;
  std::string refusalReason;
};
struct NativeVisgroupSnapshot
{
  std::vector<NativeVisgroupRecord> records;
  std::vector<NativeVisgroupMember> members;
  uint64_t revision = 0;
  std::string refusalReason;

private:
  std::shared_ptr<const void> m_scope;
  uint64_t m_contextGeneration = 0;
  friend class NativeVisgroups;
};
struct NativeVisgroupDesiredRecord
{
  NativeVisgroupTarget key;
  std::optional<NativeVisgroupTarget> parent;
  std::string name;
};
struct NativeVisgroupMembershipAction
{
  std::vector<NativeVisgroupMemberId> roots;
  NativeVisgroupTarget visgroup;
  bool assign = true;
};
struct NativeVisgroupManualAction
{
  std::vector<NativeVisgroupTarget> roots;
  bool shown = true;
};
struct NativeVisgroupColorAction
{
  NativeVisgroupTarget visgroup;
  NativeVisgroupColor color;
};
struct NativeVisgroupRequest
{
  NativeVisgroupSnapshot expected;
  std::vector<NativeVisgroupDesiredRecord> records;
  std::vector<NativeVisgroupMembershipAction> memberships;
  std::optional<NativeVisgroupManualAction> manual;
  std::optional<NativeVisgroupColorAction> color;
  explicit NativeVisgroupRequest(const NativeVisgroupSnapshot& snapshot);
};
enum class NativeVisgroupDirection
{
  Forward,
  Backward
};
class PreparedNativeVisgroupChange
{
private:
  struct Data;
  std::shared_ptr<Data> m_data;
  explicit PreparedNativeVisgroupChange(std::shared_ptr<Data> data);
  friend class NativeVisgroups;

public:
  const NativeVisgroupSnapshot& before() const;
  const NativeVisgroupSnapshot& after() const;
};
/** Once-checked immutable-document provenance for a single authored revision.
 * Owns scope only, never a document pointer. Create before any owner emits;
 * stale/foreign sessions refuse before adding output. */
class NativeVisgroupEmissionSession
{
private:
  std::shared_ptr<const void> m_scope;
  uint64_t m_revision = 0;
  NativeVisgroupEmissionSession(std::shared_ptr<const void> scope, uint64_t revision)
    : m_scope{std::move(scope)}
    , m_revision{revision}
  {
  }
  friend class NativeVisgroups;

public:
  NativeVisgroupEmissionSession() = default;
};
/** Immutable-document provenance and live authored overlay. Pure prepare returns
 * nullopt for a no-op; predictable refusals throw invalid_argument without publish.
 * First publication checks numeric freshness; replay checks semantic context.
 * Scene lifecycle registration/activation must later stage live clone/fresh/park
 * metadata with bindings; source/prototype bytes cannot certify live membership. */
class NativeVisgroups
{
private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
  friend class VmfScene;
  uint64_t revision() const;
  std::set<std::string> liveRecordIds() const;
  const std::vector<std::string>& rawReferences(NativeVisgroupMemberId id) const;
  std::vector<NativeVisgroupMemberId> pendingMemberIds(size_t count) const;
  const NativeVisgroupMember& member(NativeVisgroupMemberId id) const;
  struct Registration
  {
    std::string nativeId;
    NativeVisgroupMemberKind kind;
    std::optional<NativeVisgroupMemberId> copy;
  };
  std::vector<NativeVisgroupMemberId> registerMembers(
    const std::vector<Registration>& registrations);

public:
  static constexpr size_t MaxRecords = 4096;
  static constexpr size_t MaxMembers = 100000;
  static constexpr size_t MaxMemberships = 1000000;
  static constexpr size_t MaxNameBytes = 256;
  static constexpr size_t MaxTextBytes = 4 * 1024 * 1024;
  static constexpr size_t MaxWorkItems = 2000000;
  static constexpr size_t MaxDepth = 128;
  static constexpr size_t MaxIdBytes = 256;
  static constexpr size_t MaxReservedBytes = 1024 * 1024;
  explicit NativeVisgroups(const VmfDocument& document);
  NativeVisgroups(NativeVisgroups&&) noexcept;
  NativeVisgroups& operator=(NativeVisgroups&&) noexcept;
  ~NativeVisgroups();
  NativeVisgroupSnapshot snapshot() const;
  NativeVisgroupSnapshot snapshot(const NativeVisgroupContext& context) const;
  std::optional<PreparedNativeVisgroupChange> prepare(
    const NativeVisgroupRequest& request, const NativeVisgroupContext& context) const;
  void apply(
    PreparedNativeVisgroupChange& change,
    NativeVisgroupDirection direction,
    const NativeVisgroupContext& context);
  // One caller-owned root accumulator. Neither emitter creates AppendRoot edits.
  void emitTreeEdits(
    const VmfDocument& document,
    std::vector<VmfValueEdit>& values,
    std::vector<VmfStructuralEdit>& structure,
    std::string& roots) const;
  // Verify exact immutable document bytes ONCE outside the per-owner loop.
  NativeVisgroupEmissionSession beginEmission(const VmfDocument& document) const;
  // Existing fields patched locally; missing fields returned to Scene's ONE
  // editor composer (existing editor or new block shared with physical groupid).
  // Template clones must use subtree-local edit vectors, never global patches.
  void emitMemberEdits(
    const NativeVisgroupEmissionSession& session,
    NativeVisgroupMemberId member,
    std::vector<VmfValueEdit>& values,
    std::vector<VmfStructuralEdit>& structure,
    std::string& appendedEditorFields) const;
};
} // namespace tb::mdl
