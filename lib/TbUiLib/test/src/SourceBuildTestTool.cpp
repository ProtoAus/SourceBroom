/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
// Synthetic external compiler control, never interprets or invokes a real compiler.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QtEndian>

#include <cstdio>

int main(int argc, char** argv)
{
  auto app = QCoreApplication{argc, argv};
  const auto args = app.arguments();
  if (args.size() != 4 || args[1] != "-game")
  {
    return 11;
  }
  auto log = QFile{args[2] + "/calls.txt"};
  qsizetype previousCalls = 0;
  if (log.open(QIODevice::ReadOnly))
  {
    previousCalls = log.readAll().count("-game");
    log.close();
  }
  if (!log.open(QIODevice::WriteOnly | QIODevice::Append))
  {
    return 12;
  }
  log.write((QDir::currentPath() + "\n" + args.mid(1).join('\n') + "\n").toUtf8());
  log.close();
  auto modeFile = QFile{args[2] + "/gameinfo.txt"};
  if (!modeFile.open(QIODevice::ReadOnly))
  {
    return 13;
  }
  const auto mode = modeFile.readAll();
  if (mode == "timeout")
  {
    QThread::msleep(3000);
  }
  if (mode == "diagnostic" || mode == "leak-failure")
  {
    std::fputs("WARNING: entity 2 brush 3 at (128 -64 32)\n", stdout);
    std::fflush(stdout);
    if (QFileInfo(args[3]).suffix() == "vmf")
    {
      auto leak = QFile{QFileInfo(args[3]).dir().filePath(
        QFileInfo(args[3]).completeBaseName() + ".lin")};
      if (!leak.open(QIODevice::WriteOnly))
        return 15;
      leak.write("128 -64 32\n256 -64 32\n");
    }
  }
  if (mode != "missing")
  {
    auto bsp = QFile{
      QFileInfo(args[3]).dir().filePath(QFileInfo(args[3]).completeBaseName() + ".bsp")};
    if (!bsp.open(QIODevice::WriteOnly))
    {
      return 14;
    }
    auto data = QByteArray(1036, '\0');
    data.replace(0, 4, mode == "invalid" ? "NOPE" : "VBSP");
    qToLittleEndian<qint32>(mode == "wrong-version" ? 25 : 20, data.data() + 4);
    bsp.write(data);
  }
  return mode == "failure" || mode == "leak-failure"
             || (mode == "failure-vis" && previousCalls == 1)
             || (mode == "failure-rad" && previousCalls == 2)
           ? 23
           : 0;
}
