-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A loop's parameter takes a fresh value on each iteration, so a slice
-- written in terms of one has a length only running the loop decides
-- (10.10). A generate's parameter reads the other way round: the
-- elaboration makes one copy of the body per value (11.8), so a slice
-- written in terms of one is a fixed length in each copy.

entity t_vhdl_loopslice is
end entity;

use std.env.all;

architecture tb of t_vhdl_loopslice is
  signal g_seen : natural := 0;
begin

  -- One copy per value of i, each with a slice of its own fixed length
  g: for i in 1 to 4 generate
    signal part : bit_vector(i - 1 downto 0);
  begin
    part <= (others => '1');
  end generate;

  main: process is
    variable v : bit_vector(15 downto 0);
    variable w : bit_vector(0 to 15);
    variable n : natural;
    variable s : string(1 to 16);
  begin
    v := x"ffff";
    w := x"ffff";

    -- Read: a slice as long as the iteration says, compared with another
    for i in 0 to 15 loop
      assert v(i downto 0) = w(0 to i)
        report "descending and ascending slices of the same length differ"
        severity failure;
    end loop;

    -- Read: the length of such a slice is the iteration's own count
    for i in 0 to 15 loop
      n := v(i downto 0)'length;
      assert n = i + 1
        report "slice length is not what the iteration says" severity failure;
    end loop;

    -- Written: the other side says how long it is, as ever
    v := (others => '0');
    for i in 0 to 3 loop
      v(i + 4 downto i) := "10101";
    end loop;
    assert v(8 downto 0) = "010101111"
      report "an indexed part select inside a loop wrote the wrong bits"
      severity failure;

    -- A slice of text, as long as the iteration says
    s := "abcdefghijklmnop";
    for i in 1 to 16 loop
      assert s(1 to i)'length = i
        report "substring length is not what the iteration says"
        severity failure;
    end loop;

    report "*-* All Finished *-*";
    finish;
  end process;

end architecture;
