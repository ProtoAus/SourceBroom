/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "vm/vec.h"

#include <functional>
#include <optional>

namespace tb::ui
{
// Read-only draft values: no native IDs, scope, projection or write authority.
struct NativeCameraDraft
{
  vm::vec3d eye;
  vm::vec3d look;
};
using NativeCameraCapture = std::function<std::optional<NativeCameraDraft>()>;
} // namespace tb::ui
