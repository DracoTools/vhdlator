-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A slice whose length is a generate parameter's power of two, given a
-- fill aggregate: "lvl(i+1)(31 downto 32-2**i) <= (others => sgn) when ...",
-- neorv32's barrel shifter. The ends are not linear in the parameter, but
-- each copy of the body has a number for them; under two states the fill
-- was an assignment pattern with nothing to give it a width.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_genbarrel is
end entity;
architecture a of t_vhdl_genbarrel is
  type lvl_t is array (0 to 5) of std_ulogic_vector(31 downto 0);
  signal lvl : lvl_t;
  signal sh : std_ulogic_vector(4 downto 0) := "00101";
  signal sgn : std_ulogic := '1';
begin
  lvl(0) <= x"80F0000F";
  gen: for i in 0 to 4 generate
    lvl(i+1)(31 downto 32-2**i) <= (others => sgn) when (sh(i) = '1') else lvl(i)(31 downto 32-2**i);
    lvl(i+1)(31-2**i downto 0)  <= lvl(i)(31 downto 2**i) when (sh(i) = '1') else lvl(i)(31-2**i downto 0);
  end generate;
  process
  begin
    wait for 1 ns;
    assert lvl(5) = x"FC078000" report "first " & to_hstring(lvl(5)) severity failure;
    sgn <= '0';
    wait for 1 ns;
    assert lvl(5) = x"04078000" report "second " & to_hstring(lvl(5)) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
