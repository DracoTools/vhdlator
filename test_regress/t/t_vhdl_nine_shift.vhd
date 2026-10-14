-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, numeric_std's SHIFT_LEFT, SHIFT_RIGHT,
-- ROTATE_LEFT and ROTATE_RIGHT emitted natively: elements move as they are,
-- metavalues and all; a shift brings in '0', or copies of the sign element
-- where it shifts a SIGNED right; counts up to past the length

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nine_shift is end entity;
architecture a of t_vhdl_nine_shift is
begin
  process
    variable u : unsigned(5 downto 0) := "1X0Z1L";
    variable s : signed(5 downto 0) := "HX0Z10";
    variable n : natural;
  begin
    for i in 0 to 7 loop
      n := i;
      report integer'image(i) & ": " & to_string(shift_left(u, n)) & " " & to_string(shift_right(u, n)) & " "
        & to_string(shift_left(s, n)) & " " & to_string(shift_right(s, n)) & " "
        & to_string(rotate_left(u, n)) & " " & to_string(rotate_right(s, n));
    end loop;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
