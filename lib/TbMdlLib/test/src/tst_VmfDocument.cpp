/*
 Copyright (C) 2026 ProtoAus

 This file is part of SourceBroom, a fork of TrenchBroom.

 SourceBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 SourceBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with SourceBroom. If not, see <https://www.gnu.org/licenses/>.
 */

#include "base/ParserException.h"
#include "mdl/VmfDocument.h"

#include <array>
#include <stdexcept>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

namespace tb::mdl
{

TEST_CASE("VmfDocument explicit entry budget refuses before adding entries", "[vmf]")
{
  const auto source = std::string{"world { id 1 }"};
  const auto exact = VmfDocument::parse(source, 2);
  CHECK(exact.entries().size() == 2);
  CHECK(exact.serialize() == source);
  CHECK_THROWS_AS(VmfDocument::parse(source, 1), ParserException);
  CHECK_THROWS_AS(VmfDocument::parse(source, 0), ParserException);
  CHECK(VmfDocument::parse(" // no entries\n", 0).serialize() == " // no entries\n");
  CHECK(VmfDocument::parse(source).serialize() == source);
}

TEST_CASE("VmfDocument preserves nested structure and repeated outputs", "[vmf]")
{
  const auto source = std::string{R"vmf(versioninfo { "formatversion" "100" }
world
{
  "id" "1"
  solid
  {
    "id" "2"
    side
    {
      "id" "3"
      "plane" "(0 0 0) (1.125 0 0) (0 1 0)"
      dispinfo { "power" "2" normals { "row0" "0 0 1" } }
    }
    vertices_plus { "v" "1.125 2.25 3.5" }
  }
}
entity
{
  "id" "4"
  connections
  {
    "OnTrigger" "first,Enable,,0,-1"
    "OnTrigger" "second,Disable,,0.125,1"
  }
  future_extension { empty {} "opaque" "do not interpret" }
}
)vmf"};
  const auto document = VmfDocument::parse(source);
  const auto& entries = document.entries();
  REQUIRE(document.roots().size() == 3);
  REQUIRE(entries.size() == 23);
  CHECK(document.tokenContents(entries[document.roots()[0]].key) == "versioninfo");
  CHECK(document.tokenContents(entries[document.roots()[1]].key) == "world");
  CHECK(entries[document.roots()[1]].children.size() == 2);
  CHECK(entries[3].parent == document.roots()[1]);
  CHECK(document.serialize() == source);

  auto outputs = std::vector<VmfEntryId>{};
  for (auto i = size_t{0}; i < entries.size(); ++i)
  {
    if (document.tokenContents(entries[i].key) == "OnTrigger")
    {
      outputs.push_back(i);
    }
  }
  REQUIRE(outputs.size() == 2);
  REQUIRE(entries[outputs[0]].value);
  REQUIRE(entries[outputs[1]].value);
  CHECK(entries[outputs[0]].parent == entries[outputs[1]].parent);
  CHECK(document.tokenContents(*entries[outputs[0]].value) == "first,Enable,,0,-1");
  CHECK(document.tokenContents(*entries[outputs[1]].value) == "second,Disable,,0.125,1");
}

TEST_CASE("VmfDocument preserves original bytes and raw token contents", "[vmf]")
{
  const auto source = std::string{"\xef\xbb\xbf// header\r\nworld\r\n{\r\n"}
                      + "\t/* opaque comment */ \"name\" \"caf\xe9\"\r\n"
                      + R"vmf( "path" "C:\\maps\\test" "quote" "a\"b" "empty" "" )vmf"
                      + "\r\n \"output\" \"target\x1b" "Enable\x1b\x1b" "0\x1b-1\"\r\n"
                      + "} // trailing comment without newline";
  const auto document = VmfDocument::parse(source);
  CHECK(document.serialize() == source);
  REQUIRE(document.entries().size() == 6);
  CHECK(document.tokenContents(*document.entries()[2].value) == R"(C:\\maps\\test)");
  CHECK(document.tokenContents(*document.entries()[3].value) == R"(a\"b)");
  CHECK(document.tokenContents(*document.entries()[4].value).empty());
}

