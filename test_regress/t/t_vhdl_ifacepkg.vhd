-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A generic of a package may be another package: "package p is new work.base
-- generic map (<>)" declares a formal that stands for whatever package the
-- instantiation supplies (6.5.5). Inside the unit that declares it, "p" is a
-- name of its own -- "p.thing" and "use p.all" both work, and what they reach
-- is the uninstantiated package, with its own generics still unbound. A
-- package body continues its declaration's region (12.1), so a use clause
-- written in the declaration is still in force in the body.

package base is
  generic (n : integer := 4;
           function combine (a, b : integer) return integer is <>);
  subtype word is bit_vector(n - 1 downto 0);
  function widen (x : bit) return word;
  function fold (a, b : integer) return integer;
end package;

package body base is
  function widen (x : bit) return word is
  begin
    return (others => x);
  end function;
  function fold (a, b : integer) return integer is
  begin
    return combine(a, b);
  end function;
end package body;

package outer is
  generic (package bp is new work.base generic map (<>));
  use bp.all;                     -- makes word, widen and fold visible
  function twice (x : word) return word;
  function again (a : integer) return integer;
end package;

package body outer is
  -- Names "word" with no use clause of its own: the body continues the
  -- declaration's region
  function twice (x : word) return word is
  begin
    return x;
  end function;
  function again (a : integer) return integer is
  begin
    return fold(a, a);
  end function;
end package body;

use std.env.all;

entity t_vhdl_ifacepkg is
end entity;

architecture tb of t_vhdl_ifacepkg is

  -- "combine" is left to whatever is visible here, which is this one
  function combine (a, b : integer) return integer is
  begin
    return a + b;
  end function;

  package b4 is new work.base generic map (n => 4);
  package b8 is new work.base generic map (n => 8, combine => open);
  package o4 is new work.outer generic map (bp => b4);
  package o8 is new work.outer generic map (bp => b8);

begin

  main : process is
  begin
    assert b4.widen('1') = "1111" report "b4.widen" severity failure;
    assert b8.widen('1') = "11111111" report "b8.widen" severity failure;
    assert o4.twice("1010") = "1010" report "o4.twice" severity failure;
    assert o8.twice("11001010") = "11001010" report "o8.twice" severity failure;
    assert o4.again(3) = 6 report "o4.again" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
