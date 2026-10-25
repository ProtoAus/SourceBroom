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
  const auto load = [&](std::optional<size_t> budget = std::nullopt) {
    auto reader = fs::Reader::from(data.data(), data.data() + data.size());
    return loadVtfTexture(reader, budget);
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
  SECTION("remaining budget includes all retained mip buffers")
  {
    REQUIRE(load(20).is_success());
    const auto result = load(19);
    REQUIRE(result.is_error());
    CHECK(
      std::get<Error>(result | kdl::error()).msg
      == "VTF exceeds remaining decoded image budget");
    CHECK(load(16).is_error()); // Base level fits, but the smaller mip does not.
    CHECK(load(0).is_error());
  }
  const auto fromHex = [](const std::string& hex) {
    auto result = std::string{};
    for (size_t i = 0; i < hex.size(); i += 2)
    {
      result += static_cast<char>(std::stoul(hex.substr(i, 2), nullptr, 16));
    }
    return result;
  };
  SECTION("integer formats retain independently expected RGBA8 pixels")
  {
    struct PixelFixture
    {
      uint32_t format;
      const char* input;
      const char* rgba;
    };
    // Literal bytes, not a second implementation of the production converter.
    const auto fixtures = {
      PixelFixture{1, "12345678ff0080ff", "78563412ff8000ff"},
      PixelFixture{11, "12345678ff0080ff", "345678120080ffff"},
      PixelFixture{16, "12345600ff008080", "563412ff8000ffff"},
      PixelFixture{5, "004280ff", "000000ff424242ff808080ffffffffff"},
      PixelFixture{6, "004280ff", "00000042808080ff"},
      PixelFixture{8, "004280ff", "ffffff00ffffff42ffffff80ffffffff"},
      PixelFixture{9, "0000ff0000fe0100ff", "0000ff000000feff0100ffff"},
      PixelFixture{10, "ff0000fe0000ff0001", "0000ff000000feff0100ffff"},
      // RGB565 has red in the low five bits; BGR565 has blue there.
      PixelFixture{
        4,
        "0000ffff1f0000f8e007438c",
        "000000ffffffffff"
        "ff0000ff0000ffff00ff00ff188a8cff"},
      PixelFixture{
        17,
        "0000ffff1f0000f8e007438c",
        "000000ffffffffff"
        "0000ffffff0000ff00ff00ff8c8a18ff"},
      // X must not become alpha, even when its bit is clear.
      PixelFixture{
        18, "0000ff7fffff434443c4", "000000ffffffffffffffffff8c1018ff8c1018ff"},
      PixelFixture{
        21, "0000ff7fffff434443c4", "00000000ffffff00ffffffff8c1018008c1018ff"},
      PixelFixture{19, "0000ffff3412cdab", "00000000ffffffff22334411bbccddaa"},
    };
    for (const auto& fixture : fixtures)
    {
      CAPTURE(fixture.format);
      const auto expected = fromHex(fixture.rgba);
      data.resize(80);
      data += fromHex(fixture.input);
      put(52, fixture.format);
      put(16, static_cast<uint32_t>(expected.size() / 4), 2);
      put(18, 1, 2);
      put(56, 1, 1);
      put(20, 0x2000);
      auto result = load(expected.size());
      REQUIRE(result.is_success());
      const auto texture = std::move(result) | kdl::value();
      CHECK(texture.format() == GL_RGBA);
      CHECK(texture.alphaDomain() == img::ImageAlphaDomain::Graduated);
      REQUIRE(texture.buffersIfLoaded().size() == 1);
      const auto& buffer = texture.buffersIfLoaded()[0];
      REQUIRE(buffer.size() == expected.size());
      CHECK(
        std::equal(
          expected.begin(),
          expected.end(),
          buffer.data(),
          [](const char expectedByte, const unsigned char actual) {
            return static_cast<unsigned char>(expectedByte) == actual;
          }));
      CHECK(load(expected.size() - 1).is_error());
      data.pop_back();
      CHECK(load().is_error());
    }
  }
  SECTION("converted mips reverse and budgets count retained not encoded bytes")
  {
    put(52, 5); // Five I8 bytes become twenty retained RGBA8 bytes.
    data.resize(80);
    data += fromHex("2a004080ff");
    auto result = load(20);
    REQUIRE(result.is_success());
    const auto texture = std::move(result) | kdl::value();
    const auto expected = fromHex("000000ff404040ff808080ffffffffff2a2a2aff");
    REQUIRE(texture.buffersIfLoaded().size() == 2);
    size_t offset = 0;
    for (const auto& buffer : texture.buffersIfLoaded())
    {
      for (size_t i = 0; i < buffer.size(); ++i)
      {
        CHECK(buffer.data()[i] == static_cast<unsigned char>(expected[offset++]));
      }
    }
    CHECK(offset == expected.size());
    for (const auto budget : {0u, 5u, 16u, 19u})
    {
      const auto rejected = load(budget);
      REQUIRE(rejected.is_error());
      CHECK(
        std::get<Error>(rejected | kdl::error()).msg
        == "VTF exceeds remaining decoded image budget");
    }
    data.pop_back();
    CHECK(load().is_error());
  }
  SECTION("legacy byte formats retain their original channel layout")
  {
    for (const auto format : {0u, 2u, 3u, 12u})
    {
      CAPTURE(format);
      put(52, format);
      put(56, 1, 1);
      put(16, 1, 2);
      put(18, 1, 2);
      const auto bytes = format == 2 || format == 3 ? 3u : 4u;
      data.resize(80);
      data += fromHex(bytes == 3 ? "123456" : "12345678");
      const auto texture = load(bytes) | kdl::value();
      CHECK(
        texture.format()
        == static_cast<GLenum>(
          format == 0   ? GL_RGBA
          : format == 2 ? GL_RGB
          : format == 3 ? GL_BGR
                        : GL_BGRA));
      REQUIRE(texture.buffersIfLoaded()[0].size() == bytes);
      for (size_t i = 0; i < bytes; ++i)
      {
        CHECK(
          texture.buffersIfLoaded()[0].data()[i]
          == static_cast<unsigned char>(data[80 + i]));
      }
    }
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
    for (const auto format :
         {7u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 34u, 35u, 70u, 71u, 999u})
    {
      CAPTURE(format);
      put(52, format);
      CHECK(load().is_error());
    }
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
    SECTION("budget refusal precedes decompression")
    {
      data[112] = 0; // Invalid zlib bytes: budget must refuse before inspecting these.
      const auto result = load(19);
      REQUIRE(result.is_error());
      CHECK(
        std::get<Error>(result | kdl::error()).msg
        == "VTF exceeds remaining decoded image budget");
      CHECK(load(20).is_error());
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
  SECTION("AXC inflates encoded I8 before RGBA conversion")
  {
    put(52, 5);
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
    put(104, 9);
    put(108, 12);
    // Python zlib.compress of literal [42] and [0,64,128,255].
    data += fromHex("789cd30200002b002b789c637068f80f0002c301c0");
    SECTION("both converted mips match exact channels")
    {
      const auto texture = load(20) | kdl::value();
      REQUIRE(texture.buffersIfLoaded()[0].size() == 16);
      REQUIRE(texture.buffersIfLoaded()[1].size() == 4);
      const auto expected = fromHex("000000ff404040ff808080ffffffffff");
      for (size_t i = 0; i < expected.size(); ++i)
      {
        CHECK(
          texture.buffersIfLoaded()[0].data()[i]
          == static_cast<unsigned char>(expected[i]));
      }
      for (size_t i = 0; i < 4; ++i)
      {
        CHECK(texture.buffersIfLoaded()[1].data()[i] == (i == 3 ? 255 : 42));
      }
    }
    SECTION("converted full chain budget refuses before decompressing corrupt input")
    {
      data[112] = 0;
      const auto result = load(19);
      REQUIRE(result.is_error());
      CHECK(
        std::get<Error>(result | kdl::error()).msg
        == "VTF exceeds remaining decoded image budget");
      CHECK(load(20).is_error());
    }
    SECTION("encoded sizes truncation and resource overlap remain guarded")
    {
      put(52, 6); // IA88 needs twice as many inflated bytes; streams are invalid.
      CHECK(load().is_error());
      put(52, 5);
      data.pop_back();
      CHECK(load().is_error());
      put(92, 96); // AXC and image must not share an offset.
      CHECK(load().is_error());
    }
#ifdef TB_SOURCE_VTF_ZSTD
    SECTION("Zstd also inflates encoded bytes before conversion")
    {
      data.resize(112);
      put(100, 0x005d0016);
      put(104, 10);
      put(108, 13);
      // Single raw blocks, content sizes one and four.
      data += fromHex("28b52ffd20010900002a28b52ffd2004210000004080ff");
      const auto texture = load(20) | kdl::value();
      CHECK(texture.buffersIfLoaded()[0].size() == 16);
      CHECK(texture.buffersIfLoaded()[0].data()[4] == 64);
      CHECK(texture.buffersIfLoaded()[0].data()[11] == 255);
      CHECK(texture.buffersIfLoaded()[1].data()[0] == 42);
    }
#endif
  }
  SECTION("conversion expansion cannot bypass the global retained output limit")
  {
    put(52, 5);
    put(16, 16384, 2);
    put(18, 8192, 2);
    put(56, 1, 1);
    const auto result = load();
    REQUIRE(result.is_error());
    CHECK(
      std::get<Error>(result | kdl::error()).msg
      == "VTF exceeds 256 MiB decoded image budget");
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
