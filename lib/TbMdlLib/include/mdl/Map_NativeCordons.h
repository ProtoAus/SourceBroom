/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/NativeCordons.h"

namespace tb::mdl
{
class Map;

enum class NativeCordonApplyStatus
{
  Changed,
  Unchanged,
  Refused
};
struct NativeCordonApplyResult
{
  NativeCordonApplyStatus status;
  std::string reason;
};

// Owns all values; refusalReason also explains non-native/preview admission.
NativeCordonSnapshot nativeCordons(const Map& map);
// Capture the snapshot at row-edit time, not dispatch time. Refused/no-op requests
// never enter history. Existing selectionBounds() supplies Fit input at invocation.
NativeCordonApplyResult applyNativeCordons(Map& map, const NativeCordonRequest& request);
} // namespace tb::mdl
