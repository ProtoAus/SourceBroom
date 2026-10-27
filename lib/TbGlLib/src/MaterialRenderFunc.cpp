/*
 Copyright (C) 2010 Kristian Duske

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

#include "gl/MaterialRenderFunc.h"

#include "gl/ActiveShader.h"
#include "gl/GlInterface.h"
#include "gl/Material.h"
#include "gl/Texture.h"

#include "kd/path_utils.h"

#include <stdexcept>

namespace tb::gl
{

MaterialRenderFunc::~MaterialRenderFunc() = default;
void MaterialRenderFunc::before(Gl&, const Material*) {}
void MaterialRenderFunc::after(Gl&, const Material*) {}

DefaultMaterialRenderFunc::DefaultMaterialRenderFunc(
  const int minFilter, const int magFilter)
  : m_minFilter{minFilter}
  , m_magFilter{magFilter}
{
}

void DefaultMaterialRenderFunc::before(Gl& gl, const Material* material)
{
  if (material)
  {
    material->activate(gl, m_minFilter, m_magFilter);
  }
}

void DefaultMaterialRenderFunc::after(Gl& gl, const Material* material)
{
  if (material)
  {
    material->deactivate(gl);
  }
}

SourceBlendMaterialRenderFunc::SourceBlendMaterialRenderFunc(
  ActiveShader& shader,
  const bool applyMaterial,
  const int minFilter,
  const int magFilter)
  : DefaultMaterialRenderFunc(minFilter, magFilter)
  , m_shader(shader)
  , m_applyMaterial(applyMaterial)
  , m_minFilter(minFilter)
  , m_magFilter(magFilter)
{
}

void SourceBlendMaterialRenderFunc::before(Gl& gl, const Material* material)
{
  if (m_previousActiveTexture)
  {
    throw std::logic_error("Source blend material activation must be paired");
  }
  // Fail inactive/unlinked uniforms before any GL state or resource activation.
  m_shader.set("ApplySourceBlend", false);
  m_shader.set("SourceBlendMaterial", 1);
  auto active = GLint{};
  gl.getIntegerv(GL_ACTIVE_TEXTURE, &active);
  m_previousActiveTexture = active;
  gl.activeTexture(GL_TEXTURE0);
  const auto* texture = getTexture(material);
  // Preserve the Face demand planner: don't request every still-unloaded material.
  if (texture)
  {
    DefaultMaterialRenderFunc::before(gl, material);
    m_materialActivated = true;
  }
  const auto* second = texture ? texture->sourceBlendTexture() : nullptr;
  if (!m_applyMaterial || !texture || !texture->isReady() || !second)
  {
    return;
  }
  gl.activeTexture(GL_TEXTURE1);
  auto binding = GLint{};
  gl.getIntegerv(GL_TEXTURE_BINDING_2D, &binding);
  m_previousSecondaryTexture = binding;
  const auto ready = second->activate(gl, m_minFilter, m_magFilter);
  gl.activeTexture(GL_TEXTURE0);
  m_shader.set("ApplySourceBlend", ready);
}

void SourceBlendMaterialRenderFunc::after(Gl& gl, const Material* material)
{
  if (!m_previousActiveTexture)
  {
    return;
  }
  if (m_previousSecondaryTexture)
  {
    gl.activeTexture(GL_TEXTURE1);
    gl.bindTexture(GL_TEXTURE_2D, static_cast<GLuint>(*m_previousSecondaryTexture));
    m_previousSecondaryTexture.reset();
  }
  gl.activeTexture(GL_TEXTURE0);
  if (m_materialActivated)
  {
    DefaultMaterialRenderFunc::after(gl, material);
    m_materialActivated = false;
  }
  gl.activeTexture(static_cast<GLenum>(*m_previousActiveTexture));
  m_previousActiveTexture.reset();
}

void setAlphaFuncUniforms(ActiveShader& shader, const Material* material)
{
  const auto alphaFunc =
    material ? material->effectiveAlphaFunc() : std::optional<MaterialAlphaFunc>{};
  shader.set("EnableMasked", alphaFunc.has_value());
  const auto* texture = material ? material->texture() : nullptr;
  shader.set(
    "IgnoreTextureAlpha",
    texture && texture->alphaDomain() != img::ImageAlphaDomain::Graduated
      && kdl::path_to_lower(material->relativePath().extension()) == ".vmt");
  if (alphaFunc)
  {
    shader.set("AlphaFuncCompare", static_cast<size_t>(alphaFunc->compare));
    shader.set("AlphaFuncThreshold", alphaFunc->threshold);
  }
}

AlphaTestedMaterialRenderFunc::AlphaTestedMaterialRenderFunc(
  ActiveShader& shader, const int minFilter, const int magFilter)
  : DefaultMaterialRenderFunc{minFilter, magFilter}
  , m_shader{shader}
{
}

void AlphaTestedMaterialRenderFunc::before(Gl& gl, const Material* material)
{
  DefaultMaterialRenderFunc::before(gl, material);
  setAlphaFuncUniforms(m_shader, material);
}

} // namespace tb::gl
