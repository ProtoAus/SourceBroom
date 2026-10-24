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

#include "mdl/Brush.h"
#include "mdl/BrushNode.h"
#include "mdl/Node.h"
#include "mdl/VmfDocument.h"
#include "mdl/VmfScene.h"
#include "mdl/WorldNode.h"

#include "vm/bbox.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

namespace fs = std::filesystem;

void usage()
{
  std::cout
    << "SourceBroomVmfCheck [--probe-edit] [--scene] [--world-bounds extent] "
       "[--] <file.vmf|directory>...\n"
       "Read-only native syntax/byte-preservation checker.\n"
       "--probe-edit tests a surgical value edit in memory only.\n"
       "--scene loads the production plain-brush scene (default bounds +/-131072)\n"
       "and checks its unchanged serialization; not a BSP/gameplay validator.\n"
       "--world-bounds sets an explicit preview extent, not a Source branch contract.\n";
}

bool isVmf(const fs::path& path)
{
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), [](const auto c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  });
  return extension == ".vmf";
}

void probeEdit(const tb::mdl::VmfDocument& document)
{
  const auto& entries = document.entries();
  const auto property =
    std::find_if(entries.begin(), entries.end(), [](const auto& entry) {
      return entry.value.has_value();
    });
  if (property == entries.end())
  {
    throw std::runtime_error{"Edit probe needs at least one property"};
  }
  const auto id = static_cast<size_t>(std::distance(entries.begin(), property));
  const auto replacement = document.tokenText(*property->value) == "\"sourcebroom-probe\""
                             ? "\"sourcebroom-probe-2\""
                             : "\"sourcebroom-probe\"";
  const auto edits = std::array{tb::mdl::VmfValueEdit{id, replacement}};
  const auto edited = document.serialize(edits);
  auto expected = document.source();
  expected.replace(property->value->offset, property->value->length, replacement);
  if (edited != expected || edited == document.source())
  {
    throw std::runtime_error{"Surgical edit changed the wrong bytes or did not act"};
  }
  const auto reparsed = tb::mdl::VmfDocument::parse(edited);
  if (
    reparsed.entries().size() != entries.size() || reparsed.roots() != document.roots()
    || !reparsed.entries()[id].value
    || reparsed.tokenText(*reparsed.entries()[id].value) != replacement)
  {
    throw std::runtime_error{"Edited document did not reparse with the expected value"};
  }
  for (auto i = size_t{0}; i < entries.size(); ++i)
  {
    const auto& before = entries[i];
    const auto& after = reparsed.entries()[i];
    if (
      before.parent != after.parent || before.children != after.children
      || document.tokenText(before.key) != reparsed.tokenText(after.key)
      || before.isBlock() != after.isBlock())
    {
      throw std::runtime_error{"Edit probe changed document structure"};
    }
  }
}

std::array<size_t, 3> probeScene(
  const tb::mdl::VmfDocument& document, const double worldExtent)
{
  const auto scene =
    tb::mdl::VmfScene::read(document.source(), vm::bbox3d{-worldExtent, worldExtent});
  if (scene.serialize(scene.world()) != document.source())
  {
    throw std::runtime_error{"Unedited native scene differs from input"};
  }
  auto counts = std::array<size_t, 3>{};
  const auto count = [&](const auto& self, const tb::mdl::Node& node) -> void {
    ++counts[0];
    if (const auto* brush = dynamic_cast<const tb::mdl::BrushNode*>(&node))
    {
      ++counts[1];
      counts[2] += brush->brush().faceCount();
    }
    for (const auto* child : node.children())
    {
      self(self, *child);
    }
  };
  count(count, scene.world());
  return counts;
}

