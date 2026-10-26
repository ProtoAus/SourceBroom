/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/VmfFile.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace tb::mdl
{
namespace
{
std::filesystem::path uniqueSibling(const std::filesystem::path& path, const char* suffix)
{
  static auto sequence = std::atomic<unsigned long long>{0};
  const auto clock = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
  const auto process = GetCurrentProcessId();
#else
  const auto process = ::getpid();
#endif
  // A valid destination leaf can already approach the platform's path limit.
  // Independent leaves also need a process ID to separate concurrent editors.
  return path.parent_path()
         / (".sourcebroom-" + std::to_string(process) + "-" + std::to_string(clock) + "-"
            + std::to_string(sequence.fetch_add(1)) + suffix);
}

struct PendingFile
{
  std::filesystem::path path;
  bool removeOnExit = false;
  ~PendingFile()
  {
    if (removeOnExit)
    {
      auto ec = std::error_code{};
      std::filesystem::remove(path, ec);
    }
  }
};

Result<std::string> readBytes(const std::filesystem::path& path)
{
  auto stream = std::ifstream{path, std::ios::binary};
  if (!stream)
  {
    return Error{"Could not read existing VMF; destination was not changed"};
  }
  auto bytes =
    std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  if (stream.bad())
  {
    return Error{"Failed reading existing VMF; destination was not changed"};
  }
  return bytes;
}

Result<void> writeTemporary(PendingFile& file, std::string_view bytes)
{
#ifdef _WIN32
  const auto handle = CreateFileW(
    file.path.c_str(),
    GENERIC_WRITE,
    0,
    nullptr,
    CREATE_NEW,
    FILE_ATTRIBUTE_NORMAL,
    nullptr);
  if (handle == INVALID_HANDLE_VALUE)
  {
    return Error{"Cannot exclusively create VMF temporary file"};
  }
  file.removeOnExit = true;
  auto success = true;
  while (!bytes.empty())
  {
    auto written = DWORD{0};
    const auto count = static_cast<DWORD>((std::min)(bytes.size(), size_t{1024 * 1024}));
    if (!WriteFile(handle, bytes.data(), count, &written, nullptr) || written == 0)
    {
      success = false;
      break;
    }
    bytes.remove_prefix(written);
  }
  success = FlushFileBuffers(handle) != 0 && success;
  success = CloseHandle(handle) != 0 && success;
#else
  const auto handle = ::open(file.path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (handle < 0)
  {
    return Error{"Cannot exclusively create VMF temporary file"};
  }
  file.removeOnExit = true;
  auto success = true;
  while (!bytes.empty())
  {
    const auto written = ::write(handle, bytes.data(), bytes.size());
    if (written <= 0)
    {
      success = false;
      break;
    }
    bytes.remove_prefix(static_cast<size_t>(written));
  }
  success = ::fsync(handle) == 0 && success;
  success = ::close(handle) == 0 && success;
#endif
  return success ? Result<void>{}
                 : Result<void>{Error{"Failed writing/flushing VMF temporary file"}};
}
} // namespace

Result<VmfSaveResult> saveVmfFile(
  const std::filesystem::path& path,
  const std::string_view bytes,
  const std::optional<std::string_view> expectedOriginal,
  const bool allowOverwrite)
{
  if (!path.is_absolute())
  {
    return Error{"VMF save requires an absolute path"};
  }
  auto ec = std::error_code{};
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec && ec != std::errc::no_such_file_or_directory)
  {
    return Error{"Cannot inspect VMF destination: " + ec.message()};
  }
  const auto exists = std::filesystem::exists(status);
  if (exists && !std::filesystem::is_regular_file(status))
  {
    return Error{"VMF destination must be a regular file, not a directory or link"};
  }
  if (expectedOriginal && !exists)
  {
    return Error{"Loaded VMF was removed externally; use Save As instead"};
  }
  if (exists && !expectedOriginal && !allowOverwrite)
  {
    return Error{"Destination exists; explicit overwrite approval is required"};
  }
  auto original = std::string{};
  if (exists)
  {
    auto read = readBytes(path);
    if (!read)
    {
      return std::get<Error>(read.error());
    }
    original = std::move(read).value();
    if (expectedOriginal && original != *expectedOriginal)
    {
      return Error{
        "VMF changed externally; refusing to overwrite. Use Save As or reload"};
    }
    if (original == bytes)
    {
      return VmfSaveResult{};
    }
  }

  auto temporary = PendingFile{uniqueSibling(path, ".tmp")};
  auto write = writeTemporary(temporary, bytes);
  if (!write)
  {
    return std::get<Error>(write.error());
  }
  if (exists)
  {
    auto current = readBytes(path);
    if (!current || current.value() != original)
    {
      return Error{"VMF changed while saving; refusing to overwrite"};
    }
  }
  const auto backup = exists ? std::optional{uniqueSibling(path, ".bak")} : std::nullopt;
#ifdef _WIN32
  const auto success =
    exists ? ReplaceFileW(
               path.c_str(), temporary.path.c_str(), backup->c_str(), 0, nullptr, nullptr)
           : MoveFileExW(temporary.path.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH);
  if (!success)
  {
    const auto code = GetLastError();
    // A failed ReplaceFile can move the original to the backup. Retain all
    // recovery candidates rather than guessing which path is disposable.
    temporary.removeOnExit = false;
    return Error{
      "Atomic VMF promotion failed (" + std::to_string(code) + "). Recovery temporary: "
      + temporary.path.string() + (backup ? "; backup: " + backup->string() : "")};
  }
#else
  if (backup)
  {
    std::filesystem::copy_file(path, *backup, std::filesystem::copy_options::none, ec);
    if (ec)
    {
      return Error{"Could not preserve VMF backup: " + ec.message()};
    }
    std::filesystem::rename(temporary.path, path, ec);
  }
  else
  {
    // A hard link gives atomic no-replace publication on the same filesystem.
    std::filesystem::create_hard_link(temporary.path, path, ec);
    if (!ec)
    {
      std::filesystem::remove(temporary.path, ec);
    }
  }
  if (ec)
  {
    temporary.removeOnExit = false;
    return Error{
      "Atomic VMF promotion failed: " + ec.message()
      + "; recovery temporary: " + temporary.path.string()};
  }
#endif
  temporary.removeOnExit = false;
  return VmfSaveResult{backup};
}
} // namespace tb::mdl
