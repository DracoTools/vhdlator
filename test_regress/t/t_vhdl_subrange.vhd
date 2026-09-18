-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A discrete range may be written as a subtype mark, and then it is that
-- subtype's own range (5.3.2.1): "v(fld)" is "v(fld'range)" as a slice, and
-- "when fld =>" covers every value of it as a choice. The parser cannot tell
-- an index from a slice, nor a choice that is a value from one that is a
-- range, so the mark arrives as an ordinary expression either way.

use std.env.all;

entity t_vhdl_subrange is
end entity;

architecture tb of t_vhdl_subrange is
  subtype fld is natural range 5 downto 2;
  subtype low2 is natural range 1 downto 0;
  subtype small is integer range 3 to 6;
  -- Wide enough that listing the values it covers is not an option, which is
  -- what an ordinary "integer range N to INTEGER'HIGH" always is
  subtype too_high is integer range 500 to integer'high;
  subtype too_low is integer range integer'low to -500;
  type colour is (red, green, blue, white);
  subtype warm is colour range red to green;

  signal v : bit_vector(7 downto 0) := "10110100";
  signal agg : bit_vector(7 downto 0);

  function classify (x : integer) return integer is
  begin
    case x is
      when too_high => return 1;
      when too_low => return -1;
      when small => return 2;
      when others => return 0;
    end case;
  end function;

begin

  -- An aggregate choice naming a subtype covers every index of it
  agg <= (fld => '1', low2 => '0', others => '1');

  main : process is
    variable c : colour;
    variable seen : integer := 0;
  begin
    wait for 1 ns;

    -- A slice named by a subtype mark
    assert v(fld) = "1101" report "slice" severity failure;
    assert v(fld)'length = 4 report "slice length" severity failure;
    assert v(low2) = "00" report "slice low2" severity failure;
    assert agg = "11111100" report "aggregate is " & to_string(agg) severity failure;

    -- A case choice naming a subtype, narrow and wide
    assert classify(4) = 2 report "small" severity failure;
    assert classify(9) = 0 report "others" severity failure;
    assert classify(1000) = 1 report "too_high" severity failure;
    assert classify(integer'high) = 1 report "too_high top" severity failure;
    assert classify(-1000) = -1 report "too_low" severity failure;
    assert classify(integer'low) = -1 report "too_low bottom" severity failure;
    assert classify(500) = 1 report "too_high edge" severity failure;
    assert classify(499) = 0 report "just below too_high" severity failure;

    -- An enumeration subtype as a choice
    c := green;
    case c is
      when warm => seen := 1;
      when others => seen := 2;
    end case;
    assert seen = 1 report "warm" severity failure;
    c := white;
    case c is
      when warm => seen := 1;
      when others => seen := 2;
    end case;
    assert seen = 2 report "not warm" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
