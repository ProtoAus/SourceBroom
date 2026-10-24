/* Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later */
#include "mdl/LoadSourceModel.h"

#include "base/Logger.h"
#include "fs/FileSystem.h"
#include "fs/PathInfo.h"
#include "gl/IndexRangeMap.h"
#include "gl/Material.h"
#include "gl/PrimType.h"
#include "gl/TextureBuffer.h"
#include "gl/TextureResource.h"
#include "mdl/LoadVtfTexture.h"
#include "mdl/SourceContent.h"
#include "mdl/SourceModelReader.h"

#include "kd/path_utils.h"

#include <map>
#include <stdexcept>

namespace tb::mdl
{
namespace
{
std::string readFile(
  const fs::FileSystem& fs, const std::filesystem::path& path, size_t limit)
{
  auto result = fs.openFile(path);
  if (result.is_error())
  {
    throw std::runtime_error("Source model: cannot open " + path.generic_string());
  }
  const auto file = std::move(result) | kdl::value();
  auto reader = file->reader();
  if (reader.size() > limit)
  {
    throw std::runtime_error("Source model: file size limit exceeded");
  }
  auto bytes = std::string(reader.size(), '\0');
  reader.read(bytes.data(), bytes.size());
  return bytes;
}
// Entity-model materials currently require ready pixels at upload (unlike the brush
// catalog). Decode only this prop's referenced textures, with an aggregate budget.
struct Materials
{
  const fs::FileSystem& fs;
  Logger& logger;
  // Reserve one shared 32x32 RGBA fallback, even if every material decodes.
  static constexpr size_t FallbackBytes = 32u * 32u * 4u;
  size_t remaining;
  std::shared_ptr<gl::TextureResource> fallback;
  std::map<std::string, std::shared_ptr<gl::TextureResource>> cache;

  gl::Material load(const std::string& name, const std::vector<std::string>& directories)
  {
    auto path = std::filesystem::path{};
    for (const auto& directory : directories)
    {
      auto candidate = std::filesystem::path{"materials"} / directory / name;
      candidate.replace_extension(".vmt");
      if (fs.pathInfo(candidate) == fs::PathInfo::File)
      {
        path = std::move(candidate);
        break;
      }
    }
    const auto key = path.empty() ? name : path.generic_string();
    if (const auto it = cache.find(key); it != cache.end())
    {
      return gl::Material{key, it->second};
    }
    if (cache.size() >= 256)
    {
      throw std::runtime_error("Source model: referenced material limit exceeded");
    }
    auto texture = [&]() -> std::shared_ptr<gl::TextureResource> {
      try
      {
        if (path.empty())
        {
          throw std::runtime_error("material not found: " + name);
        }
        const auto vmt = readFile(fs, path, 1024u * 1024u);
        auto base = sourceBaseTexture(vmt, SourceMaterialMode::BaseTexturePreview);
        if (base.is_error())
        {
          throw std::runtime_error("unsupported or invalid VMT: " + key);
        }
        const auto bytes =
          readFile(fs, std::move(base) | kdl::value(), SourceModelMaxFileSize);
        auto reader = fs::Reader::from(bytes.data(), bytes.data() + bytes.size());
        auto decoded = loadVtfTexture(reader, remaining);
        if (decoded.is_error())
        {
          throw std::runtime_error("unsupported or invalid VTF: " + key);
        }
        auto pixels = std::move(decoded) | kdl::value();
        for (const auto& buffer : pixels.buffersIfLoaded())
        {
          remaining -= buffer.size();
        }
        return gl::createTextureResource(std::move(pixels));
      }
      catch (const std::exception& e)
      {
        logger.warn() << "Source prop material fallback: " << e.what();
        if (!fallback)
        {
          auto pixels = gl::TextureBuffer{FallbackBytes};
          for (size_t i = 0; i < pixels.size(); i += 4)
          {
            pixels.data()[i] = 255;
            pixels.data()[i + 1] = 0;
            pixels.data()[i + 2] = 255;
            pixels.data()[i + 3] = 255;
          }
          fallback = gl::createTextureResource(
            gl::Texture{
              32, 32, RgbaF{}, GL_RGBA, gl::NoEmbeddedDefaults{}, std::move(pixels)});
        }
        return fallback;
      }
    }();
    cache.emplace(key, texture);
    return gl::Material{key, std::move(texture)};
  }
};
} // namespace

bool canLoadSourceModel(const std::filesystem::path& path, fs::Reader reader)
{
  return kdl::path_to_lower(path.extension()) == ".mdl" && reader.size() >= 4
         && reader.readUnsignedInt<uint32_t>() == 0x54534449
         // IDST v10 belongs to GoldSrc and is supported by Assimp. Unknown
         // Source versions remain ours so unsupported features cannot fall through.
         && (reader.size() < 8 || reader.readUnsignedInt<uint32_t>() != 10);
}

Result<EntityModelData> loadSourceModel(
  const std::filesystem::path& path,
  const fs::FileSystem& fs,
  Logger& logger,
  const size_t decodedTextureBudget)
{
  try
  {
    auto vvdPath = path;
    vvdPath.replace_extension(".vvd");
    auto vtxPath = path;
    vtxPath.replace_extension(".dx90.vtx");
    const auto geometry = readSourceModel(
      readFile(fs, path, SourceModelMaxFileSize),
      readFile(fs, vvdPath, SourceModelMaxFileSize),
      readFile(fs, vtxPath, SourceModelMaxFileSize));
    auto data = EntityModelData{PitchType::Normal, Orientation::Oriented};
    auto& frame = data.addFrame(
      "static",
      vm::bbox3f{
        vm::vec3f{geometry.min[0], geometry.min[1], geometry.min[2]},
        vm::vec3f{geometry.max[0], geometry.max[1], geometry.max[2]}});
    if (decodedTextureBudget < Materials::FallbackBytes)
    {
      throw std::runtime_error("Source model: texture budget cannot hold fallback");
    }
    auto materials =
      Materials{fs, logger, decodedTextureBudget - Materials::FallbackBytes, {}, {}};
    for (size_t i = 0; i < geometry.meshes.size(); ++i)
    {
      const auto& mesh = geometry.meshes[i];
      auto& surface = data.addSurface("mesh" + std::to_string(i), 1);
      auto skins = std::vector<gl::Material>{};
      for (const auto& name : mesh.materials)
      {
        skins.push_back(materials.load(name, geometry.materialDirectories));
      }
      surface.setSkins(std::move(skins));
      auto vertices = std::vector<EntityModelVertex>{};
      vertices.reserve(mesh.indices.size());
      for (const auto index : mesh.indices)
      {
        const auto& v = geometry.vertices[index];
        vertices.emplace_back(
          vm::vec3f{v.position[0], v.position[1], v.position[2]},
          vm::vec2f{v.uv[0], v.uv[1]});
      }
      surface.addMesh(
        frame,
        std::move(vertices),
        gl::IndexRangeMap{gl::PrimType::Triangles, 0, mesh.indices.size()});
    }
    return data;
  }
  catch (const std::exception& e)
  {
    return Error{e.what()};
  }
}
} // namespace tb::mdl
