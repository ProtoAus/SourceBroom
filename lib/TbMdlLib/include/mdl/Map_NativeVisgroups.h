/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
#include "mdl/NativeVisgroups.h"
#include "mdl/NativeVisibility.h"
namespace tb::mdl
{
class Map;
enum class NativeVisgroupApplyStatus
{
  Changed,
  Unchanged,
  Refused
};
struct NativeVisgroupApplyResult
{
  NativeVisgroupApplyStatus status;
  std::string reason;
};
NativeVisgroupSnapshot nativeVisgroups(const Map& map);
// Display-only observations, never request or replay authority. Unknown for
// members without a proven current projected node. Session excludes view filters.
struct NativeVisgroupMemberView
{
  NativeVisgroupMemberId id;
  std::optional<NativeVisibility> effectiveNativeVisibility;
  std::optional<bool> sessionVisible;
};
struct NativeVisgroupView
{
  NativeVisgroupSnapshot metadata;
  std::vector<NativeVisgroupMemberView> members;
};
NativeVisgroupView nativeVisgroupView(const Map& map);
NativeVisgroupApplyResult applyNativeVisgroups(
  Map& map, const NativeVisgroupRequest& request);
// Resolves only actual current explicit selection; backend expands owning closures.
std::vector<NativeVisgroupMemberId> nativeVisgroupSelection(const Map& map);
} // namespace tb::mdl
