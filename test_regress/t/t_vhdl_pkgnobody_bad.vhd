-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A package whose declarations a body completes is instantiated only once
-- that body is analyzed (4.9). References: nvc 1.22.1, GHDL 7.0.0-dev,
-- Questa 2025.2.

package gen1 is
  generic (v : natural := 5);
  function get return natural;
end package;

package gen2 is
  generic (v : natural := 5);
  constant c : natural := v;
end package;

package ok_inst is new work.gen2;
package bad_inst is new work.gen1;  -- Bad
