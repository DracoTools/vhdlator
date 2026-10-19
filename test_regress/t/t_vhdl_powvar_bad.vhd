-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An INTEGER "**" whose result INTEGER cannot hold stops the run (9.2.8), also where the
-- exponent is a variable no statement writes, which is its initial value (GHDL gna issue2646).

entity t_vhdl_powvar_bad is
end entity;

architecture a of t_vhdl_powvar_bad is
begin
  process is
    variable e : integer := 31;
    variable v : integer;
  begin
    report "before";
    v := (2 ** e) - 1;
    report "2 ** 31 should have stopped the run, got " & integer'image(v) severity failure;
    wait;
  end process;
end architecture;
