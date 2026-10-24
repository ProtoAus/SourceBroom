/*
 Copyright (C) 2026 ProtoAus

 This file is part of SourceBroom, a fork of TrenchBroom.
 SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "mdl/VmfDocument.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace tb::mdl
{

enum class SourceCapabilityTarget
{
  Generic,
  ClassicCss,
  Strata
};

enum class SourceCapabilitySeverity
{
  Information,
  Warning,
  Error
};

enum class SourceCapabilityFeature
{
  AssessmentScope,
  NativeStructure,
  TargetContract,
  Version,
  Displacement,
  Overlay,
  Instance,
  PropScaling,
  Bounds,
  EditorMetadata,
  UnknownData,
  AssessmentLimit
};

struct SourceCapabilityDiagnostic
{
  SourceCapabilitySeverity severity;
  SourceCapabilityFeature feature;
  std::string message;
  std::optional<VmfEntryId> entry;
  /** Nearest ancestor (or entry itself) with a native id property. Not a scene ID. */
  std::optional<VmfEntryId> objectEntry;
  std::optional<std::string> nativeId;
  /** Byte offset and one-based line/byte-column; absent for document-wide findings. */
  std::optional<size_t> offset;
  std::optional<size_t> line;
  std::optional<size_t> column;
};

/** Explicit external project contract, never inferred from VMF/editor versions. */
struct SourceRequiredTarget
{
  SourceCapabilityTarget target;
  std::string reason;
};

struct SourceCapabilityOptions
{
  std::optional<SourceRequiredTarget> requiredTarget;
  size_t maxEntries = 1'000'000;
  size_t maxSourceBytes = 128 * 1024 * 1024;
  /** Includes the final assessment-limit error if findings are truncated. */
  size_t maxDiagnostics = 1024;
};

struct SourceCapabilityAssessment
{
  SourceCapabilityTarget target;
  bool complete = true;
  std::vector<SourceCapabilityDiagnostic> diagnostics;

  /** No known fatal finding; NOT certification of compiler/game compatibility. */
  bool mayCompile() const;
};

/**
 * Read-only, bounded native-document preflight. Not whole-map validation, scene
 * edit permission, geometry/bounds validation, asset resolution or BSP validation.
 * Warnings must be disclosed but do not prevent unchanged external compilation.
 * Syntax parsing/resource errors remain VmfDocument::parse's responsibility.
 */
SourceCapabilityAssessment assessSourceCapabilities(
  const VmfDocument& document,
  SourceCapabilityTarget target,
  const SourceCapabilityOptions& options = {});

} // namespace tb::mdl
