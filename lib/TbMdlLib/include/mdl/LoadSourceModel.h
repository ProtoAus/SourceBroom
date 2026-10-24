/* Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "base/Result.h"
#include "fs/Reader.h"
#include "mdl/EntityModel.h"

#include <cstddef>
#include <filesystem>
namespace tb
{
class Logger;
namespace fs
{
class FileSystem;
}
namespace mdl
{
bool canLoadSourceModel(const std::filesystem::path& path, fs::Reader reader);
inline constexpr size_t SourceModelTextureBudget = 128u * 1024u * 1024u;
// Resolves only this model's sidecars and referenced skins. Owns the completed data.
// The aggregate retained-mip budget reserves 4 KiB for a shared magenta fallback.
// Over-budget/unsupported materials warn and use that fallback; budgets below
// 4 KiB refuse the model. Other material callers keep the decoder's default cap.
Result<EntityModelData> loadSourceModel(
  const std::filesystem::path& path,
  const fs::FileSystem& fs,
  Logger& logger,
  size_t decodedTextureBudget = SourceModelTextureBudget);
} // namespace mdl
} // namespace tb
