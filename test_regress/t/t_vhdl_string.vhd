-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A STRING is an array like any other: it has a range of its own, it is
-- indexed and sliced by that range, and one declared with a constraint is
-- that long from the start whether or not anything has been written to it.

use std.env.all;

entity t_vhdl_string is
end entity;

architecture tb of t_vhdl_string is

  -- The shape every hand-written image function has: a variable as long as
  -- the argument, filled in one character at a time
  function img (v : bit_vector) return string is
    variable s : string(1 to v'length);
  begin
    for i in v'range loop
      if v(i) = '1' then
        s(i - v'low + 1) := '1';
      else
        s(i - v'low + 1) := '0';
      end if;
    end loop;
    return s;
  end function;

  signal y : bit_vector(0 to 3) := "0101";

begin

  main : process is
    variable s : string(1 to 5);
    variable t : string(3 to 7);
  begin
    assert img(y) = "0101" report "img" severity failure;

    -- A constrained string is its length from the start
    assert s'length = 5 report "length" severity failure;
    for i in s'range loop
      s(i) := character'val(character'pos('a') + i - 1);
    end loop;
    assert s = "abcde" report "fill" severity failure;
    assert s(2) = 'b' report "index" severity failure;
    assert s(2 to 4) = "bcd" report "slice" severity failure;

    -- And one whose range does not start at one is indexed by that range
    for i in t'range loop
      t(i) := character'val(character'pos('a') + i - 3);
    end loop;
    assert t = "abcde" report "fill from three" severity failure;
    assert t(4) = 'b' report "index from three" severity failure;
    assert t(4 to 6) = "bcd" report "slice from three" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
