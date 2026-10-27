/*
 Copyright (C) 2024 Kristian Duske

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

#include "base/Color.h"
#include "gl/GlUtils.h"
#include "gl/TextureBuffer.h"
#include "img/ImageAlphaDomain.h"

#include "kd/reflection_decl.h"

#include <memory>
#include <variant>
#include <vector>

namespace tb::gl
{
class Gl;

struct NoEmbeddedDefaults
{
  kdl_reflect_decl_empty(NoEmbeddedDefaults);
};

struct Q2EmbeddedDefaults
{
  int flags;
  int contents;
  int value;

  kdl_reflect_decl(Q2EmbeddedDefaults, flags, contents, value);
};

// Source VMT render hints travel with demanded pixels, not catalog metadata.
struct SourceEmbeddedDefaults
{
  bool noCull = false;
  float alphaTestReference = 0.5f;
  float opacity = 1.0f;
  kdl_reflect_decl(SourceEmbeddedDefaults, noCull, alphaTestReference, opacity);
};

using EmbeddedDefaults =
  std::variant<NoEmbeddedDefaults, Q2EmbeddedDefaults, SourceEmbeddedDefaults>;

std::ostream& operator<<(std::ostream& lhs, const EmbeddedDefaults& rhs);

struct TextureLoadedState
{
  std::vector<TextureBuffer> buffers;

  kdl_reflect_decl(TextureLoadedState, buffers);
};

struct TextureReadyState
{
  GLuint textureId;
  bool useMipmap;

  kdl_reflect_decl(TextureReadyState, textureId, useMipmap);
};

struct TextureDroppedState
{
  kdl_reflect_decl_empty(TextureDroppedState);
};

using TextureState =
  std::variant<TextureLoadedState, TextureReadyState, TextureDroppedState>;

class Texture
{
private:
  size_t m_width;
  size_t m_height;
  Color m_averageColor;

  GLenum m_format;
  img::ImageAlphaDomain m_alphaDomain = img::ImageAlphaDomain::Opaque;

  EmbeddedDefaults m_embeddedDefaults;
  std::unique_ptr<Texture> m_sourceBlendTexture;

  mutable TextureState m_state;

  kdl_reflect_decl(
    Texture,
    m_width,
    m_height,
    m_averageColor,
    m_format,
    m_alphaDomain,
    m_embeddedDefaults,
    m_sourceBlendTexture,
    m_state);

public:
  Texture(
    size_t width,
    size_t height,
    Color averageColor,
    GLenum format,
    EmbeddedDefaults embeddedDefaults,
    std::vector<TextureBuffer> buffers);

  Texture(
    size_t width,
    size_t height,
    Color averageColor,
    GLenum format,
    EmbeddedDefaults embeddedDefaults,
    TextureBuffer buffer);

  Texture(size_t width, size_t height);

  Texture(const Texture& other) = delete;
  Texture(Texture&& other) = default;

  Texture& operator=(const Texture& other) = delete;
  Texture& operator=(Texture&& other) = default;

  ~Texture() = default;

  size_t width() const;
  size_t height() const;
  vm::vec2f sizef() const;
  const Color& averageColor() const;

  GLenum format() const;

  img::ImageAlphaDomain alphaDomain() const;
  void setAlphaDomain(img::ImageAlphaDomain alphaDomain);

  const EmbeddedDefaults& embeddedDefaults() const;
  void setEmbeddedDefaults(EmbeddedDefaults defaults);

  const Texture* sourceBlendTexture() const;
  void setSourceBlendTexture(Texture texture);

  bool isReady() const;

  bool activate(Gl& gl, int minFilter, int magFilter) const;
  bool deactivate(Gl& gl) const;

  void upload(Gl& gl);
  void drop(Gl& gl);

  const std::vector<TextureBuffer>& buffersIfLoaded() const;

private:
  void setFilterMode(Gl& gl, int minFilter, int magFilter, bool useMipmap) const;
};

} // namespace tb::gl
