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

/* Newly authored VPK fixtures; no game content. */
#include "fs/File.h"
#include "fs/VpkFileSystem.h"

#include "kd/result.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>
namespace tb::fs
{
TEST_CASE("VpkFileSystem")
{
  std::string tree;
  const auto word = [&](uint32_t value, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i)
    {
      tree += static_cast<char>(value >> (i * 8));
    }
  };
  tree.append("txt\0 \0control\0", 14);
  word(0, 4);
  word(3, 2);
  word(0x7fff, 2);
  word(0, 4);
  word(4, 4);
  word(0xffff, 2);
  tree.append("A\0B", 3);
  tree.append(3, '\0');
  auto data = std::string{};
  const auto header = [&](uint32_t value) {
    for (size_t i = 0; i < 4; ++i)
    {
      data += static_cast<char>(value >> (i * 8));
    }
  };
  header(0x55aa1234);
  header(1);
  header(static_cast<uint32_t>(tree.size()));
  data += tree;
  data += "body";
  const auto open = [&](std::filesystem::path archivePath = {}) {
    auto bytes = std::make_unique<char[]>(data.size());
    std::memcpy(bytes.get(), data.data(), data.size());
    return createImageFileSystem<VpkFileSystem>(
      std::make_shared<OwningBufferFile>(std::move(bytes), data.size()),
      std::move(archivePath));
  };
  SECTION("embedded data and binary preload concatenate exactly")
  {
    auto image = open() | kdl::value();
    const auto file = image->openFile("control.txt") | kdl::value();
    REQUIRE(file->size() == 7);
    auto bytes = std::string(7, '\0');
    file->reader().read(bytes.data(), bytes.size());
    CHECK(bytes == std::string("A\0Bbody", 7));
  }
  SECTION("numbered archive plus preload and missing archive diagnostic")
  {
    const auto root = std::filesystem::temp_directory_path() /
      ("sourcebroom-vpk-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup
    {
      std::filesystem::path path;
      ~Cleanup()
      {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
      }
    } cleanup{root};
    std::filesystem::create_directories(root);
    data[32] = 0;
    data[33] = 0;
    std::ofstream{root / "test_000.vpk", std::ios::binary} << "body";
    auto image = open(root / "test_dir.vpk") | kdl::value();
    auto file = image->openFile("control.txt") | kdl::value();
    auto contents = std::string(7, '\0');
    file->reader().read(contents.data(), contents.size());
    CHECK(contents == std::string("A\0Bbody", 7));
    file.reset();
    std::ofstream{root / "test_000.vpk", std::ios::binary} << "bo";
    CHECK(image->openFile("control.txt").is_error());
    std::filesystem::remove(root / "test_000.vpk");
    CHECK(image->openFile("control.txt").is_error());
    std::ofstream{root / "test_000.vpk", std::ios::binary} << "body";
    for (size_t i = 38; i < 42; ++i)
    {
      data[i] = static_cast<char>(0xff);
    }
    auto oversized = open(root / "test_dir.vpk") | kdl::value();
    CHECK(oversized->openFile("control.txt").is_error());
    data[38] = 4;
    data[39] = 0;
    data[40] = 0;
    data[41] = 0;
    for (size_t i = 34; i < 38; ++i)
    {
      data[i] = static_cast<char>(0xff);
    }
    auto badOffset = open(root / "test_dir.vpk") | kdl::value();
    CHECK(badOffset->openFile("control.txt").is_error());
  }
  SECTION("v2 directory header")
  {
    data[4] = 2;
    data.insert(12, 16, '\0');
    data[12] = 4;
    CHECK(open().is_success());
    data[12] = 3;
    CHECK(open().is_error());
  }
  SECTION("buffer-backed tree length must be checked before making a subreader")
  {
    data.resize(12);
    data[8] = 0;
    data[9] = 0;
    data[10] = 1;
    data[11] = 0;
    CHECK(open().is_error());
    data[4] = 2;
    CHECK(open().is_error());
  }
  SECTION("bad headers and truncation are errors")
  {
    data[0] = 0;
    CHECK(open().is_error());
    data[0] = 0x34;
    data[4] = 54;
    CHECK(open().is_error());
    data[4] = 1;
    data.resize(15);
    CHECK(open().is_error());
  }
}
} // namespace tb::fs
