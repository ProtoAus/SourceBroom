/*
 Copyright (C) 2026 ProtoAus

 This file is part of SourceBroom, a fork of TrenchBroom.
 SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mdl/SourceCapabilities.h"

#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace tb::mdl
{
namespace
{
using Severity = SourceCapabilitySeverity;
using Feature = SourceCapabilityFeature;

bool oneOf(
  const std::string_view value, const std::initializer_list<std::string_view> values)
{
  return std::find(values.begin(), values.end(), value) != values.end();
}

std::string_view key(const VmfDocument& document, const VmfEntryId id)
{
  return document.tokenContents(document.entries()[id].key);
}
} // namespace

bool SourceCapabilityAssessment::mayCompile() const
{
  return complete
         && std::none_of(
           diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
             return diagnostic.severity == Severity::Error;
           });
}

SourceCapabilityAssessment assessSourceCapabilities(
  const VmfDocument& document,
  const SourceCapabilityTarget target,
  const SourceCapabilityOptions& options)
{
  auto result = SourceCapabilityAssessment{target, true, {}};
  const auto global =
    [&](const Severity severity, const Feature feature, std::string message) {
      return SourceCapabilityDiagnostic{
        severity, feature, std::move(message), {}, {}, {}, {}, {}, {}};
    };
  // Reserve the last slot for a refusal rather than silently dropping findings.
  const auto add = [&](SourceCapabilityDiagnostic diagnostic) {
    if (!result.complete)
    {
      return;
    }
    if (result.diagnostics.size() + 1 >= options.maxDiagnostics)
    {
      result.complete = false;
      if (options.maxDiagnostics != 0)
      {
        result.diagnostics.push_back(global(
          Severity::Error,
          Feature::AssessmentLimit,
          "Preflight diagnostic budget exhausted; increase maxDiagnostics and reassess. "
          "No compiler should start with an incomplete assessment."));
      }
      return;
    }
    result.diagnostics.push_back(std::move(diagnostic));
  };

  if (
    document.entries().size() > options.maxEntries
    || document.source().size() > options.maxSourceBytes)
  {
    add(global(
      Severity::Error,
      Feature::AssessmentLimit,
      "Native document exceeds the preflight entry/byte budget; increase the explicit "
      "budget and reassess before starting a compiler."));
    result.complete = false;
    return result;
  }

  add(global(
    Severity::Information,
    Feature::AssessmentScope,
    "Bounded native VMF preflight only, not whole-map validation or a compatibility "
    "certificate. Geometry, numerical world bounds, entity/FGD semantics, assets and "
    "external compiler support are not validated. Editor preview/edit restrictions "
    "do not prohibit compiling unchanged native features. No VMF bytes are changed."));
  if (target == SourceCapabilityTarget::Generic)
  {
    add(global(
      Severity::Warning,
      Feature::TargetContract,
      "Generic Source does not certify any Source game or branch. Select the installed "
      "game's compiler/profile and verify its output and runtime independently."));
  }
  if (options.requiredTarget)
  {
    const auto& required = *options.requiredTarget;
    if (required.reason.empty())
    {
      add(global(
        Severity::Error,
        Feature::TargetContract,
        "Explicit required-target contract lacks a provenance/reason; correct the "
        "project "
        "contract before compiling."));
    }
    add(global(
      required.target == target ? Severity::Information : Severity::Error,
      Feature::TargetContract,
      std::string{
        required.target == target
          ? "Explicit caller/project target contract matches: "
          : "Selected target conflicts with explicit caller/project target contract; "
            "select the required target or correct the project contract: "}
        + required.reason));
  }

  const auto& entries = document.entries();
  // Parser entries are in source order, with parents before children. Keep identity
  // propagation linear even for documents containing many deeply nested entries.
  auto nativeIds = std::vector<std::optional<VmfEntryId>>(entries.size());
  for (VmfEntryId id = 0; id < entries.size(); ++id)
  {
    if (entries[id].value && entries[id].parent && key(document, id) == "id")
    {
      auto& nativeId = nativeIds[*entries[id].parent];
      if (!nativeId)
      {
        nativeId = id;
      }
    }
  }
  auto objects = std::vector<std::optional<VmfEntryId>>(entries.size());
  auto opaque = std::vector<bool>(entries.size(), false);
  size_t scannedOffset = 0;
  size_t line = 1;
  size_t column = 1;
  size_t worlds = 0;
  bool sawFormat = false;
  bool sawMetadata = false;
  for (VmfEntryId id = 0; id < entries.size() && result.complete; ++id)
  {
    const auto& entry = entries[id];
    if (entry.parent)
    {
      objects[id] = objects[*entry.parent];
      opaque[id] = opaque[*entry.parent];
    }
    if (nativeIds[id])
    {
      objects[id] = id;
    }
    while (scannedOffset < entry.key.offset)
    {
      const auto c = document.source()[scannedOffset++];
      if (
        c == '\n'
        || (c == '\r' && (scannedOffset == document.source().size() || document.source()[scannedOffset] != '\n')))
      {
        ++line;
        column = 1;
      }
      else
      {
        ++column;
      }
    }
    const auto finding =
      [&](const Severity severity, const Feature feature, std::string message) {
        auto diagnostic = global(severity, feature, std::move(message));
        diagnostic.entry = id;
        diagnostic.objectEntry = objects[id];
        if (objects[id])
        {
          diagnostic.nativeId =
            std::string{document.tokenContents(*entries[*nativeIds[*objects[id]]].value)};
        }
        diagnostic.offset = entry.key.offset;
        diagnostic.line = line;
        diagnostic.column = column;
        add(std::move(diagnostic));
      };
    const auto name = key(document, id);
    const auto parentName =
      entry.parent ? key(document, *entry.parent) : std::string_view{};
    if (!entry.parent && name == "world" && entry.isBlock())
    {
      if (++worlds > 1)
      {
        finding(
          Severity::Error,
          Feature::NativeStructure,
          "Multiple native world blocks; provide one authoritative world before "
          "compiling.");
      }
    }
    if (opaque[id])
    {
      continue;
    }
    if (entry.isBlock())
    {
      if (name == "dispinfo")
      {
        finding(
          Severity::Warning,
          Feature::Displacement,
          "Displacement data requires external compiler support; topology/power/branch "
          "limits are unassessed. Native bytes may compile unchanged; editor sculpting "
          "and safe transforms are not established. Check in the target "
          "Hammer/compiler.");
        opaque[id] = true;
      }
      else if (
        oneOf(
          name,
          {"editor",
           "visgroups",
           "viewsettings",
           "cameras",
           "cordons",
           "cordon",
           "group"}))
      {
        if (!sawMetadata)
        {
          finding(
            Severity::Information,
            Feature::EditorMetadata,
            "Native editor metadata is preserved, not a compiler-target declaration. "
            "Check active cordons/hidden selections and target Hammer interpretation; "
            "metadata authoring support is assessed separately.");
          sawMetadata = true;
        }
        if (name == "cordon" || name == "cordons")
        {
          finding(
            Severity::Warning,
            Feature::Bounds,
            "Cordon bounds/activation require review in the selected compiler/Hammer; "
            "this preflight does not validate branch coordinate limits.");
        }
        opaque[id] = true;
      }
      else if (!oneOf(
                 name,
                 {"world",
                  "entity",
                  "solid",
                  "side",
                  "hidden",
                  "connections",
                  "versioninfo"}))
      {
        finding(Severity::Warning, Feature::UnknownData,
                "Unassessed native block '" + std::string{name}
                  + "' is preserved. Confirm this extension with the target compiler; "
                    "it is not certified by matching a BSP header.");
        opaque[id] = true;
      }
    }
    else
    {
      const auto value = document.tokenContents(*entry.value);
      if (parentName == "versioninfo" && name == "formatversion")
      {
        sawFormat = true;
        if (value != "100")
        {
          finding(
            Severity::Warning,
            Feature::Version,
            "Unrecognized VMF formatversion; native edit support and external compiler "
            "acceptance must be checked separately. VMF formatversion is not BSP "
            "version.");
        }
      }
      else if (
        parentName == "versioninfo"
        && !oneOf(name, {"editorversion", "editorbuild", "mapversion", "prefab"}))
      {
        finding(
          Severity::Warning,
          Feature::UnknownData,
          "Unknown version metadata '" + std::string{name}
            + "'; no target capability is inferred from it.");
      }
      if (parentName == "entity" && name == "classname")
      {
        if (oneOf(value, {"info_overlay", "info_overlay_transition"}))
        {
          finding(
            Severity::Warning,
            Feature::Overlay,
            "Overlay face references/projection require target compiler verification. "
            "Unchanged native overlays may compile; editor preview/reference editing "
            "is not established by this assessment.");
        }
        else if (
          oneOf(
            value, {"func_instance", "func_instance_parms", "func_instance_io_proxy"}))
        {
          finding(
            Severity::Warning,
            Feature::Instance,
            "Instance expansion/fixups and referenced VMFs require the selected external "
            "compiler. This preflight neither resolves instances nor flattens them; "
            "verify paths, branch support and expansion results.");
        }
      }
      if (parentName == "entity" && oneOf(name, {"modelscale", "uniformscale", "scale"}))
      {
        finding(Severity::Warning, Feature::PropScaling,
                "Entity scaling property '" + std::string{name}
                  + "' requires classname/game/compiler verification; branch scaling limits "
                    "and model rendering are unassessed, not automatically incompatible.");
      }
      if (
        oneOf(
          name,
          {"world_mins", "world_maxs", "mapbounds", "bounds", "minbounds", "maxbounds"}))
      {
        finding(
          Severity::Warning,
          Feature::Bounds,
          "Explicit bounds metadata requires branch/compiler verification; neither "
          "extended world bounds nor numerical coordinate limits are certified.");
      }
      if ((parentName == "solid" && name != "id")
          || (parentName == "side" && !oneOf(name, {"id", "plane", "material", "uaxis", "vaxis", "rotation", "lightmapscale", "smoothing_groups"})))
      {
        finding(
          Severity::Warning,
          Feature::UnknownData,
          "Unassessed geometry property '" + std::string{name}
            + "'; preserve it and verify the selected branch compiler's interpretation.");
      }
    }
  }
  if (worlds == 0)
  {
    add(global(
      Severity::Error,
      Feature::NativeStructure,
      "Missing native root world block; provide an authoritative world before "
      "compiling."));
  }
  if (!sawFormat)
  {
    add(global(
      Severity::Warning,
      Feature::Version,
      "No recognized versioninfo/formatversion declaration was assessed. Verify native "
      "format acceptance in the target compiler; no branch is inferred."));
  }
  return result;
}

} // namespace tb::mdl
