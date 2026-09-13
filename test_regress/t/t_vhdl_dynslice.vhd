-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A slice whose ends are not a fixed distance apart has no length written in
-- it, but the value it is assigned has one, and the two are the same length
-- while the design runs (10.5.2). So the length of whichever side the text
-- fixes is the length of both.

entity t_vhdl_dynslice is
end entity;

use std.env.all;

architecture tb of t_vhdl_dynslice is
  signal v : bit_vector(7 downto 0) := "00000000";
  signal w : bit_vector(0 to 7) := "00000000";
  signal got : bit_vector(3 downto 0);
begin

  process is
    variable hi : integer := 5;
    variable lo : integer := 2;
  begin
    -- The target's ends move independently; the value says four
    v(hi downto lo) <= "1111";
    w(lo to hi) <= "1011";
    wait for 1 ns;
    assert v = "00111100" report "v is " & to_string(v) severity failure;
    assert w = "00101100" report "w is " & to_string(w) severity failure;

    -- And the other way round: the target says how long the slice is
    got <= v(hi downto lo);
    wait for 1 ns;
    assert got = "1111" report "got is " & to_string(got) severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