TEST_CASE("VmfDocument edits only the selected value tokens", "[vmf]")
{
  const auto source = std::string{R"vmf(// preserve formatting
world { "plane" "(0 0 0) (1.125 0 0) (0 1 0)" }
entity { connections { "OnTrigger" "first" /* keep */ "OnTrigger" "second" } }
)vmf"};
  const auto document = VmfDocument::parse(source);
  REQUIRE(document.entries().size() == 6);
  const auto edits = std::array{
    VmfValueEdit{5, "\"changed second\""}, VmfValueEdit{1, "\"precise 1.123456789\""}};
  auto expected = source;
  expected.replace(expected.find("\"second\""), 8, "\"changed second\"");
  const auto plane = std::string{"\"(0 0 0) (1.125 0 0) (0 1 0)\""};
  expected.replace(expected.find(plane), plane.size(), "\"precise 1.123456789\"");
  const auto edited = document.serialize(edits);
  CHECK(edited == expected);
  CHECK(edited != source);
  CHECK(document.serialize() == source);

  const auto reparsed = VmfDocument::parse(edited);
  REQUIRE(reparsed.entries().size() == document.entries().size());
  CHECK(reparsed.tokenContents(*reparsed.entries()[4].value) == "first");
  CHECK(reparsed.tokenContents(*reparsed.entries()[5].value) == "changed second");
  CHECK(reparsed.tokenContents(*reparsed.entries()[1].value) == "precise 1.123456789");
}

TEST_CASE("VmfDocument rejects malformed syntax with locations", "[vmf]")
{
  const auto invalid = std::vector<std::string>{
    "world {",
    "}",
    "{ }",
    "world { \"id\" }",
    "world { \"id\"",
    "world { \"id\" \"unterminated }",
    "world {} /* unterminated",
    std::string{"world {}\0", 9},
    std::string{"\xff\xfeW\0", 4},
    std::string{"\xfe\xff\0W", 4},
  };
  for (const auto& source : invalid)
  {
    INFO(source.size());
    CHECK_THROWS_AS(VmfDocument::parse(source), ParserException);
  }
  CHECK_THROWS_WITH(
    VmfDocument::parse("world\r\n{\r\n\"id\"\r\n}\r\n"),
    Catch::Matchers::ContainsSubstring("line 4"));
}

TEST_CASE("VmfDocument rejects unsafe token edits", "[vmf]")
{
  const auto document = VmfDocument::parse("world { \"key\" \"value\" }");
  const auto invalidTokens = std::vector<std::string>{
    "",
    "bare",
    "two words",
    "{}",
    "\"x\" \"injected\" \"y\"",
    " \"x\"",
    "\"x\" // comment",
    "\"unfinished",
    std::string{"\"x\0y\"", 5}};
  for (const auto& token : invalidTokens)
  {
    INFO(token.size());
    const auto edits = std::array{VmfValueEdit{1, token}};
    CHECK_THROWS_AS(document.serialize(edits), std::invalid_argument);
  }
  const auto badId = std::array{VmfValueEdit{2, "\"x\""}};
  const auto block = std::array{VmfValueEdit{0, "\"x\""}};
  const auto duplicate = std::array{VmfValueEdit{1, "\"x\""}, VmfValueEdit{1, "\"y\""}};
  CHECK_THROWS_AS(document.serialize(badId), std::invalid_argument);
  CHECK_THROWS_AS(document.serialize(block), std::invalid_argument);
  CHECK_THROWS_AS(document.serialize(duplicate), std::invalid_argument);
  CHECK(document.serialize() == "world { \"key\" \"value\" }");
}

TEST_CASE("VmfDocument does not merge adjacent tokens during edits", "[vmf]")
{
  const auto edit = std::array{VmfValueEdit{1, "bare"}};
  const auto left = VmfDocument::parse(R"(world {key"value"})");
  const auto right = VmfDocument::parse(R"(world {"key""value"next last})");
  CHECK_THROWS_AS(left.serialize(edit), std::invalid_argument);
  CHECK_THROWS_AS(right.serialize(edit), std::invalid_argument);
  const auto quoted = std::array{VmfValueEdit{1, "\"new\""}};
  CHECK(left.serialize(quoted) == R"(world {key"new"})");
  CHECK(right.serialize(quoted) == R"(world {"key""new"next last})");
  const auto comments = VmfDocument::parse(R"(world {"key" /*c*/original/*c*/})");
  CHECK(comments.serialize(quoted) == R"(world {"key" /*c*/"new"/*c*/})");
  CHECK(VmfDocument::parse(comments.serialize(quoted)).entries().size() == 2);
}

