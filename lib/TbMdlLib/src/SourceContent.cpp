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

#include "mdl/SourceContent.h"

#include "fs/FileSystem.h"

#include "kd/string_format.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>

namespace tb::mdl
{
namespace
{
struct Entry
{
  std::string key;
  std::string value;
  std::vector<Entry> children;
  bool block;
};
class KeyValues
{
  std::string_view text;
  size_t pos = 0;
  size_t count = 0;
  bool present = false;
  std::string token()
  {
    present = false;
    while (pos < text.size())
    {
      if (std::isspace(static_cast<unsigned char>(text[pos])))
      {
        ++pos;
        continue;
      }
      if (text.substr(pos, 2) == "//")
      {
        while (pos < text.size() && text[pos] != '\n')
        {
          ++pos;
        }
        continue;
      }
      break;
    }
    if (pos == text.size())
    {
      return {};
    }
    present = true;
    if (text[pos] == '{' || text[pos] == '}')
    {
      return std::string(1, text[pos++]);
    }
    std::string out;
    if (text[pos] == '"')
    {
      ++pos;
      while (pos < text.size() && text[pos] != '"')
      {
        if (
          text[pos] == '\\' && pos + 1 < text.size()
          && (text[pos + 1] == '\\' || text[pos + 1] == '"'))
        {
          ++pos;
        }
        out += text[pos++];
      }
      if (pos == text.size())
      {
        throw std::runtime_error("Unterminated KeyValues string");
      }
      ++pos;
      return out;
    }
    while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos]))
           && text[pos] != '{' && text[pos] != '}')
    {
      out += text[pos++];
    }
    return out;
  }
  std::vector<Entry> entries(size_t depth)
  {
    if (depth > 64)
    {
      throw std::runtime_error("KeyValues nesting limit");
    }
    std::vector<Entry> out;
    for (auto key = token(); present; key = token())
    {
      if (key == "}")
      {
        if (!depth)
        {
          throw std::runtime_error("Unexpected KeyValues closing brace");
        }
        return out;
      }
      if (key.empty() || key == "{" || ++count > 100000)
      {
        throw std::runtime_error("Invalid KeyValues entry");
      }
      auto value = token();
      if (!present || value == "}")
      {
        throw std::runtime_error("Missing KeyValues value");
      }
      const auto block = value == "{";
      auto children = block ? entries(depth + 1) : std::vector<Entry>{};
      out.push_back(
        {kdl::str_to_lower(key), std::move(value), std::move(children), block});
    }
    if (depth)
    {
      throw std::runtime_error("Unclosed KeyValues block");
    }
    return out;
  }

