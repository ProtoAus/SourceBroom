/*
 Copyright (C) 2026 SourceBroom contributors

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "base/Result.h"

#include <cstddef>
#include <filesystem>
#include <optional>

namespace tb
{
namespace gl
{
class Texture;
}

namespace fs
{
class Reader;
} // namespace fs

namespace mdl
{

bool isVtfTexture(const std::filesystem::path& path);

// Additional retained-mip budget, checked before pixel allocation/decompression.
// The decoder's global 256 MiB limit still applies.
Result<gl::Texture> loadVtfTexture(
  fs::Reader& reader, std::optional<size_t> decodedByteBudget = std::nullopt);

} // namespace mdl
} // namespace tb
