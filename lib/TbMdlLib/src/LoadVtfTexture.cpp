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

#include "mdl/LoadVtfTexture.h"

#include "fs/Reader.h"
#include "fs/ReaderException.h"
#include "gl/Texture.h"
#include "gl/TextureBuffer.h"

#include "kd/path_utils.h"

#include <miniz.h>

#include <algorithm>
#include <optional>
#include <vector>
#ifdef TB_SOURCE_VTF_ZSTD
#include <zstd.h>
#endif

namespace tb::mdl
{
namespace
{
// Packed VTF words are little endian, with the first named channel in the low
// bits. Expand sub-byte UNORM channels by bit replication (no color-space change).
void convertVtfPixels(
  const unsigned char* input,
  unsigned char* output,
  const size_t pixels,
  const uint32_t format,
  const size_t stride)
{
  const auto expand5 = [](const unsigned int v) { return (v << 3) | (v >> 2); };
  const auto expand6 = [](const unsigned int v) { return (v << 2) | (v >> 4); };
  for (size_t i = 0; i < pixels; ++i, input += stride, output += 4)
  {
    unsigned int r = 0, g = 0, b = 0, a = 255;
    switch (format)
    {
    case 1: // ABGR8888
      a = input[0];
      b = input[1];
      g = input[2];
      r = input[3];
      break;
    case 11: // ARGB8888
      a = input[0];
      r = input[1];
      g = input[2];
      b = input[3];
      break;
    case 16: // BGRX8888: X is not alpha.
      b = input[0];
      g = input[1];
      r = input[2];
      break;
    case 5: // I8
    case 6: // IA88
      r = g = b = input[0];
      if (format == 6)
      {
        a = input[1];
      }
      break;
    case 8: // A8: white modulated by alpha.
      r = g = b = 255;
      a = input[0];
      break;
    case 9:  // RGB888_BLUESCREEN
    case 10: // BGR888_BLUESCREEN
      r = input[format == 9 ? 0 : 2];
      g = input[1];
      b = input[format == 9 ? 2 : 0];
      a = r == 0 && g == 0 && b == 255 ? 0 : 255;
      break;
    default: // RGB565, BGR565, BGRX5551, BGRA4444, BGRA5551
      const auto word =
        static_cast<unsigned int>(input[0]) | (static_cast<unsigned int>(input[1]) << 8);
      if (format == 19)
      {
        b = (word & 15) * 17;
        g = ((word >> 4) & 15) * 17;
        r = ((word >> 8) & 15) * 17;
        a = ((word >> 12) & 15) * 17;
      }
      else if (format == 4 || format == 17)
      {
        const auto low = expand5(word & 31);
        const auto high = expand5((word >> 11) & 31);
        r = format == 4 ? low : high;
        g = expand6((word >> 5) & 63);
        b = format == 4 ? high : low;
      }
      else
      {
        b = expand5(word & 31);
        g = expand5((word >> 5) & 31);
        r = expand5((word >> 10) & 31);
        if (format == 21)
        {
          a = (word & 0x8000) ? 255 : 0;
        }
      }
      break;
    }
    output[0] = static_cast<unsigned char>(r);
    output[1] = static_cast<unsigned char>(g);
    output[2] = static_cast<unsigned char>(b);
    output[3] = static_cast<unsigned char>(a);
  }
}
} // namespace
bool isVtfTexture(const std::filesystem::path& path)
{
  return kdl::path_to_lower(path.extension()) == ".vtf";
}
Result<gl::Texture> loadVtfTexture(
  fs::Reader& reader, const std::optional<size_t> decodedByteBudget)
{
  try
  {
    if (reader.readUnsignedInt<uint32_t>() != 0x00465456)
    {
      return Error{"Invalid VTF signature"};
    }
    const auto major = reader.readUnsignedInt<uint32_t>();
    const auto minor = reader.readUnsignedInt<uint32_t>();
    if (major != 7 || minor > 6)
    {
      return Error{"Unsupported VTF version"};
    }
    const auto headerSize = reader.readSize<uint32_t>();
    const auto width = reader.readSize<uint16_t>();
    const auto height = reader.readSize<uint16_t>();
    const auto flags = reader.readUnsignedInt<uint32_t>();
    const auto frames = reader.readSize<uint16_t>();
    if (!width || !height || width > 16384 || height > 16384)
    {
      return Error{"Invalid VTF dimensions"};
    }
    if (frames != 1 || (flags & 0x4000))
    {
      return Error{"Unsupported animated or cubemap VTF"};
    }
    reader.seekFromBegin(52);
    const auto format = reader.readUnsignedInt<uint32_t>();
    const auto mips = reader.readSize<uint8_t>();
    const auto lowFormat = reader.readUnsignedInt<uint32_t>();
    const auto lowWidth = reader.readSize<uint8_t>();
    const auto lowHeight = reader.readSize<uint8_t>();
    if (minor >= 2 && reader.readSize<uint16_t>() != 1)
    {
      return Error{"Unsupported volume VTF"};
    }
    size_t maxMips = 1;
    for (auto d = std::max(width, height); d > 1; d >>= 1)
    {
      ++maxMips;
    }
    if (!mips || mips > maxMips)
    {
      return Error{"Invalid VTF mip count"};
    }
    if (format == 70 || format == 71)
    {
      return Error{"Unsupported VTF BC7/BC6H image format"};
    }
    GLenum glFormat = 0;
    size_t bytes = 0;
    size_t block = 0;
    bool convert = false;
    switch (format)
    {
    case 0:
      glFormat = GL_RGBA;
      bytes = 4;
      break;
    case 2:
      glFormat = GL_RGB;
      bytes = 3;
      break;
    case 3:
      glFormat = GL_BGR;
      bytes = 3;
      break;
    case 12:
      glFormat = GL_BGRA;
      bytes = 4;
      break;
    case 1:  // ABGR8888
    case 11: // ARGB8888
    case 16: // BGRX8888
      bytes = 4;
      convert = true;
      break;
    case 9:  // RGB888_BLUESCREEN
    case 10: // BGR888_BLUESCREEN
      bytes = 3;
      convert = true;
      break;
    case 4:  // RGB565
    case 6:  // IA88
    case 17: // BGR565
    case 18: // BGRX5551
    case 19: // BGRA4444
    case 21: // BGRA5551
      bytes = 2;
      convert = true;
      break;
    case 5: // I8
    case 8: // A8
      bytes = 1;
      convert = true;
      break;
    case 13:
    case 20:
      glFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
      block = 8;
      break;
    case 14:
      glFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
      block = 16;
      break;
    case 15:
      glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
      block = 16;
      break;
    default:
      return Error{"Unsupported VTF image format: " + std::to_string(format)};
    }
    if (convert)
    {
      glFormat = GL_RGBA;
    }
    const auto size = [&](size_t w, size_t h) {
      return block ? ((w + 3) / 4) * ((h + 3) / 4) * block : w * h * bytes;
    };
    if (headerSize < (minor >= 3 ? 80u : minor >= 2 ? 65u : 63u)
        || headerSize > reader.size())
    {
      return Error{"Invalid VTF header size"};
    }
    auto imageOffset = headerSize;
    auto imageEnd = reader.size();
    std::optional<size_t> auxiliaryOffset;
    std::vector<size_t> resourceOffsets;
    if (minor >= 3)
    {
      reader.seekFromBegin(68);
      const auto resources = reader.readSize<uint32_t>();
      if (resources > 32 || resources * 8 > headerSize - 80)
      {
        return Error{"Invalid VTF resource directory"};
      }
      reader.seekFromBegin(80);
      bool found = false;
      for (size_t i = 0; i < resources; ++i)
      {
        const auto tag = reader.readUnsignedInt<uint32_t>();
        const auto offset = reader.readSize<uint32_t>();
        const auto type = tag & 0xffffff;
        const auto resourceFlags = tag >> 24;
        if (resourceFlags != 0 && resourceFlags != 2)
        {
          return Error{"Unsupported VTF resource flags"};
        }
        if (resourceFlags == 0)
        {
          if (offset < headerSize || offset > reader.size())
          {
            return Error{"Invalid VTF resource offset"};
          }
          resourceOffsets.push_back(offset);
        }
        if (type == 0x30)
        {
          if (found || resourceFlags != 0)
          {
            return Error{"Invalid VTF image resource"};
          }
          imageOffset = offset;
          found = true;
        }
        if (type == 0x435841) // Strata AXC, little-endian ASCII.
        {
          if (auxiliaryOffset || resourceFlags != 0)
          {
            return Error{"Invalid VTF AXC resource"};
          }
          auxiliaryOffset = offset;
        }
      }
      if (!found)
      {
        return Error{"Missing VTF image resource"};
      }
      for (const auto offset : resourceOffsets)
      {
        if (offset > imageOffset)
        {
          imageEnd = std::min(imageEnd, offset);
        }
        if (offset == imageOffset && std::ranges::count(resourceOffsets, offset) > 1)
        {
          return Error{"Overlapping VTF image resource"};
        }
      }
    }
    else if (lowWidth && lowHeight)
    {
      if (lowFormat != 13)
      {
        return Error{"Unsupported VTF thumbnail format"};
      }
      imageOffset += ((lowWidth + 3) / 4) * ((lowHeight + 3) / 4) * 8;
    }
    if (imageOffset < headerSize || imageOffset > imageEnd)
    {
      return Error{"Invalid VTF image range"};
    }

    size_t method = 0;
    std::vector<size_t> compressedSizes;
    if (auxiliaryOffset)
    {
      const auto offset = *auxiliaryOffset;
      auto end = reader.size();
      for (const auto other : resourceOffsets)
      {
        if (other > offset)
        {
          end = std::min(end, other);
        }
      }
      if (std::ranges::count(resourceOffsets, offset) != 1 || end - offset < 8)
      {
        return Error{"Truncated or overlapping VTF AXC resource"};
      }
      reader.seekFromBegin(offset);
      const auto length = reader.readSize<uint32_t>();
      if (length < 4 || length > end - offset - 4)
      {
        return Error{"Invalid VTF AXC resource length"};
      }
      const auto compression = reader.readUnsignedInt<uint32_t>();
      const auto methodField = static_cast<int16_t>(compression >> 16);
      const auto strength =
        methodField <= 0
          ? static_cast<int32_t>(compression)
          : static_cast<int32_t>(static_cast<int16_t>(compression & 0xffff));
      if (strength != 0)
      {
        method = methodField <= 0 ? 8 : static_cast<size_t>(methodField);
        if (method != 8 && method != 93)
        {
          return Error{"Invalid VTF AXC compression method"};
        }
#ifndef TB_SOURCE_VTF_ZSTD
        if (method == 93)
        {
          return Error{"Unsupported VTF AXC Zstd compression (decoder unavailable)"};
        }
#endif
        if (method == 8 && (strength < -1 || strength > 9))
        {
          return Error{"Invalid VTF AXC Deflate strength"};
        }
        if (length != 4 + 4 * mips)
        {
          return Error{"Invalid VTF AXC mip size table"};
        }
        for (size_t i = 0; i < mips; ++i)
        {
          compressedSizes.push_back(reader.readSize<uint32_t>());
        }
      }
    }

    // Validate every input range and the entire output budget before allocating
    // pixel buffers. Reader subreaders alone do not validate their length.
    constexpr size_t MaxImageBytes = 256 * 1024 * 1024;
    auto remaining = imageEnd - imageOffset;
    size_t totalOutput = 0;
    size_t totalEncoded = 0;
    std::vector<size_t> encodedMipSizes(mips);
    std::vector<size_t> mipSizes(mips);
    for (size_t i = mips; i-- > 0;)
    {
      const auto w = std::max(size_t{1}, width >> i);
      const auto h = std::max(size_t{1}, height >> i);
      const auto encodedSize = size(w, h);
      if (encodedSize > MaxImageBytes - totalEncoded)
      {
        return Error{"VTF exceeds 256 MiB encoded image budget"};
      }
      totalEncoded += encodedSize;
      encodedMipSizes[i] = encodedSize;
      const auto outputSize = convert ? w * h * 4 : encodedSize;
      if (outputSize > MaxImageBytes - totalOutput)
      {
        return Error{"VTF exceeds 256 MiB decoded image budget"};
      }
      totalOutput += outputSize;
      mipSizes[i] = outputSize;
      const auto inputSize = method ? compressedSizes[mips - 1 - i] : encodedSize;
      if (!inputSize || inputSize > MaxImageBytes || inputSize > remaining)
      {
        return Error{"Truncated or oversized VTF mip payload"};
      }
      remaining -= inputSize;
    }

    if (decodedByteBudget && totalOutput > *decodedByteBudget)
    {
      return Error{"VTF exceeds remaining decoded image budget"};
    }

    reader.seekFromBegin(imageOffset);
    auto buffers = gl::TextureBufferList(mips);
    // VTF stores the smallest mip first; the renderer expects largest first.
    for (size_t i = mips; i-- > 0;)
    {
      buffers[i] = gl::TextureBuffer{mipSizes[i]};
      // AXC inflates to encoded pixels, not to the retained RGBA conversion.
      auto encoded =
        convert ? gl::TextureBuffer{encodedMipSizes[i]} : gl::TextureBuffer{};
      auto* destination = convert ? encoded.data() : buffers[i].data();
      const auto destinationSize = encodedMipSizes[i];
      if (!method)
      {
        reader.read(destination, destinationSize);
      }
      else
      {
        const auto inputSize = compressedSizes[mips - 1 - i];
        auto input = std::vector<unsigned char>(inputSize);
        reader.read(input.data(), input.size());
        if (method == 8)
        {
          auto outputSize = static_cast<mz_ulong>(destinationSize);
          auto consumed = static_cast<mz_ulong>(input.size());
          const auto status =
            mz_uncompress2(destination, &outputSize, input.data(), &consumed);
          if (
            status != MZ_OK || outputSize != destinationSize || consumed != input.size())
          {
            return Error{"Invalid VTF AXC Deflate mip payload"};
          }
        }
#ifdef TB_SOURCE_VTF_ZSTD
        else
        {
          const auto frameSize = ZSTD_findFrameCompressedSize(input.data(), input.size());
          if (ZSTD_isError(frameSize) || frameSize != input.size())
          {
            return Error{"Invalid VTF AXC Zstd frame size"};
          }
          const auto outputSize =
            ZSTD_decompress(destination, destinationSize, input.data(), input.size());
          if (ZSTD_isError(outputSize) || outputSize != destinationSize)
          {
            return Error{"Invalid VTF AXC Zstd mip payload"};
          }
        }
#endif
      }
      if (convert)
      {
        convertVtfPixels(
          encoded.data(), buffers[i].data(), destinationSize / bytes, format, bytes);
      }
    }
    auto texture = gl::Texture{
      width, height, RgbaF{}, glFormat, gl::NoEmbeddedDefaults{}, std::move(buffers)};
    texture.setAlphaDomain(
      (flags & 0x2000)   ? img::ImageAlphaDomain::Graduated
      : (flags & 0x1000) ? img::ImageAlphaDomain::Binary
                         : img::ImageAlphaDomain::Opaque);
    return texture;
  }
  catch (const fs::ReaderException& e)
  {
    return Error{e.what()};
  }
}
} // namespace tb::mdl
