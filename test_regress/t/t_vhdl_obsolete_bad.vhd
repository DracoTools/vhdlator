-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Analyzing a design unit again replaces the one before, and a unit analyzed
-- against the one before is obsolete until it is analyzed again (13.5).
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package p1 is
  constant c1 : integer := 5;
end package;

use work.p1.all;

package p2 is
  constant c2 : integer := c1;
end package;

package p1 is
  constant c1 : integer := 6;
end package;

entity t is
end entity;

use work.p1.all;
use work.p2.all;

architecture a of t is
begin
  assert c1 = 6;
  assert c2 = 6;
end architecture;
