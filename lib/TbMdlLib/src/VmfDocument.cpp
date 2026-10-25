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

#include "mdl/VmfDocument.h"

#include "base/FileLocation.h"
#include "base/ParserException.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tb::mdl
{
namespace
{

constexpr auto MaxBlockDepth = size_t{256};

enum class TokenType
{
  Text,
  Open,
  Close,
  End,
};

struct Token
{
  TokenType type;
  VmfToken source;
};

bool isWhitespace(const char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

[[noreturn]] void fail(
  const std::string_view source, const size_t offset, const std::string& message)
{
  auto line = size_t{1};
  auto column = size_t{1};
  for (auto i = size_t{0}; i < offset; ++i)
  {
    if (
      source[i] == '\n'
      || (source[i] == '\r' && (i + 1 == offset || source[i + 1] != '\n')))
    {
      ++line;
      column = 1;
    }
    else
    {
      ++column;
    }
  }
  throw ParserException{FileLocation{line, column}, message};
}

class Lexer
{
private:
  std::string_view m_source;
  size_t m_position = 0;

public:
  explicit Lexer(const std::string_view source)
    : m_source{source}
  {
    if (source.starts_with("\xff\xfe") || source.starts_with("\xfe\xff"))
    {
      fail(source, 0, "UTF-16 VMF input is not supported");
    }
    if (const auto nul = source.find('\0'); nul != std::string_view::npos)
    {
      fail(source, nul, "NUL byte in VMF input");
    }
    if (source.starts_with("\xef\xbb\xbf"))
    {
      m_position = 3;
    }
  }

  Token next()
  {
    skipTrivia();
    const auto start = m_position;
    if (m_position == m_source.size())
    {
      return {TokenType::End, {start, 0, false}};
    }

    switch (m_source[m_position++])
    {
    case '{':
      return {TokenType::Open, {start, 1, false}};
    case '}':
      return {TokenType::Close, {start, 1, false}};
    case '"':
      while (m_position < m_source.size())
      {
        const auto c = m_source[m_position++];
        if (c == '"')
        {
          return {TokenType::Text, {start, m_position - start, true}};
        }
        if (c == '\\' && m_position < m_source.size())
        {
          ++m_position;
        }
      }
      fail(m_source, start, "Unterminated quoted VMF token");
    default:
      while (m_position < m_source.size())
      {
        const auto c = m_source[m_position];
        if (isWhitespace(c) || c == '{' || c == '}' || c == '"' || startsComment())
        {
          break;
        }
        ++m_position;
      }
      return {TokenType::Text, {start, m_position - start, false}};
    }
  }

private:
  bool startsComment() const
  {
    return m_source.substr(m_position, 2) == "//"
           || m_source.substr(m_position, 2) == "/*";
  }

  void skipTrivia()
  {
    while (m_position < m_source.size())
    {
      if (isWhitespace(m_source[m_position]))
      {
        ++m_position;
      }
      else if (m_source.substr(m_position, 2) == "//")
      {
        m_position = m_source.find_first_of("\r\n", m_position + 2);
        if (m_position == std::string_view::npos)
        {
          m_position = m_source.size();
        }
      }
      else if (m_source.substr(m_position, 2) == "/*")
      {
        const auto end = m_source.find("*/", m_position + 2);
        if (end == std::string_view::npos)
        {
          fail(m_source, m_position, "Unterminated VMF block comment");
        }
        m_position = end + 2;
      }
      else
      {
        break;
      }
    }
  }
};

void validateReplacement(const std::string& replacement)
{
  try
  {
    auto lexer = Lexer{replacement};
    const auto token = lexer.next();
    if (
      token.type != TokenType::Text || !token.source.quoted || token.source.offset != 0
      || token.source.length != replacement.size() || lexer.next().type != TokenType::End)
    {
      throw std::invalid_argument{
        "VMF replacement must be exactly one quoted value token"};
    }
  }
  catch (const ParserException& e)
  {
    throw std::invalid_argument{
      std::string{"Invalid VMF replacement token: "} + e.what()};
  }
}

} // namespace

VmfDocument VmfDocument::parse(std::string source)
{
  return parse(std::move(source), std::numeric_limits<size_t>::max());
}

VmfDocument VmfDocument::parse(std::string source, const size_t maxEntries)
{
  auto document = VmfDocument{};
  document.m_source = std::move(source);
  auto lexer = Lexer{document.m_source};
  auto parents = std::vector<VmfEntryId>{};

  while (true)
  {
    const auto key = lexer.next();
    if (key.type == TokenType::End)
    {
      if (!parents.empty())
      {
        fail(document.m_source, key.source.offset, "Missing closing VMF brace");
      }
      return document;
    }
    if (key.type == TokenType::Close)
    {
      if (parents.empty())
      {
        fail(document.m_source, key.source.offset, "Unexpected closing VMF brace");
      }
      document.m_entries[parents.back()].end = key.source.offset + key.source.length;
      parents.pop_back();
      continue;
    }
    if (key.type != TokenType::Text)
    {
      fail(document.m_source, key.source.offset, "Expected VMF key or block name");
    }

    const auto value = lexer.next();
    if (value.type != TokenType::Text && value.type != TokenType::Open)
    {
      fail(document.m_source, value.source.offset, "Expected VMF value or opening brace");
    }
    if (value.type == TokenType::Open && parents.size() >= MaxBlockDepth)
    {
      fail(document.m_source, key.source.offset, "VMF block nesting exceeds 256 levels");
    }

    const auto id = document.m_entries.size();
    if (id >= maxEntries)
    {
      fail(document.m_source, key.source.offset, "VMF exceeds the caller's entry budget");
    }
    const auto parent = parents.empty() ? std::nullopt : std::optional{parents.back()};
    document.m_entries.push_back(
      VmfEntry{
        key.source,
        value.type == TokenType::Text ? std::optional{value.source} : std::nullopt,
        parent,
        {},
        value.source.offset + value.source.length});
    if (parent)
    {
      document.m_entries[*parent].children.push_back(id);
    }
    else
    {
      document.m_roots.push_back(id);
    }
    if (value.type == TokenType::Open)
    {
      parents.push_back(id);
    }
  }
}

const std::string& VmfDocument::source() const
{
  return m_source;
}

const std::vector<VmfEntry>& VmfDocument::entries() const
{
  return m_entries;
}

const std::vector<VmfEntryId>& VmfDocument::roots() const
{
  return m_roots;
}

std::string_view VmfDocument::tokenText(const VmfToken& token) const
{
  if (token.offset > m_source.size() || token.length > m_source.size() - token.offset)
  {
    throw std::out_of_range{"VMF token range is outside the source document"};
  }
  return std::string_view{m_source}.substr(token.offset, token.length);
}

std::string_view VmfDocument::tokenContents(const VmfToken& token) const
{
  const auto text = tokenText(token);
  if (token.quoted)
  {
    if (text.size() < 2 || text.front() != '"' || text.back() != '"')
    {
      throw std::invalid_argument{"Invalid quoted VMF token range"};
    }
    return text.substr(1, text.size() - 2);
  }
  return text;
}

std::string VmfDocument::serializeEntry(
  const VmfEntryId id,
  const std::span<const VmfValueEdit> edits,
  const std::span<const VmfStructuralEdit> structuralEdits) const
{
  if (id >= m_entries.size())
  {
    throw std::invalid_argument{"Invalid VMF subtree entry ID"};
  }
  const auto& entry = m_entries[id];
  const auto fragment =
    VmfDocument::parse(m_source.substr(entry.key.offset, entry.end - entry.key.offset));
  const auto localId = [&](const VmfEntryId original) {
    if (original < id || original - id >= fragment.entries().size())
    {
      throw std::invalid_argument{"VMF subtree edit is outside the selected entry"};
    }
    const auto local = original - id;
    if (
      fragment.entries()[local].key.offset + entry.key.offset
      != m_entries[original].key.offset)
    {
      throw std::invalid_argument{"VMF subtree identity mismatch"};
    }
    return local;
  };
  auto values = std::vector<VmfValueEdit>{};
  for (const auto& edit : edits)
  {
    values.push_back({localId(edit.entry), edit.token});
  }
  auto structure = std::vector<VmfStructuralEdit>{};
  for (const auto& edit : structuralEdits)
  {
    structure.push_back({edit.type, localId(edit.entry), edit.text});
  }
  return fragment.serialize(values, structure);
}

std::string VmfDocument::serialize(
  const std::span<const VmfValueEdit> edits,
  const std::span<const VmfStructuralEdit> structuralEdits) const
{
  struct Replacement
  {
    size_t offset;
    size_t length;
    std::string text;
  };
  auto replacements = std::vector<Replacement>{};
  for (const auto& edit : edits)
  {
    if (edit.entry >= m_entries.size() || !m_entries[edit.entry].value)
    {
      throw std::invalid_argument{"VMF value edit must target an existing property"};
    }
    validateReplacement(edit.token);
    const auto& token = *m_entries[edit.entry].value;
    replacements.push_back({token.offset, token.length, edit.token});
  }
  for (const auto& edit : structuralEdits)
  {
    if (edit.type == VmfStructuralEdit::Type::AppendRoot)
    {
      if (
        edit.entry != VmfStructuralEdit::Root || edit.text.empty()
        || edit.text.starts_with("\xEF\xBB\xBF"))
      {
        throw std::invalid_argument{"Invalid VMF root append"};
      }
      try
      {
        const auto fragment = VmfDocument::parse(edit.text);
        if (fragment.roots().empty())
        {
          throw std::invalid_argument{"Empty VMF root append"};
        }
      }
      catch (const ParserException& e)
      {
        throw std::invalid_argument{"Invalid VMF root append: " + std::string{e.what()}};
      }
      replacements.push_back({m_source.size(), 0, "\n" + edit.text + "\n"});
      continue;
    }
    if (edit.entry >= m_entries.size())
    {
      throw std::invalid_argument{"VMF structural edit has invalid entry ID"};
    }
    const auto& entry = m_entries[edit.entry];
    switch (edit.type)
    {
    case VmfStructuralEdit::Type::Key:
      validateReplacement(edit.text);
      replacements.push_back({entry.key.offset, entry.key.length, edit.text});
      break;
    case VmfStructuralEdit::Type::Remove:
      if (!edit.text.empty())
      {
        throw std::invalid_argument{"VMF removal must have empty replacement"};
      }
      // Removing adjacent quoted tokens can join bare tokens or even form a
      // comment delimiter. Keep every surviving byte and separate the new boundary.
      replacements.push_back(
        {entry.key.offset,
         entry.end - entry.key.offset,
         entry.key.offset > 0 && entry.end < m_source.size()
             && !isWhitespace(m_source[entry.key.offset - 1])
             && !isWhitespace(m_source[entry.end])
           ? " "
           : ""});
      break;
    case VmfStructuralEdit::Type::Append:
      if (!entry.isBlock() || edit.text.empty() || edit.text.starts_with("\xef\xbb\xbf"))
      {
        throw std::invalid_argument{"VMF append requires a block and complete entries"};
      }
      try
      {
        const auto fragment = VmfDocument::parse(edit.text);
        if (fragment.roots().empty())
        {
          throw std::invalid_argument{"VMF append requires complete entries"};
        }
        auto destinationDepth = size_t{1};
        for (auto parent = entry.parent; parent; parent = m_entries[*parent].parent)
        {
          ++destinationDepth;
        }
        auto fragmentDepths = std::vector<size_t>{};
        fragmentDepths.reserve(fragment.entries().size());
        for (const auto& child : fragment.entries())
        {
          const auto depth = (child.parent ? fragmentDepths[*child.parent] : size_t{0})
                             + (child.isBlock() ? size_t{1} : size_t{0});
          if (depth > MaxBlockDepth - destinationDepth)
          {
            throw std::invalid_argument{"VMF append exceeds the document nesting limit"};
          }
          fragmentDepths.push_back(depth);
        }
      }
      catch (const ParserException& e)
      {
        throw std::invalid_argument{std::string{"Invalid VMF append: "} + e.what()};
      }
      // Separate tokens even when the original closing brace has no leading trivia.
      replacements.push_back({entry.end - 1, 0, "\n" + edit.text + "\n"});
      break;
    case VmfStructuralEdit::Type::AppendRoot:
      throw std::invalid_argument{"Unexpected VMF root append dispatch"};
    }
  }
  std::sort(replacements.begin(), replacements.end(), [](const auto& a, const auto& b) {
    return a.offset < b.offset;
  });
  auto result = std::string{};
  auto position = size_t{0};
  auto previousOffset = std::optional<size_t>{};
  for (const auto& replacement : replacements)
  {
    if (replacement.offset < position || previousOffset == replacement.offset)
    {
      throw std::invalid_argument{"Overlapping or duplicate VMF edits"};
    }
    result.append(m_source, position, replacement.offset - position);
    result.append(replacement.text);
    position = replacement.offset + replacement.length;
    previousOffset = replacement.offset;
  }
  result.append(m_source, position, m_source.size() - position);
  if (!structuralEdits.empty())
  {
    try
    {
      static_cast<void>(VmfDocument::parse(result));
    }
    catch (const ParserException& e)
    {
      throw std::invalid_argument{
        "Invalid combined VMF structure: " + std::string{e.what()}};
    }
  }
  return result;
}

} // namespace tb::mdl
