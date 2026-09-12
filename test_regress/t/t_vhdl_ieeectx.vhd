-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The two context declarations IEEE itself defines (16.9), which stand for
-- the library and use clauses a design would otherwise write out.

library ieee;
context ieee.ieee_std_context;

entity t_vhdl_ieeectx is
end entity;

use std.env.all;

architecture tb of t_vhdl_ieeectx is
  -- STD_LOGIC_1164 and NUMERIC_STD are visible, and nothing said so here
  signal v : std_logic_vector(7 downto 0) := x"5a";
  signal u : unsigned(7 downto 0);
begin

  u <= unsigned(v) + 1;

  process is
  begin
    wait for 1 ns;
    assert u = x"5b" report "sum" severity failure;
    assert to_integer(unsigned(v)) = 90 report "to_integer" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
