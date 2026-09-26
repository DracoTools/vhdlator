-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "f(i)", where every parameter of f has a default, is two legal readings of
-- one piece of text: a call handing i to the first parameter, and the
-- defaulted call f with its result indexed by i (4.2.2.1, 8.4). Which is
-- meant is settled the way any other overloading is -- by what the context
-- asks for.

package callindexpkg is
  type intvec is array (natural range <>) of integer;
  subtype six is intvec (1 to 6);
  -- Reads both ways wherever it is written with one parenthesis
  function pick (a, b : integer := 3) return six;
end package;

package body callindexpkg is
  function pick (a, b : integer := 3) return six is
  begin
    if a = 3 and b = 3 then
      return (1, 2, 3, 4, 5, 6);
    elsif a = 3 then
      return (11, 22, 33, 44, 55, 66);
    else
      return (111, 222, 333, 444, 555, 666);
    end if;
  end function;
end package body;

use work.callindexpkg.all;

entity t_vhdl_callindex is
end entity;

architecture rtl of t_vhdl_callindex is
begin
  check : process
    variable q : intvec (1 to 4);
    variable v : six;
  begin
    -- An integer is wanted, so these index the result of "pick" called with
    -- both parameters defaulted -- they are not calls passing 1, 2, 3
    q(1) := pick(1);
    q(2) := pick(2);
    q(3) := pick(6);
    q(4) := pick(3);
    assert q = (1, 2, 6, 3) report "bad index of a defaulted call" severity failure;
    -- Where the array is what is wanted, the very same text is the call
    v := pick(1);
    assert v = (111, 222, 333, 444, 555, 666) report "bad call with one argument"
      severity failure;
    v := pick;
    assert v = (1, 2, 3, 4, 5, 6) report "bad call with every parameter defaulted"
      severity failure;
    -- And a call written with both arguments is never the other reading
    v := pick(3, 9);
    assert v = (11, 22, 33, 44, 55, 66) report "bad call with two arguments" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
