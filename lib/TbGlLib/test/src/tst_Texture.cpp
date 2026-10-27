/*
 Copyright (C) 2026 Kristian Duske

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

#include "gl/MockGl.h"
#include "gl/TestUtils.h"
#include "gl/Texture.h"

#include <stdexcept>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace tb::gl
{
namespace
{

using TexParam = std::pair<GLenum, GLint>;

} // namespace

TEST_CASE("Texture")
{
  // NOLINTBEGIN(bugprone-use-after-move)

  auto buffers = TextureBufferList{};
  setMipBufferSize(buffers, 1, 4, 4, GL_RGBA);

  SECTION("constructor with buffers")
  {
    const auto texture = Texture{
      4,
      4,
      RgbaF{1.0f, 0.0f, 0.0f, 1.0f},
      GL_RGBA,
      Q2EmbeddedDefaults{1, 2, 3},
      std::move(buffers)};

    CHECK(texture.width() == 4u);
    CHECK(texture.height() == 4u);
    CHECK(texture.sizef() == vm::vec2f{4.0f, 4.0f});
    CHECK(texture.averageColor() == Color{RgbaF{1.0f, 0.0f, 0.0f, 1.0f}});
    CHECK(texture.format() == GL_RGBA);
    CHECK(texture.alphaDomain() == img::ImageAlphaDomain::Opaque);
    CHECK(texture.embeddedDefaults() == EmbeddedDefaults{Q2EmbeddedDefaults{1, 2, 3}});
    CHECK(texture.buffersIfLoaded().size() == 1u);
    CHECK(!texture.isReady());
  }

  SECTION("alphaDomain defaults to Opaque and round-trips through setAlphaDomain")
  {
    auto texture =
      Texture{4, 4, RgbaF{}, GL_RGBA, NoEmbeddedDefaults{}, std::move(buffers)};

    CHECK(texture.alphaDomain() == img::ImageAlphaDomain::Opaque);

    texture.setAlphaDomain(img::ImageAlphaDomain::Binary);
    CHECK(texture.alphaDomain() == img::ImageAlphaDomain::Binary);
  }

  SECTION("Source hints are tagged independently of existing embedded defaults")
  {
    auto texture = Texture{4, 4};
    CHECK(texture.embeddedDefaults() == EmbeddedDefaults{NoEmbeddedDefaults{}});
    texture.setEmbeddedDefaults(SourceEmbeddedDefaults{true});
    CHECK(texture.embeddedDefaults() == EmbeddedDefaults{SourceEmbeddedDefaults{true}});
    texture.setEmbeddedDefaults(SourceEmbeddedDefaults{true, 0.75f});
    CHECK(
      texture.embeddedDefaults()
      == EmbeddedDefaults{SourceEmbeddedDefaults{true, 0.75f}});
    texture.setEmbeddedDefaults(SourceEmbeddedDefaults{false});
    CHECK(texture.embeddedDefaults() == EmbeddedDefaults{SourceEmbeddedDefaults{false}});
    texture.setEmbeddedDefaults(Q2EmbeddedDefaults{1, 2, 3});
    CHECK(texture.embeddedDefaults() == EmbeddedDefaults{Q2EmbeddedDefaults{1, 2, 3}});
  }

  SECTION("constructor with a single buffer")
  {
    const auto texture =
      Texture{4, 4, RgbaF{}, GL_RGBA, NoEmbeddedDefaults{}, std::move(buffers.front())};

    CHECK(texture.width() == 4u);
    CHECK(texture.buffersIfLoaded().size() == 1u);
  }

  SECTION("constructor with only a width and height")
  {
    const auto texture = Texture{8, 4};

    CHECK(texture.width() == 8u);
    CHECK(texture.height() == 4u);
    CHECK(texture.format() == GL_RGBA);
    CHECK(texture.alphaDomain() == img::ImageAlphaDomain::Opaque);
    CHECK(texture.buffersIfLoaded().empty());
  }

  SECTION("isReady, activate, deactivate, upload and drop")
  {
    auto gl = MockGl{};
    installTextureUploadSupport(gl);

    auto texture =
      Texture{4, 4, RgbaF{}, GL_RGBA, NoEmbeddedDefaults{}, std::move(buffers)};

    CHECK(!texture.isReady());
    CHECK(!texture.activate(gl, GL_LINEAR, GL_LINEAR));
    CHECK(!texture.deactivate(gl));
    CHECK(texture.buffersIfLoaded().size() == 1u);

    texture.upload(gl);
    CHECK(texture.isReady());
    CHECK(texture.buffersIfLoaded().empty());

    auto boundTextures = std::vector<GLuint>{};
    gl.onBindTexture = [&](GLenum, const GLuint id) { boundTextures.push_back(id); };

    CHECK(texture.activate(gl, GL_LINEAR, GL_LINEAR));
    CHECK(texture.deactivate(gl));

    REQUIRE(boundTextures.size() == 2u);
    CHECK(boundTextures[0] != 0u);
    CHECK(boundTextures[1] == 0u);

    // uploading an already-ready texture is a no-op that leaves it ready
    texture.upload(gl);
    CHECK(texture.isReady());

    texture.drop(gl);
    CHECK(!texture.isReady());
    CHECK(!texture.activate(gl, GL_LINEAR, GL_LINEAR));
    CHECK(!texture.deactivate(gl));
    CHECK(texture.buffersIfLoaded().empty());

    // dropping an already-dropped texture is a no-op
    texture.drop(gl);
    CHECK(!texture.isReady());

    // uploading an already-dropped texture is also a no-op
    texture.upload(gl);
    CHECK(!texture.isReady());
  }

  SECTION("Source blend layers own paired upload and drop without changing single images")
  {
    const auto image = [](EmbeddedDefaults defaults) {
      auto pixels = TextureBufferList{};
      setMipBufferSize(pixels, 1, 1, 1, GL_RGBA);
      return Texture{1, 1, RgbaF{}, GL_RGBA, std::move(defaults), std::move(pixels)};
    };
    auto gl = MockGl{};
    installTextureUploadSupport(gl);
    auto nextId = GLuint{10};
    auto created = std::vector<GLuint>{};
    auto deleted = std::vector<GLuint>{};
    gl.onGenTextures = [&](const GLsizei count, GLuint* ids) {
      for (GLsizei i = 0; i < count; ++i)
      {
        ids[i] = nextId++;
        created.push_back(ids[i]);
      }
    };
    gl.onDeleteTextures = [&](const GLsizei count, const GLuint* ids) {
      deleted.insert(deleted.end(), ids, ids + count);
    };
    // The old single-image lifecycle acts before the new layer API.
    auto single = image(NoEmbeddedDefaults{});
    single.upload(gl);
    REQUIRE(single.isReady());
    CHECK(single.sourceBlendTexture() == nullptr);
    single.drop(gl);
    CHECK(created == std::vector<GLuint>{10});
    CHECK(deleted == created);
    created.clear();
    deleted.clear();
    auto generic = image(NoEmbeddedDefaults{});
    CHECK_THROWS_AS(
      generic.setSourceBlendTexture(image(NoEmbeddedDefaults{})), std::invalid_argument);
    CHECK(generic.sourceBlendTexture() == nullptr);
    auto texture = image(SourceEmbeddedDefaults{});
    texture.setSourceBlendTexture(image(NoEmbeddedDefaults{}));
    REQUIRE(texture.sourceBlendTexture());
    CHECK(texture.sourceBlendTexture()->buffersIfLoaded().size() == 1);
    CHECK_FALSE(texture.isReady());
    CHECK_FALSE(texture.activate(gl, GL_LINEAR, GL_LINEAR));
    texture.setAlphaDomain(img::ImageAlphaDomain::Binary);
    CHECK(texture.sourceBlendTexture()->alphaDomain() == img::ImageAlphaDomain::Binary);
    CHECK_THROWS_AS(
      texture.setEmbeddedDefaults(NoEmbeddedDefaults{}), std::invalid_argument);
    CHECK_THROWS_AS(
      texture.setSourceBlendTexture(image(NoEmbeddedDefaults{})), std::invalid_argument);
    auto nested = image(SourceEmbeddedDefaults{});
    nested.setSourceBlendTexture(image(NoEmbeddedDefaults{}));
    auto parent = image(SourceEmbeddedDefaults{});
    CHECK_THROWS_AS(
      parent.setSourceBlendTexture(std::move(nested)), std::invalid_argument);
    CHECK(parent.sourceBlendTexture() == nullptr);
    texture.upload(gl);
    REQUIRE(texture.isReady());
    REQUIRE(texture.sourceBlendTexture()->isReady());
    CHECK(created == std::vector<GLuint>{11, 12});
    CHECK(texture.buffersIfLoaded().empty());
    CHECK(texture.sourceBlendTexture()->buffersIfLoaded().empty());
    CHECK_THROWS_AS(
      texture.setSourceBlendTexture(image(NoEmbeddedDefaults{})), std::invalid_argument);
    texture.upload(gl);
    CHECK(created.size() == 2);
    texture.drop(gl);
    CHECK(deleted == std::vector<GLuint>{12, 11});
    CHECK_FALSE(texture.isReady());
    CHECK_FALSE(texture.sourceBlendTexture()->isReady());
    texture.drop(gl);
    texture.upload(gl);
    CHECK(created.size() == 2);
    CHECK(deleted.size() == 2);
  }

  SECTION("Source blend masks own a third unnested data image and paired lifetime")
  {
    const auto image = [](EmbeddedDefaults defaults) {
      auto pixels = TextureBufferList{};
      setMipBufferSize(pixels, 1, 1, 1, GL_RGBA);
      return Texture{1, 1, RgbaF{}, GL_RGBA, std::move(defaults), std::move(pixels)};
    };
    auto gl = MockGl{};
    installTextureUploadSupport(gl);
    auto nextId = GLuint{10};
    auto created = std::vector<GLuint>{};
    auto deleted = std::vector<GLuint>{};
    gl.onGenTextures = [&](const GLsizei count, GLuint* ids) {
      for (GLsizei i = 0; i < count; ++i)
      {
        ids[i] = nextId++;
        created.push_back(ids[i]);
      }
    };
    gl.onDeleteTextures = [&](const GLsizei count, const GLuint* ids) {
      deleted.insert(deleted.end(), ids, ids + count);
    };
    auto generic = image(NoEmbeddedDefaults{});
    CHECK_THROWS_AS(
      generic.setSourceBlendMaskTexture(image(NoEmbeddedDefaults{})),
      std::invalid_argument);
    CHECK(generic.sourceBlendMaskTexture() == nullptr);
    auto texture = image(SourceEmbeddedDefaults{});
    CHECK_THROWS_AS(
      texture.setSourceBlendMaskTexture(image(NoEmbeddedDefaults{})),
      std::invalid_argument);
    texture.setSourceBlendTexture(image(NoEmbeddedDefaults{}));
    auto nested = image(SourceEmbeddedDefaults{});
    nested.setSourceBlendTexture(image(NoEmbeddedDefaults{}));
    CHECK_THROWS_AS(
      texture.setSourceBlendMaskTexture(std::move(nested)), std::invalid_argument);
    auto dropped = image(NoEmbeddedDefaults{});
    dropped.drop(gl);
    CHECK_THROWS_AS(
      texture.setSourceBlendMaskTexture(std::move(dropped)), std::invalid_argument);
    CHECK_THROWS_AS(
      texture.setSourceBlendMaskTexture(Texture{1, 1}), std::invalid_argument);
    CHECK(texture.sourceBlendMaskTexture() == nullptr);
    auto mask = image(NoEmbeddedDefaults{});
    mask.setAlphaDomain(img::ImageAlphaDomain::Binary);
    texture.setSourceBlendMaskTexture(std::move(mask));
    REQUIRE(texture.sourceBlendMaskTexture());
    REQUIRE(texture.sourceBlendTexture());
    CHECK(texture.sourceBlendMaskTexture()->buffersIfLoaded().size() == 1);
    CHECK(
      texture.sourceBlendMaskTexture()->alphaDomain() == img::ImageAlphaDomain::Opaque);
    texture.setAlphaDomain(img::ImageAlphaDomain::Binary);
    CHECK(texture.sourceBlendTexture()->alphaDomain() == img::ImageAlphaDomain::Binary);
    CHECK(
      texture.sourceBlendMaskTexture()->alphaDomain() == img::ImageAlphaDomain::Opaque);
    CHECK_THROWS_AS(
      texture.setSourceBlendMaskTexture(image(NoEmbeddedDefaults{})),
      std::invalid_argument);
    CHECK_THROWS_AS(
      texture.setEmbeddedDefaults(NoEmbeddedDefaults{}), std::invalid_argument);
    CHECK_FALSE(texture.isReady());
    CHECK_FALSE(texture.activate(gl, GL_LINEAR, GL_LINEAR));
    auto moved = std::move(texture);
    REQUIRE(moved.sourceBlendTexture());
    REQUIRE(moved.sourceBlendMaskTexture());
    moved.upload(gl);
    REQUIRE(moved.isReady());
    CHECK(moved.sourceBlendTexture()->isReady());
    CHECK(moved.sourceBlendMaskTexture()->isReady());
    CHECK(created == std::vector<GLuint>{10, 11, 12});
    CHECK(moved.buffersIfLoaded().empty());
    CHECK(moved.sourceBlendTexture()->buffersIfLoaded().empty());
    CHECK(moved.sourceBlendMaskTexture()->buffersIfLoaded().empty());
    moved.upload(gl);
    CHECK(created.size() == 3);
    moved.drop(gl);
    CHECK(deleted == std::vector<GLuint>{12, 11, 10});
    CHECK_FALSE(moved.isReady());
    CHECK_FALSE(moved.sourceBlendTexture()->isReady());
    CHECK_FALSE(moved.sourceBlendMaskTexture()->isReady());
    moved.drop(gl);
    moved.upload(gl);
    CHECK(created.size() == 3);
    CHECK(deleted.size() == 3);
    created.clear();
    deleted.clear();
    auto late = image(SourceEmbeddedDefaults{});
    late.setSourceBlendTexture(image(NoEmbeddedDefaults{}));
    late.upload(gl);
    REQUIRE(late.isReady());
    CHECK_THROWS_AS(
      late.setSourceBlendMaskTexture(image(NoEmbeddedDefaults{})), std::invalid_argument);
    CHECK(late.sourceBlendMaskTexture() == nullptr);
    late.drop(gl);
    CHECK(created == std::vector<GLuint>{13, 14});
    CHECK(deleted == std::vector<GLuint>{14, 13});
    CHECK_THROWS_AS(
      late.setSourceBlendMaskTexture(image(NoEmbeddedDefaults{})), std::invalid_argument);
  }

  SECTION("activate selects the mipmap or masked filter mode")
  {
    auto gl = MockGl{};
    installTextureUploadSupport(gl);

    auto filterParams = std::vector<TexParam>{};
    gl.onTexParameteri = [&](GLenum, const GLenum pname, const GLint param) {
      if (pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MAG_FILTER)
      {
        filterParams.emplace_back(pname, param);
      }
    };

    SECTION("uploads mipmaps for an unmasked, multi-level texture")
    {
      auto multiLevelBuffers = TextureBufferList{};
      setMipBufferSize(multiLevelBuffers, 2, 4, 4, GL_RGBA);

      auto texture = Texture{
        4, 4, RgbaF{}, GL_RGBA, NoEmbeddedDefaults{}, std::move(multiLevelBuffers)};

      texture.upload(gl);
      // upload() itself may already have written filter params (e.g. for a masked
      // texture); only the ones from activate() are under test here
      filterParams.clear();
      CHECK(texture.activate(gl, GL_LINEAR_MIPMAP_LINEAR, GL_NEAREST_MIPMAP_NEAREST));

      CHECK(
        filterParams
        == std::vector<TexParam>{
          {GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR},
          {GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST}});
    }

    SECTION("falls back to the non-mipmap filter for a single-level texture")
    {
      auto texture =
        Texture{4, 4, RgbaF{}, GL_RGBA, NoEmbeddedDefaults{}, std::move(buffers)};

      texture.upload(gl);
      filterParams.clear();
      CHECK(texture.activate(gl, GL_LINEAR_MIPMAP_LINEAR, GL_NEAREST_MIPMAP_NEAREST));

      // useMipmap is false here (only one mip level), so each requested filter
      // collapses to its non-mipmap equivalent
      CHECK(
        filterParams
        == std::vector<TexParam>{
          {GL_TEXTURE_MIN_FILTER, GL_LINEAR}, {GL_TEXTURE_MAG_FILTER, GL_NEAREST}});
    }

    SECTION("forces nearest filtering for a masked texture")
    {
      auto texture =
        Texture{4, 4, RgbaF{}, GL_RGBA, NoEmbeddedDefaults{}, std::move(buffers)};
      texture.setAlphaDomain(img::ImageAlphaDomain::Binary);

      texture.upload(gl);
      filterParams.clear();
      CHECK(texture.activate(gl, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR));

      // masked textures always force GL_NEAREST, regardless of the requested filter
      CHECK(
        filterParams
        == std::vector<TexParam>{
          {GL_TEXTURE_MIN_FILTER, GL_NEAREST}, {GL_TEXTURE_MAG_FILTER, GL_NEAREST}});
    }
  }

  SECTION("upload of a compressed texture")
  {
    auto compressedBuffers = TextureBufferList{};
    setMipBufferSize(compressedBuffers, 1, 4, 4, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);

    auto gl = MockGl{};
    installTextureUploadSupport(gl);

    auto capturedFormat = GLenum{0};
    gl.onCompressedTexImage2D = [&](
                                  GLenum,
                                  GLint,
                                  const GLenum internalformat,
                                  GLsizei,
                                  GLsizei,
                                  GLint,
                                  GLsizei,
                                  const GLvoid*) { capturedFormat = internalformat; };

    auto texture = Texture{
      4,
      4,
      RgbaF{},
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
      NoEmbeddedDefaults{},
      std::move(compressedBuffers)};

    texture.upload(gl);
    CHECK(texture.isReady());
    CHECK(capturedFormat == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
  }

  // NOLINTEND(bugprone-use-after-move)
}

} // namespace tb::gl
