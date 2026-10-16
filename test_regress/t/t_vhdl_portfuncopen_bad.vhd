-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A port that takes its bounds from its actual cannot take them from a
-- function's result, or an operator's, whose declared type leaves its index
-- open: such an expression has no globally static subtype (6.5.6.3).
-- nvc 1.22.1 and GHDL 7.0.0-dev refuse all three (GHDL gna issue916);
-- a constrained formal, and a signal, stay accepted.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_portfuncopen_bad_sub is
  port (reg_i : in std_ulogic_vector;
        fix_i : in std_ulogic_vector(1 downto 0) := "00");
end entity;
architecture a of t_vhdl_portfuncopen_bad_sub is
begin
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_portfuncopen_bad is
end entity;
architecture a of t_vhdl_portfuncopen_bad is
  function to_slv(i : integer; n : natural) return std_ulogic_vector is
  begin
    return std_ulogic_vector(to_unsigned(i, n));
  end function;
  function inv(v : std_ulogic_vector) return std_ulogic_vector is
  begin
    return not v;
  end function;
  signal int : integer := 1;
  signal s : std_ulogic_vector(3 downto 0) := "0101";
begin
  u1 : entity work.t_vhdl_portfuncopen_bad_sub port map (reg_i => to_slv(int, 2));
  u2 : entity work.t_vhdl_portfuncopen_bad_sub port map (reg_i => inv(s));
  u3 : entity work.t_vhdl_portfuncopen_bad_sub port map (reg_i => s and s);
  ok : entity work.t_vhdl_portfuncopen_bad_sub port map (reg_i => s, fix_i => to_slv(int, 2));
end architecture;
