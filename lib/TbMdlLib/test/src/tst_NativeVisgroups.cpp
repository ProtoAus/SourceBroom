/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/NativeVisgroups.h"
#include "mdl/SourceEditorMetadata.h"

#include <algorithm>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::mdl
{
namespace
{
const auto source = std::string{
  "// synthetic loaded visgroups\r\n"
  "visgroups {\r\n"
  "visgroup { \"name\" \"parent\" \"visgroupid\" \"007\"\r\n"
  "visgroup { \"name\" \"child\" \"visgroupid\" \"8\" } }\r\n"
  "visgroup { \"name\" \"sibling\" \"visgroupid\" \"9\" } }\r\n"
  "world { \"id\" \"1\" \"classname\" \"worldspawn\"\r\n"
  "group { \"id\" \"2\" }\r\n"
  "group { \"id\" \"3\" editor { \"groupid\" \"2\" } }\r\n"
  "solid { \"id\" \"4\" editor { \"visgroupid\" \"007\" \"visgroupid\" \"8\" \"color\" "
  "\"1 2 3\" } } }\r\n"
  "entity { \"id\" \"5\" \"classname\" \"func_detail\"\r\n"
  "solid { \"id\" \"6\" } solid { \"id\" \"7\" } editor { \"visgroupid\" \"8\" } }\r\n"
  "entity { \"id\" \"8\" \"classname\" \"info_target\" editor { \"visgroupid\" \"9\" "
  "\"visgroupshown\" \"0\" \"visgroupautoshown\" \"0\" } }\r\n"
  "hidden { entity { \"id\" \"9\" \"classname\" \"info_target\" editor { \"visgroupid\" "
  "\"9\" } } }\r\n"
  "entity { \"id\" \"10\" \"classname\" \"info_target\" connections { \"OnX\" \"a\" "
  "\"OnX\" \"b\" } }\r\n"
  "opaque { \"unrelated\" \"001.50e0\" }\r\n"};
NativeVisgroupContext admitted(const NativeVisgroupSnapshot& snapshot)
{
  auto c = NativeVisgroupContext{};
  for (const auto& m : snapshot.members)
  {
    c.members.push_back({m.id, {}, m.kind, true, true, true, {}});
  }
  // Physical ownership, independently supplied by the scene in later integration.
  c.members[1].owner = c.members[0].id;
  c.members[2].owner = c.members[1].id;
  c.members[4].owner = c.members[3].id;
  c.members[5].owner = c.members[3].id;
  return c;
}
std::string serialized(const VmfDocument& d, const NativeVisgroups& model)
{
  auto values = std::vector<VmfValueEdit>{};
  auto structure = std::vector<VmfStructuralEdit>{};
  auto roots = std::string{};
  const auto session = model.beginEmission(d);
  model.emitTreeEdits(d, values, structure, roots);
  for (const auto& member : model.snapshot().members)
  {
    auto fields = std::string{};
    model.emitMemberEdits(session, member.id, values, structure, fields);
    if (!fields.empty())
    {
      structure.push_back(
        {VmfStructuralEdit::Type::Append,
         member.editor.value_or(*member.source),
         member.editor ? fields : "editor {\n" + fields + "}\n"});
    }
  }
  if (!roots.empty())
  {
    structure.push_back(
      {VmfStructuralEdit::Type::AppendRoot, VmfStructuralEdit::Root, roots});
  }
  return d.serialize(values, structure);
}
void apply(
  NativeVisgroups& model,
  const NativeVisgroupRequest& request,
  const NativeVisgroupContext& context)
{
  auto change = model.prepare(request, context);
  REQUIRE(change);
  model.apply(*change, NativeVisgroupDirection::Forward, context);
}
} // namespace

TEST_CASE("NativeVisgroups loaded state and acted manual membership emission")
{
  const auto d = VmfDocument::parse(source);
  auto model = NativeVisgroups{d};
  const auto c = admitted(model.snapshot());
  const auto initial = model.snapshot(c);
  REQUIRE(initial.records.size() == 3);
  REQUIRE(initial.members.size() == 9);
  CHECK(initial.records[0].nativeId == "7");
  CHECK(initial.members[2].memberships == std::vector<std::string>{"7", "8"});
  CHECK(initial.members[7].wrapperHidden);
  CHECK_FALSE(initial.members[8].manualShown.has_value());
  CHECK(serialized(d, model) == source);
  CHECK_FALSE(model.prepare(NativeVisgroupRequest{initial}, c));
  auto invalid = NativeVisgroupRequest{initial};
  invalid.records[1].parent = invalid.records[1].key;
  CHECK_THROWS_AS(model.prepare(invalid, c), std::invalid_argument);
  CHECK(serialized(d, model) == source);

  auto request = NativeVisgroupRequest{initial};
  request.memberships.push_back({{initial.members[8].id}, initial.records[1].id, true});
  request.manual = NativeVisgroupManualAction{{initial.records[0].id}, false};
  auto change = model.prepare(request, c);
  REQUIRE(change);
  CHECK(serialized(d, model) == source);
  model.apply(*change, NativeVisgroupDirection::Forward, c);
  const auto state = model.snapshot(c);
  CHECK(state.members[8].memberships == std::vector<std::string>{"8"});
  CHECK(state.members[8].manualShown == false);
  CHECK(state.members[3].manualShown == false);
  CHECK(state.members[4].manualShown == false);
  CHECK(state.members[5].manualShown == false);
  CHECK_FALSE(state.members[0].manualShown.has_value());
  const auto output = serialized(d, model);
  CHECK(output != source);
  const auto parsed = SourceEditorMetadata::read(VmfDocument::parse(output));
  REQUIRE(parsed.diagnostics.empty());
  CHECK(output.find("\"visgroupid\" \"007\" \"visgroupid\" \"8\"") != std::string::npos);
  CHECK(output.find("\"visgroupshown\" \"0\"") != std::string::npos);
  CHECK(output.find("\"unrelated\" \"001.50e0\"") != std::string::npos);
  CHECK(serialized(d, model) == output);
  model.apply(*change, NativeVisgroupDirection::Backward, c);
  CHECK(serialized(d, model) == source);
  model.apply(*change, NativeVisgroupDirection::Forward, c);
  CHECK(serialized(d, model) == output);
}

TEST_CASE("NativeVisgroups explicit RGB preserves lexical no-op and both tree emitters")
{
  const auto bytes = std::string{
    "// synthetic RGB; original numeric text is authoritative\r\n"
    "visgroups { visgroup { name Parent visgroupid 007 color \"001 002 003\"\r\n"
    "visgroup { name Child visgroupid 8 color \"004 005 006\" } }\r\n"
    "visgroup { name Sibling visgroupid 9 } }\r\n"
    "world { id 1 classname worldspawn }\r\n"
    "entity { id 0002 classname info_target editor { color \"17 18 19\" } }\r\n"
    "opaque { token \"001.250\" token \"-002\" }\r\n"};
  const auto d = VmfDocument::parse(bytes);
  auto model = NativeVisgroups{d};
  const auto context = NativeVisgroupContext{};
  const auto initial = model.snapshot(context);
  REQUIRE(initial.records.size() == 3);
  CHECK(initial.records[0].color == NativeVisgroupColor{1, 2, 3});
  CHECK(initial.records[1].color == NativeVisgroupColor{4, 5, 6});
  CHECK_FALSE(initial.records[2].color);

  // Acted ordinary-authoring control before every scalar arm.
  auto control = NativeVisgroupRequest{initial};
  control.records[2].name = "Control acted";
  auto controlled = model.prepare(control, context);
  REQUIRE(controlled);
  model.apply(*controlled, NativeVisgroupDirection::Forward, context);
  CHECK(serialized(d, model).find("Control acted") != std::string::npos);
  model.apply(*controlled, NativeVisgroupDirection::Backward, context);
  CHECK(serialized(d, model) == bytes);
  auto request = NativeVisgroupRequest{model.snapshot(context)};

  SECTION("same parsed value is a true lexical no-op")
  {
    request.color = NativeVisgroupColorAction{initial.records[0].id, {1, 2, 3}};
    const auto revision = model.snapshot().revision;
    CHECK_FALSE(model.prepare(request, context));
    CHECK(model.snapshot().revision == revision);
    CHECK(serialized(d, model) == bytes);
  }
  SECTION("retained known field changes and replays")
  {
    const auto index = GENERATE(size_t{0}, size_t{1});
    request.color = NativeVisgroupColorAction{initial.records[index].id, {17, 0, 255}};
    auto change = model.prepare(request, context);
    REQUIRE(change);
    model.apply(*change, NativeVisgroupDirection::Forward, context);
    CHECK(model.snapshot().records[index].color == NativeVisgroupColor{17, 0, 255});
    const auto saved = serialized(d, model);
    CHECK(saved != bytes);
    const auto reopened = NativeVisgroups{VmfDocument::parse(saved)};
    CHECK(reopened.snapshot().records[index].color == NativeVisgroupColor{17, 0, 255});
    CHECK(saved.find("visgroupid 007") != std::string::npos);
    CHECK(saved.find("color \"17 18 19\"") != std::string::npos);
    CHECK(saved.find("token \"001.250\" token \"-002\"") != std::string::npos);
    CHECK(serialized(d, model) == saved);
    model.apply(*change, NativeVisgroupDirection::Backward, context);
    CHECK(serialized(d, model) == bytes);
    model.apply(*change, NativeVisgroupDirection::Forward, context);
    CHECK(serialized(d, model) == saved);
  }
  SECTION("missing field requires an explicit write")
  {
    request.color = NativeVisgroupColorAction{initial.records[2].id, {255, 128, 0}};
    auto change = model.prepare(request, context);
    REQUIRE(change);
    model.apply(*change, NativeVisgroupDirection::Forward, context);
    const auto saved = serialized(d, model);
    CHECK(saved != bytes);
    CHECK(
      NativeVisgroups{VmfDocument::parse(saved)}.snapshot().records[2].color
      == NativeVisgroupColor{255, 128, 0});
    model.apply(*change, NativeVisgroupDirection::Backward, context);
    CHECK(serialized(d, model) == bytes);
  }
  SECTION("request-local new record may receive explicit RGB")
  {
    const auto draft = NativeVisgroupDraftKey{1};
    request.records.push_back({draft, initial.records[0].id, "Draft RGB"});
    request.color = NativeVisgroupColorAction{draft, {0, 255, 127}};
    auto change = model.prepare(request, context);
    REQUIRE(change);
    model.apply(*change, NativeVisgroupDirection::Forward, context);
    const auto saved = serialized(d, model);
    const auto reopened = NativeVisgroups{VmfDocument::parse(saved)}.snapshot();
    const auto found =
      std::ranges::find(reopened.records, "Draft RGB", &NativeVisgroupRecord::name);
    REQUIRE(found != reopened.records.end());
    CHECK(found->color == NativeVisgroupColor{0, 255, 127});
    model.apply(*change, NativeVisgroupDirection::Backward, context);
    CHECK(serialized(d, model) == bytes);
  }
  SECTION("moved source subtree patches its own or retained child RGB locally")
  {
    const auto index = GENERATE(size_t{0}, size_t{1});
    auto parent = request.records[0];
    parent.parent = initial.records[2].id;
    request.records.erase(request.records.begin());
    request.records.push_back(std::move(parent));
    request.color = NativeVisgroupColorAction{initial.records[index].id, {20, 30, 40}};
    auto change = model.prepare(request, context);
    REQUIRE(change);
    model.apply(*change, NativeVisgroupDirection::Forward, context);
    const auto saved = serialized(d, model);
    const auto reopened = NativeVisgroups{VmfDocument::parse(saved)}.snapshot();
    REQUIRE(reopened.records.size() == 3);
    CHECK(reopened.records[0].name == "Sibling");
    CHECK(reopened.records[1].name == "Parent");
    CHECK(reopened.records[index + 1].color == NativeVisgroupColor{20, 30, 40});
    CHECK(serialized(d, model) == saved);
    model.apply(*change, NativeVisgroupDirection::Backward, context);
    CHECK(serialized(d, model) == bytes);
  }
}

TEST_CASE("NativeVisgroups unfamiliar RGB is color-only protection")
{
  const auto field = GENERATE(
    "color \"1.0 2 3\"",
    "color \"-1 2 3\"",
    "color \"256 2 3\"",
    "color \"1 2\"",
    "color \"1 2 3 4\"",
    "color \"1 2 x\"",
    "color \"1 2 3\" color \"4 5 6\"");
  const auto bytes =
    "visgroups { visgroup { name Bad visgroupid 7 " + std::string{field}
    + " } visgroup { name Good visgroupid 8 } }\nworld { id 1 classname worldspawn }\n";
  const auto d = VmfDocument::parse(bytes);
  auto model = NativeVisgroups{d};
  const auto context = NativeVisgroupContext{};
  const auto initial = model.snapshot(context);
  REQUIRE(initial.records.size() == 2);
  CHECK(initial.records[0].refusalReason.empty());
  CHECK_FALSE(initial.records[0].colorRefusalReason.empty());
  auto rename = NativeVisgroupRequest{initial};
  rename.records[0].name = "Control acted";
  auto control = model.prepare(rename, context);
  REQUIRE(control);
  model.apply(*control, NativeVisgroupDirection::Forward, context);
  CHECK(serialized(d, model).find(field) != std::string::npos);
  CHECK(serialized(d, model).find("Control acted") != std::string::npos);
  model.apply(*control, NativeVisgroupDirection::Backward, context);
  CHECK(serialized(d, model) == bytes);
  const auto state = model.snapshot(context);
  auto request = NativeVisgroupRequest{state};
  request.color = NativeVisgroupColorAction{state.records[0].id, {1, 2, 3}};
  CHECK_THROWS_AS(model.prepare(request, context), std::invalid_argument);
  CHECK(model.snapshot().revision == state.revision);
  CHECK(serialized(d, model) == bytes);
  request.color = NativeVisgroupColorAction{state.records[1].id, {17, 18, 19}};
  auto independent = model.prepare(request, context);
  REQUIRE(independent);
  model.apply(*independent, NativeVisgroupDirection::Forward, context);
  CHECK(model.snapshot().records[1].color == NativeVisgroupColor{17, 18, 19});
  CHECK(serialized(d, model).find(field) != std::string::npos);
}

TEST_CASE("NativeVisgroups RGB rejects invalid scope and channel values before publish")
{
  const auto d = VmfDocument::parse("visgroups { visgroup { name RGB visgroupid 7 } }");
  auto model = NativeVisgroups{d};
  const auto context = NativeVisgroupContext{};
  const auto initial = model.snapshot(context);
  const auto unchanged = serialized(d, model);
  SECTION("channel bounds")
  {
    const auto color = GENERATE(
      NativeVisgroupColor{-1, 2, 3},
      NativeVisgroupColor{256, 2, 3},
      NativeVisgroupColor{1, -1, 3},
      NativeVisgroupColor{1, 256, 3},
      NativeVisgroupColor{1, 2, -1},
      NativeVisgroupColor{1, 2, 256});
    auto request = NativeVisgroupRequest{initial};
    request.color = NativeVisgroupColorAction{initial.records[0].id, color};
    CHECK_THROWS_AS(model.prepare(request, context), std::invalid_argument);
  }
  SECTION("foreign target")
  {
    auto foreign = NativeVisgroups{d};
    auto request = NativeVisgroupRequest{initial};
    request.color =
      NativeVisgroupColorAction{foreign.snapshot().records[0].id, {1, 2, 3}};
    CHECK_THROWS_AS(model.prepare(request, context), std::invalid_argument);
  }
  SECTION("deleted target")
  {
    auto request = NativeVisgroupRequest{initial};
    request.records.clear();
    request.color = NativeVisgroupColorAction{initial.records[0].id, {1, 2, 3}};
    CHECK_THROWS_AS(model.prepare(request, context), std::invalid_argument);
  }
  SECTION("forged expected RGB is not authority")
  {
    auto request = NativeVisgroupRequest{initial};
    request.expected.records[0].color = NativeVisgroupColor{1, 2, 3};
    CHECK_THROWS_AS(model.prepare(request, context), std::invalid_argument);
  }
  CHECK(model.snapshot().revision == initial.revision);
  CHECK(serialized(d, model) == unchanged);
}

TEST_CASE("NativeVisgroups reintroduced direct references persist in live order")
{
  const auto d = VmfDocument::parse(source);
  auto model = NativeVisgroups{d};
  const auto context = admitted(model.snapshot());
  const auto initial = model.snapshot(context);
  const auto subject = initial.members[2].id;
  const auto group = initial.records[0].id;
  auto remove = NativeVisgroupRequest{initial};
  remove.memberships.push_back({{subject}, group, false});
  auto removed = model.prepare(remove, context);
  REQUIRE(removed);
  model.apply(*removed, NativeVisgroupDirection::Forward, context);
  CHECK(model.snapshot(context).members[2].memberships == std::vector<std::string>{"8"});
  const auto unassigned = serialized(d, model);
  auto append = NativeVisgroupRequest{model.snapshot(context)};
  append.memberships.push_back({{subject}, group, true});
  auto appended = model.prepare(append, context);
  REQUIRE(appended);
  model.apply(*appended, NativeVisgroupDirection::Forward, context);
  const auto ordered = std::vector<std::string>{"8", "7"};
  CHECK(model.snapshot(context).members[2].memberships == ordered);
  const auto saved = serialized(d, model);
  CHECK(saved != source);
  const auto reopenedDocument = VmfDocument::parse(saved);
  auto reopened = NativeVisgroups{reopenedDocument};
  CHECK(reopened.snapshot().members[2].memberships == ordered);
  CHECK(serialized(d, model) == saved);
  CHECK(saved.find("\"unrelated\" \"001.50e0\"") != std::string::npos);
  model.apply(*appended, NativeVisgroupDirection::Backward, context);
  CHECK(serialized(d, model) == unassigned);
  model.apply(*removed, NativeVisgroupDirection::Backward, context);
  CHECK(serialized(d, model) == source);
}

TEST_CASE(
  "NativeVisgroups overlap preserves independent causes and direct internal targeting")
{
  const auto d = VmfDocument::parse(source);
  auto model = NativeVisgroups{d};
  const auto c = admitted(model.snapshot());
  auto state = model.snapshot(c);
  auto hide = NativeVisgroupRequest{state};
  hide.manual = NativeVisgroupManualAction{{state.records[0].id}, false};
  apply(model, hide, c);
  state = model.snapshot(c);
  auto show = NativeVisgroupRequest{state};
  show.memberships.push_back({{state.members[4].id}, state.records[2].id, true});
  show.manual = NativeVisgroupManualAction{{state.records[2].id}, true};
  apply(model, show, c);
  state = model.snapshot(c);
  CHECK(state.members[4].manualShown == true);
  CHECK(state.members[5].manualShown == false);
  CHECK(state.members[3].manualShown == false);
  CHECK(state.members[6].manualShown == true);
  CHECK(state.members[6].autoShown == false);
  CHECK(state.members[7].manualShown == true);
  CHECK(state.members[7].wrapperHidden);
  CHECK(serialized(d, model).find("\"visgroupautoshown\" \"0\"") != std::string::npos);
}

TEST_CASE(
  "NativeVisgroups aggregate closure refuses protected required children and conflicting "
  "intents")
{
  const auto d = VmfDocument::parse(source);
  auto model = NativeVisgroups{d};
  auto c = admitted(model.snapshot());
  c.members[5].editable = false;
  auto state = model.snapshot(c);
  auto r = NativeVisgroupRequest{state};
  // Earlier valid leaf cannot publish when later whole entity requires protected child.
  r.memberships.push_back({{state.members[8].id}, state.records[1].id, true});
  r.memberships.push_back({{state.members[3].id}, state.records[1].id, true});
  CHECK_THROWS_AS(model.prepare(r, c), std::invalid_argument);
  CHECK(serialized(d, model) == source);
  r.memberships.pop_back();
  apply(model, r, c);
  CHECK(model.snapshot(c).members[8].memberships == std::vector<std::string>{"8"});
  c.members[5].editable = true;
  state = model.snapshot(c);
  r = NativeVisgroupRequest{state};
  r.memberships.push_back({{state.members[3].id}, state.records[0].id, true});
  r.memberships.push_back({{state.members[4].id}, state.records[0].id, false});
  CHECK_THROWS_AS(model.prepare(r, c), std::invalid_argument);
  r.memberships.pop_back();
  apply(model, r, c);
  state = model.snapshot(c);
  CHECK(state.members[3].memberships == std::vector<std::string>{"8", "7"});
  CHECK(state.members[4].memberships == std::vector<std::string>{"7"});
  CHECK(state.members[5].memberships == std::vector<std::string>{"7"});
  r = NativeVisgroupRequest{state};
  r.memberships.push_back({{state.members[0].id}, state.records[2].id, true});
  apply(model, r, c);
  CHECK(
    model.snapshot(c).members[2].memberships == std::vector<std::string>{"7", "8", "9"});
}

TEST_CASE("NativeVisgroups draft hierarchy move rename deletion and exact undo")
{
  const auto d = VmfDocument::parse(source);
  auto model = NativeVisgroups{d};
  const auto c = admitted(model.snapshot());
  const auto initial = model.snapshot(c);
  auto r = NativeVisgroupRequest{initial};
  r.records[1].name = "renamed-child";
  r.records[1].parent = initial.records[2].id;
  r.records.push_back({NativeVisgroupDraftKey{10}, {}, "new-parent"});
  r.records.push_back(
    {NativeVisgroupDraftKey{11}, NativeVisgroupDraftKey{10}, "new-child"});
  r.memberships.push_back({{initial.members[8].id}, NativeVisgroupDraftKey{11}, true});
  auto change = model.prepare(r, c);
  REQUIRE(change);
  model.apply(*change, NativeVisgroupDirection::Forward, c);
  const auto output = serialized(d, model);
  const auto metadata = SourceEditorMetadata::read(VmfDocument::parse(output));
  CHECK(metadata.diagnostics.empty());
  REQUIRE(metadata.visgroups.size() == 5);
  CHECK(output.find("renamed-child") != std::string::npos);
  CHECK(output.find("new-child") != std::string::npos);
  auto state = model.snapshot(c);
  CHECK(state.records[3].nativeId == "1");
  CHECK(state.records[4].nativeId == "2");
  auto remove = NativeVisgroupRequest{state};
  remove.records.erase(remove.records.begin() + 3, remove.records.end());
  auto deleted = model.prepare(remove, c);
  REQUIRE(deleted);
  model.apply(*deleted, NativeVisgroupDirection::Forward, c);
  CHECK(model.snapshot(c).members[8].memberships.empty());
  CHECK(serialized(d, model).find("new-parent") == std::string::npos);
  model.apply(*deleted, NativeVisgroupDirection::Backward, c);
  CHECK(serialized(d, model) == output);
  model.apply(*change, NativeVisgroupDirection::Backward, c);
  CHECK(serialized(d, model) == source);
  auto next = NativeVisgroupRequest{model.snapshot(c)};
  next.records.push_back({NativeVisgroupDraftKey{1}, {}, "not-recycled"});
  auto staged = model.prepare(next, c);
  REQUIRE(staged);
  CHECK(staged->after().records.back().nativeId == "3");
}

TEST_CASE("NativeVisgroups freshness semantic context replay and foreign handles")
{
  const auto d = VmfDocument::parse(source);
  auto model = NativeVisgroups{d};
  auto c = admitted(model.snapshot());
  c.generation = 1;
  const auto initial = model.snapshot(c);
  auto r = NativeVisgroupRequest{initial};
  r.records[0].name = "changed";
  auto staged = model.prepare(r, c);
  REQUIRE(staged);
  auto newer = c;
  newer.generation = 2;
  CHECK_THROWS_AS(
    model.apply(*staged, NativeVisgroupDirection::Forward, newer), std::invalid_argument);
  CHECK(serialized(d, model) == source);
  model.apply(*staged, NativeVisgroupDirection::Forward, c);
  newer.members[8].active = false;
  CHECK_THROWS_AS(
    model.apply(*staged, NativeVisgroupDirection::Backward, newer),
    std::invalid_argument);
  newer.members[8].active = true;
  newer.generation = 3;
  model.apply(*staged, NativeVisgroupDirection::Backward, newer);
  CHECK(serialized(d, model) == source);
  model.apply(*staged, NativeVisgroupDirection::Forward, newer);
  CHECK(model.snapshot(newer).records[0].name == "changed");
  CHECK_THROWS_AS(model.prepare(r, newer), std::invalid_argument);
  auto foreign = NativeVisgroups{d};
  auto f = NativeVisgroupRequest{model.snapshot(newer)};
  f.records[0].parent = foreign.snapshot().records[1].id;
  CHECK_THROWS_AS(model.prepare(f, newer), std::invalid_argument);
  f = NativeVisgroupRequest{model.snapshot(newer)};
  f.memberships.push_back({{foreign.snapshot().members[0].id}, f.records[0].key, true});
  CHECK_THROWS_AS(model.prepare(f, newer), std::invalid_argument);
}

TEST_CASE(
  "NativeVisgroups deletion active protected raw barriers and parked preservation")
{
  const auto extra = GENERATE(
    std::string{},
    std::string{"opaque { \"visgroupid\" \"0009\" }"},
    std::string{"world { \"id\" \"99\" editor { \"visgroupid\" \"9\" } }"});
  const auto d = VmfDocument::parse(source + extra);
  auto model = NativeVisgroups{d};
  auto c = admitted(model.snapshot());
  auto state = model.snapshot(c);
  auto r = NativeVisgroupRequest{state};
  r.records.pop_back();
  if (!extra.empty())
  {
    CHECK_THROWS_AS(model.prepare(r, c), std::invalid_argument);
    CHECK(serialized(d, model) == source + extra);
    r = NativeVisgroupRequest{state};
    r.records[0].name = "independent";
    apply(model, r, c);
    CHECK(model.snapshot(c).records[0].name == "independent");
    return;
  }
  c.members[6].editable = false;
  state = model.snapshot(c);
  r = NativeVisgroupRequest{state};
  r.records.pop_back();
  CHECK_THROWS_AS(model.prepare(r, c), std::invalid_argument);
  c.members[6].active = false;
  c.members[7].active = false;
  state = model.snapshot(c);
  r = NativeVisgroupRequest{state};
  r.records.pop_back();
  auto change = model.prepare(r, c);
  REQUIRE(change);
  model.apply(*change, NativeVisgroupDirection::Forward, c);
  CHECK(model.snapshot(c).members[6].memberships == std::vector<std::string>{"9"});
  CHECK_FALSE(model.snapshot(c).members[6].active);
  // Do not serialize inactive owners in Scene integration. Model retains exact
  // parked metadata; undo tree deletion precedes object activation (LIFO).
  model.apply(*change, NativeVisgroupDirection::Backward, c);
  CHECK(serialized(d, model) == source);
  c.members[6].active = true;
  c.members[6].editable = true;
  c.members[7].active = true;
  ++c.generation;
  auto active = NativeVisgroupRequest{model.snapshot(c)};
  active.manual = NativeVisgroupManualAction{{state.records[2].id}, true};
  apply(model, active, c);
  CHECK(model.snapshot(c).members[6].manualShown == true);
}

TEST_CASE(
  "NativeVisgroups unresolved malformed isolated records and namespace reservation")
{
  const auto bad = GENERATE(
    std::string{"entity { \"id\" \"20\" editor { \"visgroupid\" \"0001\" } }"},
    std::string{
      "entity { \"id\" \"20\" editor { \"visgroupid\" \"9\" \"visgroupid\" \"09\" } }"},
    std::string{"entity { \"id\" \"20\" editor { \"visgroupid\" \"9\" } editor { "
                "\"visgroupid\" \"9\" } }"},
    std::string{"entity { \"id\" \"20\" editor { visgroupid { \"x\" \"1\" } "
                "\"visgroupshown\" \"bad\" } }"});
  const auto d = VmfDocument::parse(source + bad);
  auto model = NativeVisgroups{d};
  auto c = admitted(model.snapshot());
  const auto state = model.snapshot(c);
  CHECK_FALSE(state.members.back().editable);
  auto r = NativeVisgroupRequest{state};
  r.records[0].name = "independent";
  r.records.push_back({NativeVisgroupDraftKey{1}, {}, "fresh"});
  auto change = model.prepare(r, c);
  REQUIRE(change);
  CHECK(
    change->after().records.back().nativeId
    == (bad.find("0001") != std::string::npos ? "2" : "1"));
  model.apply(*change, NativeVisgroupDirection::Forward, c);
  CHECK(serialized(d, model).find(bad) != std::string::npos);
  auto remove = NativeVisgroupRequest{model.snapshot(c)};
  remove.records.erase(remove.records.begin() + 2);
  if (bad.find("\"9\"") != std::string::npos)
  {
    CHECK_THROWS_AS(model.prepare(remove, c), std::invalid_argument);
  }
}

TEST_CASE("NativeVisgroups draft limits no-op unbound and emission document controls")
{
  const auto d = VmfDocument::parse(source);
  auto model = NativeVisgroups{d};
  const auto c = admitted(model.snapshot());
  const auto state = model.snapshot(c);
  auto r = NativeVisgroupRequest{state};
  r.records.push_back({NativeVisgroupDraftKey{1}, {}, "valid-first"});
  r.records.push_back(
    {NativeVisgroupDraftKey{2}, {}, std::string(NativeVisgroups::MaxNameBytes + 1, 'x')});
  CHECK_THROWS_AS(model.prepare(r, c), std::invalid_argument);
  r.records.pop_back();
  auto control = model.prepare(r, c);
  REQUIRE(control);
  CHECK(control->after().records.back().nativeId == "1");
  r.records.push_back({NativeVisgroupDraftKey{1}, {}, "duplicate-draft"});
  CHECK_THROWS_AS(model.prepare(r, c), std::invalid_argument);
  r.records.back().key = NativeVisgroupDraftKey{0};
  CHECK_THROWS_AS(model.prepare(r, c), std::invalid_argument);
  r.records.back().key = NativeVisgroupDraftKey{2};
  r.records.back().parent = NativeVisgroupDraftKey{999};
  CHECK_THROWS_AS(model.prepare(r, c), std::invalid_argument);
  auto unbound = NativeVisgroupRequest{model.snapshot()};
  unbound.memberships.push_back({{state.members[2].id}, state.records[0].id, false});
  CHECK_THROWS_AS(model.prepare(unbound, {}), std::invalid_argument);
  auto readonly = c;
  readonly.editable = false;
  CHECK_THROWS_AS(
    model.prepare(NativeVisgroupRequest{model.snapshot(readonly)}, readonly),
    std::invalid_argument);
  auto values = std::vector<VmfValueEdit>{};
  auto structure = std::vector<VmfStructuralEdit>{};
  auto roots = std::string{};
  const auto foreign = VmfDocument::parse(source + "// different");
  CHECK_THROWS_AS(
    model.emitTreeEdits(foreign, values, structure, roots), std::invalid_argument);
  CHECK_THROWS_AS(model.beginEmission(foreign), std::invalid_argument);
  const auto session = model.beginEmission(d);
  auto otherModel = NativeVisgroups{d};
  const auto foreignSession = otherModel.beginEmission(d);
  CHECK_THROWS_AS(
    model.emitMemberEdits(foreignSession, state.members[0].id, values, structure, roots),
    std::invalid_argument);
  CHECK_THROWS_AS(
    model.emitMemberEdits(
      session, otherModel.snapshot().members[0].id, values, structure, roots),
    std::invalid_argument);
  model.apply(*control, NativeVisgroupDirection::Forward, c);
  CHECK_THROWS_AS(
    model.emitMemberEdits(session, state.members[0].id, values, structure, roots),
    std::invalid_argument);
  model.apply(*control, NativeVisgroupDirection::Backward, c);
  CHECK_THROWS_AS(
    model.emitMemberEdits(session, state.members[0].id, values, structure, roots),
    std::invalid_argument);
  CHECK(serialized(d, model) == source);
  CHECK(values.empty());
  CHECK(structure.empty());
  CHECK(roots.empty());
}
TEST_CASE(
  "NativeVisgroups moved sibling returned to source parent keeps live append order")
{
  const auto bytes = std::string{
    "visgroups {\r\n"
    "// A trivia\r\nvisgroup { \"name\" \"A\" \"visgroupid\" \"007\" }\r\n"
    "// B trivia\r\nvisgroup { \"name\" \"B\" \"visgroupid\" \"8\" }\r\n"
    "// C trivia\r\nvisgroup { \"name\" \"C\" \"visgroupid\" \"9\" }\r\n}\r\n"};
  const auto document = VmfDocument::parse(bytes);
  auto model = NativeVisgroups{document};
  const auto initial = model.snapshot();
  auto away = NativeVisgroupRequest{initial};
  away.records[0].parent = initial.records[1].id;
  auto first = model.prepare(away, {});
  REQUIRE(first);
  model.apply(*first, NativeVisgroupDirection::Forward, {});
  auto back = NativeVisgroupRequest{model.snapshot()};
  back.records[0].parent.reset();
  std::rotate(back.records.begin(), back.records.begin() + 1, back.records.end());
  auto second = model.prepare(back, {});
  REQUIRE(second);
  model.apply(*second, NativeVisgroupDirection::Forward, {});
  const auto output = serialized(document, model);
  const auto metadata = SourceEditorMetadata::read(VmfDocument::parse(output));
  REQUIRE(metadata.visgroups.size() == 3);
  CHECK(metadata.visgroups[0].name == "B");
  CHECK(metadata.visgroups[1].name == "C");
  CHECK(metadata.visgroups[2].name == "A");
  CHECK(output.find("\"visgroupid\" \"007\"") != std::string::npos);
  CHECK(output.find("// A trivia\r\n") != std::string::npos);
  CHECK(serialized(document, model) == output);
  model.apply(*second, NativeVisgroupDirection::Backward, {});
  model.apply(*first, NativeVisgroupDirection::Backward, {});
  CHECK(serialized(document, model) == bytes);
}

} // namespace tb::mdl
