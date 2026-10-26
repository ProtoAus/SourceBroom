/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/VmfDocument.h"

#include "vm/vec.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tb::mdl
{
class NativeCordons;

/** Opaque scene-scoped identity, unrelated to VMF entries and native numeric IDs.
 * A default handle requests creation. Handles survive scene moves, not replacement. */
template <typename Tag>
class NativeCordonHandle
{
private:
  std::shared_ptr<const void> m_scope;
  uint64_t m_serial = 0;
  NativeCordonHandle(std::shared_ptr<const void> scope, uint64_t serial)
    : m_scope{std::move(scope)}
    , m_serial{serial}
  {
  }
  friend class NativeCordons;

public:
  NativeCordonHandle() = default;
  bool isNew() const { return !m_scope; }
  bool operator==(const NativeCordonHandle&) const = default;
};
using NativeCordonId = NativeCordonHandle<struct NativeCordonRecordTag>;
using NativeCordonBoxId = NativeCordonHandle<struct NativeCordonBoxTag>;

enum class NativeCordonForm
{
  None,
  Modern,
  Legacy
};
struct NativeCordonBox
{
  NativeCordonBoxId id;
  // Keep ordered input: bbox constructors may silently sort reversed coordinates.
  vm::vec3d mins;
  vm::vec3d maxs;
  bool operator==(const NativeCordonBox&) const = default;
};
struct NativeCordonRecord
{
  NativeCordonId id;
  std::optional<std::string> name;
  std::optional<bool> active;
  std::vector<NativeCordonBox> boxes;
  bool operator==(const NativeCordonRecord&) const = default;
};
struct NativeCordonSnapshot
{
  NativeCordonForm form = NativeCordonForm::None;
  std::optional<bool> active;
  std::vector<NativeCordonRecord> records;
  uint64_t revision = 0;
  // Unsupported controller data stays preserved; unrelated scene editing is independent.
  std::string refusalReason;

private:
  std::shared_ptr<const void> m_scope;
  friend class NativeCordons;
};
/** Full desired local state, not a raw patch. Start from the current snapshot.
 * Preserve existing order/identities; default record/box IDs request additions.
 * None removes the controller. Legacy only admits bounds/active/removal in place. */
struct NativeCordonRequest
{
  NativeCordonSnapshot expected;
  NativeCordonForm form;
  std::optional<bool> active;
  std::vector<NativeCordonRecord> records;
  explicit NativeCordonRequest(const NativeCordonSnapshot& snapshot)
    : expected{snapshot}
    , form{snapshot.form}
    , active{snapshot.active}
    , records{snapshot.records}
  {
  }
};
enum class NativeCordonDirection
{
  Forward,
  Backward
};

class PreparedNativeCordonChange
{
private:
  struct Data;
  std::shared_ptr<Data> m_data;
  explicit PreparedNativeCordonChange(std::shared_ptr<Data> data);
  friend class NativeCordons;

public:
  const NativeCordonSnapshot& before() const;
  const NativeCordonSnapshot& after() const;
};

/** Small live overlay. No retained document pointers; provenance is private.
 * prepare is pure; nullopt is a semantic no-op. All refusals throw invalid_argument.
 * Initial apply also checks the staged UI revision; replay checks semantic
 * before/after independently of that monotonic revision. New UI requests must
 * explicitly supply their intended activation values (normally inactive).
 * Predictable refusals publish nothing and consume no identities. */
class NativeCordons
{
private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

public:
  static constexpr size_t MaxRecords = 1024;
  static constexpr size_t MaxBoxes = 4096;
  static constexpr size_t MaxNameBytes = 256;
  static constexpr size_t MaxTextBytes = 65536;
  static constexpr size_t MaxBatchItems = 5000;

  explicit NativeCordons(const VmfDocument& document);
  NativeCordons(NativeCordons&&) noexcept;
  NativeCordons& operator=(NativeCordons&&) noexcept;
  ~NativeCordons();
  NativeCordonSnapshot snapshot() const;
  std::optional<PreparedNativeCordonChange> prepare(
    const NativeCordonRequest& request) const;
  void apply(PreparedNativeCordonChange& change, NativeCordonDirection direction);

private:
  friend class VmfScene;
  // Internal serialization seam: root text is merged with other generated roots.
  void emitEdits(
    std::vector<VmfValueEdit>& values,
    std::vector<VmfStructuralEdit>& structure,
    std::string& roots) const;
};
} // namespace tb::mdl
