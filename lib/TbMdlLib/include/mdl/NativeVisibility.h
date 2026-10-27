/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

namespace tb::mdl
{
/** Derived viewport causes, independent of ordinary session VisibilityState.
 * NativeVisgroups retains the authoritative optional flags and provenance. */
struct NativeVisibility
{
  bool wrapper = false;
  bool manual = false;
  bool automatic = false;
  bool hidden() const { return wrapper || manual || automatic; }
  bool operator==(const NativeVisibility&) const = default;
};
} // namespace tb::mdl
