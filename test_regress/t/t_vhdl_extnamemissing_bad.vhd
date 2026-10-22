-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An alias of an external name whose path names no instance, never read:
-- the object has to exist when the alias is elaborated (8.7). nvc 1.22.1 and
-- Questa refuse it, as GHDL does with pre-elaboration (its testsuite expects
-- the failure there; its default backend runs it). From GHDL gna issue2508.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_extnamemissing_bad_top is
end entity;

architecture behv of t_vhdl_extnamemissing_bad_top is
  signal clk : std_logic := '0';
begin
  clk <= not clk after 10 ns;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_extnamemissing_bad is
end entity;

architecture behv of t_vhdl_extnamemissing_bad is
  alias tb_clk is <<signal .t_vhdl_extnamemissing_bad.th.clk : std_logic>>;
begin
  top_i : entity work.t_vhdl_extnamemissing_bad_top;
end architecture;
