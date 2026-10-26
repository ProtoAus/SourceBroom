/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TestEnvironment.h"
#include "TestParserStatus.h"
#include "el/TestUtils.h"
#include "el/VariableStore.h"
#include "fs/DiskIO.h"
#include "fs/Reader.h"
#include "fs/TestEnvironment.h"
#include "fs/TraversalMode.h"
#include "mdl/CatchConfig.h"
#include "mdl/Entity.h"
#include "mdl/EntityDefinition.h"
#include "mdl/EntityDefinitionTestUtils.h"
#include "mdl/EntityPropertiesVariableStore.h"
#include "mdl/FgdParser.h"
#include "mdl/LoadEntityDefinitions.h"
#include "mdl/ModelSpecification.h"
#include "mdl/PropertyDefinition.h"

#include "kd/ranges/contains.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <tuple>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace tb::mdl
{

TEST_CASE("FgdParser")
{
  SECTION("Source fixed studio literals follow entity skin without changing properties")
  {
    const auto* alias = GENERATE("studio", "studioprop");
    const auto* literal = GENERATE(R"("models/probe.mdl")", "'models/probe.mdl'");
    const auto source = std::string{"@PointClass "} + alias + "(" + literal
                        + R"() = probe [ skin(integer) : "Skin" : 9 ])";
    auto parser = FgdParser{source, RgbaF{1, 1, 1, 1}, {}, true};
    auto status = TestParserStatus{};
    const auto definitions = parser.parseDefinitions(status) | kdl::value();
    REQUIRE(definitions.size() == 1);
    auto entity =
      Entity{{{"classname", "probe"}, {"unrelated", "01.000"}, {"scale", "7"}}};
    entity.setDefinition(&definitions.front());
    const auto originalProperties = entity.properties();
    CHECK(entity.modelSpecification() == ModelSpecification{"models/probe.mdl", 0, 0});
    CHECK(entity.properties() == originalProperties);
    CHECK_FALSE(entity.hasProperty("skin")); // FGD default is not an authored value.
    CHECK_FALSE(entity.hasProperty("model"));
    for (const auto* raw :
         {"0",
          "1",
          "0",
          "2",
          "1.75",
          "-1",
          "1e100",
          "-1e100",
          "inf",
          "nan",
          "invalid",
          ""})
    {
      CAPTURE(alias, raw);
      entity.addOrUpdateProperty("skin", raw);
      const auto before = entity.properties();
      const auto expectedSkin = std::string{raw} == "2" ? 2u
                                : (std::string{raw} == "1" || std::string{raw} == "1.75")
                                  ? 1u
                                  : 0u;
      CHECK(
        entity.modelSpecification()
        == ModelSpecification{"models/probe.mdl", expectedSkin, 0});
      CHECK(entity.properties() == before);
      CHECK(*entity.property("skin") == raw);
    }
    const auto& model = definitions.front().pointEntityDefinition->modelDefinition;
    const auto variables = EntityPropertiesVariableStore{entity};
    CHECK(model.scale(variables, std::nullopt) == vm::vec3d{1, 1, 1});
    CHECK(model.scale(variables, el::lit(3)) == vm::vec3d{3, 3, 3});
  }

  SECTION("Source fixed studio normalization preserves other declaration semantics")
  {
    using T = std::tuple<std::string, bool, ModelSpecification>;
    const auto [declaration, sourceMode, expected] = GENERATE(
      values<T>({
        {R"(studio("models/probe.mdl"))", false, {"models/probe.mdl", 0, 0}},
        {R"(studioprop("models/probe.mdl"))", false, {"models/probe.mdl", 0, 0}},
        {R"(model("models/probe.mdl"))", true, {"models/probe.mdl", 0, 0}},
        {R"(sprite("models/probe.mdl"))", true, {"models/probe.mdl", 0, 0}},
        {R"(iconsprite("models/probe.mdl"))", true, {"models/probe.mdl", 0, 0}},
        {R"(studio({path: "models/probe.mdl", skin: 0, frame: 2}))",
         true,
         {"models/probe.mdl", 0, 2}},
        {R"(studioprop({path: "models/probe.mdl", skin: 0}))",
         true,
         {"models/probe.mdl", 0, 0}},
        {R"(studio("models/probe.mdl" 2 3))", true, {"models/probe.mdl", 2, 3}},
        {R"(studioprop("models/probe.mdl" 2 3))", true, {"models/probe.mdl", 2, 3}},
        {R"(studio(model))", true, {"models/entity.mdl", 0, 0}},
        {R"(studio({{ skin == 1 -> "models/probe.mdl", model }}))",
         true,
         {"models/probe.mdl", 0, 0}},
        {R"(studio("models/" + "probe.mdl"))", true, {"models/probe.mdl", 0, 0}},
        {R"(studio(""))", true, {{}, 0, 0}},
        {R"(studio())", true, {"models/entity.mdl", 1, 0}},
        {R"(studio())", false, {"models/entity.mdl", 1, 0}},
        {R"(studioprop())", true, {"models/entity.mdl", 1, 0}},
      }));
    CAPTURE(declaration, sourceMode);
    const auto source = "@PointClass " + declaration + " = probe []";
    auto parser = FgdParser{source, RgbaF{1, 1, 1, 1}, {}, sourceMode};
    auto status = TestParserStatus{};
    const auto definitions = parser.parseDefinitions(status) | kdl::value();
    REQUIRE(definitions.size() == 1);
    auto entity = Entity{{{"model", "models/entity.mdl"}, {"skin", "1"}, {"frame", "9"}}};
    entity.setDefinition(&definitions.front());
    const auto before = entity.properties();
    CHECK(entity.modelSpecification() == expected);
    CHECK(entity.properties() == before);
  }

  SECTION("Source-only literal and empty studio body policy preserves explicit controls")
  {
    using T = std::tuple<std::string, bool, ModelSpecification>;
    const auto [declaration, sourceMode, expected] = GENERATE(
      values<T>({
        {R"(studio("probe.mdl"))", true, {"probe.mdl", 1, 0, 3}},
        {R"(studioprop("probe.mdl"))", true, {"probe.mdl", 1, 0, 3}},
        {R"(studio())", true, {"entity.mdl", 1, 0, 3}},
        {R"(studioprop())", true, {"entity.mdl", 1, 0, 3}},
        {R"(studio("probe.mdl"))", false, {"probe.mdl", 0, 0, 0}},
        {R"(studio())", false, {"entity.mdl", 1, 0, 0}},
        {R"(model("probe.mdl"))", true, {"probe.mdl", 0, 0, 0}},
        {R"(sprite("probe.mdl"))", true, {"probe.mdl", 0, 0, 0}},
        {R"(studio("probe.mdl" 2 4))", true, {"probe.mdl", 2, 4, 0}},
        {R"(studio(model))", true, {"entity.mdl", 0, 0, 0}},
        {R"(studio({path: "probe.mdl", skin: 2, frame: 4}))",
         true,
         {"probe.mdl", 2, 4, 0}},
        {R"(studio({path: "probe.mdl", skin: 2, frame: 4, body: 1}))",
         true,
         {"probe.mdl", 2, 4, 1}},
        {R"(studio({path: "probe.mdl", body: body}))", false, {"probe.mdl", 0, 0, 3}},
      }));
    CAPTURE(declaration, sourceMode);
    const auto source = "@PointClass " + declaration + " = probe []";
    auto parser = FgdParser{source, RgbaF{1, 1, 1, 1}, {}, sourceMode};
    auto status = TestParserStatus{};
    const auto definitions = parser.parseDefinitions(status) | kdl::value();
    REQUIRE(definitions.size() == 1);
    auto entity =
      Entity{{{"model", "entity.mdl"}, {"skin", "1"}, {"frame", "9"}, {"body", "3"}}};
    entity.setDefinition(&definitions.front());
    const auto properties = entity.properties();
    CHECK(entity.modelSpecification() == expected);
    CHECK(entity.properties() == properties);
    if (sourceMode && declaration == R"(studio("probe.mdl"))")
    {
      entity.removeProperty("body");
      CHECK(entity.modelSpecification() == ModelSpecification{"probe.mdl", 1, 0, 0});
    }
  }

  SECTION("loadEntityDefinitions transports Source normalization through includes")
  {
    auto env = fs::TestEnvironment{};
    env.createFile("root.fgd", R"(@include "included.fgd")");
    env.createFile("included.fgd", R"(
      @PointClass studio("models/probe.mdl") = probe []
      @PointClass studioprop("models/probe.mdl") = prop []
    )");
    const auto rootBefore = env.loadFile("root.fgd");
    const auto includeBefore = env.loadFile("included.fgd");
    for (const auto sourceMode : {false, true})
    {
      auto status = TestParserStatus{};
      const auto definitions =
        (sourceMode
           ? loadEntityDefinitions(
               env.dir() / "root.fgd", RgbaF{1, 1, 1, 1}, status, true)
           : loadEntityDefinitions(env.dir() / "root.fgd", RgbaF{1, 1, 1, 1}, status))
        | kdl::value();
      REQUIRE(definitions.size() == 2);
      CHECK(status.countStatus(LogLevel::Error) == 0);
      for (const auto& definition : definitions)
      {
        auto entity = Entity{{{"skin", "1"}, {"body", "3"}}};
        entity.setDefinition(&definition);
        CHECK(
          entity.modelSpecification()
          == ModelSpecification{
            "models/probe.mdl", sourceMode ? 1u : 0u, 0, sourceMode ? 3u : 0u});
      }
    }
    CHECK(env.loadFile("root.fgd") == rootBefore);
    CHECK(env.loadFile("included.fgd") == includeBefore);
  }

  SECTION("Source keyvalues inputs and outputs have independent namespaces")
  {
    const auto source = R"(
      @PointClass = timer [
        input RefireTime(float) : "Set interval"
        RefireTime(float) : "Initial interval" : 1
        output RefireTime(float) : "Interval event"
        input RefireTime(string) : "Duplicate input must still be refused"
      ]
    )";
    auto parser = FgdParser{source, RgbaF{1, 1, 1, 1}};
    auto status = TestParserStatus{};
    const auto definitions = parser.parseDefinitions(status) | kdl::value();
    REQUIRE(definitions.size() == 1);
    const auto& definition = definitions.front();
    REQUIRE(definition.propertyDefinitions.size() == 3);
    CHECK(status.countStatus(LogLevel::Warn) == 1);
    CHECK(status.countStatus(LogLevel::Error) == 0);
    CHECK(
      std::holds_alternative<PropertyValueTypes::Input>(
        definition.propertyDefinitions[0].valueType));
    CHECK(
      std::holds_alternative<PropertyValueTypes::Float>(
        definition.propertyDefinitions[1].valueType));
    CHECK(
      std::holds_alternative<PropertyValueTypes::Output>(
        definition.propertyDefinitions[2].valueType));
    const auto* value = getPropertyDefinition(definition, "RefireTime");
    REQUIRE(value);
    CHECK(std::holds_alternative<PropertyValueTypes::Float>(value->valueType));
    CHECK(std::get<PropertyValueTypes::Float>(value->valueType).defaultValue == 1.0f);
  }

  SECTION("Source declaration namespaces are retained through inheritance")
  {
    const auto source = R"(
      @BaseClass = Base [
        value(string) : "Initial value" : "base"
        input value(void) : "Base input"
        output value(void) : "Base output"
      ]
      @PointClass base(Base) = Inherit [ value(integer) : "Value" : 4 ]
      @PointClass base(Base) = Override [
        input value(string) : "Derived input"
        output value(float) : "Derived output"
      ]
    )";
    auto parser = FgdParser{source, RgbaF{1, 1, 1, 1}};
    auto status = TestParserStatus{};
    const auto definitions = parser.parseDefinitions(status) | kdl::value();
    REQUIRE(definitions.size() == 2);
    CHECK(status.countStatus(LogLevel::Warn) == 0);
    CHECK(status.countStatus(LogLevel::Error) == 0);
    for (const auto& definition : definitions)
    {
      REQUIRE(definition.propertyDefinitions.size() == 3);
      const auto* value = getPropertyDefinition(definition, "value");
      REQUIRE(value);
      CHECK(
        PropertyDefinition::defaultValue(*value)
        == (definition.name == "Inherit" ? "4" : "base"));
      const auto input =
        std::ranges::find_if(definition.propertyDefinitions, [](const auto& p) {
          return std::holds_alternative<PropertyValueTypes::Input>(p.valueType);
        });
      const auto output =
        std::ranges::find_if(definition.propertyDefinitions, [](const auto& p) {
          return std::holds_alternative<PropertyValueTypes::Output>(p.valueType);
        });
      REQUIRE(input != definition.propertyDefinitions.end());
      REQUIRE(output != definition.propertyDefinitions.end());
      const auto overridden = definition.name == "Override";
      CHECK(
        std::get<PropertyValueTypes::Input>(input->valueType).parameterType
        == (overridden ? PropertyValueTypes::IOParameterType::String : PropertyValueTypes::IOParameterType::Void));
      CHECK(
        std::get<PropertyValueTypes::Output>(output->valueType).parameterType
        == (overridden ? PropertyValueTypes::IOParameterType::Float : PropertyValueTypes::IOParameterType::Void));
    }
  }

  SECTION("Source studio optional skin cannot overflow and does not rewrite properties")
  {
    auto parser = FgdParser{"@PointClass studio() = probe []", RgbaF{1, 1, 1, 1}};
    auto status = TestParserStatus{};
    const auto definitions = parser.parseDefinitions(status) | kdl::value();
    REQUIRE(definitions.size() == 1);
    REQUIRE(definitions[0].pointEntityDefinition.has_value());
    for (const auto* raw : {"2", "1.75", "1e100", "-1e100", "inf", "nan"})
    {
      CAPTURE(raw);
      const auto entity = Entity{{{"model", "models/probe.mdl"}, {"skin", raw}}};
      const auto variables = EntityPropertiesVariableStore{entity};
      const auto spec =
        definitions[0].pointEntityDefinition->modelDefinition.modelSpecification(
          variables)
        | kdl::value();
      CHECK(spec.path == std::filesystem::path{"models/probe.mdl"});
      CHECK(spec.skinIndex == (std::string{raw} == "2" ? 2u
        : std::string{raw} == "1.75" ? 1u : 0u));
      REQUIRE(entity.property("skin"));
      CHECK(*entity.property("skin") == raw);
    }
  }

  SECTION("SourceBroom builtin authoring definitions are actionable")
  {
    const auto path = getFixtureRoot() / "games/Source-common/source.fgd";
    auto file = fs::Disk::openFile(path) | kdl::value();
    auto reader = file->reader().buffer();
    auto parser = FgdParser{reader.stringView(), RgbaF{1, 1, 1, 1}, path};
    auto status = TestParserStatus{};
    const auto definitions = parser.parseDefinitions(status) | kdl::value();
    CHECK(status.countStatus(LogLevel::Warn) == 0);
    CHECK(status.countStatus(LogLevel::Error) == 0);
    const auto findDefinition = [&](const std::string& name) -> const EntityDefinition& {
      const auto found = std::ranges::find(definitions, name, &EntityDefinition::name);
      REQUIRE(found != definitions.end());
      return *found;
    };
    const auto& relay = findDefinition("logic_relay");
    const auto* event = getPropertyDefinition(relay, "OnTrigger");
    const auto* input = getPropertyDefinition(relay, "Trigger");
    REQUIRE(event);
    REQUIRE(input);
    CHECK(std::holds_alternative<PropertyValueTypes::Output>(event->valueType));
    CHECK(std::holds_alternative<PropertyValueTypes::Input>(input->valueType));
    const auto& counter = findDefinition("math_counter");
    const auto* add = getPropertyDefinition(counter, "Add");
    REQUIRE(add);
    CHECK(
      std::get<PropertyValueTypes::Input>(add->valueType).parameterType
      == PropertyValueTypes::IOParameterType::Float);
    const auto& timer = findDefinition("logic_timer");
    CHECK(
      std::ranges::count(
        timer.propertyDefinitions, "RefireTime", &PropertyDefinition::key)
      == 2);
    for (const auto* name : {"prop_static", "prop_dynamic", "prop_physics"})
    {
      const auto& prop = findDefinition(name);
      REQUIRE(prop.pointEntityDefinition.has_value());
      const auto variables = el::VariableTable{{
        {"model", el::Value{std::string{"models/props/sourcebroom_probe.mdl"}}},
        {"skin", el::Value{2}},
      }};
      const auto spec =
        prop.pointEntityDefinition->modelDefinition.modelSpecification(variables)
        | kdl::value();
      CHECK(spec.path == std::filesystem::path{"models/props/sourcebroom_probe.mdl"});
      CHECK(spec.skinIndex == 2u);
      CHECK(getPropertyDefinition(prop, "model"));
      CHECK(getPropertyDefinition(prop, "skin"));
    }
  }

  SECTION("Included files")
  {
    const auto basePath = getFixtureRoot() / "games/";
    const auto cfgFiles =
      fs::Disk::find(
        basePath, fs::TraversalMode::Recursive, fs::makeExtensionPathMatcher({".fgd"}))
      | kdl::value();

    for (const auto& path : cfgFiles)
    {
      CAPTURE(path);

      auto file = fs::Disk::openFile(path) | kdl::value();
      auto reader = file->reader().buffer();

      auto parser = FgdParser{reader.stringView(), RgbaF{1.0f, 1.0f, 1.0f, 1.0f}, path};

      auto status = TestParserStatus{};
      CHECK(parser.parseDefinitions(status));

      /* Disabled because our files are full of previously undetected problems
      if (status.countStatus(LogLevel::Warn) > 0u) {
          UNSCOPED_INFO("Parsing FGD file " << path.string() << " produced warnings");
          for (const auto& message : status.messages(LogLevel::Warn)) {
              UNSCOPED_INFO(message);
          }
          CHECK(status.countStatus(LogLevel::Warn) == 0u);
      }

      if (status.countStatus(LogLevel::Error) > 0u) {
          UNSCOPED_INFO("Parsing FGD file " << path.string() << " produced errors");
          for (const auto& message : status.messages(LogLevel::Error)) {
              UNSCOPED_INFO(message);
          }
          CHECK(status.countStatus(LogLevel::Error) == 0u);
      }
      */
    }
  }

  SECTION("empty and whitespace input")
  {
    SECTION("parseEmptyFile")
    {
      const auto file = "";
      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(parser.parseDefinitions(status) == std::vector<mdl::EntityDefinition>{});
    }

    SECTION("parseWhitespaceFile")
    {
      const auto file = "     \n  \t \n  ";
      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(parser.parseDefinitions(status) == std::vector<mdl::EntityDefinition>{});
    }

    SECTION("parseCommentsFile")
    {
      const auto file = R"(// asdfasdfasdf
  //kj3k4jkdjfkjdf
  )";
      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(parser.parseDefinitions(status) == std::vector<mdl::EntityDefinition>{});
    }
  }

  SECTION("class parsing")
  {
    SECTION("parseEmptyFlagDescription")
    {
      const auto file = R"(
      @PointClass color(0 255 0) size(-2 -2 -12, 2 2 12) = light_mine1 : 
      "Dusty fluorescent light fixture"
      [
          spawnflags(Flags) =
          [
              1 : "" : 0
          ]
      ]
      // 0221 - changed inheritance from "light" to "light_min1"
  )";
      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "light_mine1",
            RgbB{0, 255, 0},
            "Dusty fluorescent light fixture",
            {
              {"spawnflags",
               mdl::PropertyValueTypes::Flags{
                 {
                   {1, "", ""},
                 },
               },
               "",
               ""},
            },
            mdl::PointEntityDefinition{{{-2, -2, -12}, {2, 2, 12}}, {}, {}},
          },
        });
    }

    SECTION("parseSolidClass")
    {
      const auto file = R"-(
    @SolidClass = worldspawn : "World entity"
    [
       message(string) : "Text on entering the world"
       worldtype(choices) : "Ambience" : 0 =
       [
           0 : "Medieval"
           1 : "Metal (runic)"
           2 : "Base"
       ]
       sounds(integer) : "CD track to play" : 0
       light(integer) : "Ambient light"
       _sunlight(integer) : "Sunlight"
       _sun_mangle(string) : "Sun mangle (Yaw pitch roll)"
    ])-";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "worldspawn",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "World entity",
            {
              {"message",
               mdl::PropertyValueTypes::String{},
               "Text on entering the world",
               ""},
              {"worldtype",
               mdl::PropertyValueTypes::Choice{
                 {
                   {"0", "Medieval"},
                   {"1", "Metal (runic)"},
                   {"2", "Base"},
                 },
                 "0"},
               "Ambience",
               ""},
              {"sounds", mdl::PropertyValueTypes::Integer{0}, "CD track to play", ""},
              {"light", mdl::PropertyValueTypes::Integer{}, "Ambient light", ""},
              {"_sunlight", mdl::PropertyValueTypes::Integer{}, "Sunlight", ""},
              {"_sun_mangle",
               mdl::PropertyValueTypes::String{},
               "Sun mangle (Yaw pitch roll)",
               ""},
            },
          },
        });
    }

    SECTION("parsePointClass")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
      	use(string) : "self.use"
      	think(string) : "self.think"
      	nextthink(integer) : "nextthink"
      	noise(string) : "noise"
      	touch(string) : "self.touch"
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {"info_notnull",
           RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
           "Wildcard entity",
           {
             {"use", mdl::PropertyValueTypes::String{}, "self.use", ""},
             {"think", mdl::PropertyValueTypes::String{}, "self.think", ""},
             {"nextthink", mdl::PropertyValueTypes::Integer{}, "nextthink", ""},
             {"noise", mdl::PropertyValueTypes::String{}, "noise", ""},
             {"touch", mdl::PropertyValueTypes::String{}, "self.touch", ""},
           },
           mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}}},
        });
    }

    SECTION("parseDuplicatePointClassKeepsLast")
    {
      const auto file = R"(
      @PointClass = info_notnull : "First definition"
      [
      ]
      @PointClass = info_notnull : "Second definition"
      [
      ]
      )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {"info_notnull",
           RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
           "Second definition",
           {},
           mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}}},
        });
      CHECK(status.countStatus(LogLevel::Warn) == 1u);
      CHECK(status.countStatus(LogLevel::Error) == 0u);
    }

    SECTION("parseUnknownClassType")
    {
      const auto file = R"(
      @WeirdClass = info_notnull : "Wildcard entity"
      [
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      // the ParserException thrown while parsing the unknown class type is caught and
      // converted to an Error by parseDefinitions
      CHECK(parser.parseDefinitions(status).is_error());
    }

    SECTION("parseMainClass")
    {
      const auto file = R"(
      @Main = []
      @PointClass = info_notnull : "Wildcard entity"
      [
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {"info_notnull",
           RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
           "Wildcard entity",
           {},
           mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}}},
        });
      CHECK(status.countStatus(LogLevel::Warn) == 0u);
      CHECK(status.countStatus(LogLevel::Error) == 0u);
    }

    SECTION("parseSolidClassWarnsAboutSize")
    {
      const auto file = R"(
      @SolidClass size(-8 -8 -8, 8 8 8) = worldspawn : "World entity"
      [
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      (void)parser.parseDefinitions(status);
      CHECK(status.countStatus(LogLevel::Warn) == 1u);
    }

    SECTION("parseSolidClassWarnsAboutDecal")
    {
      const auto file = R"(
      @SolidClass decal() = worldspawn : "World entity"
      [
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      (void)parser.parseDefinitions(status);
      CHECK(status.countStatus(LogLevel::Warn) == 1u);
    }

    SECTION("parseDuplicateBasePropertyWarns")
    {
      const auto file = R"(
      @PointClass base(A) base(B) = info_notnull : "Wildcard entity"
      [
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      (void)parser.parseDefinitions(status);
      CHECK(status.countStatus(LogLevel::Warn) == 1u);
    }

    SECTION("parseDuplicateColorPropertyWarns")
    {
      const auto file = R"(
      @PointClass color(0 255 0) color(255 0 0) = info_notnull : "Wildcard entity"
      [
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      (void)parser.parseDefinitions(status);
      CHECK(status.countStatus(LogLevel::Warn) == 1u);
    }

    SECTION("parseDuplicateSizePropertyWarns")
    {
      const auto file = R"(
      @PointClass size(-8 -8 -8, 8 8 8) size(-16 -16 -16, 16 16 16) = info_notnull : "Wildcard entity"
      [
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      (void)parser.parseDefinitions(status);
      CHECK(status.countStatus(LogLevel::Warn) == 1u);
    }

    SECTION("parseDuplicateModelPropertyWarns")
    {
      const auto file = R"(
      @PointClass sprite() sprite() = info_notnull : "Wildcard entity"
      [
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      (void)parser.parseDefinitions(status);
      CHECK(status.countStatus(LogLevel::Warn) == 1u);
    }

    SECTION("parseDuplicateDecalPropertyWarns")
    {
      const auto file = R"(
      @PointClass decal() decal() = info_notnull : "Wildcard entity"
      [
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      (void)parser.parseDefinitions(status);
      CHECK(status.countStatus(LogLevel::Warn) == 1u);
    }

    SECTION("parseBaseProperty")
    {
      const auto file = R"(
      @baseclass = Appearflags [
      	spawnflags(Flags) =
      	[
      		256 : "Not on Easy" : 0
      		512 : "Not on Normal" : 0
      		1024 : "Not on Hard" : 0
      		2048 : "Not in Deathmatch" : 0
      	]
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(parser.parseDefinitions(status) == std::vector<mdl::EntityDefinition>{});
    }

    SECTION("parsePointClassWithBaseClasses")
    {
      const auto file = R"(
      @baseclass = Appearflags [
      	spawnflags(Flags) =
      	[
      		256 : "Not on Easy" : 0
      		512 : "Not on Normal" : 0
      		1024 : "Not on Hard" : 0
      		2048 : "Not in Deathmatch" : 0
      	]
      ]
      @baseclass = Targetname [ targetname(target_source) : "Name" ]
      @baseclass = Target [ 
      	target(target_destination) : "Target" 
      	killtarget(target_destination) : "Killtarget"
      ]
      @PointClass base(Appearflags, Target, Targetname) = info_notnull : "Wildcard entity" // I love you
      [
      	use(string) : "self.use"
      	think(string) : "self.think"
      	nextthink(integer) : "nextthink"
      	noise(string) : "noise"
      	touch(string) : "self.touch"
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {"info_notnull",
           RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
           "Wildcard entity",
           {
             {"use", mdl::PropertyValueTypes::String{}, "self.use", ""},
             {"think", mdl::PropertyValueTypes::String{}, "self.think", ""},
             {"nextthink", mdl::PropertyValueTypes::Integer{}, "nextthink", ""},
             {"noise", mdl::PropertyValueTypes::String{}, "noise", ""},
             {"touch", mdl::PropertyValueTypes::String{}, "self.touch", ""},
             {"spawnflags",
              mdl::PropertyValueTypes::Flags{{
                {256, "Not on Easy", ""},
                {512, "Not on Normal", ""},
                {1024, "Not on Hard", ""},
                {2048, "Not in Deathmatch", ""},
              }},
              "",
              ""},
             {"target", mdl::PropertyValueTypes::LinkSource{}, "Target", ""},
             {"killtarget", mdl::PropertyValueTypes::LinkSource{}, "Killtarget", ""},
             {"targetname", mdl::PropertyValueTypes::LinkTarget{}, "Name", ""},
           },
           mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}}},
        });
    }

    SECTION("parsePointClassWithUnknownClassProperties")
    {
      const auto file = R"(
      @PointClass unknown1 unknown2(spaghetti) = info_notnull : "Wildcard entity" // I love you
      [
      	use(string) : "self.use"
      	think(string) : "self.think"
      	nextthink(integer) : "nextthink"
      	noise(string) : "noise"
      	touch(string) : "self.touch"
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {"info_notnull",
           RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
           "Wildcard entity",
           {
             {"use", mdl::PropertyValueTypes::String{}, "self.use", ""},
             {"think", mdl::PropertyValueTypes::String{}, "self.think", ""},
             {"nextthink", mdl::PropertyValueTypes::Integer{}, "nextthink", ""},
             {"noise", mdl::PropertyValueTypes::String{}, "noise", ""},
             {"touch", mdl::PropertyValueTypes::String{}, "self.touch", ""},
           },
           mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}}},
        });
    }
  }

  SECTION("property definitions")
  {
    SECTION("parseType_TargetSourcePropertyDefinition")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
      	targetname(target_source) : "Source" : : "A long description" 
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"targetname",
               mdl::PropertyValueTypes::LinkTarget{},
               "Source",
               "A long description"},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }

    SECTION("parseType_TargetDestinationPropertyDefinition")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
      	target(target_destination) : "Target" 
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};


      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"target", mdl::PropertyValueTypes::LinkSource{}, "Target", ""},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }

    SECTION("parseStringPropertyDefinition")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
         message(string) : "Text on entering the world" : : "Long description 1"
         message2(string) : "With a default value" : "DefaultValue" : "Long description 2"
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"message",
               mdl::PropertyValueTypes::String{},
               "Text on entering the world",
               "Long description 1"},
              {"message2",
               mdl::PropertyValueTypes::String{"DefaultValue"},
               "With a default value",
               "Long description 2"},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }

    SECTION("parsePropertyDefinitionWithNumericKey")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
         123(string) : "Something" : : "Long description 1"
         456(string) : "Something" : : "Long description 1"
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"123",
               mdl::PropertyValueTypes::String{},
               "Something",
               "Long description 1"},
              {"456",
               mdl::PropertyValueTypes::String{},
               "Something",
               "Long description 1"},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }

    /**
     * Support having an integer (or decimal) as a default for a string
     * propertyDefinition. Technically a type mismatch, but appears in the wild; see:
     * https://github.com/TrenchBroom/TrenchBroom/issues/2833
     */
    SECTION("parseStringPropertyDefinition_IntDefault")
    {
      const auto file = R"(@PointClass = info_notnull : "Wildcard entity"
  [
      name(string) : "Description" : 3
      other(string) : "" : 1.5
  ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"name", mdl::PropertyValueTypes::String{"3"}, "Description", ""},
              {"other", mdl::PropertyValueTypes::String{"1.5"}, "", ""},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }

    SECTION("parseIntegerPropertyDefinition")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
         sounds(integer) : "CD track to play" : : "Longer description"
         sounds2(integer) : "CD track to play with default" : 2 : "Longer description"
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"sounds",
               mdl::PropertyValueTypes::Integer{},
               "CD track to play",
               "Longer description"},
              {"sounds2",
               mdl::PropertyValueTypes::Integer{2},
               "CD track to play with default",
               "Longer description"},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }

    SECTION("parseReadOnlyPropertyDefinition")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
         sounds(integer) readonly : "CD track to play" : : "Longer description"
         sounds2(integer) : "CD track to play with default" : 2 : "Longe
      description"
      ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"sounds",
               mdl::PropertyValueTypes::Integer{},
               "CD track to play",
               "Longer description",
               true},
              {"sounds2",
               mdl::PropertyValueTypes::Integer{2},
               "CD track to play with default",
               R"(Longe
      description)"},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }

    SECTION("parseFloatPropertyDefinition")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
         test(float) : "Some test propertyDefinition" : : "Longer description 1"
         test2(float) : "Some test propertyDefinition with default" : "2.7" : "Longer description 2"
         test3(float) : "Some test propertyDefinition with integer default" : 2 : "Longer description 3"
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"test",
               mdl::PropertyValueTypes::Float{},
               "Some test propertyDefinition",
               "Longer description 1"},
              {"test2",
               mdl::PropertyValueTypes::Float{2.7f},
               "Some test propertyDefinition with default",
               "Longer description 2"},
              {"test3",
               mdl::PropertyValueTypes::Float{2.0f},
               "Some test propertyDefinition with integer default",
               "Longer description 3"},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
      CHECK(status.messages(LogLevel::Warn) == std::vector<std::string>{});
    }

    SECTION("parseChoicePropertyDefinition")
    {
      const auto file = R"-(
            @PointClass = info_notnull : "Wildcard entity" // I love you\n
[
    worldtype(choices) : "Ambience" : : "Long description 1" =
    [
        0 : "Medieval"
        1 : "Metal (runic)"
        2 : "Base"
    ]
    worldtype2(choices) : "Ambience with default" : 1 : "Long description 2" =
    [
        0 : "Medieval"
        1 : "Metal (runic)"
    ]
    puzzle_id(choices) : "Puzzle id" : "cskey" =
    [
        "keep3" : "Mill key"
        "cskey" : "Castle key"
        "scrol" : "Disrupt Magic Scroll"
    ]
    floaty(choices) : "Floaty" : 2.3 =
    [
        1.0 : "Something"
        2.3 : "Something else"
        0.1 : "Yet more"
    ]
    negative(choices) : "Negative values" : -1 =
    [
        -2 : "Something"
        -1 : "Something else"
         1 : "Yet more"
    ]
]
            )-";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"worldtype",
               mdl::PropertyValueTypes::Choice{{
                 {"0", "Medieval"},
                 {"1", "Metal (runic)"},
                 {"2", "Base"},
               }},
               "Ambience",
               "Long description 1"},
              {"worldtype2",
               mdl::PropertyValueTypes::Choice{
                 {
                   {"0", "Medieval"},
                   {"1", "Metal (runic)"},
                 },
                 "1"},
               "Ambience with default",
               "Long description 2"},
              {"puzzle_id",
               mdl::PropertyValueTypes::Choice{
                 {
                   {"keep3", "Mill key"},
                   {"cskey", "Castle key"},
                   {"scrol", "Disrupt Magic Scroll"},
                 },
                 "cskey"},
               "Puzzle id",
               ""},
              {"floaty",
               mdl::PropertyValueTypes::Choice{
                 {
                   {"1.0", "Something"},
                   {"2.3", "Something else"},
                   {"0.1", "Yet more"},
                 },
                 "2.3"},
               "Floaty",
               ""},
              {"negative",
               mdl::PropertyValueTypes::Choice{
                 {
                   {"-2", "Something"},
                   {"-1", "Something else"},
                   {"1", "Yet more"},
                 },
                 "-1"},
               "Negative values",
               ""},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }

    SECTION("parseFlagsPropertyDefinition")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
      	spawnflags(Flags) =
      	[
      		256 : "Not on Easy" : 0
      		512 : "Not on Normal" : 1
      		1024 : "Not on Hard" : 0
      		2048 : "Not in Deathmatch" : 1
      	]
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"spawnflags",
               mdl::PropertyValueTypes::Flags{
                 {
                   {256, "Not on Easy", ""},
                   {512, "Not on Normal", ""},
                   {1024, "Not on Hard", ""},
                   {2048, "Not in Deathmatch", ""},
                 },
                 512 | 2048},
               "",
               ""},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }

    SECTION("parseColorPropertyDefinition")
    {
      constexpr auto entityTemplate = R"(
      @PointClass = info_colors : "Entity with different color types"
      [
        {}
      ])";

      using T = std::tuple<std::string, mdl::PropertyDefinition>;

      const auto [str, expectedPropertyDefinition] = GENERATE(
        values<T>({
          {R"(test1(color1) : "Property 1" : "1.0 0.5 0.0" : "Longer description 1")",
           {"test1",
            mdl::PropertyValueTypes::Color<RgbF>{"1.0 0.5 0.0"},
            "Property 1",
            "Longer description 1"}},

          {R"(test2(color255) : "Property 2" : "255 127 0" : "Longer description 2")",
           {"test2",
            mdl::PropertyValueTypes::Color<RgbB>{"255 127 0"},
            "Property 2",
            "Longer description 2"}},

          {R"(test3(color1) : "Property 3" : "0.2 0.3 0.4 1000" : "Longer description 3")",
           {"test3",
            mdl::PropertyValueTypes::Color<RgbF>{"0.2 0.3 0.4 1000"},
            "Property 3",
            "Longer description 3"}},

          {R"(test4(color255) : "Property 4" : "10 20 30 1000" : "Longer description 4")",
           {"test4",
            mdl::PropertyValueTypes::Color<RgbB>{"10 20 30 1000"},
            "Property 4",
            "Longer description 4"}},

          {R"(test5(color255) : "Property 5" : : "Longer description 5")",
           {"test5",
            mdl::PropertyValueTypes::Color<RgbB>{},
            "Property 5",
            "Longer description 5"}},
        }));

      CAPTURE(str);

      const auto file = fmt::format(entityTemplate, str);

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {"info_colors",
           RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
           "Entity with different color types",
           {expectedPropertyDefinition},
           mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}}}});
    }

    SECTION("parsePropertyDefinitionForInputOutputProperty")
    {
      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
         input(string) : "A property named input" : : ""
         output(string) : "A property named output" : : ""
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"input", mdl::PropertyValueTypes::String{}, "A property named input", ""},
              {"output",
               mdl::PropertyValueTypes::String{},
               "A property named output",
               ""},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }


    SECTION("parseIOProperties")
    {
      using mdl::PropertyValueTypes::IOParameterType;

      const auto file = R"(
      @PointClass = info_notnull : "Wildcard entity" // I love you
      [
         input voidInput(void) : "An input property without parameters"
         input stringInput(string)
         input integerInput(integer)
         input floatInput(float)
         input boolInput(bool)
         input booleanInput(boolean)
         input ehandleInput(ehandle)

         output voidOutput(void) : "An output property without parameters"
         output stringOutput(string)
         output integerOutput(integer)
         output floatOutput(float)
         output boolOutput(bool)
         output booleanOutput(boolean)
         output ehandleOutput(ehandle)
      ]
  )";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "info_notnull",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Wildcard entity",
            {
              {"voidInput",
               mdl::PropertyValueTypes::Input{IOParameterType::Void},
               "An input property without parameters",
               ""},
              {"stringInput",
               mdl::PropertyValueTypes::Input{IOParameterType::String},
               "",
               ""},
              {"integerInput",
               mdl::PropertyValueTypes::Input{IOParameterType::Integer},
               "",
               ""},
              {"floatInput",
               mdl::PropertyValueTypes::Input{IOParameterType::Float},
               "",
               ""},
              {"boolInput",
               mdl::PropertyValueTypes::Input{IOParameterType::Boolean},
               "",
               ""},
              {"booleanInput",
               mdl::PropertyValueTypes::Input{IOParameterType::Boolean},
               "",
               ""},
              {"ehandleInput",
               mdl::PropertyValueTypes::Input{IOParameterType::EHandle},
               "",
               ""},

              {"voidOutput",
               mdl::PropertyValueTypes::Output{IOParameterType::Void},
               "An output property without parameters",
               ""},
              {"stringOutput",
               mdl::PropertyValueTypes::Output{IOParameterType::String},
               "",
               ""},
              {"integerOutput",
               mdl::PropertyValueTypes::Output{IOParameterType::Integer},
               "",
               ""},
              {"floatOutput",
               mdl::PropertyValueTypes::Output{IOParameterType::Float},
               "",
               ""},
              {"boolOutput",
               mdl::PropertyValueTypes::Output{IOParameterType::Boolean},
               "",
               ""},
              {"booleanOutput",
               mdl::PropertyValueTypes::Output{IOParameterType::Boolean},
               "",
               ""},
              {"ehandleOutput",
               mdl::PropertyValueTypes::Output{IOParameterType::EHandle},
               "",
               ""},
            },
            mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
          },
        });
    }
  }

  SECTION("model definitions")
  {
    static const auto FgdModelDefinitionTemplate =
      R"(@PointClass model(${MODEL}) = item_shells : "Shells" [])";

    using mdl::getModelSpecification;

    SECTION("parseLegacyStaticModelDefinition")
    {
      static const auto ModelDefinition =
        R"(":maps/b_shell0.bsp", ":maps/b_shell1.bsp" spawnflags = 1)";

      // CHECK(
      //   getModelSpecification<FgdParser>(ModelDefinition, FgdModelDefinitionTemplate)
      //   == mdl::ModelSpecification{"maps/b_shell0.bsp", 0, 0});
      CHECK(
        getModelSpecification<FgdParser>(
          ModelDefinition, FgdModelDefinitionTemplate, "{ 'spawnflags': 1 }")
        == mdl::ModelSpecification{"maps/b_shell1.bsp", 0, 0});
    }

    SECTION("parseLegacyDynamicModelDefinition")
    {
      static const auto ModelDefinition =
        R"(pathKey = "model" skinKey = "skin" frameKey = "frame")";

      CHECK(
        getModelSpecification<FgdParser>(
          ModelDefinition, FgdModelDefinitionTemplate, "{ 'model': 'maps/b_shell1.bsp' }")
        == mdl::ModelSpecification{"maps/b_shell1.bsp", 0, 0});
      CHECK(
        getModelSpecification<FgdParser>(
          ModelDefinition,
          FgdModelDefinitionTemplate,
          "{ 'model': 'maps/b_shell1.bsp', 'skin': 1, 'frame': 2 }")
        == mdl::ModelSpecification{"maps/b_shell1.bsp", 1, 2});
    }

    SECTION("parseELModelDefinition")
    {
      static const auto ModelDefinition =
        R"({{ spawnflags == 1 -> 'maps/b_shell1.bsp', 'maps/b_shell0.bsp' }})";

      CHECK(
        getModelSpecification<FgdParser>(ModelDefinition, FgdModelDefinitionTemplate)
        == mdl::ModelSpecification{"maps/b_shell0.bsp", 0, 0});
    }

    SECTION("parseLegacyModelWithParseError")
    {
      const auto file = R"(
  @PointClass base(Monster) size(-16 -16 -24, 16 16 40) model(":progs/polyp.mdl" 0 153, ":progs/polyp.mdl" startonground = "1") = monster_polyp: "Polyp"
  [
    startonground(choices) : "Starting pose" : 0 =
    [
      0 : "Flying"
      1 : "On ground"
    ]
  ])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "monster_polyp",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Polyp",
            {
              {"startonground",
               mdl::PropertyValueTypes::Choice{
                 {
                   {"0", "Flying"},
                   {"1", "On ground"},
                 },
                 "0"},
               "Starting pose",
               ""},
            },
            mdl::PointEntityDefinition{
              {{-16, -16, -24}, {16, 16, 40}},
              mdl::ModelDefinition{el::swt({
                el::cs(
                  el::eq(el::var("startonground"), el::lit("1")),
                  el::lit(el::MapType{{"path", el::Value{":progs/polyp.mdl"}}})),
                el::lit(
                  el::MapType{
                    {"path", el::Value{":progs/polyp.mdl"}},
                    {"frame", el::Value{153}},
                    {"skin", el::Value{0}},
                  }),
              })},
              {}},
          },
        });
    }
  }

  SECTION("decal and sprite definitions")
  {
    static const auto FgdDecalDefinitionTemplate =
      R"(@PointClass decal(${DECAL}) = infodecal : "Decal" [])";

    using mdl::assertDecalDefinition;

    SECTION("parseEmptyDecalDefinition")
    {
      static const auto DecalDefinition = "";

      assertDecalDefinition<FgdParser>(
        mdl::DecalSpecification{"decal1"},
        DecalDefinition,
        FgdDecalDefinitionTemplate,
        R"({ "texture": "decal1" })");
    }

    SECTION("parseELDecalDefinition")
    {
      static const auto DecalDefinition = R"({ texture: "decal1" })";

      assertDecalDefinition<FgdParser>(
        mdl::DecalSpecification{"decal1"}, DecalDefinition, FgdDecalDefinitionTemplate);
    }

    static const auto FgdSpriteDefinitionTemplate =
      R"(@PointClass sprite(${MODEL}) = env_sprite : "Sprite" [])";

    SECTION("parseEmptySpriteDefinition")
    {
      static const auto SpriteDefinition = "";

      CHECK(
        getModelSpecification<FgdParser>(
          SpriteDefinition, FgdSpriteDefinitionTemplate, R"({ "model": "spritex.spr" })")
        == mdl::ModelSpecification{"spritex.spr", 0, 0});
    }

    SECTION("parseELSpriteDefinition")
    {
      static const auto SpriteDefinition = R"({ path: "spritex.spr" })";

      CHECK(
        getModelSpecification<FgdParser>(SpriteDefinition, FgdSpriteDefinitionTemplate)
        == mdl::ModelSpecification{"spritex.spr", 0, 0});
    }

    SECTION("parseELSpriteDefinitionShorthand")
    {
      static const auto SpriteDefinition = R"("spritex.spr")";

      CHECK(
        getModelSpecification<FgdParser>(SpriteDefinition, FgdSpriteDefinitionTemplate)
        == mdl::ModelSpecification{"spritex.spr", 0, 0});
    }
  }

  SECTION("bounds and model errors")
  {
    SECTION("parseMissingBounds")
    {
      const auto file = R"(
  @PointClass model({"path" : ":progs/goddess-statue.mdl" }) =
  decor_goddess_statue : "Goddess Statue" []
  )";

      auto parser = FgdParser(file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f});
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "decor_goddess_statue",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Goddess Statue",
            {},
            mdl::PointEntityDefinition{
              {{-8, -8, -8}, {8, 8, 8}},
              mdl::ModelDefinition{
                el::lit(el::MapType{{"path", el::Value{":progs/goddess-statue.mdl"}}})},
              {}},
          },
        });
    }

    SECTION("parseInvalidBounds")
    {
      const auto file = R"(
  @PointClass size(32 32 0, -32 -32 256) model({"path" : ":progs/goddess-statue.mdl" }) =
  decor_goddess_statue : "Goddess Statue" [])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(
        parser.parseDefinitions(status)
        == std::vector<mdl::EntityDefinition>{
          {
            "decor_goddess_statue",
            RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
            "Goddess Statue",
            {},
            mdl::PointEntityDefinition{
              {{-32, -32, 0}, {32, 32, 256}},
              mdl::ModelDefinition{
                el::lit(el::MapType{{"path", el::Value{":progs/goddess-statue.mdl"}}})},
              {}},
          },
        });
    }

    SECTION("parseInvalidModel")
    {
      const auto file = R"(@PointClass
  size(-16 -16 -24, 16 16 40)
  model({1}) =
  decor_goddess_statue : "Goddess Statue" [])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(parser.parseDefinitions(status).is_error());
    }

    SECTION("parseErrorAfterModel")
    {
      const auto file = R"(@PointClass
  size(-16 -16 -24, 16 16 40)
  model({"path"
         : ":progs/goddess-statue.mdl" }) = decor_goddess_statue ; "Goddess Statue" [])";

      auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
      auto status = TestParserStatus{};

      CHECK(parser.parseDefinitions(status).is_error());
    }
  }

  SECTION("includes")
  {
    SECTION("parseInclude")
    {
      const auto path = getFixtureRoot() / "test/mdl/FgdParser/parseInclude/host.fgd";
      auto file = fs::Disk::openFile(path) | kdl::value();
      auto reader = file->reader().buffer();

      auto parser = FgdParser{reader.stringView(), RgbaF{1.0f, 1.0f, 1.0f, 1.0f}, path};

      auto status = TestParserStatus{};
      auto defs = parser.parseDefinitions(status);
      REQUIRE(defs);
      CHECK(defs.value().size() == 2u);
      CHECK(kdl::ranges::contains(defs.value(), "worldspawn", &EntityDefinition::name));
      CHECK(
        kdl::ranges::contains(
          defs.value(), "info_player_start", &EntityDefinition::name));
    }

    SECTION("parseNestedInclude")
    {
      const auto path =
        getFixtureRoot() / "test/mdl/FgdParser/parseNestedInclude/host.fgd";
      auto file = fs::Disk::openFile(path) | kdl::value();
      auto reader = file->reader().buffer();

      auto parser = FgdParser{reader.stringView(), RgbaF{1.0f, 1.0f, 1.0f, 1.0f}, path};

      auto status = TestParserStatus{};
      auto defs = parser.parseDefinitions(status);
      REQUIRE(defs);
      CHECK(defs.value().size() == 3u);
      CHECK(kdl::ranges::contains(defs.value(), "worldspawn", &EntityDefinition::name));
      CHECK(
        kdl::ranges::contains(
          defs.value(), "info_player_start", &EntityDefinition::name));
      CHECK(
        kdl::ranges::contains(defs.value(), "info_player_coop", &EntityDefinition::name));
    }

    SECTION("parseRecursiveInclude")
    {
      const auto path =
        getFixtureRoot() / "test/mdl/FgdParser/parseRecursiveInclude/host.fgd";
      auto file = fs::Disk::openFile(path) | kdl::value();
      auto reader = file->reader().buffer();

      auto parser = FgdParser{reader.stringView(), RgbaF{1.0f, 1.0f, 1.0f, 1.0f}, path};

      auto status = TestParserStatus{};
      auto defs = parser.parseDefinitions(status);
      REQUIRE(defs);
      CHECK(defs.value().size() == 1u);
      CHECK(kdl::ranges::contains(defs.value(), "worldspawn", &EntityDefinition::name));
    }

    SECTION("parseIncludeEscapingBasePath")
    {
      // an @include path that escapes the host file's directory (here, into the sibling
      // parseInclude fixture directory) must be rejected rather than followed - the host
      // file's own definitions should still parse
      const auto path =
        getFixtureRoot() / "test/mdl/FgdParser/parseIncludeEscapingBasePath/host.fgd";
      auto file = fs::Disk::openFile(path) | kdl::value();
      auto reader = file->reader().buffer();

      auto parser = FgdParser{reader.stringView(), RgbaF{1.0f, 1.0f, 1.0f, 1.0f}, path};

      auto status = TestParserStatus{};
      auto defs = parser.parseDefinitions(status);
      REQUIRE(defs);
      CHECK(defs.value().size() == 1u);
      CHECK(kdl::ranges::contains(defs.value(), "worldspawn", &EntityDefinition::name));
      CHECK(!kdl::ranges::contains(
        defs.value(), "info_player_start", &EntityDefinition::name));
    }
  }

  SECTION("parseStringContinuations")
  {
    const auto file = R"(
@PointClass = cont_description :
  "This is an example description for"+
  " this example entity. It will appear"+
  " in the help dialog for this entity"
[])";

    auto parser = FgdParser{file, RgbaF{1.0f, 1.0f, 1.0f, 1.0f}};
    auto status = TestParserStatus{};


    CHECK(
      parser.parseDefinitions(status)
      == std::vector<mdl::EntityDefinition>{
        {
          "cont_description",
          RgbaF{1.0f, 1.0f, 1.0f, 1.0f},
          R"(This is an example description for this example entity. It will appear in the help dialog for this entity)",
          {},
          mdl::PointEntityDefinition{{{-8, -8, -8}, {8, 8, 8}}, {}, {}},
        },
      });
  }
}

} // namespace tb::mdl
