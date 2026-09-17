-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A generic subprogram written "is <>" stands for the subprogram of that name
-- visible where the *instance* is written, which is not where the entity is
-- written (6.5.6.3). GHDL 7.0.0-dev crashes on this shape and nvc 1.22.1
-- runs it; Questa 2025.2 analyzes it.

package t_vhdl_gensubdef_pkg is
  type col is (red, green);
  function pick(a, b : col) return col;
  function pick(a, b : integer) return integer;
end package;

package body t_vhdl_gensubdef_pkg is
  function pick(a, b : col) return col is
  begin
    if a = red then return b; else return a; end if;
  end function;
  function pick(a, b : integer) return integer is
  begin
    return a + b;
  end function;
end package body;

entity t_vhdl_gensubdef_sub is
  generic (type foo;
           function pick(a, b : foo) return foo is <>);
  port (p, q : in foo; r : out foo);
end entity;
architecture rtl of t_vhdl_gensubdef_sub is
begin
  r <= pick(p, q);
end architecture;

use work.t_vhdl_gensubdef_pkg.all;
use std.env.all;

entity t_vhdl_gensubdef is
end entity;

architecture tb of t_vhdl_gensubdef is
  signal x : col := red;
  signal y : col := green;
  signal z : col;
  signal i, j, k : integer := 3;
begin

  -- The profile picks which "pick" each instance means, read with the type
  -- the instance chose put in
  u1 : entity work.t_vhdl_gensubdef_sub(rtl)
    generic map (foo => col) port map (p => x, q => y, r => z);
  u2 : entity work.t_vhdl_gensubdef_sub(rtl)
    generic map (foo => integer) port map (p => i, q => j, r => k);

  main : process is
  begin
    wait for 1 ns;
    assert z = green report "z wrong" severity failure;
    assert k = 6 report "k is " & integer'image(k) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
