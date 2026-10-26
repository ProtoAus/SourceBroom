/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/SourceEditorMetadata.h"

#include <algorithm>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
bool hasIssue(const SourceEditorMetadata& metadata, const SourceMetadataIssue issue)
{
  return std::any_of(
    metadata.diagnostics.begin(),
    metadata.diagnostics.end(),
    [&](const auto& diagnostic) { return diagnostic.issue == issue; });
}

std::string fixture()
{
  return "// synthetic metadata; retain CRLF and numeric spelling\r\n"
         "world { \"id\" \"1\" \"classname\" \"worldspawn\" "
         "group { \"id\" \"002\" editor { \"color\" \"1 2 3\" } } "
         "group { \"id\" \"3\" editor { \"groupid\" \"02\" } } "
         "hidden { solid { \"id\" \"4\" editor { \"groupid\" \"3\" "
         "\"visgroupid\" \"010\" \"visgroupid\" \"11\" "
         "\"visgroupshown\" \"0\" \"visgroupautoshown\" \"1\" } } } }\r\n"
         "hidden { entity { \"id\" \"5\" \"classname\" \"info_target\" } }\r\n"
         "visgroups { visgroup { \"name\" \"Parent\" \"visgroupid\" \"10\" "
         "visgroup { \"name\" \"Child\" \"visgroupid\" \"11\" } } }\r\n"
         "cameras { \"activecamera\" \"0\" camera { "
         "\"position\" \"[1.000 2 3]\" \"look\" \"[4 5 6]\" } }\r\n"
         "cordons { \"active\" \"1\" cordon { \"name\" \"Bounds\" \"active\" \"0\" "
         "box { \"mins\" \"-16 -32 -64\" \"maxs\" \"16 32 64\" } } }\r\n"
         "unknown { camera { \"position\" \"opaque\" } \"x\" \"a\" \"x\" \"b\" }\r\n";
}
} // namespace