public:
  explicit KeyValues(std::string_view input)
    : text{input}
  {
    if (input.size() > 16 * 1024 * 1024 || input.find('\0') != std::string_view::npos)
    {
      throw std::runtime_error("Invalid or oversized KeyValues input");
    }
  }
  std::vector<Entry> parse() { return entries(0); }
};
const Entry* child(const std::vector<Entry>& entries, const std::string& key)
{
  const auto it = std::ranges::find(entries, key, &Entry::key);
  return it == entries.end() ? nullptr : &*it;
}
std::string slashes(std::string value)
{
  std::ranges::replace(value, '\\', '/');
  return value;
}
std::string readText(const std::filesystem::path& path)
{
  std::ifstream stream{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}
constexpr size_t MaxMaterialFileSize = 1024u * 1024u;
constexpr size_t MaxMaterialTextSize = 4u * MaxMaterialFileSize;
constexpr size_t MaxMaterialIncludes = 16;
using MaterialReader = std::function<Result<std::string>(const std::filesystem::path&)>;

Entry resolveMaterial(
  const std::string_view text,
  const MaterialReader& read,
  size_t& remaining,
  std::vector<std::filesystem::path>& active)
{
  if (text.size() > MaxMaterialFileSize || text.size() > remaining)
  {
    throw std::runtime_error("Source material text budget exceeded");
  }
  remaining -= text.size();
  auto entries = KeyValues{text}.parse();
  if (entries.size() != 1 || !entries.front().block)
  {
    throw std::runtime_error("Invalid VMT shader block");
  }
  auto shader = std::move(entries.front());
  if (shader.key != "patch")
  {
    return shader;
  }
  if (!read)
  {
    throw std::runtime_error("Source Patch requires a mounted include resolver");
  }
  auto commands = std::set<std::string>{};
  auto parameters = std::set<std::string>{};
  for (const auto& command : shader.children)
  {
    if (!commands.insert(command.key).second)
    {
      throw std::runtime_error("Repeated Source Patch command");
    }
    if (command.key == "include")
    {
      if (command.block || command.value.empty())
      {
        throw std::runtime_error("Invalid Source Patch include");
      }
    }
    else if (command.key == "insert" || command.key == "replace")
    {
      if (!command.block)
      {
        throw std::runtime_error("Source Patch operation must be a block");
      }
      for (const auto& parameter : command.children)
      {
        if (parameter.block || parameter.value.empty())
        {
          throw std::runtime_error("Unsupported nested or empty Source Patch value");
        }
        // Refuse overlap as well as duplicates: no guessed cross-command order.
        if (!parameters.insert(parameter.key).second)
        {
          throw std::runtime_error("Ambiguous Source Patch parameter");
        }
      }
    }
    else
    {
      throw std::runtime_error("Unsupported Source Patch command: " + command.key);
    }
  }
  const auto* include = child(shader.children, "include");
  if (!include)
  {
    throw std::runtime_error("Missing Source Patch include");
  }
  const auto name = slashes(kdl::str_to_lower(include->value));
  auto path = std::filesystem::path{name};
  if (
    path.is_absolute() || path.has_root_name() || name.find(':') != std::string::npos
    || path.extension() != ".vmt" || path.begin() == path.end()
    || *path.begin() != "materials")
  {
    throw std::runtime_error("Source Patch include must be a relative materials VMT");
  }
  for (const auto& part : path)
  {
    if (part == "..")
    {
      throw std::runtime_error("Parent traversal in Source Patch include");
    }
  }
  path = path.lexically_normal();
  if (
    active.size() >= MaxMaterialIncludes
    || std::ranges::find(active, path) != active.end())
  {
    throw std::runtime_error("Source Patch include cycle or depth limit");
  }
  auto included = read(path);
  if (included.is_error())
  {
    throw std::runtime_error(
      "Cannot read Source Patch include: " + path.generic_string());
  }
  active.push_back(path);
  auto base = resolveMaterial(included | kdl::value(), read, remaining, active);
  active.pop_back();
  struct Location
  {
    size_t index;
    size_t count;
  };
  // Stable indices survive inserts; ordered lookup bounds work for many keys.
  auto inherited = std::map<std::string, Location>{};
  for (size_t i = 0; i < base.children.size(); ++i)
  {
    const auto [position, added] =
      inherited.try_emplace(base.children[i].key, Location{i, 1});
    if (!added)
    {
      ++position->second.count;
    }
  }
  for (const auto& command : shader.children)
  {
    if (command.key == "include")
    {
      continue;
    }
    for (const auto& parameter : command.children)
    {
      const auto existing = inherited.find(parameter.key);
      if (existing != inherited.end())
      {
        if (existing->second.count != 1)
        {
          throw std::runtime_error("Ambiguous inherited Source Patch parameter");
        }
        auto& entry = base.children[existing->second.index];
        if (entry.block)
        {
          throw std::runtime_error("Cannot replace a Source Patch block with a scalar");
        }
        entry = parameter;
      }
      else if (command.key == "insert")
      {
        inherited.emplace(parameter.key, Location{base.children.size(), 1});
        base.children.push_back(parameter);
      }
    }
  }
  return base;
}

Result<std::filesystem::path> sourceTexturePath(const std::string& value)
{
  auto path = std::filesystem::path{slashes(kdl::str_to_lower(value))};
  if (path.is_absolute() || path.has_root_name())
  {
    return Error{"Absolute Source texture path"};
  }
  for (const auto& part : path)
  {
    if (part == "..")
    {
      return Error{"Parent traversal in Source texture path"};
    }
  }
  path.replace_extension(".vtf");
  return std::filesystem::path{"materials"} / path;
}

std::optional<std::array<float, 3>> parseColorModulation(std::string_view value)
{
  const auto trim = [](std::string_view& text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    {
      text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    {
      text.remove_suffix(1);
    }
  };
  trim(value);
  // Preserve the existing base-image preview's neutral scalar omission.
  if (value == "1")
  {
    return std::array{1.0f, 1.0f, 1.0f};
  }
  auto scale = 1.0f;
  if (!value.empty() && (value.front() == '[' || value.front() == '{'))
  {
    const auto closing = value.front() == '[' ? ']' : '}';
    if (value.back() != closing)
    {
      return std::nullopt;
    }
    scale = value.front() == '{' ? 255.0f : 1.0f;
    value.remove_prefix(1);
    value.remove_suffix(1);
  }
  auto result = std::array<float, 3>{};
  for (auto& component : result)
  {
    trim(value);
    if (!value.empty() && value.front() == '+')
    {
      value.remove_prefix(1);
    }
    if (value.empty())
    {
      return std::nullopt;
    }
    const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), component);
    if (parsed.ec != std::errc{} || !std::isfinite(component) || component < 0.0f
        || component > scale
        || (parsed.ptr != value.data() + value.size()
            && !std::isspace(static_cast<unsigned char>(*parsed.ptr))))
    {
      return std::nullopt;
    }
    value.remove_prefix(static_cast<size_t>(parsed.ptr - value.data()));
    component /= scale;
  }
  trim(value);
  return value.empty() ? std::optional{result} : std::nullopt;
}