size_t checkFile(
  const fs::path& path, const bool probe, const bool sceneCheck, const double worldExtent)
{
  auto stream = std::ifstream{path, std::ios::binary};
  if (!stream)
  {
    throw std::runtime_error{"Cannot open input"};
  }
  auto source =
    std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  if (stream.bad())
  {
    throw std::runtime_error{"Failed reading input"};
  }
  const auto document = tb::mdl::VmfDocument::parse(std::move(source));
  if (document.serialize() != document.source())
  {
    throw std::runtime_error{"Unedited serialization differs from input"};
  }

  auto worlds = size_t{0};
  for (const auto id : document.roots())
  {
    const auto& entry = document.entries()[id];
    if (entry.isBlock() && document.tokenContents(entry.key) == "world")
    {
      ++worlds;
    }
  }
  if (worlds != 1)
  {
    throw std::runtime_error{"Expected exactly one top-level world block"};
  }
  if (probe)
  {
    probeEdit(document);
  }

  const auto sceneCounts =
    sceneCheck ? probeScene(document, worldExtent) : std::array<size_t, 3>{};
  auto blocks = size_t{0};
  auto solids = size_t{0};
  auto sides = size_t{0};
  auto displacements = size_t{0};
  auto outputs = size_t{0};
  for (const auto& entry : document.entries())
  {
    if (entry.isBlock())
    {
      ++blocks;
      const auto key = document.tokenContents(entry.key);
      solids += key == "solid" ? 1u : 0u;
      sides += key == "side" ? 1u : 0u;
      displacements += key == "dispinfo" ? 1u : 0u;
    }
    else if (
      entry.parent
      && document.tokenContents(document.entries()[*entry.parent].key) == "connections")
    {
      ++outputs;
    }
  }
  std::cout << "OK\tbytes=" << document.source().size()
            << "\tentries=" << document.entries().size() << "\tblocks=" << blocks
            << "\tsolids=" << solids << "\tsides=" << sides
            << "\tdisplacements=" << displacements << "\toutputs=" << outputs
            << "\tedit=" << (probe ? "pass" : "not-run")
            << "\tscene=" << (sceneCheck ? "pass" : "not-run")
            << "\tscene_nodes=" << sceneCounts[0] << "\tscene_brushes=" << sceneCounts[1]
            << "\tscene_faces=" << sceneCounts[2] << '\t' << std::quoted(path.string())
            << '\n';
  return document.source().size();
}

} // namespace

int main(const int argc, char* argv[])
{
  auto probe = false;
  auto sceneCheck = false;
  auto worldExtent = tb::mdl::VmfScene::PreviewWorldExtent;
  auto customWorldExtent = false;
  auto options = true;
  auto inputs = std::vector<fs::path>{};
  for (auto i = 1; i < argc; ++i)
  {
    const auto arg = std::string{argv[i]};
    if (options && arg == "--help")
    {
      usage();
      return 0;
    }
    if (options && arg == "--probe-edit")
    {
      probe = true;
    }
    else if (options && arg == "--scene")
    {
      sceneCheck = true;
    }
    else if (options && arg == "--world-bounds")
    {
      if (++i >= argc)
      {
        std::cerr << "--world-bounds requires a finite extent in (0, 1048576]\n";
        return 2;
      }
      const auto value = std::string{argv[i]};
      auto consumed = size_t{0};
      try
      {
        worldExtent = std::stod(value, &consumed);
      }
      catch (const std::exception&)
      {
        consumed = std::string::npos;
      }
      if (
        consumed != value.size() || !std::isfinite(worldExtent) || worldExtent <= 0.0
        || worldExtent > 1048576.0)
      {
        std::cerr << "--world-bounds requires a finite extent in (0, 1048576]\n";
        return 2;
      }
      customWorldExtent = true;
    }
    else if (options && arg == "--")
    {
      options = false;
    }
    else if (options && arg.starts_with('-'))
    {
      std::cerr << "Unknown option: " << arg << '\n';
      return 2;
    }
    else
    {
      inputs.emplace_back(arg);
    }
  }
  if (customWorldExtent && !sceneCheck)
  {
    std::cerr << "--world-bounds requires --scene\n";
    return 2;
  }
  if (inputs.empty())
  {
    usage();
    return 2;
  }

  auto files = std::vector<fs::path>{};
  try
  {
    for (const auto& input : inputs)
    {
      if (fs::is_directory(input))
      {
        for (const auto& item : fs::recursive_directory_iterator{input})
        {
          if (item.is_regular_file() && isVmf(item.path()))
          {
            files.push_back(item.path());
          }
        }
      }
      else if (fs::is_regular_file(input) && isVmf(input))
      {
        files.push_back(input);
      }
      else
      {
        throw std::runtime_error{"Input must be a VMF file or directory"};
      }
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    if (files.empty())
    {
      throw std::runtime_error{"No VMF files found"};
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Input error: " << e.what() << '\n';
    return 2;
  }

  auto failed = size_t{0};
  auto bytes = size_t{0};
  for (const auto& file : files)
  {
    try
    {
      bytes += checkFile(file, probe, sceneCheck, worldExtent);
    }
    catch (const std::exception& e)
    {
      ++failed;
      std::cerr << "FAIL\t" << std::quoted(file.string()) << '\t' << e.what() << '\n';
    }
  }
  std::cout << "SUMMARY\tfiles=" << files.size() << "\tpassed=" << files.size() - failed
            << "\tfailed=" << failed << "\tpassed_bytes=" << bytes << '\n';
  return failed == 0 ? 0 : 1;
}