TEST_CASE("SourceEditorMetadata", "[vmf]")
{
  SECTION("read-only snapshot owns values and preserves every original byte")
  {
    const auto source = fixture();
    const auto document = VmfDocument::parse(source);
    const auto metadata = SourceEditorMetadata::read(document);
    CHECK(metadata.diagnostics.empty());
    REQUIRE(metadata.groups.size() == 2);
    CHECK(metadata.groups[0].nativeId == "2");
    CHECK(metadata.groups[1].parentId == "2");
    CHECK(metadata.groups[1].parent == metadata.groups[0].entry);
    REQUIRE(metadata.visgroups.size() == 2);
    CHECK(metadata.visgroups[1].parent == metadata.visgroups[0].entry);
    REQUIRE(metadata.memberships.size() == 5);
    const auto& solid = metadata.memberships[3];
    CHECK(solid.nativeId == "4");
    CHECK(solid.hiddenByContainer);
    CHECK(solid.group == metadata.groups[1].entry);
    CHECK(solid.visgroupIds == std::vector<std::string>{"10", "11"});
    CHECK(
      solid.visgroups
      == std::vector<VmfEntryId>{
        metadata.visgroups[0].entry, metadata.visgroups[1].entry});
    CHECK(solid.visgroupShown == false);
    CHECK(solid.visgroupAutoShown == true);
    CHECK(metadata.memberships[4].hiddenByContainer);
    CHECK_FALSE(metadata.memberships[0].hiddenByContainer);
    CHECK_FALSE(metadata.memberships[0].visgroupShown.has_value());
    REQUIRE(metadata.cameras.size() == 1);
    CHECK(metadata.cameras[0].navigable());
    CHECK(metadata.cameras[0].eye == vm::vec3d{1, 2, 3});
    CHECK(metadata.activeCamera == metadata.cameras[0].entry);
    REQUIRE(metadata.cordons.size() == 1);
    CHECK(metadata.cordonsActive == true);
    CHECK(metadata.cordons[0].active == false);
    CHECK(metadata.cordons[0].boundsValid);
    REQUIRE(metadata.cordons[0].boxes.size() == 1);
    CHECK(metadata.cordons[0].boxes[0] == vm::bbox3d{{-16, -32, -64}, {16, 32, 64}});
    CHECK(document.serialize() == source);
    // Unknown lookalikes are not interpreted; the snapshot outlives its document.
    const auto detached = SourceEditorMetadata::read(VmfDocument::parse(source));
    CHECK(detached.visgroups[1].name == "Child");
    CHECK(detached.cameras[0].look == vm::vec3d{4, 5, 6});
  }
  SECTION("acted token changes refresh navigation and membership without rewriting bytes")
  {
    const auto source = fixture();
    const auto document = VmfDocument::parse(source);
    const auto snapshot = SourceEditorMetadata::read(document);
    REQUIRE(snapshot.cameras.size() == 1);
    auto edits = std::vector<VmfValueEdit>{};
    for (VmfEntryId entry = 0; entry < document.entries().size(); ++entry)
    {
      const auto& item = document.entries()[entry];
      const auto key = document.tokenContents(item.key);
      const auto owner =
        item.parent ? document.entries()[*item.parent].parent : std::nullopt;
      const auto knownMembership =
        owner
        && std::any_of(
          snapshot.memberships.begin(),
          snapshot.memberships.end(),
          [&](const auto& member) { return member.entry == *owner; });
      if (
        item.value
        && ((key == "position" && item.parent == snapshot.cameras[0].entry) || (key == "groupid" && knownMembership)))
      {
        edits.push_back({entry, key == "position" ? "\"[8 16 24]\"" : "\"2\""});
      }
    }
    REQUIRE(edits.size() == 3);
    const auto saved = document.serialize(edits);
    auto expected = source;
    for (const auto& pair :
         {std::pair{std::string{"[1.000 2 3]"}, std::string{"[8 16 24]"}},
          std::pair{std::string{"\"groupid\" \"02\""}, std::string{"\"groupid\" \"2\""}},
          std::pair{std::string{"\"groupid\" \"3\""}, std::string{"\"groupid\" \"2\""}}})
    {
      expected.replace(expected.find(pair.first), pair.first.size(), pair.second);
    }
    CHECK(saved == expected);
    const auto acted = SourceEditorMetadata::read(VmfDocument::parse(saved));
    CHECK(acted.diagnostics.empty());
    CHECK(acted.cameras[0].eye == vm::vec3d{8, 16, 24});
    CHECK(acted.memberships[3].group == acted.groups[0].entry);
    CHECK(acted.memberships[3].hiddenByContainer);
    CHECK(document.serialize() == source);
    // Low-level document edits are test controls, not a metadata authoring API.
  }
  SECTION("duplicate canonical IDs and dangling memberships do not resolve")
  {
    const auto metadata = SourceEditorMetadata::read(
      VmfDocument::parse(
        "world { \"id\" \"1\" group { \"id\" \"2\" } group { \"id\" \"002\" } "
        "solid { \"id\" \"4\" editor { \"groupid\" \"2\" \"visgroupid\" \"9\" } } "
        "solid { \"id\" \"5\" editor { \"groupid\" \"6\" \"visgroupid\" \"8\" } } } "
        "visgroups { visgroup { \"visgroupid\" \"8\" } visgroup { \"visgroupid\" \"08\" "
        "} }"));
    CHECK(hasIssue(metadata, SourceMetadataIssue::DuplicateId));
    CHECK(hasIssue(metadata, SourceMetadataIssue::DanglingReference));
    CHECK(hasIssue(metadata, SourceMetadataIssue::AmbiguousReference));
    CHECK_FALSE(metadata.memberships[3].group);
    CHECK(metadata.memberships[3].visgroups.empty());
    CHECK_FALSE(metadata.memberships[4].group);
    CHECK(metadata.memberships[4].visgroups.empty());
    for (const auto& diagnostic : metadata.diagnostics)
    {
      CHECK_FALSE(diagnostic.message.empty());
    }
  }
  SECTION("group cycles and descendants never expose cyclic parent bindings")
  {
    const auto metadata = SourceEditorMetadata::read(
      VmfDocument::parse(
        "world { \"id\" \"1\" "
        "group { \"id\" \"2\" editor { \"groupid\" \"3\" } } "
        "group { \"id\" \"3\" editor { \"groupid\" \"2\" } } "
        "group { \"id\" \"4\" editor { \"groupid\" \"2\" } } "
        "group { \"id\" \"5\" editor { \"groupid\" \"5\" } } "
        "solid { \"id\" \"6\" editor { \"groupid\" \"4\" } } }"));
    CHECK(hasIssue(metadata, SourceMetadataIssue::Cycle));
    REQUIRE(metadata.groups.size() == 4);
    for (const auto& group : metadata.groups)
    {
      CHECK_FALSE(group.parent);
    }
    for (const auto& member : metadata.memberships)
    {
      CHECK_FALSE(member.group);
    }
  }
  SECTION("malformed editor metadata reports ambiguity instead of choosing a value")
  {
    const auto metadata = SourceEditorMetadata::read(
      VmfDocument::parse(
        "world { \"id\" \"1\" group { \"id\" \"bad\" } "
        "solid { \"id\" \"4\" editor { \"groupid\" \"2\" \"groupid\" \"3\" "
        "\"visgroupshown\" \"yes\" \"visgroupid\" \"9\" \"visgroupid\" \"09\" } } "
        "solid { \"id\" \"5\" editor {} editor {} } group {} }"));
    CHECK(hasIssue(metadata, SourceMetadataIssue::InvalidValue));
    CHECK(hasIssue(metadata, SourceMetadataIssue::DuplicateProperty));
    CHECK(hasIssue(metadata, SourceMetadataIssue::MissingProperty));
    CHECK_FALSE(metadata.memberships[2].groupId);
    CHECK_FALSE(metadata.memberships[2].visgroupShown);
  }
  SECTION(
    "camera navigation requires checked finite nondegenerate tuples and active index")
  {
    for (const auto& position :
         {"[nan 0 0]",
          "[1e999 0 0]",
          "[0 0 0] trailing",
          "0 0 0",
          "[0 0 0]",
          "[1e308 0 0]"})
    {
      CAPTURE(position);
      const auto source =
        std::string{"cameras { \"activecamera\" \"0\" camera { \"position\" \""}
        + position + "\" \"look\" \"[0 0 0]\" } }";
      const auto metadata = SourceEditorMetadata::read(VmfDocument::parse(source));
      REQUIRE(metadata.cameras.size() == 1);
      CHECK_FALSE(metadata.cameras[0].navigable());
      CHECK_FALSE(metadata.activeCamera);
      CHECK(hasIssue(metadata, SourceMetadataIssue::InvalidValue));
    }
    for (const auto& index : {"1", "999999999999999999999999999999999999999999", "-2"})
    {
      CAPTURE(index);
      const auto metadata = SourceEditorMetadata::read(
        VmfDocument::parse(
          std::string{"cameras { \"activecamera\" \""} + index
          + "\" camera { \"position\" \"[1 2 3]\" \"look\" \"[4 5 6]\" } }"));
      CHECK_FALSE(metadata.activeCamera);
      CHECK_FALSE(metadata.diagnostics.empty());
    }
    const auto none = SourceEditorMetadata::read(
      VmfDocument::parse("cameras { \"activecamera\" \"-1\" }"));
    CHECK(none.diagnostics.empty());
    CHECK_FALSE(none.activeCamera);
  }
  SECTION("native parenthesized cordon tuples remain exact and have strict delimiters")
  {
    for (const auto& source :
         {std::string{"cordon { active 1 mins \"(-16 -32 -64)\" maxs \"(16 32 64)\" }"},
          std::string{"cordons { active 1 cordon { active 1 box { mins \"(-16 -32 -64)\" "
                      "maxs \"(16 32 64)\" } } }"}})
    {
      const auto document = VmfDocument::parse(source);
      const auto metadata = SourceEditorMetadata::read(document);
      REQUIRE(metadata.cordons.size() == 1);
      CHECK(metadata.cordons[0].boundsValid);
      REQUIRE(metadata.cordons[0].boxes.size() == 1);
      CHECK(metadata.cordons[0].boxes[0] == vm::bbox3d{{-16, -32, -64}, {16, 32, 64}});
      CHECK(metadata.diagnostics.empty());
      CHECK(document.serialize() == source);
    }
    for (const auto* bad :
         {"(-1 -2 -3]",
          "[-1 -2 -3]",
          "(-1 -2 -3",
          "-1 -2 -3)",
          "(-1 -2 -3) trailing",
          "(-1 -2 -3 4)",
          "(nan -2 -3)"})
    {
      CAPTURE(bad);
      const auto source =
        std::string{"cordon { active 1 mins \""} + bad + "\" maxs \"(1 2 3)\" }";
      const auto document = VmfDocument::parse(source);
      const auto metadata = SourceEditorMetadata::read(document);
      REQUIRE(metadata.cordons.size() == 1);
      CHECK_FALSE(metadata.cordons[0].boundsValid);
      CHECK(metadata.cordons[0].boxes.empty());
      CHECK(hasIssue(metadata, SourceMetadataIssue::InvalidValue));
      CHECK(document.serialize() == source);
    }
  }
  SECTION("legacy and multiple modern cordon boxes are checked without repair")
  {
    const auto source = std::string{
      "cordon { \"active\" \"1\" \"mins\" \"-1 -2 -3\" \"maxs\" \"1 2 3\" } "
      "cordons { \"active\" \"bad\" cordon { \"active\" \"1\" "
      "box { \"mins\" \"0 0 0\" \"maxs\" \"1 2 3\" } "
      "box { \"mins\" \"-4 -5 -6\" \"maxs\" \"4 5 6\" } } "
      "cordon { box { \"mins\" \"1 0 0\" \"maxs\" \"0 0 1\" } } "
      "cordon { box { \"mins\" \"nan 0 0\" \"maxs\" \"1 1 1\" } } cordon {} }"};
    const auto document = VmfDocument::parse(source);
    const auto metadata = SourceEditorMetadata::read(document);
    REQUIRE(metadata.cordons.size() == 5);
    CHECK(metadata.cordons[0].active == true);
    CHECK(metadata.cordons[0].boxes.size() == 1);
    CHECK(metadata.cordons[1].boxes.size() == 2);
    CHECK(metadata.cordons[1].boundsValid);
    CHECK_FALSE(metadata.cordons[2].boundsValid);
    CHECK(metadata.cordons[2].boxes.empty());
    CHECK(metadata.cordons[3].boxes.empty());
    CHECK(metadata.cordons[4].boxes.empty());
    CHECK_FALSE(metadata.cordonsActive);
    CHECK(hasIssue(metadata, SourceMetadataIssue::InvalidValue));
    CHECK(hasIssue(metadata, SourceMetadataIssue::MissingProperty));
    CHECK(document.serialize() == source);
  }
  SECTION("partial or non-finite cordon extents cannot masquerade as complete bounds")
  {
    const auto metadata = SourceEditorMetadata::read(
      VmfDocument::parse(
        "cordons { cordon { box { \"mins\" \"0 0 0\" \"maxs\" \"1 1 1\" } "
        "box { \"mins\" \"0 0 0\" \"maxs\" \"0 1 1\" } } "
        "cordon { box { \"mins\" \"-1e308 0 0\" \"maxs\" \"1e308 1 1\" } } }"));
    REQUIRE(metadata.cordons.size() == 2);
    CHECK(metadata.cordons[0].boxes.size() == 1);
    CHECK_FALSE(metadata.cordons[0].boundsValid);
    CHECK(metadata.cordons[1].boxes.empty());
    CHECK_FALSE(metadata.cordons[1].boundsValid);
    CHECK(hasIssue(metadata, SourceMetadataIssue::InvalidValue));
  }
  SECTION("multiple root controllers cannot choose an active camera or cordon state")
  {
    const auto metadata = SourceEditorMetadata::read(
      VmfDocument::parse(
        "cameras { \"activecamera\" \"0\" camera { \"position\" \"[1 2 3]\" \"look\" "
        "\"[4 5 6]\" } } "
        "cameras { \"activecamera\" \"0\" camera { \"position\" \"[7 8 9]\" \"look\" "
        "\"[4 5 6]\" } } "
        "cordons { \"active\" \"1\" } cordons { \"active\" \"0\" }"));
    CHECK(metadata.cameras.size() == 2);
    CHECK_FALSE(metadata.activeCamera);
    CHECK_FALSE(metadata.cordonsActive);
    CHECK(hasIssue(metadata, SourceMetadataIssue::DuplicateProperty));
  }
}
TEST_CASE("SourceEditorMetadata native group admission candidates and property shapes", "[vmf]")
{
  for (const auto& [editor, candidates] :
       std::vector<std::pair<std::string, std::vector<std::string>>>{
         {"editor { groupid 10 groupid 10 }", {"10", "10"}},
         {"editor { groupid 00010 groupid 020 }", {"10", "20"}},
         {"editor { groupid 10 } editor { groupid 20 }", {"10", "20"}},
         {"editor {} editor { groupid 010 }", {"10"}},
         {"editor scalar editor { groupid 010 }", {"10"}},
         {"editor { groupid 010 groupid { raw 20 } }", {"10"}},
         {"editor { groupid { raw 10 } }", {}}})
  {
    CAPTURE(editor);
    const auto source =
      "world { id 1 group { id 10 } group { id 20 } } entity { id 2 classname "
      "info_target "
      + editor + " }";
    const auto document = VmfDocument::parse(source);
    const auto metadata = SourceEditorMetadata::read(document);
    REQUIRE(metadata.memberships.size() == 4);
    const auto& member = metadata.memberships.back();
    CHECK(member.candidateGroupIds == candidates);
    CHECK_FALSE(member.groupId);
    CHECK_FALSE(member.group);
    CHECK_FALSE(metadata.diagnostics.empty());
    CHECK(document.serialize() == source);
  }
  for (const auto* fields :
       {"visgroupid { raw 77 }",
        "visgroupid 77 visgroupid { raw 77 }",
        "visgroupshown { raw 1 }",
        "visgroupshown 1 visgroupshown { raw 1 }",
        "visgroupautoshown { raw 1 }",
        "visgroupautoshown 1 visgroupautoshown { raw 1 }",
        "visgroupshown invalid",
        "visgroupautoshown invalid"})
  {
    CAPTURE(fields);
    const auto source = std::string{"world { id 1 editor { "} + fields + " } }";
    const auto document = VmfDocument::parse(source);
    const auto metadata = SourceEditorMetadata::read(document);
    CHECK(hasIssue(metadata, SourceMetadataIssue::InvalidValue));
    CHECK(document.serialize() == source);
  }
}

} // namespace tb::mdl
