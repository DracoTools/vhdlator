-- DESCRIPTION: Verilator: VHDL numeric_std SIZE_RES overloads
--
-- "to_unsigned(1, bin)", "to_signed(-3, s)" and "resize(bin, v)" are as long
-- as their second argument (16.6). Its value was taken as the width, and
-- "to_unsigned(1, bin)" made a value of one element (GHDL synth issue2667).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_sizeres is
end entity;

architecture a of t_vhdl_sizeres is
  signal bin : unsigned(3 downto 0);
  signal s : signed(5 downto 0);
begin
  bin <= to_unsigned(1, bin);
  s <= to_signed(-3, s);

  process
    variable v : unsigned(7 downto 0);
  begin
    wait for 1 ns;
    v := to_unsigned(200, v);
    assert bin = "0001" severity failure;
    assert s = "111101" severity failure;
    assert v = x"c8" severity failure;
    assert resize(bin, v) = x"01" severity failure;
    report "sizeres done";
    std.env.finish;
  end process;
end architecture;
