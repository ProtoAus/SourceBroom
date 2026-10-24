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

#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tb::mdl
{

using VmfEntryId = size_t;

/** A complete lexical token, including quotes when quoted is true. */
struct VmfToken
{
  size_t offset;
  size_t length;
  bool quoted;
};

/** Entries remain in source order; repeated names are never collapsed. */
struct VmfEntry
{
  VmfToken key;
  std::optional<VmfToken> value;
  std::optional<VmfEntryId> parent;
  std::vector<VmfEntryId> children;

  bool isBlock() const { return !value.has_value(); }
};

struct VmfValueEdit
{
  VmfEntryId entry;
  /** Exactly one raw quoted VMF value token, including the outer quotes. */
  std::string token;
};

/**
 * Lossless VMF syntax document. Does not validate geometry or game semantics.
 * Numeric spans survive copying/moving the document; returned views do not.
 */
class VmfDocument
{
private:
  std::string m_source;
  std::vector<VmfEntry> m_entries;
  std::vector<VmfEntryId> m_roots;

  VmfDocument() = default;

public:
  /** Throws ParserException on malformed syntax, binary input, or excessive depth. */
  static VmfDocument parse(std::string source);

  const std::string& source() const;
  const std::vector<VmfEntry>& entries() const;
  const std::vector<VmfEntryId>& roots() const;

  std::string_view tokenText(const VmfToken& token) const;
  /** Removes outer quotes only; deliberately does not decode escape sequences. */
  std::string_view tokenContents(const VmfToken& token) const;

  /**
   * Copies original bytes, replacing only explicitly selected value tokens.
   * Rejects invalid IDs, block targets, duplicate edits, and non-quoted replacements
   * with std::invalid_argument. Does not write files or change this document.
   */
  std::string serialize(std::span<const VmfValueEdit> edits = {}) const;
};

} // namespace tb::mdl
