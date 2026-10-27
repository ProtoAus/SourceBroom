/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/EditorContext.h"
#include "mdl/EntityNode.h"
#include "mdl/GroupNode.h"
#include "mdl/LayerNode.h"
#include "mdl/NodeContents.h"
#include "mdl/PickResult.h"
#include "mdl/SourceEditorMetadata.h"
#include "mdl/VmfScene.h"

#include "vm/mat.h"
#include "vm/ray.h"

#include <algorithm>
#include <array>
#include <memory>

#include <catch2/catch_test_macros.hpp>

namespace tb::mdl
{
namespace
{
const auto bounds = vm::bbox3d{4096};
std::string cube(const int id, const std::string& editor = {})
{
  const auto planes = std::array<std::string, 6>{
    "(0 0 0) (0 64 0) (0 0 64)",
    "(64 0 0) (64 0 64) (64 64 0)",
    "(0 0 0) (0 0 64) (64 0 0)",
    "(0 64 0) (64 64 0) (0 64 64)",
    "(0 0 0) (64 0 0) (0 64 0)",
    "(0 0 64) (0 64 64) (64 0 64)"};
  auto s = "solid { id " + std::to_string(id) + "\n";
  for (size_t i = 0; i < planes.size(); ++i)
  {
    s += "side { id " + std::to_string(id + static_cast<int>(i) + 1)
      + " plane \"" + planes[i] + "\" material synthetic/grid"
      " uaxis \"[1 0 0 0] 0.25\" vaxis \"[0 -1 0 0] 0.25\""
      " rotation 0 lightmapscale 16 }\n";
  }
  return s + editor + "}\n";
}
std::string source(const std::string& editor = "editor { visgroupid 7 visgroupshown 0 }")
{
  return "// public synthetic native visgroup fixture\n"
    "visgroups { visgroup { name Manual visgroupid 7 color \"1 2 3\" } visgroup { name Other visgroupid 8 color \"2 3 4\" } visgroup { name Group visgroupid 9 color \"3 4 5\" } }\n"
    "world { id 1 classname worldspawn " + cube(2, editor) + "}\n"
    "entity { id 40 classname info_target origin \"128 32 32\" }\n";
}
NativeVisgroupMember member(const VmfScene& scene, const std::string& nativeId)
{
  const auto snapshot = scene.visgroups();
  const auto found =
    std::find_if(snapshot.members.begin(), snapshot.members.end(), [&](const auto& m) {
      return m.nativeId == nativeId;
    });
  REQUIRE(found != snapshot.members.end());
  return *found;
}
BrushNode& brush(VmfScene& scene)
{
  return *dynamic_cast<BrushNode*>(scene.world().defaultLayer()->children().front());
}
PreparedNativeVisgroupChange show(
  VmfScene& scene, const NativeVisgroupMemberId id, bool shown)
{
  const auto snapshot = scene.visgroups();
  const auto m =
    std::find_if(snapshot.members.begin(), snapshot.members.end(), [&](const auto& row) {
      return row.id == id;
    });
  REQUIRE(m != snapshot.members.end());
  REQUIRE_FALSE(m->memberships.empty());
  const auto group =
    std::find_if(snapshot.records.begin(), snapshot.records.end(), [&](const auto& row) {
      return row.nativeId == m->memberships.front();
    });
  REQUIRE(group != snapshot.records.end());
  auto request = NativeVisgroupRequest{snapshot};
  request.manual = NativeVisgroupManualAction{{group->id}, shown};
  auto prepared = scene.prepareVisgroupChange(request);
  REQUIRE(prepared);
  return std::move(*prepared);
}
} // namespace

TEST_CASE("VmfScene native visgroups projection and picking", "[vmf][visgroups]")
{
  auto adjacent = cube(100, "editor { visgroupshown 1 }");
  for (const auto& [from, to] :
       {std::pair{std::string{"(0 "}, std::string{"(128 "}},
        std::pair{std::string{"(64 "}, std::string{"(192 "}}})
  {
    size_t position = 0;
    while ((position = adjacent.find(from, position)) != std::string::npos)
    {
      adjacent.replace(position, from.size(), to);
      position += to.size();
    }
  }
  auto text = source();
  text.insert(text.find("}\nentity"), adjacent);
  auto scene = VmfScene::read(text, bounds);
  auto& hidden = brush(scene);
  auto& control = *scene.world().defaultLayer()->children().back();
  const auto context = EditorContext{};
  REQUIRE(control.visible());
  CHECK(context.visible(control));
  CHECK(hidden.visibilityState() == VisibilityState::Inherited);
  CHECK_FALSE(hidden.visible());
  hidden.select();
  CHECK_FALSE(context.visible(hidden));
  CHECK_FALSE(context.visible(static_cast<const Node&>(hidden)));
  auto hits = PickResult::byDistance();
  hidden.pick(context, vm::ray3d{{32, 32, 128}, {0, 0, -1}}, hits);
  CHECK(hits.empty());
  auto* adjacentControl =
    dynamic_cast<BrushNode*>(scene.world().defaultLayer()->children()[1]);
  REQUIRE(adjacentControl);
  REQUIRE(adjacentControl->visible());
  auto controlHits = PickResult::byDistance();
  adjacentControl->pick(context, vm::ray3d{{160, 32, 128}, {0, 0, -1}}, controlHits);
  CHECK_FALSE(controlHits.empty());
  CHECK(scene.serialize(scene.world()) == text);
  const auto id = member(scene, "2").id;
  auto change = show(scene, id, true);
  const auto closure =
    scene.applyVisgroupChange(change, NativeVisgroupDirection::Forward);
  CHECK(std::find(closure.begin(), closure.end(), &hidden) != closure.end());
  CHECK(closure.size() == 1);
  CHECK(hidden.visible());
  CHECK(control.visible());
  auto reopened = VmfScene::read(scene.serialize(scene.world()), bounds);
  CHECK(brush(reopened).visible());
  scene.applyVisgroupChange(change, NativeVisgroupDirection::Backward);
  CHECK_FALSE(hidden.visible());
  CHECK(scene.serialize(scene.world()) == text);
}

TEST_CASE(
  "VmfScene native visgroups loaded family and structural visibility", "[vmf][visgroups]")
{
  SECTION(
    "unprojected protected group keeps known source-ancestor native veto on flat member")
  {
    auto text = source("");
    text.insert(text.find("}\nentity"), "group { id 30 editor { visgroupshown 0 } }\n");
    text +=
      "entity { id 45 classname info_target origin \"0 0 0\" editor { groupid 30 "
      "visgroupshown 1 } future { x 1 } }";
    auto scene = VmfScene::read(text, bounds);
    auto* flat =
      dynamic_cast<EntityNode*>(scene.world().defaultLayer()->children().back());
    auto* control =
      dynamic_cast<EntityNode*>(scene.world().defaultLayer()->children()[1]);
    REQUIRE(flat);
    REQUIRE(control);
    CHECK(flat->parent() == scene.world().defaultLayer());
    CHECK_FALSE(member(scene, "30").projected);
    CHECK_FALSE(member(scene, "45").editable);
    CHECK(member(scene, "45").manualShown == true);
    CHECK_FALSE(flat->visible());
    flat->select();
    auto hiddenHits = PickResult::byDistance();
    flat->pick(EditorContext{}, vm::ray3d{{0, 0, 128}, {0, 0, -1}}, hiddenHits);
    CHECK(hiddenHits.empty());
    auto controlHits = PickResult::byDistance();
    control->pick(EditorContext{}, vm::ray3d{{128, 32, 128}, {0, 0, -1}}, controlHits);
    CHECK_FALSE(controlHits.empty());
    CHECK(scene.serialize(scene.world()) == text);
  }

  SECTION("automatic and wrapper causes cannot be shown by manual action")
  {
    auto text = source("editor { visgroupid 7 visgroupshown 0 visgroupautoshown 0 }");
    text +=
      "hidden { entity { id 41 classname info_target editor { visgroupid 7 visgroupshown "
      "0 } } }";
    auto scene = VmfScene::read(text, bounds);
    const auto autoId = member(scene, "2").id;
    auto action = show(scene, autoId, true);
    scene.applyVisgroupChange(action, NativeVisgroupDirection::Forward);
    CHECK_FALSE(brush(scene).visible());
    CHECK(brush(scene).nativeVisibility().automatic);
    CHECK_FALSE(scene.world().defaultLayer()->children().back()->visible());
    CHECK_THROWS(scene.validateDuplication({&brush(scene)}));
    CHECK_NOTHROW(scene.serialize(scene.world()));
    auto reopened = VmfScene::read(scene.serialize(scene.world()), bounds);
    CHECK(member(reopened, "2").autoShown == false);
    CHECK(member(reopened, "41").wrapperHidden);
  }
  SECTION("brush entity expands to internal solids and physical groups expand to members")
  {
    auto text = source("editor { visgroupid 7 visgroupshown 1 }");
    const auto close = text.find("}\nentity");
    REQUIRE(close != std::string::npos);
    text.insert(close, "group { id 30 editor { visgroupid 9 visgroupshown 0 } }\n");
    text += "entity { id 20 classname func_detail editor { visgroupid 8 visgroupshown 1 } "
      + cube(21, "editor { visgroupid 8 visgroupshown 1 }") + "}\n"
      "entity { id 45 classname info_target editor { groupid 30 visgroupshown 1 } }\n";
    auto scene = VmfScene::read(text, bounds);
    CHECK(member(scene, "21").kind == NativeVisgroupMemberKind::InternalSolid);
    CHECK(member(scene, "21").owner == member(scene, "20").id);
    CHECK(member(scene, "45").owner == member(scene, "30").id);
    auto action = show(scene, member(scene, "20").id, false);
    scene.applyVisgroupChange(action, NativeVisgroupDirection::Forward);
    CHECK(member(scene, "21").manualShown == false);
    CHECK(member(scene, "2").manualShown == true);
    auto group = show(scene, member(scene, "30").id, true);
    scene.applyVisgroupChange(group, NativeVisgroupDirection::Forward);
    CHECK(member(scene, "45").manualShown == true);
    CHECK_NOTHROW(scene.serialize(scene.world()));
  }
  SECTION("unsupported closure refuses without publishing valid sibling")
  {
    auto text = source();
    text +=
      "entity { id 42 classname func_instance editor { visgroupid 7 visgroupshown 0 } }";
    auto scene = VmfScene::read(text, bounds);
    const auto before = scene.visgroups();
    auto request = NativeVisgroupRequest{before};
    request.manual = NativeVisgroupManualAction{{before.records.front().id}, true};
    CHECK_THROWS(scene.prepareVisgroupChange(request));
    CHECK(scene.visgroups().members == before.members);
    CHECK_FALSE(brush(scene).visible());
    CHECK(scene.serialize(scene.world()) == text);
  }
}

TEST_CASE("VmfScene native visgroups lifecycle and removal witnesses", "[vmf][visgroups]")
{
  SECTION(
    "initially hidden shown removal needs live proof; disposal retains deletion evidence")
  {
    auto scene = VmfScene::read(source(), bounds);
    auto& node = brush(scene);
    auto* parent = node.parent();
    CHECK_THROWS(scene.prepareRemoval({&node}));
    parent->removeChild(&node);
    CHECK_THROWS(scene.serialize(scene.world()));
    parent->addChild(&node);
    auto action = show(scene, member(scene, "2").id, true);
    scene.applyVisgroupChange(action, NativeVisgroupDirection::Forward);
    auto proof = scene.prepareRemoval({&node});
    parent->removeChild(&node);
    scene.publishRemoval(proof, {&node});
    CHECK_FALSE(member(scene, "2").active);
    CHECK_NOTHROW(scene.serialize(scene.world()));
    CHECK_THROWS(scene.publishRemoval(proof, {&node}));
    parent->addChild(&node);
    CHECK_THROWS(scene.serialize(scene.world()));
    scene.activateRemoval(proof, {&node});
    CHECK(member(scene, "2").active);
    CHECK_THROWS(scene.activateRemoval(proof, {&node}));
    scene.applyVisgroupChange(action, NativeVisgroupDirection::Backward);
    parent->removeChild(&node);
    CHECK_THROWS(scene.serialize(scene.world()));
    parent->addChild(&node);
    scene.applyVisgroupChange(action, NativeVisgroupDirection::Forward);
    auto next = scene.prepareRemoval({&node});
    parent->removeChild(&node);
    scene.publishRemoval(next, {&node});
    scene.disposeRemoval(next);
    delete &node;
    CHECK_NOTHROW(scene.serialize(scene.world()));
    CHECK_THROWS(scene.activateRemoval(next, {}));
    auto reopened = VmfScene::read(scene.serialize(scene.world()), bounds);
    CHECK(reopened.world().defaultLayer()->children().size() == 1);
  }
  SECTION(
    "live unsaved metadata is copied with distinct generated handles and raw provenance")
  {
    auto scene = VmfScene::read(
      source("editor { visgroupid 007 visgroupshown 0 visgroupautoshown 1 }"), bounds);
    auto& original = brush(scene);
    auto action = show(scene, member(scene, "2").id, true);
    scene.applyVisgroupChange(action, NativeVisgroupDirection::Forward);
    auto clone =
      std::unique_ptr<BrushNode>{static_cast<BrushNode*>(original.clone(bounds))};
    auto* parent = original.parent();
    scene.registerAdditions({clone.get()}, true);
    auto* copy = clone.release();
    parent->addChild(copy);
    const auto snapshot = scene.visgroups();
    const auto generated = snapshot.members.back();
    CHECK(generated.id != member(scene, "2").id);
    CHECK(generated.manualShown == true);
    CHECK(generated.memberships == std::vector<std::string>{"7"});
    CHECK(generated.autoShown == true);
    auto reassign = NativeVisgroupRequest{scene.visgroups()};
    reassign.memberships.push_back(
      {{generated.id}, reassign.expected.records[0].id, false});
    reassign.memberships.push_back(
      {{generated.id}, reassign.expected.records[1].id, true});
    auto membership = scene.prepareVisgroupChange(reassign);
    REQUIRE(membership);
    scene.applyVisgroupChange(*membership, NativeVisgroupDirection::Forward);
    auto hide = show(scene, generated.id, false);
    scene.applyVisgroupChange(hide, NativeVisgroupDirection::Forward);
    CHECK_FALSE(copy->visible());
    CHECK(original.visible());
    CHECK_NOTHROW(scene.serialize(scene.world()));
    auto reopened = VmfScene::read(scene.serialize(scene.world()), bounds);
    CHECK(member(reopened, generated.nativeId).manualShown == false);
    CHECK(scene.serialize(scene.world()).find("007") != std::string::npos);
  }
}
TEST_CASE("VmfScene native visgroups fresh Both and parked replay", "[vmf][visgroups]")
{
  SECTION(
    "binding reuse and unregistered generated detach never certify inactive members")
  {
    auto scene =
      VmfScene::read(source("editor { visgroupid 7 visgroupshown 1 }"), bounds);
    auto& original = brush(scene);
    auto* parent = original.parent();
    auto impostor = std::unique_ptr<Node>{original.clone(bounds)};
    parent->removeChild(&original);
    parent->addChild(impostor.get());
    CHECK_THROWS(scene.visgroups());
    CHECK_THROWS(scene.serialize(scene.world()));
    parent->removeChild(impostor.get());
    parent->addChild(&original);
    auto clone = std::unique_ptr<Node>{original.clone(bounds)};
    scene.registerAdditions({clone.get()}, true, parent);
    CHECK_FALSE(scene.visgroups().members.back().active);
    auto* copy = clone.release();
    parent->addChild(copy);
    CHECK(scene.visgroups().members.back().active);
    parent->removeChild(copy);
    CHECK_THROWS(scene.visgroups());
    CHECK_THROWS(scene.serialize(scene.world()));
    parent->addChild(copy);
    CHECK_NOTHROW(scene.serialize(scene.world()));
  }

  SECTION("native group clone registers its entire live metadata ownership closure")
  {
    auto text = source("editor { visgroupid 7 visgroupshown 1 }");
    text.insert(
      text.find("}\nentity"),
      "group { id 30 editor { visgroupid 9 visgroupshown 0 } }\n");
    text +=
      "entity { id 45 classname info_target editor { groupid 30 visgroupshown 1 } }";
    auto scene = VmfScene::read(text, bounds);
    auto action = show(scene, member(scene, "30").id, true);
    scene.applyVisgroupChange(action, NativeVisgroupDirection::Forward);
    auto* group =
      dynamic_cast<GroupNode*>(scene.world().defaultLayer()->children().back());
    REQUIRE(group);
    auto clone = std::unique_ptr<Node>{group->cloneRecursively(bounds)};
    scene.registerAdditions({clone.get()}, true, group->parent());
    auto* copy = clone.release();
    group->parent()->addChild(copy);
    const auto groupId = scene.nativeId(*copy);
    const auto childId = scene.nativeId(*copy->children().front());
    CHECK(member(scene, groupId).id != member(scene, "30").id);
    CHECK(member(scene, childId).owner == member(scene, groupId).id);
    CHECK(member(scene, groupId).manualShown == true);
    auto hidden = show(scene, member(scene, groupId).id, false);
    const auto closure =
      scene.applyVisgroupChange(hidden, NativeVisgroupDirection::Forward);
    CHECK(std::find(closure.begin(), closure.end(), copy) != closure.end());
    CHECK_FALSE(copy->children().front()->visible());
    CHECK(brush(scene).visible());
    auto reopened = VmfScene::read(scene.serialize(scene.world()), bounds);
    CHECK(member(reopened, childId).manualShown == false);
    CHECK(member(reopened, childId).owner == member(reopened, groupId).id);
  }

  SECTION("wrap updates live owner kind without borrowing visibility authority")
  {
    auto scene = VmfScene::read(source(), bounds);
    auto& node = brush(scene);
    auto visible = show(scene, member(scene, "2").id, true);
    scene.applyVisgroupChange(visible, NativeVisgroupDirection::Forward);
    auto* layer = scene.world().defaultLayer();
    auto entity = std::make_unique<EntityNode>(Entity{{{"classname", "func_detail"}}});
    scene.registerAdditions({entity.get()}, false, layer);
    auto* owner = entity.release();
    layer->addChild(owner);
    const auto ownerId = scene.visgroups().members.back().id;
    REQUIRE_NOTHROW(scene.validateReparent(*owner, {&node}));
    layer->removeChild(&node);
    owner->addChild(&node);
    const auto internal = member(scene, "2");
    CHECK(internal.owner == ownerId);
    CHECK(internal.kind == NativeVisgroupMemberKind::InternalSolid);
    CHECK(scene.visgroups().members.back().kind == NativeVisgroupMemberKind::BrushEntity);
    auto hidden = show(scene, internal.id, false);
    scene.applyVisgroupChange(hidden, NativeVisgroupDirection::Forward);
    CHECK_FALSE(node.visible());
    CHECK_THROWS(scene.validateReparent(*owner, {&node}));
    CHECK_NOTHROW(scene.serialize(scene.world()));
    auto reopened = VmfScene::read(scene.serialize(scene.world()), bounds);
    CHECK(member(reopened, "2").kind == NativeVisgroupMemberKind::InternalSolid);
    CHECK(member(reopened, "2").manualShown == false);
  }
  SECTION("native viewport state cannot itself authorize hidden native geometry")
  {
    auto scene = VmfScene::read(source(), bounds);
    auto& node = brush(scene);
    node.setNativeVisibility({});
    CHECK(node.visible());
    CHECK_THROWS(scene.validateDuplication({&node}));
    CHECK_THROWS(scene.validateRemoval({&node}));
    CHECK_THROWS(scene.serialize(scene.world()));
    node.setNativeVisibility({false, true, false});
    CHECK(scene.serialize(scene.world()) == source());
  }

  SECTION(
    "fresh grouped hidden brush emits one composed editor and remains operationally "
    "closed")
  {
    auto scene = VmfScene::read(source(""), bounds);
    auto* layer = scene.world().defaultLayer();
    auto group = std::make_unique<GroupNode>(Group{"new"});
    scene.registerGroupChanges(group.get(), {{group.get(), layer}});
    auto* owner = group.release();
    layer->addChild(owner);
    auto fresh = std::make_unique<BrushNode>(brush(scene).brush());
    for (auto& face : fresh->brush().faces())
    {
      face.setFilePosition(0, 0);
    }
    scene.registerAdditions({fresh.get()}, false, owner);
    auto* node = fresh.release();
    owner->addChild(node);
    const auto id = scene.visgroups().members.back().id;
    const auto nativeId = scene.nativeId(*node);
    auto assign = NativeVisgroupRequest{scene.visgroups()};
    assign.memberships.push_back({{id}, assign.expected.records.front().id, true});
    auto membership = scene.prepareVisgroupChange(assign);
    REQUIRE(membership);
    scene.applyVisgroupChange(*membership, NativeVisgroupDirection::Forward);
    auto hide = show(scene, id, false);
    scene.applyVisgroupChange(hide, NativeVisgroupDirection::Forward);
    CHECK_FALSE(node->visible());
    CHECK_THROWS(scene.validateDuplication({node}));
    CHECK_THROWS(scene.validateRemoval({node}));
    CHECK_THROWS(scene.validateClipContents(*node, NodeContents{node->brush()}, bounds));
    const auto saved = scene.serialize(scene.world());
    const auto metadata = SourceEditorMetadata::read(VmfDocument::parse(saved));
    CHECK(metadata.diagnostics.empty());
    auto reopened = VmfScene::read(saved, bounds);
    const auto generated = member(reopened, nativeId);
    CHECK(generated.manualShown == false);
    CHECK(generated.owner == member(reopened, scene.nativeId(*owner)).id);
    CHECK(reopened.serialize(reopened.world()) == saved);
  }
  SECTION(
    "Both copies live unsaved metadata and hidden admitted halves save without new "
    "geometry authority")
  {
    auto scene = VmfScene::read(source(), bounds);
    auto& node = brush(scene);
    auto visible = show(scene, member(scene, "2").id, true);
    scene.applyVisgroupChange(visible, NativeVisgroupDirection::Forward);
    const auto half = [&](const bool back) {
      auto result = node.brush();
      auto cut = BrushFace::create(
        {32, 0, 0},
        back ? vm::vec3d{32, 64, 0} : vm::vec3d{32, 0, 64},
        back ? vm::vec3d{32, 0, 64} : vm::vec3d{32, 64, 0},
        "synthetic/cut",
        UvAttributes{},
        SurfaceAttributes{},
        MapFormat::SourceVMF);
      REQUIRE(cut.is_success());
      REQUIRE(result.clip(bounds, std::move(cut).value()).is_success());
      return result;
    };
    auto sibling =
      std::unique_ptr<BrushNode>{static_cast<BrushNode*>(node.clone(bounds))};
    sibling->setBrush(half(true));
    auto keepers =
      std::vector<std::pair<Node*, NodeContents>>{{&node, NodeContents{half(false)}}};
    scene.registerBothClips({&node}, keepers, {sibling.get()}, bounds);
    auto* copy = sibling.release();
    node.parent()->addChild(copy);
    node.setBrush(std::get<Brush>(keepers.front().second.get()));
    const auto generated = scene.visgroups().members.back();
    CHECK(generated.id != member(scene, "2").id);
    CHECK(generated.manualShown == true);
    auto hide = show(scene, generated.id, false);
    scene.applyVisgroupChange(hide, NativeVisgroupDirection::Forward);
    CHECK_FALSE(copy->visible());
    CHECK_FALSE(node.visible());
    CHECK_THROWS(scene.validateDuplication({copy}));
    CHECK_THROWS(scene.validateClipContents(*copy, NodeContents{copy->brush()}, bounds));
    const auto saved = scene.serialize(scene.world());
    auto reopened = VmfScene::read(saved, bounds);
    CHECK(member(reopened, generated.nativeId).manualShown == false);
    CHECK(member(reopened, "2").manualShown == false);
  }
  SECTION(
    "parked appended member permits semantic older replay but definitions must precede "
    "activation")
  {
    auto scene = VmfScene::read(source(), bounds);
    auto& node = brush(scene);
    auto visible = show(scene, member(scene, "2").id, true);
    scene.applyVisgroupChange(visible, NativeVisgroupDirection::Forward);
    auto clone = std::unique_ptr<BrushNode>{static_cast<BrushNode*>(node.clone(bounds))};
    scene.registerAdditions({clone.get()}, true, node.parent());
    auto* copy = clone.release();
    auto* parent = node.parent();
    parent->addChild(copy);
    const auto id = scene.visgroups().members.back().id;
    auto proof = scene.prepareRemoval({copy});
    parent->removeChild(copy);
    scene.publishRemoval(proof, {copy});
    scene.applyVisgroupChange(visible, NativeVisgroupDirection::Backward);
    CHECK_FALSE(node.visible());
    CHECK_FALSE(scene.visgroups().members.back().active);
    CHECK(scene.visgroups().members.back().id == id);
    scene.applyVisgroupChange(visible, NativeVisgroupDirection::Forward);
    auto removeDefinition = NativeVisgroupRequest{scene.visgroups()};
    removeDefinition.records.erase(removeDefinition.records.begin());
    auto deletion = scene.prepareVisgroupChange(removeDefinition);
    REQUIRE(deletion);
    scene.applyVisgroupChange(*deletion, NativeVisgroupDirection::Forward);
    parent->addChild(copy);
    CHECK_THROWS(scene.activateRemoval(proof, {copy}));
    parent->removeChild(copy);
    scene.applyVisgroupChange(*deletion, NativeVisgroupDirection::Backward);
    parent->addChild(copy);
    REQUIRE_NOTHROW(scene.activateRemoval(proof, {copy}));
    CHECK(scene.visgroups().members.back().active);
    CHECK_NOTHROW(scene.serialize(scene.world()));
  }
  SECTION("foreign stale and partial removal proofs publish nothing")
  {
    auto scene =
      VmfScene::read(source("editor { visgroupid 7 visgroupshown 1 }"), bounds);
    auto other =
      VmfScene::read(source("editor { visgroupid 7 visgroupshown 1 }"), bounds);
    auto& node = brush(scene);
    auto* parent = node.parent();
    auto proof = scene.prepareRemoval({&node});
    CHECK_THROWS(other.publishRemoval(proof, {}));
    parent->removeChild(&node);
    CHECK_THROWS(scene.publishRemoval(proof, {}));
    CHECK_THROWS(scene.visgroups());
    scene.publishRemoval(proof, {&node});
    CHECK_NOTHROW(scene.serialize(scene.world()));
    parent->addChild(&node);
    scene.activateRemoval(proof, {&node});
    auto stale = scene.prepareRemoval({&node});
    auto hidden = show(scene, member(scene, "2").id, false);
    scene.applyVisgroupChange(hidden, NativeVisgroupDirection::Forward);
    parent->removeChild(&node);
    CHECK_THROWS(scene.publishRemoval(stale, {&node}));
    CHECK_THROWS(scene.serialize(scene.world()));
    parent->addChild(&node);
  }
}

TEST_CASE(
  "VmfScene native visgroups authority and clone raw-reference barriers",
  "[vmf][visgroups]")
{
  SECTION(
    "raw definition restoration precedes parked activation but originally unresolved raw "
    "refs survive")
  {
    const auto unresolved = source(
      "editor { visgroupid 999 visgroupshown 1 future { visgroupid 7 visgroupid 999 } }");
    auto protectedDirect = VmfScene::read(unresolved, bounds);
    CHECK_FALSE(member(protectedDirect, "2").editable);
    CHECK_THROWS_AS(
      protectedDirect.prepareRemoval({&brush(protectedDirect)}), std::invalid_argument);
    CHECK(protectedDirect.serialize(protectedDirect.world()) == unresolved);
    auto scene = VmfScene::read(
      source("editor { visgroupshown 1 future { visgroupid 7 visgroupid 999 } }"),
      bounds);
    auto& node = brush(scene);
    auto* parent = node.parent();
    auto proof = scene.prepareRemoval({&node});
    auto movedProof = std::move(proof);
    CHECK_THROWS(scene.publishRemoval(proof, {&node}));
    CHECK_THROWS(scene.activateRemoval(proof, {&node}));
    CHECK_THROWS(scene.disposeRemoval(proof));
    proof = std::move(movedProof);
    parent->removeChild(&node);
    scene.publishRemoval(proof, {&node});
    auto remove = NativeVisgroupRequest{scene.visgroups()};
    remove.records.erase(remove.records.begin());
    auto deletion = scene.prepareVisgroupChange(remove);
    REQUIRE(deletion);
    scene.applyVisgroupChange(*deletion, NativeVisgroupDirection::Forward);
    parent->addChild(&node);
    CHECK_THROWS(scene.activateRemoval(proof, {&node}));
    parent->removeChild(&node);
    scene.applyVisgroupChange(*deletion, NativeVisgroupDirection::Backward);
    parent->addChild(&node);
    CHECK_NOTHROW(scene.activateRemoval(proof, {&node}));
    CHECK(scene.serialize(scene.world()).find("visgroupid 999") != std::string::npos);
  }
  SECTION(
    "wrapper-only legacy in-place exception does not include manual or automatic hiding")
  {
    for (const auto& editor :
         {std::string{},
          std::string{"editor { visgroupshown 0 }"},
          std::string{"editor { visgroupautoshown 0 }"}})
    {
      auto text = source(editor);
      const auto solid = cube(2, editor);
      text.replace(text.find(solid), solid.size(), "hidden { " + solid + " }");
      auto scene = VmfScene::read(text, bounds);
      auto& node = brush(scene);
      auto moved = node.brush();
      REQUIRE(moved.transform(bounds, vm::translation_matrix(vm::vec3d{16, 0, 0}), true)
                .is_success());
      if (editor.empty())
      {
        CHECK_NOTHROW(scene.validateContents(node, NodeContents{moved}, bounds));
      }
      else
      {
        CHECK_THROWS(scene.validateContents(node, NodeContents{moved}, bounds));
      }
      CHECK(scene.serialize(scene.world()) == text);
    }
  }
  SECTION(
    "safe entity with protected sibling does not globally lock independent direct solid")
  {
    auto text = source("") + "entity { id 20 classname func_detail " + cube(21)
                + cube(31, "future { x 1 }") + "}";
    auto scene = VmfScene::read(text, bounds);
    auto direct = NativeVisgroupRequest{scene.visgroups()};
    direct.memberships.push_back(
      {{member(scene, "21").id}, direct.expected.records[1].id, true});
    auto accepted = scene.prepareVisgroupChange(direct);
    REQUIRE(accepted);
    scene.applyVisgroupChange(*accepted, NativeVisgroupDirection::Forward);
    CHECK(member(scene, "21").memberships == std::vector<std::string>{"8"});
    CHECK(member(scene, "31").memberships.empty());
    auto whole = NativeVisgroupRequest{scene.visgroups()};
    whole.memberships.push_back(
      {{member(scene, "20").id}, whole.expected.records[1].id, true});
    CHECK_THROWS(scene.prepareVisgroupChange(whole));
  }

  SECTION(
    "generated-source reduplicate copies B unsaved metadata rather than prototype A")
  {
    auto scene =
      VmfScene::read(source("editor { visgroupid 7 future { visgroupid 9 } }"), bounds);
    auto& a = brush(scene);
    auto* parent = a.parent();
    auto clone = std::unique_ptr<Node>{a.clone(bounds)};
    scene.registerAdditions({clone.get()}, true, parent);
    auto* b = clone.release();
    parent->addChild(b);
    const auto bId = scene.visgroups().members.back().id;
    auto edit = NativeVisgroupRequest{scene.visgroups()};
    edit.memberships.push_back({{bId}, edit.expected.records[0].id, false});
    edit.memberships.push_back({{bId}, edit.expected.records[1].id, true});
    edit.manual = NativeVisgroupManualAction{{edit.expected.records[1].id}, true};
    auto change = scene.prepareVisgroupChange(edit);
    REQUIRE(change);
    scene.applyVisgroupChange(*change, NativeVisgroupDirection::Forward);
    CHECK(member(scene, "2").manualShown == std::nullopt);
    auto next = std::unique_ptr<Node>{b->clone(bounds)};
    scene.registerAdditions({next.get()}, true, parent);
    auto* c = next.release();
    parent->addChild(c);
    CHECK(scene.visgroups().members.back().memberships == std::vector<std::string>{"8"});
    CHECK(scene.visgroups().members.back().manualShown == true);
    auto* generated = dynamic_cast<BrushNode*>(c);
    REQUIRE(generated);
    const auto half = [&](const bool back) {
      auto result = generated->brush();
      auto cut = BrushFace::create(
        {32, 0, 0},
        back ? vm::vec3d{32, 64, 0} : vm::vec3d{32, 0, 64},
        back ? vm::vec3d{32, 0, 64} : vm::vec3d{32, 64, 0},
        "synthetic/cut",
        UvAttributes{},
        SurfaceAttributes{},
        MapFormat::SourceVMF);
      REQUIRE(cut.is_success());
      REQUIRE(result.clip(bounds, std::move(cut).value()).is_success());
      return result;
    };
    auto both =
      std::unique_ptr<BrushNode>{static_cast<BrushNode*>(generated->clone(bounds))};
    both->setBrush(half(true));
    auto keepers =
      std::vector<std::pair<Node*, NodeContents>>{{generated, NodeContents{half(false)}}};
    scene.registerBothClips({generated}, keepers, {both.get()}, bounds);
    parent->addChild(both.release());
    generated->setBrush(std::get<Brush>(keepers.front().second.get()));
    CHECK(scene.visgroups().members.back().memberships == std::vector<std::string>{"8"});
    CHECK(scene.visgroups().members.back().manualShown == true);
    auto proof = scene.prepareRemoval({&a, b});
    parent->removeChild(&a);
    parent->removeChild(b);
    scene.publishRemoval(proof, {&a, b});
    scene.disposeRemoval(proof);
    delete &a;
    delete b;
    auto deletion = NativeVisgroupRequest{scene.visgroups()};
    deletion.records.pop_back();
    CHECK_THROWS(scene.prepareVisgroupChange(deletion));
    CHECK_NOTHROW(scene.serialize(scene.world()));
  }

  SECTION(
    "original geometry and entity content checks cannot borrow spoofed derived "
    "visibility")
  {
    auto scene = VmfScene::read(source(), bounds);
    auto& node = brush(scene);
    auto moved = node.brush();
    REQUIRE(moved.transform(bounds, vm::translation_matrix(vm::vec3d{16, 0, 0}), true)
              .is_success());
    node.setNativeVisibility({});
    CHECK_THROWS(scene.validateContents(node, NodeContents{moved}, bounds));
    node.setNativeVisibility({false, true, false});
    CHECK_THROWS(scene.validateContents(node, NodeContents{moved}, bounds));
    CHECK_NOTHROW(scene.validateContents(node, NodeContents{node.brush()}, bounds));
    auto visible = show(scene, member(scene, "2").id, true);
    scene.applyVisgroupChange(visible, NativeVisgroupDirection::Forward);
    CHECK_NOTHROW(scene.validateContents(node, NodeContents{moved}, bounds));
    auto text = source("") + "entity { id 50 classname info_target origin \"0 0 0\" editor { visgroupid 7 visgroupshown 0 } }";
    auto entities = VmfScene::read(text, bounds);
    auto* point =
      dynamic_cast<EntityNode*>(entities.world().defaultLayer()->children().back());
    REQUIRE(point);
    auto properties = Entity{{{"classname", "info_target"}, {"origin", "16 0 0"}}};
    point->setNativeVisibility({});
    CHECK_THROWS(entities.validateContents(*point, NodeContents{properties}, bounds));
    point->setNativeVisibility({false, true, false});
    auto pointVisible = show(entities, member(entities, "50").id, true);
    entities.applyVisgroupChange(pointVisible, NativeVisgroupDirection::Forward);
    CHECK_NOTHROW(entities.validateContents(*point, NodeContents{properties}, bounds));
  }
  SECTION("supported hidden group closure cannot borrow spoofed derived visibility")
  {
    auto text = source("");
    text.insert(
      text.find("}\nentity"),
      "group { id 30 editor { visgroupid 9 visgroupshown 0 } }\n");
    text +=
      "entity { id 45 classname info_target editor { groupid 30 visgroupshown 1 } }";
    auto scene = VmfScene::read(text, bounds);
    auto* group =
      dynamic_cast<GroupNode*>(scene.world().defaultLayer()->children().back());
    REQUIRE(group);
    group->setNativeVisibility({});
    CHECK_THROWS(scene.validateGroupClosure({group}));
    group->setNativeVisibility({false, true, false});
    auto visible = show(scene, member(scene, "30").id, true);
    scene.applyVisgroupChange(visible, NativeVisgroupDirection::Forward);
    CHECK_NOTHROW(scene.validateGroupClosure({group}));
  }
  SECTION("opaque or unsafe entity ownership protects direct internal-solid metadata")
  {
    for (const auto& owner :
         {std::string{"classname func_instance"},
          std::string{"classname func_detail future { value 1 }"},
          std::string{"classname func_detail"}})
    {
      auto scene = VmfScene::read(
        source("") + "entity { id 20 " + owner + " " + cube(21) + "}", bounds);
      const auto snapshot = scene.visgroups();
      auto request = NativeVisgroupRequest{snapshot};
      request.memberships.push_back(
        {{member(scene, "21").id}, snapshot.records[1].id, true});
      if (owner == "classname func_detail")
      {
        CHECK_NOTHROW(scene.prepareVisgroupChange(request));
      }
      else
      {
        CHECK_THROWS(scene.prepareVisgroupChange(request));
      }
    }
  }
  SECTION("live clone retains raw reference barrier after original terminal deletion")
  {
    auto scene = VmfScene::read(
      source("editor { visgroupshown 1 future { visgroupid 7 } }"), bounds);
    auto& node = brush(scene);
    auto* parent = node.parent();
    auto clone = std::unique_ptr<Node>{node.clone(bounds)};
    scene.registerAdditions({clone.get()}, true, parent);
    parent->addChild(clone.release());
    auto proof = scene.prepareRemoval({&node});
    parent->removeChild(&node);
    scene.publishRemoval(proof, {&node});
    scene.disposeRemoval(proof);
    delete &node;
    auto request = NativeVisgroupRequest{scene.visgroups()};
    request.records.erase(request.records.begin());
    CHECK_THROWS(scene.prepareVisgroupChange(request));
    auto rename = NativeVisgroupRequest{scene.visgroups()};
    rename.records[1].name = "Independent";
    auto changed = scene.prepareVisgroupChange(rename);
    REQUIRE(changed);
    scene.applyVisgroupChange(*changed, NativeVisgroupDirection::Forward);
    const auto saved = scene.serialize(scene.world());
    CHECK(saved.find("future { visgroupid 7 }") != std::string::npos);
    CHECK_NOTHROW(VmfScene::read(saved, bounds));
  }
}

} // namespace tb::mdl
