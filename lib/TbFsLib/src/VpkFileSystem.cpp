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

#include "fs/VpkFileSystem.h"

#include "fs/DiskIO.h"
#include "fs/File.h"
#include "fs/ReaderException.h"

#include "kd/string_format.h"

#include <fmt/format.h>

namespace tb::fs
{
VpkFileSystem::VpkFileSystem(std::shared_ptr<File> file, std::filesystem::path path)
  : ImageFileSystem{std::move(file)}
  , m_path{std::move(path)}
{
}

Result<void> VpkFileSystem::doReadDirectory()
{
  try
  {
    auto reader = m_file->reader();
    if (reader.readUnsignedInt<uint32_t>() != 0x55aa1234)
    {
      return Error{"Invalid VPK signature"};
    }
    const auto version = reader.readUnsignedInt<uint32_t>();
    if (version != 1 && version != 2)
    {
      return Error{"Unsupported VPK version"};
    }
    const auto treeSize = reader.readSize<uint32_t>();
    const auto headerSize = version == 1 ? 12u : 28u;
    if (treeSize > 64 * 1024 * 1024)
    {
      return Error{"VPK directory exceeds 64 MiB limit"};
    }
    if (headerSize > reader.size() || treeSize > reader.size() - headerSize)
    {
      return Error{"VPK directory exceeds archive"};
    }
    auto tree = reader.subReaderFromBegin(headerSize, treeSize).buffer();
    const auto embeddedSize =
      version == 2 ? reader.readSize<uint32_t>() : reader.size() - headerSize - treeSize;
    if (embeddedSize > reader.size() - headerSize - treeSize)
    {
      return Error{"VPK embedded data exceeds archive"};
    }
    const auto string = [&]() {
      auto s = std::string{};
      while (const auto c = tree.readChar<char>())
      {
        s += c;
      }
      return s;
    };
    for (auto ext = string(); !ext.empty(); ext = string())
    {
      for (auto dir = string(); !dir.empty(); dir = string())
      {
        for (auto name = string(); !name.empty(); name = string())
        {
          tree.readUnsignedInt<uint32_t>(); // CRC is not an authenticity check.
          const auto preloadSize = tree.readSize<uint16_t>();
          const auto archive = tree.readUnsignedInt<uint16_t>();
          const auto offset = tree.readSize<uint32_t>();
          const auto length = tree.readSize<uint32_t>();
          if (
            archive == 0x7fff
            && (offset > embeddedSize || length > embeddedSize - offset))
          {
            return Error{"VPK entry exceeds embedded data section"};
          }
          if (tree.readUnsignedInt<uint16_t>() != 0xffff)
          {
            return Error{"Invalid VPK entry terminator"};
          }
          auto preload = std::string(preloadSize, '\0');
          tree.read(preload.data(), preloadSize);
          const auto path = std::filesystem::path{kdl::str_to_lower(
            (dir == " " ? "" : dir + "/") + name + (ext == " " ? "" : "." + ext))};
          if (path.is_absolute() || path.has_root_name())
          {
            return Error{"Absolute VPK entry path"};
          }
          for (const auto& part : path)
          {
            if (part == "..")
            {
              return Error{"Parent traversal in VPK entry"};
            }
          }
          auto archivePath = m_path;
          if (archive != 0x7fff && length)
          {
            auto stem = m_path.stem().string();
            if (!stem.ends_with("_dir"))
            {
              return Error{"VPK requires a _dir archive path"};
            }
            stem.resize(stem.size() - 4);
            archivePath =
              m_path.parent_path() / fmt::format("{}_{:03}.vpk", stem, archive);
          }
          const auto start = archive == 0x7fff ? headerSize + treeSize + offset : offset;
          addFile(
            path,
            [file = m_file,
             archivePath,
             archive,
             start,
             length,
             preload = std::move(preload)]() -> Result<std::shared_ptr<File>> {
              const auto read =
                [&](std::shared_ptr<File> source) -> Result<std::shared_ptr<File>> {
                try
                {
                  constexpr size_t MaxAssetBytes = 256 * 1024 * 1024;
                  if (length > MaxAssetBytes - preload.size())
                  {
                    return Error{"VPK asset exceeds 256 MiB budget"};
                  }
                  if (
                    length && (start > source->size() || length > source->size() - start))
                  {
                    return Error{"VPK entry exceeds archive data"};
                  }
                  auto data =
                    source->reader().subReaderFromBegin(length ? start : 0, length);
                  auto buffer = std::make_unique<char[]>(preload.size() + length);
                  std::copy(preload.begin(), preload.end(), buffer.get());
                  data.read(buffer.get() + preload.size(), length);
                  return std::static_pointer_cast<File>(
                    std::make_shared<OwningBufferFile>(
                      std::move(buffer), preload.size() + length));
                }
                catch (const ReaderException& e)
                {
                  return Error{e.what()};
                }
              };
              if (archive == 0x7fff || !length)
              {
                return read(file);
              }
              return Disk::openFile(archivePath) | kdl::and_then(read);
            });
        }
      }
    }
    if (!tree.eof())
    {
      return Error{"Trailing VPK directory bytes"};
    }
    return kdl::void_success;
  }
  catch (const ReaderException& e)
  {
    return Error{e.what()};
  }
}
} // namespace tb::fs
