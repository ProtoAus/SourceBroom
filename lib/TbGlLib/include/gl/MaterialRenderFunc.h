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

#pragma once

#include <optional>

namespace tb::gl
{
class ActiveShader;
class Gl;
class Material;

class MaterialRenderFunc
{
public:
  virtual ~MaterialRenderFunc();
  virtual void before(Gl& gl, const Material* material);
  virtual void after(Gl& gl, const Material* material);
};

class DefaultMaterialRenderFunc : public MaterialRenderFunc
{
private:
  int m_minFilter;
  int m_magFilter;

public:
  DefaultMaterialRenderFunc(int minFilter, int magFilter);

  void before(Gl& gl, const Material* material) override;
  void after(Gl& gl, const Material* material) override;
};

// Face-only two-color preview with optional modulation data. Owns unit0 activation,
// restoring the caller's active unit and prior unit1/unit2 bindings in after().
class SourceBlendMaterialRenderFunc : public DefaultMaterialRenderFunc
{
private:
  ActiveShader& m_shader;
  bool m_applyMaterial;
  int m_minFilter;
  int m_magFilter;
  bool m_materialActivated = false;
  std::optional<int> m_previousActiveTexture;
  std::optional<int> m_previousSecondaryTexture;
  std::optional<int> m_previousMaskTexture;

public:
  SourceBlendMaterialRenderFunc(
    ActiveShader& shader, bool applyMaterial, int minFilter, int magFilter);
  void before(Gl& gl, const Material* material) override;
  void after(Gl& gl, const Material* material) override;
};

/**
 * Sets the EnableMasked/AlphaFuncCompare/AlphaFuncThreshold uniforms on shader from
 * material's effectiveAlphaFunc(), so the shader's alpha-test cutout matches the
 * material's derived or overridden alpha-test state. Also sets IgnoreTextureAlpha
 * for Source opaque/cutout pixels after the cutout test, preserving editor fades without
 * blending a shader mask as opacity. Sets demanded Source MaterialOpacity too;
 * untextured draws can disable that multiplier without changing legacy cutout rules.
 * material may be null; missing/disabled opacity resets to 1.
 */
void setAlphaFuncUniforms(
  ActiveShader& shader, const Material* material, bool applyOpacity = true);

/**
 * Like DefaultMaterialRenderFunc, but also sets the alpha-test uniforms via
 * setAlphaFuncUniforms(), for shaders that support alpha-test cutout.
 */
class AlphaTestedMaterialRenderFunc : public DefaultMaterialRenderFunc
{
private:
  ActiveShader& m_shader;

public:
  AlphaTestedMaterialRenderFunc(ActiveShader& shader, int minFilter, int magFilter);

  void before(Gl& gl, const Material* material) override;
};

} // namespace tb::gl
