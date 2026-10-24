/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/SourceDocumentReader.h"

#include <QFile>

#include "mdl/SourceCapabilities.h"

#include <exception>
#include <string>

namespace tb::ui
{
std::optional<mdl::VmfDocument> readSavedSourceDocument(
  const QString& path, QString& error)
{
  error.clear();
  const auto limits = mdl::SourceCapabilityOptions{};
  auto file = QFile{path};
  if (!file.open(QIODevice::ReadOnly))
  {
    error = "Cannot read saved native VMF: " + file.errorString();
    return std::nullopt;
  }
  const auto maxBytes = static_cast<qint64>(limits.maxSourceBytes);
  if (file.size() > maxBytes)
  {
    error =
      "Native VMF exceeds the 128 MiB Source-tools read budget; "
      "use an external compiler for larger documents.";
    return std::nullopt;
  }
  try
  {
    const auto bytes = file.read(maxBytes + 1);
    if (file.error() != QFileDevice::NoError || bytes.size() > maxBytes || !file.atEnd())
    {
      error = "Cannot read a complete native VMF within the Source-tools byte budget.";
      return std::nullopt;
    }
    return mdl::VmfDocument::parse(
      std::string{bytes.constData(), static_cast<size_t>(bytes.size())},
      limits.maxEntries);
  }
  catch (const std::exception& exception)
  {
    error = "Native VMF read/parse refused: " + QString::fromUtf8(exception.what());
    return std::nullopt;
  }
}
} // namespace tb::ui