TEST_CASE("VmfDocument removal separates surviving lexical boundaries", "[vmf]")
{
  for (const auto& tokens : std::array{
         std::pair{"b", "c"},
         std::pair{"/", "/"},
         std::pair{"/", "*"},
         std::pair{"*", "/"},
         std::pair{"\"b\"", "c"},
         std::pair{"b", "\"c\""}})
  {
    const auto source = std::string{"root { a "} + tokens.first + "\"remove\" \"x\""
                        + tokens.second + " d }";
    const auto document = VmfDocument::parse(source);
    CHECK(document.serialize() == source);
    const auto removal =
      std::array{VmfStructuralEdit{VmfStructuralEdit::Type::Remove, 2, {}}};
    const auto saved = document.serialize({}, removal);
    CHECK(
      saved == std::string{"root { a "} + tokens.first + " " + tokens.second + " d }");
    const auto reopened = VmfDocument::parse(saved);
    REQUIRE(reopened.entries().size() == 3);
    CHECK(
      reopened.tokenContents(*reopened.entries()[1].value)
      == document.tokenContents(*document.entries()[1].value));
    CHECK(
      reopened.tokenContents(reopened.entries()[2].key)
      == document.tokenContents(document.entries()[3].key));
    CHECK(reopened.tokenContents(*reopened.entries()[2].value) == "d");
  }
}

TEST_CASE("VmfDocument has an explicit nesting limit", "[vmf]")
{
  const auto nested = [](const size_t depth) {
    auto source = std::string{};
    for (auto i = size_t{0}; i < depth; ++i)
    {
      source += "block { ";
    }
    source.append(depth, '}');
    return source;
  };
  CHECK(VmfDocument::parse(nested(256)).entries().size() == 256);
  CHECK_THROWS_AS(VmfDocument::parse(nested(257)), ParserException);
}

TEST_CASE("VmfDocument structural appends enforce combined nesting depth", "[vmf]")
{
  const auto nested = [](const size_t depth) {
    auto source = std::string{};
    for (auto i = size_t{0}; i < depth; ++i)
    {
      source += "block { ";
    }
    source.append(depth, '}');
    return source;
  };
  const auto root = VmfDocument::parse("world {}");
  const auto safe =
    std::array{VmfStructuralEdit{VmfStructuralEdit::Type::Append, 0, nested(255)}};
  const auto tooDeep =
    std::array{VmfStructuralEdit{VmfStructuralEdit::Type::Append, 0, nested(256)}};
  CHECK(VmfDocument::parse(root.serialize({}, safe)).entries().size() == 256);
  CHECK_THROWS_AS(root.serialize({}, tooDeep), std::invalid_argument);
  CHECK(root.serialize() == "world {}");

  const auto deepest = VmfDocument::parse(nested(256));
  const auto property = std::array{
    VmfStructuralEdit{VmfStructuralEdit::Type::Append, 255, "\"key\" \"value\""}};
  const auto block =
    std::array{VmfStructuralEdit{VmfStructuralEdit::Type::Append, 255, "child {}"}};
  CHECK(VmfDocument::parse(deepest.serialize({}, property)).entries().size() == 257);
  CHECK_THROWS_AS(deepest.serialize({}, block), std::invalid_argument);
}

TEST_CASE("VmfDocument spans survive copies moves and entry growth", "[vmf]")
{
  const auto original = VmfDocument::parse("a b");
  auto copy = original;
  const auto moved = std::move(copy);
  CHECK(moved.tokenContents(*moved.entries()[0].value) == "b");
  CHECK(original.serialize() == "a b");

  auto source = std::string{"world {"};
  for (auto i = size_t{0}; i < 10000; ++i)
  {
    source += " \"repeated\" \"" + std::to_string(i) + "\"";
  }
  source += '}';
  const auto large = VmfDocument::parse(source);
  REQUIRE(large.entries().size() == 10001);
  CHECK(large.entries()[0].children.size() == 10000);
  CHECK(large.entries().back().parent == 0);
  CHECK(large.tokenContents(*large.entries().back().value) == "9999");
  CHECK(large.serialize() == source);
}

