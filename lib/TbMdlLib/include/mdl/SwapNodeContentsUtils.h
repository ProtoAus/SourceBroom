/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "mdl/NodeContents.h"

#include <utility>
#include <vector>

namespace tb::mdl
{
class Map;
class Node;

// Notified reversible swap only; callers own admission and history.
void swapNodeContentsAndNotify(
  std::vector<std::pair<Node*, NodeContents>>& nodes, Map& map);
} // namespace tb::mdl
