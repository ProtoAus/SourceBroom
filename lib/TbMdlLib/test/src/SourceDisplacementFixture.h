/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>

namespace tb::mdl::test
{
// Newly authored 5x5 grid: flat UV base z64, raised centre z80.
inline std::string displacementBlock()
{
  auto text = std::string{
    "dispinfo { power 2 startposition \"[0 0 64]\" elevation 0 subdiv 0\n normals {\n"};
  for (size_t row = 0; row < 5; ++row)
  {
    text += "row" + std::to_string(row) + " \"0 0 1  0 0 1  0 0 1  0 0 1  0 0 1\"\n";
  }
  text += "}\n distances { // untouched spellings and spacing are intentional\n";
  for (size_t row = 0; row < 5; ++row)
  {
    text += "row" + std::to_string(row) + " \"0.00 +0  " + (row == 2 ? "16" : "0")
            + "  0 0\"\n";
  }
  text += "}\n alphas {\n";
  for (size_t row = 0; row < 5; ++row)
  {
    text += "row" + std::to_string(row) + " \"0 64 128 192 255\"\n";
  }
  text += "}\n triangle_tags {\n";
  for (size_t row = 0; row < 4; ++row)
  {
    text += "row" + std::to_string(row) + " \"0 0 0 0 0 0 0 0\"\n";
  }
  return text + "}\n allowed_verts { 10 \"-1 -1 -1 -1 -1 -1 -1 -1 -1 -1\" } }\n";
}
} // namespace tb::mdl::test