Result<SourceMaterialInfo> parseMaterial(
  const std::string_view vmt, const SourceMaterialMode mode, const MaterialReader& read)
{
  try
  {
    auto remaining = MaxMaterialTextSize;
    auto active = std::vector<std::filesystem::path>{};
    const auto shader = resolveMaterial(vmt, read, remaining, active);
    const auto terrainBlend = shader.key == "worldvertextransition"
                              && mode == SourceMaterialMode::TerrainBlendPreview;
    const auto preview = mode == SourceMaterialMode::BaseTexturePreview
                         || mode == SourceMaterialMode::TerrainBlendPreview;
    if (
      !terrainBlend && shader.key != "lightmappedgeneric" && shader.key != "unlitgeneric"
      && shader.key != "vertexlitgeneric")
    {
      return Error{"Unsupported Source shader: " + shader.key};
    }
    for (const auto& entry : shader.children)
    {
      // Basic preview omits these optional effects. Terrain mode admits two color
      // layers and a blend mask, never proxies, transforms or conditional blocks.
      const auto previewParameter = preview
        && (entry.key == "$bumpmap" || entry.key == "$bumpmap2"
            || entry.key == "$normalmapalphaenvmapmask" || entry.key == "$ssbump"
            || entry.key == "$detail" || entry.key == "$detailscale"
            || entry.key == "$detailblendfactor" || entry.key == "$detailblendmode"
            || entry.key == "$detailtint" || entry.key == "$envmap"
            || entry.key == "$envmapmask" || entry.key == "$envmaptint"
            || entry.key == "$envmapcontrast" || entry.key == "$envmapsaturation"
            || entry.key == "$selfillum" || entry.key == "$selfillummask"
            || entry.key == "$selfillumtint" || entry.key == "$phong"
            || entry.key == "$phongboost" || entry.key == "$phongexponent"
            || entry.key == "$phongfresnelranges" || entry.key == "$nocull"
            || entry.key == "$halflambert" || entry.key == "$nodecal"
            || entry.key == "$vertexcolor" || entry.key == "$vertexalpha"
            || entry.key == "$translucent" || entry.key == "$alphatest"
            || entry.key == "$alphatestreference" || entry.key == "$alpha" || entry.key == "$roughnessmap"
            || entry.key == "$ambientocclusionmap" || entry.key == "$notint"
            || entry.key == "$treesway" || entry.key == "$tintmasktexture"
            || entry.key == "$blendtintbybasealpha" || entry.key == "$envmaplightscale"
            || entry.key == "$color" || entry.key == "$color2");
      if (entry.block || (!previewParameter && entry.key != "$basetexture"
          && !(terrainBlend && (entry.key == "$basetexture2"
                                || entry.key == "$blendmodulatetexture"))
          && entry.key != "$surfaceprop" && entry.key != "$surfaceprop2"
          && entry.key != "$keywords" && entry.key != "$model"
          && !entry.key.starts_with('%') && !entry.key.starts_with("$compile")))
      {
        return Error{"Unsupported Source material feature: " + entry.key};
      }
    }
    if (std::ranges::count(shader.children, std::string{"$basetexture"}, &Entry::key) > 1)
    {
      return Error{"Repeated Source $basetexture requires explicit resolution"};
    }
    const auto* base = child(shader.children, "$basetexture");
    if (!base || base->value.empty())
    {
      return Error{"Missing Source $basetexture"};
    }
    const auto primaryPath = sourceTexturePath(base->value);
    if (primaryPath.is_error())
    {
      return std::get<Error>(primaryPath.error());
    }
    auto secondaryPath = std::optional<std::filesystem::path>{};
    auto modulationPath = std::optional<std::filesystem::path>{};
    if (terrainBlend)
    {
      if (
        std::ranges::count(shader.children, std::string{"$basetexture2"}, &Entry::key)
        > 1)
      {
        return Error{"Repeated Source $basetexture2 requires explicit resolution"};
      }
      const auto* second = child(shader.children, "$basetexture2");
      if (!second || second->value.empty())
      {
        return Error{"Missing Source $basetexture2"};
      }
      auto path = sourceTexturePath(second->value);
      if (path.is_error())
      {
        return std::get<Error>(path.error());
      }
      secondaryPath = std::move(path) | kdl::value();
      if (
        std::ranges::count(
          shader.children, std::string{"$blendmodulatetexture"}, &Entry::key)
        > 1)
      {
        return Error{
          "Repeated Source $blendmodulatetexture requires explicit resolution"};
      }
      if (const auto* mask = child(shader.children, "$blendmodulatetexture"))
      {
        if (mask->value.empty())
        {
          return Error{"Empty Source $blendmodulatetexture"};
        }
        auto maskPath = sourceTexturePath(mask->value);
        if (maskPath.is_error())
        {
          return std::get<Error>(maskPath.error());
        }
        modulationPath = std::move(maskPath) | kdl::value();
      }
    }
    auto translucent = false;
    auto alphaTest = false;
    auto noCull = false;
    for (const auto* flag : {"$translucent", "$alphatest", "$nocull", "$selfillum"})
    {
      if (!modulationPath && std::string_view{flag} == "$selfillum")
      {
        continue;
      }
      if (std::ranges::count(shader.children, std::string{flag}, &Entry::key) > 1)
      {
        return Error{"Repeated Source material flag requires explicit resolution"};
      }
      if (const auto* entry = child(shader.children, flag))
      {
        const auto value = kdl::str_to_lower(entry->value);
        auto enabled = value == "true";
        if (value != "true" && value != "false")
        {
          auto number = float{};
          const auto parsed =
            std::from_chars(value.data(), value.data() + value.size(), number);
          if (
            parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()
            || (number != 0.0f && number != 1.0f))
          {
            return Error{"Unsupported Source material flag: " + std::string{flag}};
          }
          enabled = number == 1.0f;
        }
        if (entry->key == "$selfillum")
        {
          if (enabled)
          {
            return Error{
              "Source blend modulation preview does not support self illumination"};
          }
        }
        else if (entry->key == "$alphatest")
        {
          alphaTest = enabled;
        }
        else if (entry->key == "$nocull")
        {
          noCull = enabled;
        }
        else
        {
          translucent = enabled;
        }
      }
    }
    auto colorModulation = std::array{1.0f, 1.0f, 1.0f};
    if (preview)
    {
      for (const auto* parameter : {"$color", "$color2"})
      {
        if (std::ranges::count(shader.children, std::string{parameter}, &Entry::key) > 1)
        {
          return Error{"Repeated Source RGB parameter requires explicit resolution"};
        }
        if (const auto* entry = child(shader.children, parameter))
        {
          const auto color = parseColorModulation(entry->value);
          if (!color)
          {
            return Error{"Unsupported Source RGB literal: " + std::string{parameter}};
          }
          for (size_t i = 0; i < colorModulation.size(); ++i)
          {
            colorModulation[i] *= (*color)[i];
          }
        }
      }
      if (colorModulation != std::array{1.0f, 1.0f, 1.0f})
      {
        for (const auto* parameter :
             {"$blendtintbybasealpha", "$notint", "$tintmasktexture"})
        {
          if (
            std::ranges::count(shader.children, std::string{parameter}, &Entry::key) > 1)
          {
            return Error{"Repeated Source RGB coupling requires explicit resolution"};
          }
          if (const auto* entry = child(shader.children, parameter))
          {
            const auto disabled = entry->key == "$tintmasktexture" ? entry->value.empty()
                                                                   : entry->value == "0";
            if (!disabled)
            {
              return Error{"Unsupported coupled Source RGB preview: " + entry->key};
            }
          }
        }
      }
    }
    auto alphaTestReference = 0.5f;
    if (
      std::ranges::count(shader.children, std::string{"$alphatestreference"}, &Entry::key)
      > 1)
    {
      return Error{"Repeated Source $alphatestreference requires explicit resolution"};
    }
    if (const auto* entry = child(shader.children, "$alphatestreference"))
    {
      auto value = std::string_view{entry->value};
      while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
      {
        value.remove_prefix(1);
      }
      while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
      {
        value.remove_suffix(1);
      }
      if (!value.empty() && value.front() == '+')
      {
        value.remove_prefix(1);
      }
      const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), alphaTestReference);
      if (
        parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()
        || !std::isfinite(alphaTestReference) || alphaTestReference <= 0.0f
        || alphaTestReference > 1.0f)
      {
        return Error{
          "Unsupported Source $alphatestreference: expected finite value above 0 and at "
          "most 1"};
      }
    }
    auto opacity = 1.0f;
    if (std::ranges::count(shader.children, std::string{"$alpha"}, &Entry::key) > 1)
    {
      return Error{"Repeated Source $alpha requires explicit resolution"};
    }
    if (const auto* entry = child(shader.children, "$alpha"))
    {
      if (terrainBlend || !translucent || alphaTest)
      {
        return Error{
          "Source $alpha preview requires explicit translucency without cutout or "
          "terrain layers"};
      }
      auto value = std::string_view{entry->value};
      while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
      {
        value.remove_prefix(1);
      }
      while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
      {
        value.remove_suffix(1);
      }
      if (!value.empty() && value.front() == '+')
      {
        value.remove_prefix(1);
      }
      const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), opacity);
      if (
        parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()
        || !std::isfinite(opacity) || opacity < 0.0f || opacity > 1.0f)
      {
        return Error{"Unsupported Source $alpha: expected finite opacity from 0 to 1"};
      }
    }
    return SourceMaterialInfo{
      primaryPath | kdl::value(),
      alphaTest     ? img::ImageAlphaDomain::Binary
      : translucent ? img::ImageAlphaDomain::Graduated
                    : img::ImageAlphaDomain::Opaque,
      noCull,
      alphaTestReference,
      std::move(secondaryPath),
      opacity,
      std::move(modulationPath),
      colorModulation};
  }
  catch (const std::runtime_error& e)
  {
    return Error{e.what()};
  }
}

} // namespace

