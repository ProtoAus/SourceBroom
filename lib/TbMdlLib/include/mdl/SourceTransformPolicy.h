/*
SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

namespace tb::mdl
{
/** Command intent, not an approximate matrix classification. */
enum class SourceTransformPolicy
{
  TranslationOnly,
  ProperRotation,
  PositiveUniformScale,
};
} // namespace tb::mdl
