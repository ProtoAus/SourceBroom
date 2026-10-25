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
#include "img/ImageAlphaDomain.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace tb::fs
{
class FileSystem;
}

namespace tb::mdl
{
// Results are in Source search order (first entry has highest priority).
Result<std::vector<std::filesystem::path>> sourceSearchPaths(
  std::string_view gameInfo,
  const std::filesystem::path& gameDirectory,
  const std::vector<std::filesystem::path>& steamRoots = {});
enum class SourceMaterialMode
{
  Strict,
  // Resolve the base image/basic alpha policy, not full shaders or surface effects.
  BaseTexturePreview
};
struct SourceMaterialInfo
{
  std::filesystem::path baseTexture;
  img::ImageAlphaDomain alphaDomain;
  bool noCull = false;
  float alphaTestReference = 0.5f;
};
Result<SourceMaterialInfo> sourceMaterial(
  std::string_view vmt, SourceMaterialMode mode = SourceMaterialMode::Strict);
// Resolve bounded scalar Patch chains using the mounted filesystem, on demand.
Result<SourceMaterialInfo> loadSourceMaterial(
  const std::filesystem::path& path,
  const fs::FileSystem& fs,
  SourceMaterialMode mode = SourceMaterialMode::Strict);
Result<std::filesystem::path> sourceBaseTexture(
  std::string_view vmt, SourceMaterialMode mode = SourceMaterialMode::Strict);
// Explicit roots are manual overrides; otherwise inspect standard platform locations.
Result<std::vector<std::filesystem::path>> sourceSteamLibraries(
  const std::vector<std::filesystem::path>& steamRoots = {});
// Enumerate manifest-backed installs with immediate child gameinfo.txt directories.
Result<std::vector<std::filesystem::path>> sourceInstalledGames(
  const std::vector<std::filesystem::path>& steamRoots = {});
} // namespace tb::mdl
