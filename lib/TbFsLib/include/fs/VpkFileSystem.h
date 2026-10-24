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
#include "fs/ImageFileSystem.h"
namespace tb::fs
{
// The directory archive path is required only for entries in numbered archives.
class VpkFileSystem : public ImageFileSystem<File>
{
  std::filesystem::path m_path;

public:
  explicit VpkFileSystem(std::shared_ptr<File> file, std::filesystem::path path = {});

private:
  Result<void> doReadDirectory() override;
};
} // namespace tb::fs
