/*
 Copyright (C) 2026 ProtoAus

 This file is part of SourceBroom, a fork of TrenchBroom.
 SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mdl/SourceCapabilities.h"
#include "mdl/VmfDocument.h"

#include <algorithm>
#include <array>
#include <string>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
using Feature = SourceCapabilityFeature;
using Severity = SourceCapabilitySeverity;
using Target = SourceCapabilityTarget;

bool hasFinding(
  const SourceCapabilityAssessment& assessment,
  const Feature feature,
  const Severity severity)
{
  return std::any_of(
    assessment.diagnostics.begin(),
    assessment.diagnostics.end(),
    [&](const auto& diagnostic) {
      return diagnostic.feature == feature && diagnostic.severity == severity;
    });
}

const auto plain = std::string{
  "versioninfo { \"formatversion\" \"100\" \"editorversion\" \"25\" }\r\n"
  "world { \"id\" \"001\" \"classname\" \"worldspawn\" }\r\n"};
} // namespace

TEST_CASE("SourceCapabilities assesses native documents conservatively", "[vmf]")
{
  SECTION("plain CSS and Strata may compile but are not certified by editor version")
  {
    const auto document = VmfDocument::parse(plain);
    for (const auto target : std::array{Target::ClassicCss, Target::Strata})
    {
      const auto assessment = assessSourceCapabilities(document, target);
      CHECK(assessment.target == target);
      CHECK(assessment.complete);
      CHECK(assessment.mayCompile());
      REQUIRE(assessment.diagnostics.size() == 1);
      CHECK(hasFinding(assessment, Feature::AssessmentScope, Severity::Information));
    }
    CHECK(document.serialize() == plain);
  }

  SECTION("generic discloses untested game compatibility without refusing")
  {
    const auto assessment =
      assessSourceCapabilities(VmfDocument::parse(plain), Target::Generic);
    CHECK(assessment.mayCompile());
    CHECK(hasFinding(assessment, Feature::TargetContract, Severity::Warning));
  }

  SECTION("missing and duplicate native worlds refuse")
  {
    for (const auto* source :
         {"entity { id 4 classname light }", "world {} world { id 2 }"})
    {
      const auto assessment =
        assessSourceCapabilities(VmfDocument::parse(source), Target::ClassicCss);
      CHECK_FALSE(assessment.mayCompile());
      CHECK(hasFinding(assessment, Feature::NativeStructure, Severity::Error));
    }
  }

  SECTION("an explicit caller contract refuses mismatch, not an inferred branch limit")
  {
    auto options = SourceCapabilityOptions{};
    options.requiredTarget =
      SourceRequiredTarget{Target::Strata, "Synthetic project manifest requires Strata"};
    const auto document = VmfDocument::parse(plain);
    for (const auto target : std::array{Target::Generic, Target::ClassicCss})
    {
      const auto assessment = assessSourceCapabilities(document, target, options);
      CHECK_FALSE(assessment.mayCompile());
      CHECK(hasFinding(assessment, Feature::TargetContract, Severity::Error));
      CHECK(
        std::any_of(
          assessment.diagnostics.begin(),
          assessment.diagnostics.end(),
          [](const auto& diagnostic) {
            return diagnostic.message.find("Synthetic project manifest")
                   != std::string::npos;
          }));
    }
    CHECK(assessSourceCapabilities(document, Target::Strata, options).mayCompile());
    options.requiredTarget->reason.clear();
    CHECK_FALSE(assessSourceCapabilities(document, Target::Strata, options).mayCompile());
  }

  SECTION("advanced native features warn for both targets without blocking pass through")
  {
    const auto source = plain + R"vmf(
entity { id 10 classname info_overlay sides "3 4" }
entity { id 11 classname func_instance file "synthetic_child.vmf" }
entity { id 12 classname prop_static uniformscale "2" modelscale "1.5" scale "1 2 3" }
entity { id 13 classname custom_game_entity arbitrary_string "not a number" }
world_extension { arbitrary "preserved" }
)vmf";
    // Put displacement under native geometry rather than using a converted scene.
    const auto document = VmfDocument::parse(
      source + "hidden { entity { id 14 solid { id 15 side { id 16 dispinfo { power 3 normals { row0 \"0 0 1\" } } } } } }");
    const auto original = document.serialize();
    for (const auto target :
         std::array{Target::Generic, Target::ClassicCss, Target::Strata})
    {
      const auto assessment = assessSourceCapabilities(document, target);
      CHECK(assessment.mayCompile());
      for (const auto feature : std::array{
             Feature::Overlay,
             Feature::Instance,
             Feature::PropScaling,
             Feature::Displacement,
             Feature::UnknownData})
      {
        CHECK(hasFinding(assessment, feature, Severity::Warning));
      }
    }
    CHECK(document.serialize() == original);
  }

  SECTION("feature findings identify source token and nearest native object")
  {
    for (const auto* newline : {"\n", "\r\n", "\r"})
    {
      CAPTURE(newline);
      const auto source = std::string{"versioninfo { formatversion 100 }"} + newline
                          + "world { id 1 }" + newline + "entity" + newline + "{"
                          + newline + "  id \"0007\"" + newline
                          + "  classname info_overlay" + newline + "}" + newline;
      const auto document = VmfDocument::parse(source);
      const auto assessment = assessSourceCapabilities(document, Target::ClassicCss);
      const auto it = std::find_if(
        assessment.diagnostics.begin(),
        assessment.diagnostics.end(),
        [](const auto& diagnostic) { return diagnostic.feature == Feature::Overlay; });
      REQUIRE(it != assessment.diagnostics.end());
      REQUIRE(it->entry);
      REQUIRE(it->objectEntry);
      REQUIRE(it->nativeId);
      CHECK(*it->nativeId == "0007");
      CHECK(document.tokenContents(document.entries()[*it->entry].key) == "classname");
      CHECK(document.tokenContents(document.entries()[*it->objectEntry].key) == "entity");
      CHECK(it->offset == source.find("classname"));
      CHECK(it->line == 6);
      CHECK(it->column == 3);
      CHECK(document.serialize() == source);
    }
  }

  SECTION("unknown versions, bounds, geometry extensions and metadata are disclosed")
  {
    const auto document = VmfDocument::parse(R"vmf(
versioninfo { formatversion 999 editorversion 20 future_version 25 }
world { id 1 world_mins "-100000 -100000 -100000"
  solid { id 2 extension_property x side { id 3 branch_uv "raw" } }
  editor { color "1 2 3" }
}
cordons { cordon { box { mins "-1 -1 -1" maxs "1 1 1" } } }
future_feature { repeated 1 repeated 2 nested { bytes "untouched" } }
)vmf");
    const auto assessment = assessSourceCapabilities(document, Target::ClassicCss);
    CHECK(assessment.mayCompile());
    CHECK(hasFinding(assessment, Feature::Version, Severity::Warning));
    CHECK(hasFinding(assessment, Feature::UnknownData, Severity::Warning));
    CHECK(hasFinding(assessment, Feature::Bounds, Severity::Warning));
    CHECK(hasFinding(assessment, Feature::EditorMetadata, Severity::Information));
    CHECK(
      std::count_if(
        assessment.diagnostics.begin(),
        assessment.diagnostics.end(),
        [](const auto& diagnostic) { return diagnostic.feature == Feature::UnknownData; })
      == 4);
    CHECK(hasFinding(
      assessSourceCapabilities(VmfDocument::parse("world {}"), Target::Strata),
      Feature::Version,
      Severity::Warning));
  }

  SECTION("bounded assessment fails closed, never silently certifies truncated findings")
  {
    const auto document = VmfDocument::parse(plain);
    auto options = SourceCapabilityOptions{};
    options.maxEntries = 1;
    auto assessment = assessSourceCapabilities(document, Target::ClassicCss, options);
    CHECK_FALSE(assessment.complete);
    CHECK_FALSE(assessment.mayCompile());
    CHECK(hasFinding(assessment, Feature::AssessmentLimit, Severity::Error));
    options = SourceCapabilityOptions{};
    options.maxSourceBytes = 1;
    assessment = assessSourceCapabilities(document, Target::ClassicCss, options);
    CHECK_FALSE(assessment.complete);
    CHECK_FALSE(assessment.mayCompile());
    options = SourceCapabilityOptions{};
    options.maxDiagnostics = 2;
    assessment = assessSourceCapabilities(document, Target::Generic, options);
    CHECK_FALSE(assessment.complete);
    CHECK_FALSE(assessment.mayCompile());
    CHECK(assessment.diagnostics.size() == 2);
    CHECK(hasFinding(assessment, Feature::AssessmentLimit, Severity::Error));
    options.maxDiagnostics = 0;
    assessment = assessSourceCapabilities(document, Target::ClassicCss, options);
    CHECK_FALSE(assessment.complete);
    CHECK_FALSE(assessment.mayCompile());
    CHECK(assessment.diagnostics.empty());
  }
}
} // namespace tb::mdl
