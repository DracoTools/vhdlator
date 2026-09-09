-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An operator may be called by its name, and resolution is bidirectional: a
-- subprogram overloads on its result as well as on its arguments. The
-- predefined "abs" on universal_integer fits the argument without conversion,
-- so only the type the context asks for tells the two apart.

entity t is
end entity;

architecture rtl of t is
  type newt is (zero, one, two);

  function "abs" (x : integer) return newt is
  begin
    if x > 0 then
      return one;
    end if;
    return zero;
  end function;

begin
  process
    variable n : newt;
    variable i : integer;
  begin
    n := "abs"(10);
    assert n = one report "the design's own operator was not chosen" severity failure;
    n := "abs"(-1);
    assert n = zero report "wrong value from the design's operator" severity failure;
    -- Where the context asks for an integer, the predefined one is meant
    i := abs(-7);
    assert i = 7 report "the predefined operator was not chosen" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
