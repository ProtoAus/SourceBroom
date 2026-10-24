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

/* Newly authored, minimal VTF byte fixtures. */
#include "fs/Reader.h"
#include "gl/Texture.h"
#include "mdl/LoadVtfTexture.h"

#include "kd/result.h"

#include <algorithm>
#include <string>

#include <catch2/catch_test_macros.hpp>
namespace tb::mdl
{
TEST_CASE("loadVtfTexture")
{
  auto data = std::string(80 + 20, '\0');
  const auto put = [&](size_t offset, uint32_t value, size_t bytes = 4) {
    for (size_t i = 0; i < bytes; ++i)
    {
      data[offset + i] = static_cast<char>(value >> (i * 8));
    }
  };
  put(0, 0x00465456);
  put(4, 7);
  put(8, 2);
  put(12, 80);
  put(16, 2, 2);
  put(18, 2, 2);
  put(24, 1, 2);
  put(52, 0);
  put(56, 2, 1);
  put(63, 1, 2);
  data[80] = 42;
  data[84] = 99;
  const auto load = [&]() {
    auto reader = fs::Reader::from(data.data(), data.data() + data.size());
    return loadVtfTexture(reader);
  };
  SECTION("mips are reversed into renderer order")
  {
    auto texture = load() | kdl::value();
    CHECK(texture.width() == 2);
    CHECK(texture.height() == 2);
    CHECK(texture.format() == GL_RGBA);
    REQUIRE(texture.buffersIfLoaded().size() == 2);
    CHECK(texture.buffersIfLoaded()[0].data()[0] == 99);
    CHECK(texture.buffersIfLoaded()[1].data()[0] == 42);
  }
  SECTION("common block compressed formats")
  {
    for (const auto format : {13u, 14u, 15u, 20u})
    {
      put(52, format);
      data.resize(112);
      const auto texture = load() | kdl::value();
      CHECK(gl::isCompressedFormat(texture.format()));
      CHECK(
        texture.buffersIfLoaded()[0].size() == ((format == 13 || format == 20) ? 8 : 16));
    }
  }
  SECTION("resource directory image offset")
  {
    data.resize(108);
    put(8, 3);
    put(12, 88);
    put(68, 1);
    put(80, 0x30);
    put(84, 88);
    CHECK(load().is_success());
    put(80, 0x02000030);
    CHECK(load().is_error());
  }
  SECTION("unsupported versions and formats are not misrendered")
  {
    put(8, 7);
    CHECK(load().is_error());
    put(8, 2);
    put(52, 999);
    CHECK(load().is_error());
  }
  SECTION("uncompressed Strata 7.6 has the same resource header as 7.5")
  {
    data.resize(108);
    put(8, 6);
    put(12, 88);
    put(68, 1);
    put(80, 0x30);
    put(84, 88);
    REQUIRE(load().is_success());
    put(52, 70);
    CHECK(load().is_error());
    put(52, 71);
    CHECK(load().is_error());
  }
  SECTION("AXC resources and bounded independent per-mip decompression")
  {
    data.resize(112);
    put(8, 6);
    put(12, 96);
    put(68, 2);
    put(80, 0x435841);
    put(84, 96);
    put(88, 0x30);
    put(92, 112);
    put(96, 12);
    put(100, 6);
    const auto appendHex = [&](const std::string& hex) {
      for (size_t i = 0; i < hex.size(); i += 2)
      {
        data += static_cast<char>(std::stoul(hex.substr(i, 2), nullptr, 16));
      }
    };
    put(104, 12);
    put(108, 11);
    // Independently generated zlib streams, not compressed by production code.
    appendHex("789cd3626060000000ac002b");
    appendHex("789c4b6640050006400064");
    SECTION("v1 and v2 Deflate reverse mips and preserve decoded bytes")
    {
      for (const auto compression : {6u, 0xffffffffu, 0x00080006u})
      {
        CAPTURE(compression);
        put(100, compression);
        auto result = load();
        REQUIRE(result.is_success());
        auto texture = std::move(result) | kdl::value();
        CHECK(texture.buffersIfLoaded()[0].size() == 16);
        CHECK(texture.buffersIfLoaded()[0].data()[0] == 99);
        CHECK(texture.buffersIfLoaded()[1].size() == 4);
        CHECK(texture.buffersIfLoaded()[1].data()[0] == 42);
        for (const auto& buffer : texture.buffersIfLoaded())
        {
          CHECK(
            std::all_of(
              buffer.data() + 1, buffer.data() + buffer.size(), [](const auto byte) {
                return byte == 0;
              }));
        }
      }
    }
    SECTION("strength zero ignores compression sizes")
    {
      put(100, 0);
      data.resize(132);
      data[112] = 42;
      data[116] = 99;
      REQUIRE(load().is_success());
    }
    SECTION("invalid resource flags length offset table and method")
    {
      put(80, 0x02435841);
      CHECK(load().is_error());
      put(80, 0x435841);
      put(84, 0xffffffff);
      CHECK(load().is_error());
      put(84, 96);
      put(96, 0xffffffff);
      CHECK(load().is_error());
      put(96, 11);
      CHECK(load().is_error());
      put(96, 12);
      put(100, 0x00090006);
      CHECK(load().is_error());
      put(100, 6);
      put(104, 0xffffffff);
      CHECK(load().is_error());
      put(104, 12);
      put(108, 0);
      CHECK(load().is_error());
    }
    SECTION("truncated trailing corrupted and wrong output sizes")
    {
      data.pop_back();
      CHECK(load().is_error());
      data += char{0x64};
      data[112] = 0;
      CHECK(load().is_error());
      data[112] = char{0x78};
      put(104, 13);
      CHECK(load().is_error());
      put(104, 12);
      put(16, 1, 2);
      CHECK(load().is_error());
    }
    SECTION("Zstd is supported only when the existing decoder target is available")
    {
      data.resize(112);
      put(100, 0x005d0016);
      put(104, 13);
      put(108, 25);
      // Single-segment Zstd frames with one raw block, content sizes 4 and 16.
      appendHex("28b52ffd20042100002a000000");
      appendHex("28b52ffd201081000063000000000000000000000000000000");
#ifdef TB_SOURCE_VTF_ZSTD
      auto texture = load() | kdl::value();
      CHECK(texture.buffersIfLoaded()[0].data()[0] == 99);
      CHECK(texture.buffersIfLoaded()[1].data()[0] == 42);
      data.pop_back();
      CHECK(load().is_error());
#else
      const auto result = load();
      REQUIRE(result.is_error());
      CHECK(
        std::get<Error>(result | kdl::error()).msg.find("decoder unavailable")
        != std::string::npos);
#endif
    }
  }
  SECTION("tiny buffer with huge declared dimensions is rejected before allocation")
  {
    data.resize(80);
    put(16, 16384, 2);
    put(18, 16384, 2);
    put(56, 1, 1);
    CHECK(load().is_error());
    put(16, 8192, 2);
    put(18, 8192, 2);
    CHECK(load().is_error());
  }
  SECTION("malformed sizes and unsupported dimensions")
  {
    data.resize(85);
    CHECK(load().is_error());
    data.resize(100);
    put(56, 30, 1);
    CHECK(load().is_error());
    put(56, 2, 1);
    put(24, 2, 2);
    CHECK(load().is_error());
    put(24, 1, 2);
    put(20, 0x4000);
    CHECK(load().is_error());
  }
}
} // namespace tb::mdl
