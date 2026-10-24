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
    if (source[i] == '\n' || (source[i] == '\r' && (i + 1 == offset || source[i + 1] != '\n')))
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
    if (token.type != TokenType::Text || !token.source.quoted || token.source.offset != 0
        || token.source.length != replacement.size() || lexer.next().type != TokenType::End)
    {
      throw std::invalid_argument{"VMF replacement must be exactly one quoted value token"};
    }
  }
  catch (const ParserException& e)
  {
    throw std::invalid_argument{std::string{"Invalid VMF replacement token: "} + e.what()};
  }
}

} // namespace

VmfDocument VmfDocument::parse(std::string source)
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
    const auto parent = parents.empty() ? std::nullopt : std::optional{parents.back()};
    document.m_entries.push_back(VmfEntry{
      key.source,
      value.type == TokenType::Text ? std::optional{value.source} : std::nullopt,
      parent,
      {}});
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

std::string VmfDocument::serialize(const std::span<const VmfValueEdit> edits) const
{
  struct Replacement
  {
    VmfToken target;
    const std::string* token;
  };
  auto replacements = std::vector<Replacement>{};
  replacements.reserve(edits.size());
  for (const auto& edit : edits)
  {
    if (edit.entry >= m_entries.size() || !m_entries[edit.entry].value)
    {
      throw std::invalid_argument{"VMF value edit must target an existing property"};
    }
    validateReplacement(edit.token);
    replacements.push_back({*m_entries[edit.entry].value, &edit.token});
  }
  std::sort(replacements.begin(), replacements.end(), [](const auto& a, const auto& b) {
    return a.target.offset < b.target.offset;
  });

  auto result = std::string{};
  result.reserve(m_source.size());
  auto position = size_t{0};
  for (const auto& replacement : replacements)
  {
    if (replacement.target.offset < position)
    {
      throw std::invalid_argument{"Duplicate VMF value edit"};
    }
    result.append(m_source, position, replacement.target.offset - position);
    result.append(*replacement.token);
    position = replacement.target.offset + replacement.target.length;
  }
  result.append(m_source, position, m_source.size() - position);
  return result;
}

} // namespace tb::mdl
