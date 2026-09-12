-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A block is a declarative region of its own (11.2), so two blocks may each
-- label a generate the same, and each may declare a signal of one name.

entity t_vhdl_blockgen is
end entity;

use std.env.all;

architecture tb of t_vhdl_blockgen is
  signal a : bit_vector(3 downto 0);
  signal b : bit_vector(3 downto 0);
begin

  b1: block is
    signal s : bit_vector(3 downto 0);
  begin
    g: for i in 0 to 3 generate
      s(i) <= '1' when i mod 2 = 0 else '0';
    end generate;
    a <= s;
  end block;

  b2: block is
    signal s : bit_vector(3 downto 0);
  begin
    g: for i in 0 to 3 generate
      s(i) <= '0' when i mod 2 = 0 else '1';
    end generate;
    b <= s;
  end block;

  process is
  begin
    wait for 1 ns;
    assert a = "0101" report "a is " & to_string(a) severity failure;
    assert b = "1010" report "b is " & to_string(b) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
