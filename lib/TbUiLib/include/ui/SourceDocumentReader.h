/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/VmfDocument.h"

#include <QString>

#include <optional>

namespace tb::ui
{
/** Shared read-only saved-document boundary for Source tools. Bounds bytes before
 * reading and entries during parsing; never reconstructs or writes a VMF. */
std::optional<mdl::VmfDocument> readSavedSourceDocument(
  const QString& path, QString& error);
} // namespace tb::ui
