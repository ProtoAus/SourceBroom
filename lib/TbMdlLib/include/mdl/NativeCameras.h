/*
 Copyright (C) 2026 SourceBroom contributors
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
class NativeCameraId
{
private:
  std::shared_ptr<const void> m_scope;
  uint64_t m_serial = 0;
  NativeCameraId(std::shared_ptr<const void> scope, uint64_t serial)
    : m_scope{std::move(scope)}
    , m_serial{serial}
  {
  }
  friend class NativeCameras;

public:
  NativeCameraId() = default;
  bool isNew() const { return !m_scope; }
  bool operator==(const NativeCameraId&) const = default;
};
struct NativeCameraRecord
{
  NativeCameraId id;
  vm::vec3d eye;
  vm::vec3d look;
  bool operator==(const NativeCameraRecord&) const = default;
};
enum class NativeCameraActiveKind
{
  Absent,
  None,
  Camera
};
struct NativeCameraActivation
{
  NativeCameraActiveKind kind = NativeCameraActiveKind::Absent;
  NativeCameraId camera;
  bool operator==(const NativeCameraActivation&) const = default;
};
struct NativeCameraSnapshot
{
  bool present = false;
  NativeCameraActivation active;
  std::vector<NativeCameraRecord> records;
  uint64_t revision = 0;
  std::string refusalReason;

private:
  std::shared_ptr<const void> m_scope;
  friend class NativeCameras;
};
struct NativeCameraRequest
{
  NativeCameraSnapshot expected;
  bool present;
  NativeCameraActivation active;
  std::vector<NativeCameraRecord> records;
  explicit NativeCameraRequest(const NativeCameraSnapshot& snapshot)
    : expected{snapshot}
    , present{snapshot.present}
    , active{snapshot.active}
    , records{snapshot.records}
  {
  }
};
enum class NativeCameraDirection
{
  Forward,
  Backward
};
class PreparedNativeCameraChange
{
private:
  struct Data;
  std::shared_ptr<Data> m_data;
  explicit PreparedNativeCameraChange(std::shared_ptr<Data> data);
  friend class NativeCameras;

public:
  PreparedNativeCameraChange() = default;
  const NativeCameraSnapshot& before() const;
  const NativeCameraSnapshot& after() const;
};
/** Scoped live metadata, separate from immutable source entries and geometry IDs. */
class NativeCameras
{
private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

public:
  static constexpr size_t MaxRecords = 1024;
  static constexpr size_t MaxTextBytes = 65536;
  explicit NativeCameras(const VmfDocument& document);
  NativeCameras(NativeCameras&&) noexcept;
  NativeCameras& operator=(NativeCameras&&) noexcept;
  ~NativeCameras();
  NativeCameraSnapshot snapshot() const;
  std::optional<PreparedNativeCameraChange> prepare(
    const NativeCameraRequest& request) const;
  void apply(PreparedNativeCameraChange& change, NativeCameraDirection direction);
  void emitEdits(
    std::vector<VmfValueEdit>& values,
    std::vector<VmfStructuralEdit>& structure,
    std::string& roots) const;
};
} // namespace tb::mdl
