/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/SourceDependencyPack.h"

#include "mdl/VmfDocument.h"

#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace tb::mdl
{
namespace
{
namespace fs = std::filesystem;

std::string lower(std::string text)
{
  for (auto& c : text)
  {
    if (c >= 'A' && c <= 'Z')
    {
      c = char(c + ('a' - 'A'));
    }
  }
  return text;
}

// Portable ASCII game paths only. No drive/UNC, dot segments, stream names,
// reserved Windows names, extraction aliases, or separators disguised by escapes.
std::string gamePath(std::string text)
{
  if (text.empty() || text.size() > 240)
  {
    throw std::runtime_error("Empty/long path");
  }
  std::replace(text.begin(), text.end(), '\\', '/');
  text = lower(std::move(text));
  if (text.front() == '/' || text.back() == '/')
  {
    throw std::runtime_error("Absolute/empty path component");
  }
  size_t start = 0;
  while (start < text.size())
  {
    const auto end = text.find('/', start);
    const auto part = text.substr(start, end == std::string::npos ? end : end - start);
    if (
      part.empty() || part == "." || part == ".." || part.back() == '.'
      || part.back() == ' ')
    {
      throw std::runtime_error("Unsafe path component");
    }
    for (const auto c : part)
    {
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'
            || c == '.' || c == ' '))
      {
        throw std::runtime_error("Nonportable path character (including colon)");
      }
    }
    const auto base = part.substr(0, part.find('.'));
    if (
      base == "con" || base == "prn" || base == "aux" || base == "nul"
      || (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) && base[3] >= '0' && base[3] <= '9'))
    {
      throw std::runtime_error("Reserved path name");
    }
    if (end == std::string::npos)
    {
      break;
    }
    start = end + 1;
  }
  return text;
}

