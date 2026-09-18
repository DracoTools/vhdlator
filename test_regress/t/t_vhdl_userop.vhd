-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Ranking overloads by how little conversion their operands need is a
-- tie-break between the interpretations the context allows, not a filter that
-- overrides it. Where the context asks for a type that only a lower-ranked
-- candidate returns, that candidate is the one meant (12.5): the predefined
-- operators fit an integer literal without converting it, and a user-defined
-- one taking INTEGER still wins when its result is what is being assigned.

use std.env.all;

entity t_vhdl_userop is
end entity;

architecture tb of t_vhdl_userop is
  type newt is (one, two, three, four);

  function "abs" (constant c1 : in integer) return newt is
  begin
    assert c1 = 10 report "abs operand" severity failure;
    return three;
  end function;

  function "-" (constant c1 : in integer) return newt is
  begin
    assert c1 = 70 report "neg operand" severity failure;
    return two;
  end function;

  function "+" (constant c1, c2 : in integer) return newt is
  begin
    assert c1 = 10 and c2 = 20 report "plus operands" severity failure;
    return four;
  end function;

  function "mod" (constant c1, c2 : in integer) return newt is
  begin
    assert c1 = 10 and c2 = 20 report "mod operands" severity failure;
    return one;
  end function;

begin

  main : process is
    variable n : newt;
    variable i : integer;
  begin
    n := abs 10;     assert n = three report "abs" severity failure;
    n := -70;        assert n = two report "neg" severity failure;
    n := 10 + 20;    assert n = four report "plus" severity failure;
    n := 10 mod 20;  assert n = one report "mod" severity failure;

    -- Where the context asks for the predefined result, the predefined
    -- operator is still what is meant
    i := abs (-10);  assert i = 10 report "predefined abs" severity failure;
    i := 10 + 20;    assert i = 30 report "predefined plus" severity failure;
    i := 10 mod 20;  assert i = 10 report "predefined mod" severity failure;
    i := -70;        assert i = -70 report "predefined neg" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
