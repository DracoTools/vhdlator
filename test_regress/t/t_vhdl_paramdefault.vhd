-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A parameter with a default value may be left out of the call, and a
-- function every one of whose parameters has one is written as a name with
-- no parenthesis at all (4.2.2.1).
--
-- Where that parameter is of an unconstrained array type, the value handed
-- over carries its own bounds -- and a default is a value like any other,
-- so the bounds it carries are the ones the default was written with. That
-- is what makes "v : bit_vector := "1010"" mean four elements to a caller
-- that names none.

package paramdefaultpkg is
  -- Told apart by what they give back, since neither is written with an
  -- argument at the call
  function boo (v : bit_vector := "1010") return bit_vector;
  function boo (v : string := "abcdef") return string;
  function len (v : bit_vector) return natural;
end package;

package body paramdefaultpkg is
  function boo (v : bit_vector := "1010") return bit_vector is
  begin
    return v;
  end function;
  function boo (v : string := "abcdef") return string is
  begin
    return v;
  end function;
  function len (v : bit_vector) return natural is
  begin
    return v'length;
  end function;
end package body;

use work.paramdefaultpkg.all;

entity t_vhdl_paramdefault is
end entity;

architecture rtl of t_vhdl_paramdefault is
  signal bv : bit_vector (0 to 3);
  signal sv : string (1 to 6);
  signal wv : bit_vector (0 to 3);
begin
  check : process
  begin
    -- Written as a name, with no parenthesis: the default is the whole value
    bv <= boo;
    sv <= boo;
    -- The same call with the argument written out
    wv <= boo("1100");
    wait for 1 ns;
    assert bv = "1010" report "bad bit_vector default" severity failure;
    assert sv = "abcdef" report "bad string default" severity failure;
    -- The default is as long as it was written, not as long as the caller
    assert len(boo) = 4 report "bad length of a bit_vector default" severity failure;
    -- And a call that writes the argument gets that one, so the default is
    -- only ever reached where the call leaves it out
    assert wv = "1100" report "bad written argument" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