bool linked(const fs::path& path)
{
#ifdef _WIN32
  const auto attrs = GetFileAttributesW(path.c_str());
  if (attrs == INVALID_FILE_ATTRIBUTES)
  {
    throw std::runtime_error("Cannot inspect path attributes");
  }
  return (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
  return fs::is_symlink(fs::symlink_status(path));
#endif
}

fs::path checkedAbsolute(const fs::path& path)
{
  const auto absolute = fs::absolute(path).lexically_normal();
  auto prefix = absolute.root_path();
  if (linked(prefix))
  {
    throw std::runtime_error("Linked root refused");
  }
  for (const auto& part : absolute.relative_path())
  {
    prefix /= part;
    if (linked(prefix))
    {
      throw std::runtime_error("Symlink/reparse point refused");
    }
  }
  const auto canonical = fs::canonical(absolute);
#ifdef _WIN32
  if (lower(canonical.generic_string()) != lower(absolute.generic_string()))
#else
  if (canonical != absolute)
#endif
  {
    throw std::runtime_error("Path changed during containment validation");
  }
  return canonical;
}

// Resolves case-insensitively with an explicit collision refusal on every platform.
// No recursive indexing, and directory enumeration is bounded independently.
std::optional<fs::path> resolve(const fs::path& root, const std::string& relative)
{
  auto current = root;
  for (const auto& component : fs::path(relative))
  {
    std::optional<fs::path> match;
    size_t count = 0;
    for (const auto& entry : fs::directory_iterator(current))
    {
      if (++count > 16384)
      {
        throw std::runtime_error("Directory enumeration budget exceeded");
      }
      if (lower(entry.path().filename().string()) == component.string())
      {
        if (match)
        {
          throw std::runtime_error("Case-ambiguous custom path");
        }
        match = entry.path();
      }
    }
    if (!match)
    {
      return std::nullopt;
    }
    if (linked(*match))
    {
      throw std::runtime_error("Symlink/reparse dependency refused");
    }
    current = *match;
  }
  return current;
}

#ifdef _WIN32
struct Handle
{
  HANDLE value = INVALID_HANDLE_VALUE;
  ~Handle()
  {
    if (value != INVALID_HANDLE_VALUE)
    {
      CloseHandle(value);
    }
  }
};

std::string handlePath(HANDLE handle)
{
  std::array<wchar_t, 32768> buffer{};
  const auto length = GetFinalPathNameByHandleW(
    handle, buffer.data(), DWORD(buffer.size()), FILE_NAME_NORMALIZED);
  if (!length || length >= buffer.size())
  {
    throw std::runtime_error("Cannot verify open file path");
  }
  std::wstring path(buffer.data(), length);
  if (path.starts_with(L"\\\\?\\UNC\\"))
  {
    path = L"\\\\" + path.substr(8);
  }
  else if (path.starts_with(L"\\\\?\\"))
  {
    path.erase(0, 4);
  }
  return lower(fs::path(path).generic_string());
}
#endif

std::string readExact(const fs::path& root, const fs::path& path, uint64_t limit)
{
  // Verify against the opened object, not merely a canonicalized name before open.
#ifdef _WIN32
  Handle file{CreateFileW(
    path.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ,
    nullptr,
    OPEN_EXISTING,
    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN,
    nullptr)};
  if (file.value == INVALID_HANDLE_VALUE)
  {
    throw std::runtime_error("Cannot open dependency");
  }
  BY_HANDLE_FILE_INFORMATION info{};
  if (
    !GetFileInformationByHandle(file.value, &info)
    || (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
    || handlePath(file.value) != lower(path.generic_string()))
  {
    throw std::runtime_error("Opened dependency escaped or is not a regular file");
  }
  const auto rootName = lower(root.generic_string()) + "/";
  if (!handlePath(file.value).starts_with(rootName))
  {
    throw std::runtime_error("Dependency outside custom root");
  }
  const auto size = (uint64_t(info.nFileSizeHigh) << 32u) | info.nFileSizeLow;
#else
  struct Fd
  {
    int value;
    ~Fd()
    {
      if (value >= 0)
      {
        ::close(value);
      }
    }
  };
  // Open every component from the filesystem root without following any symlink.
  Fd file{::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
  if (file.value < 0)
  {
    throw std::runtime_error("Cannot open filesystem root");
  }
  const auto relative = path.lexically_relative(root);
  if (relative.empty() || *relative.begin() == "..")
  {
    throw std::runtime_error("Dependency outside root");
  }
  const auto components = path.relative_path();
  for (auto it = components.begin(); it != components.end(); ++it)
  {
    const bool last = std::next(it) == components.end();
    const auto next = ::openat(
      file.value,
      it->c_str(),
      O_RDONLY | O_NOFOLLOW | O_CLOEXEC | (last ? O_NONBLOCK : O_DIRECTORY));
    if (next < 0)
    {
      throw std::runtime_error("Cannot safely open dependency component");
    }
    ::close(file.value);
    file.value = next;
  }
  struct stat info{};
  if (::fstat(file.value, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0)
  {
    throw std::runtime_error("Dependency is not a regular file");
  }
  const auto size = uint64_t(info.st_size);
#endif
  if (size > limit || size > std::numeric_limits<size_t>::max())
  {
    throw std::runtime_error("Dependency byte budget exceeded");
  }
  std::string bytes(static_cast<size_t>(size), '\0');
  size_t done = 0;
  while (done < bytes.size())
  {
#ifdef _WIN32
    DWORD got = 0;
    if (
      !ReadFile(
        file.value,
        bytes.data() + done,
        DWORD(std::min<size_t>(bytes.size() - done, 1u << 20)),
        &got,
        nullptr)
      || !got)
#else
    const auto got = ::read(file.value, bytes.data() + done, bytes.size() - done);
    if (got <= 0)
#endif
    {
      throw std::runtime_error("Dependency exact binary read failed");
    }
    done += size_t(got);
  }
  char extra;
#ifdef _WIN32
  DWORD got = 0;
  if (!ReadFile(file.value, &extra, 1, &got, nullptr) || got != 0)
#else
  if (::read(file.value, &extra, 1) != 0)
#endif
  {
    throw std::runtime_error("Dependency changed during read");
  }
  return bytes;
}

uint32_t crc32(std::string_view bytes)
{
  auto crc = uint32_t{0xffffffffu};
  for (const unsigned char c : bytes)
  {
    crc ^= c;
    for (int bit = 0; bit < 8; ++bit)
    {
      crc = (crc >> 1u) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
  }
  return crc ^ 0xffffffffu;
}

void le(std::string& out, uint32_t value, size_t bytes)
{
  for (size_t i = 0; i < bytes; ++i)
  {
    out.push_back(char((value >> (8u * i)) & 255u));
  }
}

uint32_t u32(std::string_view bytes, size_t offset)
{
  if (offset > bytes.size() || bytes.size() - offset < 4)
  {
    throw std::runtime_error("Truncated MDL table");
  }
  uint32_t result = 0;
  for (size_t i = 0; i < 4; ++i)
  {
    result |= uint32_t(static_cast<unsigned char>(bytes[offset + i])) << (8u * i);
  }
  return result;
}

std::string zstring(std::string_view bytes, size_t offset)
{
  if (offset >= bytes.size())
  {
    throw std::runtime_error("MDL string offset outside file");
  }
  const auto end = bytes.find('\0', offset);
  if (end == std::string_view::npos || end - offset > 240)
  {
    throw std::runtime_error("Unbounded MDL string");
  }
  return std::string(bytes.substr(offset, end - offset));
}
} // namespace

std::string sourceDependencyBspArchivePath(const fs::path& bsp)
{
  const auto filename = gamePath(bsp.filename().string());
  if (!filename.ends_with(".bsp"))
  {
    throw std::runtime_error("Selected BSP needs a safe .bsp filename");
  }
  return gamePath("maps/" + filename);
}

const std::vector<SourceDependencyPackFile>& SourceDependencyPackPlan::files() const
{
  return m_files;
}
const std::vector<SourceDependencyPackIssue>& SourceDependencyPackPlan::issues() const
{
  return m_issues;
}
bool SourceDependencyPackPlan::canWrite() const
{
  return m_authorized && !m_blocked && !m_files.empty();
}
bool SourceDependencyPackPlan::assessmentComplete() const
{
  return false;
}

SourceDependencyPackPlan planSourceDependencyPack(
  const VmfDocument& document, const SourceDependencyPackOptions& options)
{
  SourceDependencyPackPlan plan;
  auto issue =
    [&](SourceDependencyPackIssueKind kind, std::string reference, std::string message) {
      const auto cap = std::min<size_t>(options.limits.maxReferences, 100000) + 64;
      if (plan.m_issues.size() >= cap)
      {
        if (!plan.m_blocked)
        {
          plan.m_issues.push_back(
            {SourceDependencyPackIssueKind::Budget,
             "issues",
             "Diagnostic budget exceeded; assessment stopped"});
        }
        plan.m_blocked = true;
        return;
      }
      plan.m_issues.push_back({kind, std::move(reference), std::move(message)});
      if (
        kind == SourceDependencyPackIssueKind::Error
        || kind == SourceDependencyPackIssueKind::Budget)
      {
        plan.m_blocked = true;
      }
    };
  issue(
    SourceDependencyPackIssueKind::Unresolved,
    "assessment",
    "Incomplete discovery: no guarantee of self-containment. Sound scripts, particles, "
    "instances, skyboxes, runtime references and branch extensions require manual "
    "review. Only explicitly selected custom roots are searched; installed/VPK/stock "
    "content is never searched.");
  if (!options.redistributionRightsAcknowledged)
  {
    issue(
      SourceDependencyPackIssueKind::Excluded,
      "rights",
      "Redistribution rights acknowledgement required; nothing read or eligible");
    return plan;
  }
  plan.m_authorized = true;
  const auto& limits = options.limits;
  if (
    !limits.maxReferences || limits.maxReferences > 100000 || !limits.maxFiles
    || limits.maxFiles > 65534 || !limits.maxFileBytes
    || limits.maxFileBytes > 0x7fffffffu || !limits.maxTotalBytes
    || limits.maxTotalBytes > 0x7fffffffu || options.customRoots.size() > 64)
  {
    issue(
      SourceDependencyPackIssueKind::Budget,
      "limits",
      "Invalid limits (ZIP32 bounded to 2 GiB, 65534 files, 100000 references, 64 "
      "roots)");
    return plan;
  }
  try
  {
    std::vector<fs::path> roots;
    for (const auto& input : options.customRoots)
    {
      auto root = checkedAbsolute(input);
      if (!fs::is_directory(root))
      {
        throw std::runtime_error("Custom root is not a directory");
      }
      for (const auto& part : root)
      {
        if (
          lower(part.string()) == "steamapps"
          || lower(part.extension().string()) == ".vpk")
        {
          throw std::runtime_error(
            "Installed Steam/VPK roots are ineligible; select a separate custom-only "
            "tree");
        }
      }
      roots.push_back(std::move(root));
    }
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
    std::deque<std::string> queue;
    std::set<std::string> seen;
    size_t references = 0;
    bool exhausted = false;
    auto enqueue = [&](std::string reference, std::string prefix, std::string extension) {
      if (exhausted)
      {
        return;
      }
      if (++references > limits.maxReferences)
      {
        exhausted = true;
        issue(
          SourceDependencyPackIssueKind::Budget,
          "graph",
          "Dependency reference budget exceeded");
        return;
      }
      try
      {
        auto name = gamePath(reference);
        if (!prefix.empty() && !name.starts_with(prefix))
        {
          name = prefix + name;
        }
        if (!extension.empty() && !name.ends_with(extension))
        {
          name += extension;
        }
        name = gamePath(std::move(name));
        if (seen.insert(name).second)
        {
          queue.push_back(std::move(name));
        }
      }
      catch (const std::exception& e)
      {
        issue(SourceDependencyPackIssueKind::Excluded, reference, e.what());
      }
    };
    // Every occurrence is visited; repeated native properties are not flattened.
    for (const auto& entry : document.entries())
    {
      if (exhausted || plan.m_blocked)
      {
        break;
      }
      if (!entry.value)
      {
        continue;
      }
      const auto key = lower(std::string(document.tokenContents(entry.key)));
      const auto value = std::string(document.tokenContents(*entry.value));
      if (key == "material" || key == "texture")
      {
        enqueue(value, "materials/", ".vmt");
      }
      else if (key == "model")
      {
        if (lower(value).ends_with(".mdl"))
        {
          enqueue(value, "models/", "");
        }
        else if (!value.empty() && value.front() != '*')
        {
          issue(
            SourceDependencyPackIssueKind::Unresolved,
            value,
            "Non-MDL model reference not assessed");
        }
      }
      else if (
        key == "sound" || key == "message" || key == "noise" || key.starts_with("noise"))
      {
        const auto normalized = lower(value);
        if (
          normalized.ends_with(".wav") || normalized.ends_with(".mp3")
          || normalized.ends_with(".ogg"))
        {
          enqueue(value, "sound/", "");
        }
        else if (!value.empty())
        {
          issue(
            SourceDependencyPackIssueKind::Unresolved,
            value,
            "Not a direct sound file; sound scripts are not resolved");
        }
      }
    }
    uint64_t total = 0;
    auto add = [&](const fs::path& root, const fs::path& path, const std::string& name)
      -> const std::string& {
      if (plan.m_files.size() >= limits.maxFiles)
      {
        throw std::runtime_error("File-count budget exceeded");
      }
      auto bytes = readExact(
        root, path, std::min(limits.maxFileBytes, limits.maxTotalBytes - total));
      total += bytes.size();
      plan.m_files.push_back({name, uint64_t(bytes.size()), crc32(bytes)});
      plan.m_bytes.push_back(std::move(bytes));
      return plan.m_bytes.back();
    };
    while (!queue.empty() && !exhausted && !plan.m_blocked)
    {
      const auto name = std::move(queue.front());
      queue.pop_front();
      try
      {
        // No precedence guessing: duplicate paths across selected roots refuse.
        std::optional<std::pair<fs::path, fs::path>> selected;
        for (const auto& root : roots)
        {
          if (auto path = resolve(root, name))
          {
            if (selected)
            {
              throw std::runtime_error("Duplicate dependency across custom roots");
            }
            selected = std::make_pair(root, *path);
          }
        }
        if (!selected)
        {
          issue(
            SourceDependencyPackIssueKind::Missing,
            name,
            "Absent from eligible custom roots; may be stock or missing. Installed "
            "content intentionally excluded");
          continue;
        }
        const auto extension = fs::path(name).extension().string();
        static const std::set<std::string> allowed{
          ".vmt", ".vtf", ".mdl", ".vvd", ".vtx", ".phy", ".ani", ".wav", ".mp3", ".ogg"};
        if (!allowed.contains(extension))
        {
          issue(
            SourceDependencyPackIssueKind::Excluded,
            name,
            "Unsupported distribution asset type");
          continue;
        }
        const auto& bytes = add(selected->first, selected->second, name);
        if (extension == ".vmt")
        {
          try
          {
            const auto material = VmfDocument::parse(bytes);
            static const std::set<std::string> textures{
              "$basetexture",
              "$basetexture2",
              "$bumpmap",
              "$bumpmap2",
              "$normalmap",
              "$detail",
              "$envmapmask",
              "$selfillummask",
              "$phongexponenttexture",
              "$blendmodulatetexture",
              "$lightwarptexture",
              "$dudvmap",
              "$flowmap",
              "$refracttinttexture"};
            for (const auto& property : material.entries())
            {
              if (exhausted || plan.m_blocked)
              {
                break;
              }
              if (!property.value)
              {
                continue;
              }
              const auto key = lower(std::string(material.tokenContents(property.key)));
              const auto value = std::string(material.tokenContents(*property.value));
              if (
                key == "include" || key == "$bottommaterial" || key == "$fallbackmaterial"
                || key == "$crackmaterial" || key == "$underwateroverlay")
              {
                enqueue(value, "materials/", ".vmt");
              }
              else if (textures.contains(key))
              {
                enqueue(value, "materials/", ".vtf");
              }
              else if (key == "$envmap" && lower(value) != "env_cubemap")
              {
                enqueue(value, "materials/", ".vtf");
              }
              else if (!key.empty() && key.front() == '$')
              {
                issue(
                  SourceDependencyPackIssueKind::Unresolved,
                  name + ":" + key,
                  "Shader parameter not assessed for dependencies");
              }
            }
          }
          catch (const std::exception& e)
          {
            issue(
              SourceDependencyPackIssueKind::Unresolved,
              name,
              std::string("VMT dependency parsing incomplete: ") + e.what());
          }
        }
        else if (extension == ".mdl")
        {
          const auto stem = name.substr(0, name.size() - 4);
          for (const auto* suffix :
               {".vvd", ".dx90.vtx", ".dx80.vtx", ".sw.vtx", ".phy", ".ani"})
          {
            enqueue(stem + suffix, "", "");
          }
          try
          {
            if (
              bytes.size() < 408 || bytes.substr(0, 4) != "IDST" || u32(bytes, 4) < 44
              || u32(bytes, 4) > 49 || u32(bytes, 76) != bytes.size())
            {
              throw std::runtime_error(
                "Only bounded classic IDST 44..49 dependency tables assessed");
            }
            const auto textureCount = u32(bytes, 204), textureOffset = u32(bytes, 208);
            const auto directoryCount = u32(bytes, 212),
                       directoryOffset = u32(bytes, 216);
            if (
              textureCount > 4096 || directoryCount > 256
              || uint64_t(textureOffset) + uint64_t(textureCount) * 64 > bytes.size()
              || uint64_t(directoryOffset) + uint64_t(directoryCount) * 4 > bytes.size())
            {
              throw std::runtime_error("Invalid/budgeted MDL material tables");
            }
            std::vector<std::string> directories;
            for (uint32_t i = 0; i < directoryCount; ++i)
            {
              directories.push_back(zstring(bytes, u32(bytes, directoryOffset + i * 4)));
            }
            if (directories.empty())
            {
              directories.push_back("");
            }
            for (uint32_t i = 0; i < textureCount && !exhausted && !plan.m_blocked; ++i)
            {
              const size_t offset = size_t(textureOffset) + i * 64;
              const auto texture = zstring(bytes, offset + u32(bytes, offset));
              for (auto directory : directories)
              {
                if (exhausted || plan.m_blocked)
                {
                  break;
                }
                std::replace(directory.begin(), directory.end(), '\\', '/');
                if (!directory.empty() && directory.back() != '/')
                {
                  directory += '/';
                }
                enqueue(directory + texture, "materials/", ".vmt");
              }
            }
            const auto count = u32(bytes, 336), offset = u32(bytes, 340);
            if (count > 1024 || uint64_t(offset) + uint64_t(count) * 8 > bytes.size())
            {
              throw std::runtime_error("Invalid MDL include table");
            }
            for (uint32_t i = 0; i < count && !exhausted && !plan.m_blocked; ++i)
            {
              const size_t record = size_t(offset) + i * 8;
              enqueue(zstring(bytes, record + u32(bytes, record + 4)), "models/", ".mdl");
            }
            const auto animationName = u32(bytes, 348);
            if (animationName)
            {
              enqueue(zstring(bytes, animationName), "models/", "");
            }
            issue(
              SourceDependencyPackIssueKind::Unresolved,
              name,
              "MDL tables/sidecars only; animation events, skins/search precedence and "
              "branch-specific runtime dependencies need review");
          }
          catch (const std::exception& e)
          {
            issue(SourceDependencyPackIssueKind::Unresolved, name, e.what());
          }
        }
      }
      catch (const std::exception& e)
      {
        issue(SourceDependencyPackIssueKind::Error, name, e.what());
      }
    }
    if (options.compiledBsp && !plan.m_blocked)
    {
      const auto archivePath = sourceDependencyBspArchivePath(*options.compiledBsp);
      const auto bsp = checkedAbsolute(*options.compiledBsp);
      const auto snapshotIndex = plan.m_bytes.size();
      add(bsp.parent_path(), bsp, archivePath);
      if (plan.m_blocked)
      {
        return plan;
      }
      if (plan.m_bytes.size() != snapshotIndex + 1)
      {
        throw std::runtime_error("Selected BSP did not produce one exact snapshot");
      }
      const auto& bytes = plan.m_bytes[snapshotIndex];
      if (bytes.size() < 1036 || bytes.substr(0, 4) != "VBSP")
      {
        throw std::runtime_error("Included BSP snapshot lacks a complete VBSP header");
      }
      const auto version = u32(bytes, 4);
      if (
        !version || version > 0x7fffffffu
        || (options.compiledBspExpectedVersion && version != *options.compiledBspExpectedVersion))
      {
        throw std::runtime_error(
          "Included BSP snapshot does not match its version contract");
      }
    }
    // Stable archive ordering independent of graph traversal or root selection order.
    std::vector<size_t> order(plan.m_files.size());
    for (size_t i = 0; i < order.size(); ++i)
    {
      order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
      return plan.m_files[a].archivePath < plan.m_files[b].archivePath;
    });
    auto files = std::move(plan.m_files);
    auto bytes = std::move(plan.m_bytes);
    for (const auto i : order)
    {
      plan.m_files.push_back(std::move(files[i]));
      plan.m_bytes.push_back(std::move(bytes[i]));
    }
  }
  catch (const std::exception& e)
  {
    issue(SourceDependencyPackIssueKind::Error, "plan", e.what());
  }
  return plan;
}

SourceDependencyPackResult SourceDependencyPackPlan::writeZip(
  const fs::path& output, const bool explicitlyApproved) const
{
  if (!explicitlyApproved || !canWrite())
  {
    return {false, "Explicit plan approval and an eligible, unblocked plan required"};
  }
  try
  {
    // Stored ZIP32, fixed DOS date 1980-01-01, CRCs over exact binary snapshots.
    // Preflight every size/offset before allocating the archive buffer.
    uint64_t size = 22;
    std::set<std::string> names;
    for (const auto& file : m_files)
    {
      if (
        gamePath(file.archivePath) != file.archivePath
        || !names.insert(file.archivePath).second)
      {
        throw std::runtime_error("Unsafe/duplicate ZIP path");
      }
      size += 30 + 46 + 2 * file.archivePath.size() + file.size;
    }
    if (size > 0xffffffffu || size > std::numeric_limits<size_t>::max())
    {
      throw std::runtime_error("ZIP32 size budget exceeded");
    }
    std::string zip;
    zip.reserve(static_cast<size_t>(size));
    std::vector<uint32_t> offsets;
    for (size_t i = 0; i < m_files.size(); ++i)
    {
      const auto& file = m_files[i];
      if (m_bytes[i].size() != file.size || crc32(m_bytes[i]) != file.crc32)
      {
        throw std::runtime_error("Snapshot CRC mismatch");
      }
      offsets.push_back(uint32_t(zip.size()));
      le(zip, 0x04034b50, 4);
      le(zip, 20, 2);
      le(zip, 0, 2);
      le(zip, 0, 2);
      le(zip, 0, 2);
      le(zip, 33, 2);
      le(zip, file.crc32, 4);
      le(zip, uint32_t(file.size), 4);
      le(zip, uint32_t(file.size), 4);
      le(zip, uint32_t(file.archivePath.size()), 2);
      le(zip, 0, 2);
      zip += file.archivePath;
      zip += m_bytes[i];
    }
    const auto central = uint32_t(zip.size());
    for (size_t i = 0; i < m_files.size(); ++i)
    {
      const auto& file = m_files[i];
      le(zip, 0x02014b50, 4);
      le(zip, 20, 2);
      le(zip, 20, 2);
      le(zip, 0, 2);
      le(zip, 0, 2);
      le(zip, 0, 2);
      le(zip, 33, 2);
      le(zip, file.crc32, 4);
      le(zip, uint32_t(file.size), 4);
      le(zip, uint32_t(file.size), 4);
      le(zip, uint32_t(file.archivePath.size()), 2);
      le(zip, 0, 2);
      le(zip, 0, 2);
      le(zip, 0, 2);
      le(zip, 0, 2);
      le(zip, 0, 4);
      le(zip, offsets[i], 4);
      zip += file.archivePath;
    }
    const auto centralSize = uint32_t(zip.size()) - central;
    le(zip, 0x06054b50, 4);
    le(zip, 0, 2);
    le(zip, 0, 2);
    le(zip, uint32_t(m_files.size()), 2);
    le(zip, uint32_t(m_files.size()), 2);
    le(zip, centralSize, 4);
    le(zip, central, 4);
    le(zip, 0, 2);
    if (zip.size() != size)
    {
      throw std::runtime_error("ZIP size mismatch");
    }
    const auto parent =
      checkedAbsolute(output.has_parent_path() ? output.parent_path() : fs::path("."));
    const auto target = parent / output.filename();
    const auto outputName = gamePath(output.filename().string());
    if (!outputName.ends_with(".zip") || outputName.find('/') != std::string::npos)
    {
      throw std::runtime_error("Output needs a portable .zip filename");
    }
#ifdef _WIN32
    Handle file{CreateFileW(
      target.c_str(),
      GENERIC_WRITE,
      0,
      nullptr,
      CREATE_NEW,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
      nullptr)};
    if (file.value == INVALID_HANDLE_VALUE)
    {
      throw std::runtime_error(
        "Cannot create new ZIP (existing outputs never overwritten)");
    }
    if (handlePath(file.value) != lower(target.generic_string()))
    {
      throw std::runtime_error("ZIP output directory changed");
    }
#else
    // Anchor creation to a no-follow directory descriptor; no ancestor may be swapped
    // for a symlink between validation and creation.
    struct DirectoryFd
    {
      int value;
      ~DirectoryFd()
      {
        if (value >= 0)
        {
          ::close(value);
        }
      }
    } directory{::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
    if (directory.value < 0)
    {
      throw std::runtime_error("Cannot open output filesystem root");
    }
    for (const auto& component : parent.relative_path())
    {
      const auto next = ::openat(
        directory.value,
        component.c_str(),
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      if (next < 0)
      {
        throw std::runtime_error("Cannot safely open output directory");
      }
      ::close(directory.value);
      directory.value = next;
    }
    const int file = ::openat(
      directory.value,
      output.filename().c_str(),
      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
      0600);
    if (file < 0)
    {
      throw std::runtime_error(
        "Cannot create new ZIP (existing outputs never overwritten)");
    }
#endif
    bool ok = true;
    size_t done = 0;
    while (done < zip.size())
    {
#ifdef _WIN32
      DWORD wrote = 0;
      if (
        !WriteFile(
          file.value,
          zip.data() + done,
          DWORD(std::min<size_t>(zip.size() - done, 1u << 20)),
          &wrote,
          nullptr)
        || !wrote)
#else
      const auto wrote = ::write(file, zip.data() + done, zip.size() - done);
      if (wrote <= 0)
#endif
      {
        ok = false;
        break;
      }
      done += size_t(wrote);
    }
#ifdef _WIN32
    if (!FlushFileBuffers(file.value))
    {
      ok = false;
    }
#else
    if (::fsync(file) != 0)
    {
      ok = false;
    }
    if (::close(file) != 0)
    {
      ok = false;
    }
#endif
    // Do not delete by path after failure: a concurrent rename could replace it.
    if (!ok)
    {
      return {
        false,
        "ZIP write/flush failed; newly created partial output may remain (never reported "
        "successful)"};
    }
    return {true, {}};
  }
  catch (const std::exception& e)
  {
    return {false, e.what()};
  }
}
} // namespace tb::mdl
