/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/VmfDocument.h"

#include "vm/bbox.h"
#include "vm/vec.h"

#include <optional>
#include <string>
#include <vector>

namespace tb::mdl
{
enum class SourceMetadataIssue
{
  MissingProperty,
  DuplicateProperty,
  InvalidValue,
  DuplicateId,
  DanglingReference,
  AmbiguousReference,
  Cycle
};

struct SourceMetadataDiagnostic
{
  VmfEntryId entry;
  SourceMetadataIssue issue;
  std::string message;
};

/** IDs are canonical decimal strings, not machine-sized integers. Entry IDs always
 * refer to the input document, including malformed/ambiguous records.
 */
struct SourceEditorGroup
{
  VmfEntryId entry;
  std::optional<std::string> nativeId;
  std::optional<std::string> parentId;
  std::optional<VmfEntryId> parent;
};

struct SourceEditorVisgroup
{
  VmfEntryId entry;
  std::optional<std::string> nativeId;
  std::optional<VmfEntryId> parent;
  std::string name;
};

struct SourceEditorMembership
{
  VmfEntryId entry;
  std::optional<std::string> nativeId;
  std::optional<std::string> groupId;
  std::optional<VmfEntryId> group;
  std::vector<std::string> visgroupIds;
  std::vector<VmfEntryId> visgroups;
  /** True only for an enclosing native hidden block. Visgroup visibility is not
   * inferred from membership; absent/invalid editor flags remain unknown.
   */
  bool hiddenByContainer = false;
  std::optional<bool> visgroupShown;
  std::optional<bool> visgroupAutoShown;
};

struct SourceEditorCamera
{
  VmfEntryId entry;
  std::optional<vm::vec3d> eye;
  std::optional<vm::vec3d> look;
  /** Only finite eye/look pairs with a usable nonzero direction are targets. */
  bool navigable() const;
};

struct SourceEditorCordon
{
  VmfEntryId entry;
  std::string name;
  std::optional<bool> active;
  /** Valid boxes for inspection, never repaired. Use boundsValid before treating
   * these as a complete cordon: malformed siblings must not silently disappear.
   */
  std::vector<vm::bbox3d> boxes;
  bool boundsValid = false;
};

/** Read-only interpretation, not authoring authority. Owns all values; keeps no
 * views or pointers into the document. Does not change visibility, topology, or
 * bytes. Diagnostics do not certify unknown data or whole-map validity.
 */
struct SourceEditorMetadata
{
  std::vector<SourceEditorGroup> groups;
  std::vector<SourceEditorVisgroup> visgroups;
  std::vector<SourceEditorMembership> memberships;
  std::vector<SourceEditorCamera> cameras;
  std::optional<VmfEntryId> activeCamera;
  std::vector<SourceEditorCordon> cordons;
  std::optional<bool> cordonsActive;
  std::vector<SourceMetadataDiagnostic> diagnostics;

  static SourceEditorMetadata read(const VmfDocument& document);
};
} // namespace tb::mdl
