-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "rol" and "ror" move every element of a vector one place and repeat that
-- as many times as the right operand says (9.2.4). A count of zero, a whole
-- number of lengths, a negative count and a count larger than the vector all
-- have to come out right, and the count need not be static.

use std.env.all;

entity t_vhdl_rotate is
end entity;

architecture tb of t_vhdl_rotate is

  signal v : bit_vector(7 downto 0) := "10010011";
  signal u : bit_vector(0 to 3) := "1100";
  signal n : integer := 0;

begin

  main : process is
  begin
    -- A static count
    assert (v rol 3) = "10011100" report "rol 3" severity failure;
    assert (v ror 3) = "01110010" report "ror 3" severity failure;
    assert (v rol 0) = v report "rol 0" severity failure;
    assert (v ror 0) = v report "ror 0" severity failure;
    -- Rotating by the whole length comes back to where it started
    assert (v rol 8) = v report "rol 8" severity failure;
    assert (v ror 8) = v report "ror 8" severity failure;
    -- Past the length it comes round again
    assert (v rol 11) = (v rol 3) report "rol 11" severity failure;
    assert (v ror 19) = (v ror 3) report "ror 19" severity failure;
    -- A negative count goes the other way
    assert (v rol (-3)) = (v ror 3) report "rol -3" severity failure;
    assert (v ror (-3)) = (v rol 3) report "ror -3" severity failure;
    -- An ascending vector rotates by position, not by index
    assert (u rol 1) = "1001" report "u rol 1" severity failure;
    assert (u ror 1) = "0110" report "u ror 1" severity failure;

    -- A count that is only known while running
    for i in 0 to 10 loop
      n <= i;
      wait for 1 ns;
      assert (v rol n) = (v rol (i mod 8)) report "dynamic rol" severity failure;
      assert (v ror n) = (v ror (i mod 8)) report "dynamic ror" severity failure;
      assert (v rol n) = ((v ror (8 - (i mod 8))) rol 8) report "rol against ror" severity failure;
    end loop;
    n <= -5;
    wait for 1 ns;
    assert (v rol n) = (v ror 5) report "dynamic negative" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