TEST_CASE(
  "VmfDocument root additions preserve trailing comments and native fragments", "[vmf]")
{
  const auto source = std::string{"\xEF\xBB\xBF// header\r\nworld { id 1 } // tail"};
  const auto document = VmfDocument::parse(source);
  const auto entity = std::string{
    "entity { id 2 connections { OnUser1 first OnUser1 second } future { raw keep } }"};
  const auto append = std::array{VmfStructuralEdit{
    VmfStructuralEdit::Type::AppendRoot, VmfStructuralEdit::Root, entity}};
  const auto saved = document.serialize({}, append);
  CHECK(saved == source + "\n" + entity + "\n");
  const auto parsed = VmfDocument::parse(saved);
  REQUIRE(parsed.roots().size() == 2);
  CHECK(parsed.serializeEntry(parsed.roots()[1]) == entity);
  CHECK(document.serialize() == source);
  for (const auto& text :
       {std::string{},
        std::string{"// no entries"},
        std::string{"entity {"},
        std::string{"\xEF\xBB\xBF"
                    "entity {}"}})
  {
    const auto invalid = std::array{VmfStructuralEdit{
      VmfStructuralEdit::Type::AppendRoot, VmfStructuralEdit::Root, text}};
    CHECK_THROWS_AS(document.serialize({}, invalid), std::invalid_argument);
  }
  const auto wrongTarget =
    std::array{VmfStructuralEdit{VmfStructuralEdit::Type::AppendRoot, 0, entity}};
  CHECK_THROWS_AS(document.serialize({}, wrongTarget), std::invalid_argument);
}

TEST_CASE(
  "VmfDocument native subtree edits retain provenance and reject outside edits", "[vmf]")
{
  const auto document = VmfDocument::parse(
    "world { id 1 } entity { id 7 connections { OnUser1 first /*keep*/ OnUser1 second } "
    "editor { color unchanged } } future { raw untouched }");
  const auto original = document.source();
  const auto entity = document.roots()[1];
  REQUIRE(entity == 2);
  const auto values =
    std::array{VmfValueEdit{3, "\"9\""}, VmfValueEdit{6, "\"changed\""}};
  const auto add = std::array{
    VmfStructuralEdit{VmfStructuralEdit::Type::Append, entity, "spawnflags 0"}};
  const auto saved = document.serializeEntry(entity, values, add);
  CHECK(saved == "entity { id \"9\" connections { OnUser1 first /*keep*/ OnUser1 \"changed\" } "
                 "editor { color unchanged } \nspawnflags 0\n}");
  CHECK(document.source() == original);
  const auto outside = std::array{VmfValueEdit{1, "\"wrong world\""}};
  CHECK_THROWS_AS(document.serializeEntry(entity, outside), std::invalid_argument);
  const auto wrongStructure = std::array{
    VmfStructuralEdit{VmfStructuralEdit::Type::Remove, document.roots()[2], {}}};
  CHECK_THROWS_AS(
    document.serializeEntry(entity, {}, wrongStructure), std::invalid_argument);
  const auto rootAppend = std::array{VmfStructuralEdit{
    VmfStructuralEdit::Type::AppendRoot, VmfStructuralEdit::Root, "entity {}"}};
  CHECK_THROWS_AS(document.serializeEntry(entity, {}, rootAppend), std::invalid_argument);
}

TEST_CASE("VmfDocument accepts empty documents and checks token spans", "[vmf]")
{
  CHECK(VmfDocument::parse("// comments only").entries().empty());
  CHECK(VmfDocument::parse("").serialize().empty());
  const auto document = VmfDocument::parse("a b");
  CHECK_THROWS_AS(document.tokenText({4, 0, false}), std::out_of_range);
  CHECK_THROWS_AS(document.tokenText({1, 4, false}), std::out_of_range);
  CHECK_THROWS_AS(document.tokenContents({0, 1, true}), std::invalid_argument);
}

} // namespace tb::mdl
