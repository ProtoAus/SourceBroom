/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "base/Result.h"

#include <filesystem>
#include <optional>
#include <string_view>

namespace tb::mdl
{
struct VmfSaveResult
{
  std::optional<std::filesystem::path> backupPath;
};

/** Atomic native save. Existing files require an expected snapshot or explicit
 * overwrite approval. Changed snapshots are refused; replaced files get a unique
 * backup. No fallback to truncating the destination is permitted. */
Result<VmfSaveResult> saveVmfFile(
  const std::filesystem::path& path,
  std::string_view bytes,
  std::optional<std::string_view> expectedOriginal = std::nullopt,
  bool allowOverwrite = false);
} // namespace tb::mdl
