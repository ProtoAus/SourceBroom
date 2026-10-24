# Human acceptance — SourceBroom first alpha

No owner confirmation has been received for the following checks. Do not turn
an automated build/screenshot into a human pass. Use newly authored maps/copies;
never overwrite an original user/corpus VMF for a test.

- [ ] **A4 independent Hammer++ reopening.** Open the native-edited authored room
  copy in the appropriate Hammer++ branch (no save needed). Confirm the room,
  ramp and brick texture orientation load without a parse/geometry error.
  Automated native reopen plus CSS/Strata compilation already passed; this is
  the independent editor/visual gate. Record the actual observation and branch.
- [ ] **A4 ramp joins/collision and editor feel.** In the Windows SourceBroom
  candidate, open a copy of the supplied starter, move its existing plain ramp
  one small grid step, undo/redo, Save As, and compile/play with an installed
  matching toolchain. Walk up/along the ramp and inspect the join/UVs. Report
  visible gaps, unexpected collision or surprising controls. Existing automated
  FTESurf compile/load/body logs do not prove this manual feel check.

Pending is not failed, but neither is it complete acceptance. Once confirmed,
record only what was observed; do not infer measured performance, preservation
or support for other Source branches from a visual check.
