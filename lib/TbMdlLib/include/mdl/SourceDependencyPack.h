/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace tb::mdl
{
class VmfDocument;

struct SourceDependencyPackLimits
{
  size_t maxFiles = 2048;
  size_t maxReferences = 16384;
  uint64_t maxFileBytes = 64u * 1024u * 1024u;
  uint64_t maxTotalBytes = 256u * 1024u * 1024u;
};

struct SourceDependencyPackOptions
{
  // Explicitly selected, custom-only directories, relative to game content layout.
  // This acknowledgement is NOT a licence determination. Never pass game mounts.
  std::vector<std::filesystem::path> customRoots;
  bool redistributionRightsAcknowledged = false;
  // Optional read-only input. Validate the exact included snapshot's header/target,
  // not just an earlier path read. This does not validate whole-map contents.
  std::optional<std::filesystem::path> compiledBsp;
  std::optional<uint32_t> compiledBspExpectedVersion;
  SourceDependencyPackLimits limits;
};

enum class SourceDependencyPackIssueKind
{
  Excluded,
  Missing,
  Unresolved,
  Budget,
  Error
};

struct SourceDependencyPackIssue
{
  SourceDependencyPackIssueKind kind;
  std::string reference;
  std::string message;
};

struct SourceDependencyPackFile
{
  std::string archivePath;
  uint64_t size;
  uint32_t crc32;
};

struct SourceDependencyPackResult
{
  bool written = false;
  std::string error;
};

/** Immutable bounded binary snapshots: packaging cannot silently change the reviewed
 * plan. Discovery is deliberately incomplete (scripts, runtime/branch-specific
 * dependencies). A successful ZIP is NOT a certification of a self-contained or licensed
 * map.
 */
class SourceDependencyPackPlan
{
public:
  const std::vector<SourceDependencyPackFile>& files() const;
  const std::vector<SourceDependencyPackIssue>& issues() const;
  bool canWrite() const;
  bool assessmentComplete()
    const; // Always false for this bounded discovery implementation.

  // Requires a separate user approval of this exact plan. CREATE_NEW only; no overwrite.
  SourceDependencyPackResult writeZip(
    const std::filesystem::path& output, bool explicitlyApproved = false) const;

private:
  std::vector<SourceDependencyPackFile> m_files;
  std::vector<std::string> m_bytes;
  std::vector<SourceDependencyPackIssue> m_issues;
  bool m_authorized = false;
  bool m_blocked = false;

  friend SourceDependencyPackPlan planSourceDependencyPack(
    const VmfDocument&, const SourceDependencyPackOptions&);
};

/** Pure archive-name policy without filesystem reads. Validates the COMPLETE
 * maps/<filename> path including its prefix; throws on an unsafe/oversized name. */
std::string sourceDependencyBspArchivePath(const std::filesystem::path& bsp);

// Reads document tokens only; never serializes or modifies the native VMF or BSP.
SourceDependencyPackPlan planSourceDependencyPack(
  const VmfDocument& document, const SourceDependencyPackOptions& options);
} // namespace tb::mdl
