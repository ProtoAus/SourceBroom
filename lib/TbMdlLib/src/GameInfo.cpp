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

#include "mdl/GameInfo.h"

#include "mdl/SourceContent.h"

#include "kd/reflection_impl.h"

namespace tb::mdl
{

kdl_reflect_impl(GameInfo);

GameInfo makeGameInfo(GameConfig gameConfig)
{
  const auto gamePathPrefPath = std::filesystem::path{"Games"} / gameConfig.name / "Path";
  const auto defaultEnginePrefPath =
    std::filesystem::path{"Games"} / gameConfig.name / "Default Engine";

  auto defaultPath = std::filesystem::path{};
  if (gameConfig.fileSystemConfig.packageFormat.format == "vpk")
  {
    static const auto installed = sourceInstalledGames();
    if (installed)
    {
      const auto expected = gameConfig.name == "Counter-Strike: Source" ? "cstrike"
                            : gameConfig.name == "Momentum Mod"         ? "momentum"
                                                                        : "";
      for (const auto& path : installed.value())
      {
        if (path.filename() == expected && !std::string_view{expected}.empty())
        {
          defaultPath = path;
          break;
        }
      }
    }
  }
  return GameInfo{
    std::move(gameConfig),
    Preference<std::filesystem::path>{gamePathPrefPath, std::move(defaultPath)},
    Preference<std::filesystem::path>{defaultEnginePrefPath, {}},
  };
}

} // namespace tb::mdl
