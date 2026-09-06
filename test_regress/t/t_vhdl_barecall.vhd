-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A function that may be called with no arguments at all is called by
-- writing its name and nothing else (4.2.2.1), so "return put_msg" is a
-- call however little it looks like one.  Where the copy made for one call
-- site cannot read a length off the body, it falls back to a parameter of
-- the same type -- which is right for "return v and m" and wrong for
-- anything a call returns, since a call may give back any length at all.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_barecall is
end entity;

architecture a of t_vhdl_barecall is

  -- The result comes back through a nested function, written as a name
  function decorate(msg : string; tag : string) return string is
    function put_msg return string is
    begin
      return msg & "<" & tag & ">";
    end function;
  begin
    return put_msg;
  end function;

  -- and the same shape over a vector
  function widen(v : std_logic_vector) return std_logic_vector is
    function doubled return std_logic_vector is
    begin
      return v & v;
    end function;
  begin
    return doubled;
  end function;

begin
  process
    constant one : string := decorate("ab", "HERE");
    constant two : string := decorate("xyz", "Z");
    constant vec : std_logic_vector := widen("1010");
  begin
    -- The value was always right; it was how long it says it is that was not
    assert one = "ab<HERE>" report "wrong value: " & one severity failure;
    assert one'length = 8 report "one'length is " & integer'image(one'length)
      severity failure;
    assert one'right = 8 report "one'right is " & integer'image(one'right) severity failure;

    assert two = "xyz<Z>" report "wrong value: " & two severity failure;
    assert two'length = 6 report "two'length is " & integer'image(two'length)
      severity failure;

    assert vec = "10101010" report "wrong vector" severity failure;
    assert vec'length = 8 report "vec'length is " & integer'image(vec'length)
      severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