Result<SourceMaterialInfo> sourceMaterial(
  const std::string_view vmt, const SourceMaterialMode mode)
{
  return parseMaterial(vmt, mode, {});
}

Result<SourceMaterialInfo> loadSourceMaterial(
  const std::filesystem::path& path,
  const fs::FileSystem& fs,
  const SourceMaterialMode mode)
{
  auto remainingReadBytes = MaxMaterialTextSize;
  const auto read = [&](const std::filesystem::path& filePath) -> Result<std::string> {
    return fs.openFile(filePath) | kdl::and_then([&](auto file) -> Result<std::string> {
             const auto size = file->size();
             if (size > MaxMaterialFileSize)
             {
               return Error{"Source material file size limit exceeded"};
             }
             if (size > remainingReadBytes)
             {
               return Error{"Source material text budget exceeded"};
             }
             remainingReadBytes -= size;
             auto text = std::string(size, '\0');
             auto reader = file->reader();
             reader.read(text.data(), text.size());
             return text;
           });
  };
  return read(path) | kdl::and_then([&](const auto& text) {
           return parseMaterial(text, mode, read);
         });
}

Result<std::filesystem::path> sourceBaseTexture(
  std::string_view vmt, const SourceMaterialMode mode)
{
  return sourceMaterial(vmt, mode)
         | kdl::transform([](auto info) { return std::move(info.baseTexture); });
}
Result<std::vector<std::filesystem::path>> sourceSearchPaths(
  std::string_view gameInfo,
  const std::filesystem::path& gameDirectory,
  const std::vector<std::filesystem::path>& steamRoots)
{
  // Joining an empty GameConfig search path adds a trailing separator. It must
  // not change the engine-relative base from the install root to the mod folder.
  const auto directory =
    gameDirectory.filename().empty() ? gameDirectory.parent_path() : gameDirectory;
  try
  {
    const auto entries = KeyValues{gameInfo}.parse();
    const auto* game = child(entries, "gameinfo");
    const auto* fs = game ? child(game->children, "filesystem") : nullptr;
    const auto* search = fs ? child(fs->children, "searchpaths") : nullptr;
    if (!search)
    {
      return Error{"Missing gameinfo FileSystem/SearchPaths"};
    }
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : search->children)
    {
      // Game+Mod and Game+Game_Write are also content search paths.
      auto ids = entry.key;
      std::ranges::replace(ids, '+', ' ');
      if ((" " + ids + " ").find(" game ") == std::string::npos)
      {
        continue;
      }
      auto value = slashes(entry.value);
      const auto lower = kdl::str_to_lower(value);
      auto root = directory.parent_path();
      if (lower.starts_with("|gameinfo_path|"))
      {
        root = directory;
        value.erase(0, 15);
      }
      else if (lower.starts_with("|all_source_engine_paths|"))
      {
        value.erase(0, 25);
      }
      else if (value.find('|') != std::string::npos)
      {
        return Error{"Unsupported gameinfo search path token: " + value};
      }
      auto path = (root / value).lexically_normal();
      if (value.find('*') != std::string::npos)
      {
        if (path.filename() != "*")
        {
          return Error{"Unsupported gameinfo wildcard: " + value};
        }
        std::error_code ec;
        std::vector<std::filesystem::path> matches;
        for (std::filesystem::directory_iterator it{path.parent_path(), ec}, end;
             !ec && it != end;
             it.increment(ec))
        {
          const auto stem = it->path().stem().string();
          const auto numbered = stem.size() >= 4 && stem[stem.size() - 4] == '_'
                                && std::ranges::all_of(
                                  stem.substr(stem.size() - 3),
                                  [](unsigned char c) { return c >= '0' && c <= '9'; });
          if (it->is_directory(ec) || (it->path().extension() == ".vpk" && !numbered))
          {
            matches.push_back(it->path());
          }
        }
        if (ec && ec != std::errc::no_such_file_or_directory)
        {
          return Error{ec.message()};
        }
        std::ranges::sort(matches);
        paths.insert(paths.end(), matches.begin(), matches.end());
      }
      else
      {
        paths.push_back(std::move(path));
      }
    }
    if (const auto* mounts = child(game->children, "mount"))
    {
      auto librariesResult = sourceSteamLibraries(steamRoots);
      if (librariesResult.is_error())
      {
        return std::move(librariesResult);
      }
      const auto libraries = std::move(librariesResult) | kdl::value();
      for (const auto& app : mounts->children)
      {
        if (
          !app.block || app.key.empty()
          || !std::ranges::all_of(
            app.key, [](unsigned char c) { return std::isdigit(c); }))
        {
          return Error{"Invalid Source mount app ID"};
        }
        std::filesystem::path install;
        for (const auto& library : libraries)
        {
          const auto manifest =
            KeyValues{
              readText(library / "steamapps" / ("appmanifest_" + app.key + ".acf"))}
              .parse();
          const auto* state = child(manifest, "appstate");
          const auto* dir = state ? child(state->children, "installdir") : nullptr;
          if (!dir)
          {
            continue;
          }
          const auto relative = std::filesystem::path{slashes(dir->value)};
          if (relative.has_parent_path() || relative.has_root_path())
          {
            return Error{"Invalid Steam install directory"};
          }
          install = library / "steamapps/common" / relative;
          break;
        }
        std::vector<std::filesystem::path> mounted;
        bool head = false;
        for (const auto& folder : app.children)
        {
          if (folder.key == "head")
          {
            head = folder.value == "1";
            continue;
          }
          if (folder.key == "fallback")
          {
            continue;
          }
          if (!folder.block)
          {
            return Error{"Unsupported Source mount option: " + folder.key};
          }
          if (install.empty())
          {
            continue;
          }
          for (const auto& entry : folder.children)
          {
            auto relative = std::filesystem::path{slashes(folder.key)}
                            / std::filesystem::path{slashes(entry.value)};
            if (relative.is_absolute() || relative.has_root_name())
            {
              return Error{"Absolute Source mount path"};
            }
            for (const auto& part : relative)
            {
              if (part == "..")
              {
                return Error{"Parent traversal in Source mount path"};
              }
            }
            if (entry.key == "vpk")
            {
              relative.replace_extension(".vpk");
            }
            else if (entry.key != "dir")
            {
              return Error{"Unsupported Source mount entry: " + entry.key};
            }
            mounted.push_back(install / relative);
          }
        }
        if (install.empty())
        {
          if (const auto* fallback = child(app.children, "fallback"))
          {
            mounted.push_back(directory.parent_path() / slashes(fallback->value));
          }
        }
        if (head)
        {
          paths.insert(paths.begin(), mounted.begin(), mounted.end());
        }
        else
        {
          paths.insert(paths.end(), mounted.begin(), mounted.end());
        }
      }
    }
    return paths;
  }
  catch (const std::runtime_error& e)
  {
    return Error{e.what()};
  }
}
Result<std::vector<std::filesystem::path>> sourceSteamLibraries(
  const std::vector<std::filesystem::path>& steamRoots)
{
  auto roots = steamRoots;
  if (roots.empty())
  {
    if (const auto* program = std::getenv("ProgramFiles(x86)"))
    {
      roots.emplace_back(std::filesystem::path{program} / "Steam");
    }
    if (const auto* home = std::getenv("HOME"))
    {
      roots.emplace_back(std::filesystem::path{home} / ".steam/steam");
      roots.emplace_back(std::filesystem::path{home} / ".local/share/Steam");
      roots.emplace_back(
        std::filesystem::path{home} / "Library/Application Support/Steam");
    }
  }
  try
  {
    std::vector<std::filesystem::path> libraries;
    const auto add = [&](const std::filesystem::path& path) {
      if (std::ranges::find(libraries, path) == libraries.end())
      {
        libraries.push_back(path);
      }
    };
    for (const auto& root : roots)
    {
      std::error_code ec;
      if (!std::filesystem::is_directory(root / "steamapps", ec))
      {
        continue;
      }
      add(root);
      const auto entries =
        KeyValues{readText(root / "steamapps/libraryfolders.vdf")}.parse();
      const auto* folders = child(entries, "libraryfolders");
      if (!folders)
      {
        continue;
      }
      for (const auto& folder : folders->children)
      {
        const auto* path = child(folder.children, "path");
        if (path)
        {
          add(std::filesystem::path{slashes(path->value)});
        }

        else if (
          !folder.block && !folder.key.empty()
          && std::ranges::all_of(
            folder.key, [](unsigned char c) { return std::isdigit(c); }))
        {
          add(std::filesystem::path{slashes(folder.value)});
        }
      }
    }
    return libraries;
  }
  catch (const std::runtime_error& e)
  {
    return Error{e.what()};
  }
}
Result<std::vector<std::filesystem::path>> sourceInstalledGames(
  const std::vector<std::filesystem::path>& steamRoots)
{
  return sourceSteamLibraries(steamRoots)
         | kdl::and_then(
           [](const auto& libraries) -> Result<std::vector<std::filesystem::path>> {
             try
             {
               std::vector<std::filesystem::path> games;
               for (const auto& library : libraries)
               {
                 const auto apps = library / "steamapps";
                 std::error_code ec;
                 for (std::filesystem::directory_iterator it{apps, ec}, end;
                      !ec && it != end;
                      it.increment(ec))
                 {
                   if (
                     it->path().extension() != ".acf"
                     || !it->path().filename().string().starts_with("appmanifest_"))
                   {
                     continue;
                   }
                   const auto entries = KeyValues{readText(it->path())}.parse();
                   const auto* app = child(entries, "appstate");
                   const auto* dir = app ? child(app->children, "installdir") : nullptr;
                   if (!dir)
                   {
                     continue;
                   }
                   const auto relative = std::filesystem::path{slashes(dir->value)};
                   if (relative.has_parent_path() || relative.has_root_path())
                   {
                     return Error{"Invalid Steam install directory"};
                   }
                   const auto install = apps / "common" / relative;
                   std::error_code inner;
                   for (std::filesystem::directory_iterator game{install, inner}, last;
                        !inner && game != last;
                        game.increment(inner))
                   {
                     if (
                       game->is_directory(inner)
                       && std::filesystem::is_regular_file(
                         game->path() / "gameinfo.txt", inner))
                     {
                       games.push_back(game->path());
                     }
                   }
                 }
               }
               std::ranges::sort(games);
               games.erase(std::unique(games.begin(), games.end()), games.end());
               return games;
             }
             catch (const std::runtime_error& e)
             {
               return Error{e.what()};
             }
           });
}

} // namespace tb::mdl
