/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "fs/TestEnvironment.h"
#include "mdl/VmfFile.h"

#include <fstream>
#include <iterator>
#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tb::mdl
{
namespace
{
std::string loadBytes(const std::filesystem::path& path)
{
  auto input = std::ifstream{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
} // namespace
TEST_CASE("VmfFile atomic saves preserve originals and backups", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "test ramp.vmf";
  const auto original = std::string{"\xef\xbb\xbfworld { \"id\" \"1\" }\r\n"};
  const auto changed = std::string{"world { \"id\" \"2\" }\n"};
  REQUIRE(saveVmfFile(path, original));
  REQUIRE(loadBytes(path) == original);
  CHECK_FALSE(saveVmfFile(path, changed));
  CHECK(loadBytes(path) == original);

  const auto saved = saveVmfFile(path, changed, original);
  REQUIRE(saved);
  REQUIRE(saved.value().backupPath);
  CHECK(loadBytes(path) == changed);
  CHECK(loadBytes(*saved.value().backupPath) == original);
  CHECK_FALSE(saveVmfFile(path, "wrong", original));
  CHECK(loadBytes(path) == changed);
  CHECK(loadBytes(*saved.value().backupPath) == original);

  const auto again = saveVmfFile(path, original, changed);
  REQUIRE(again);
  REQUIRE(again.value().backupPath);
  CHECK(again.value().backupPath != saved.value().backupPath);
  CHECK(loadBytes(*again.value().backupPath) == changed);
  CHECK(loadBytes(*saved.value().backupPath) == original);
}

TEST_CASE("VmfFile refuses unsafe destinations and external changes", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "external.vmf";
  env.createFile(path, "original");
  env.createFile(path, "external edit");
  CHECK_FALSE(saveVmfFile(path, "editor edit", "original"));
  CHECK(env.loadFile(path) == "external edit");
  CHECK_FALSE(saveVmfFile("relative.vmf", "anything"));
  CHECK_FALSE(saveVmfFile(env.dir(), "anything", std::nullopt, true));
  CHECK_FALSE(saveVmfFile(env.dir() / "missing" / "file.vmf", "anything"));
  CHECK_FALSE(saveVmfFile(env.dir() / "deleted.vmf", "anything", "original"));
  CHECK(env.directoryContents(env.dir()).size() == 1);
}

TEST_CASE("VmfFile unchanged save does not create backups", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "same.vmf";
  env.createFile(path, "same bytes");
  const auto saved = saveVmfFile(path, "same bytes", "same bytes");
  REQUIRE(saved);
  CHECK_FALSE(saved.value().backupPath);
  CHECK(env.directoryContents(env.dir()).size() == 1);
}

#ifdef _WIN32
TEST_CASE("VmfFile long leaf saves", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto parentSize = env.dir().native().size();
  REQUIRE(parentSize < 180);
  const auto stemSize = size_t{248} - parentSize - 1 - 4;
  const auto path = env.dir() / (std::string(stemSize, 'm') + ".vmf");
  const auto freshPath = env.dir() / (std::string(stemSize, 'n') + ".vmf");
  REQUIRE(path.native().size() == 248);
  const auto original = std::string{"world { id 1 }\r\n"};
  const auto changed = std::string{"world { id 2 }\n"};
  {
    auto stream = std::ofstream{path, std::ios::binary};
    stream << original;
  }
  REQUIRE(loadBytes(path) == original);
  const auto saved = saveVmfFile(path, changed, original);
  INFO((saved ? std::string{} : std::get<Error>(saved.error()).msg));
  REQUIRE(saved);
  REQUIRE(saved.value().backupPath);
  CHECK(saved.value().backupPath->parent_path() == env.dir());
  CHECK(loadBytes(path) == changed);
  CHECK(loadBytes(*saved.value().backupPath) == original);
  REQUIRE(saveVmfFile(freshPath, original));
  CHECK(loadBytes(freshPath) == original);
  CHECK(loadBytes(path) == changed);
  CHECK_FALSE(saveVmfFile(path, "unapproved", original));
  CHECK(loadBytes(path) == changed);
  CHECK(loadBytes(*saved.value().backupPath) == original);
}

TEST_CASE("VmfFile failed atomic promotion retains original and recovery file", "[vmf]")
{
  auto env = fs::TestEnvironment{};
  const auto path = env.dir() / "locked.vmf";
  env.createFile(path, "original");
  const auto handle = CreateFileW(
    path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  REQUIRE(handle != INVALID_HANDLE_VALUE);
  const auto close = kdl::invoke_later{[&]() { CloseHandle(handle); }};
  const auto saved = saveVmfFile(path, "changed", "original");
  CHECK_FALSE(saved);
  CHECK(env.loadFile(path) == "original");
  CHECK(env.directoryContents(env.dir()).size() >= 2);
}
#endif
} // namespace tb::mdl
