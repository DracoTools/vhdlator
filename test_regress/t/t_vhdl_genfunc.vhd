-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package genfunc_pkg is
  type pair is record
    a : integer;
  end record;
  type pairs is array (natural range <>) of pair;
  function "+" (l, r : pair) return pair;
  function "+" (l, r : pairs) return pairs;
end package;

package body genfunc_pkg is
  -- No arithmetic: an element of a record has no default, so a value that
  -- has not been driven yet is INTEGER'LOW and adding two of them overflows
  function "+" (l, r : pair) return pair is
  begin
    return l;
  end function;
  function "+" (l, r : pairs) return pairs is
    variable res : pairs(l'range);
  begin
    for i in l'range loop res(i) := l(i) + r(i); end loop;
    return res;
  end function;
end package body;

use work.genfunc_pkg.all;

-- A tree that instantiates itself, so that one elaboration takes the base
-- case and another the general one
entity genfunc_node is
  generic (POW : positive := 2; WIDE : positive := 2 ** POW);
  port (x : in pairs(0 to WIDE-1) := (others => (a => 0));
        y : out pairs(0 to WIDE-1));
end entity;

architecture a of genfunc_node is
  subtype half is pairs(0 to WIDE/2-1);
  signal lo, hi : half := (others => (a => 0));
begin
  g : if base : POW = 1 generate
    -- The only use of the element operator, and it is in the branch that
    -- the outer elaborations do not take
    y(0) <= x(0) + x(1);
    y(1) <= x(1) + x(0);
  else deeper : generate
    sub : entity work.genfunc_node generic map (POW => POW-1)
      port map (x => x(0 to WIDE/2-1), y => lo);
    sub2 : entity work.genfunc_node generic map (POW => POW-1)
      port map (x => x(WIDE/2 to WIDE-1), y => hi);
    -- and the array operator, whose body calls the element one
    y(0 to WIDE/2-1) <= lo + hi;
    y(WIDE/2 to WIDE-1) <= lo + hi;
  end generate;
end architecture;

use work.genfunc_pkg.all;

entity t_vhdl_genfunc is
end entity;

architecture a of t_vhdl_genfunc is
  signal x, y : pairs(0 to 3) := (others => (a => 0));
begin

  u : entity work.genfunc_node generic map (POW => 2) port map (x, y);

  main : process is
  begin
    x <= (0 => (a => 1), 1 => (a => 2), 2 => (a => 4), 3 => (a => 8));
    wait for 1 ns;
    assert y(0).a = 1 report "y0 is " & integer'image(y(0).a) severity failure;
    assert y(2).a = 1 report "y2 is " & integer'image(y(2).a) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
