-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- "use pkg.name" makes only the declarations of that name visible, with the
-- enumeration literals and predefined operations of a type it names (12.4).
-- A use clause looks into a library a library clause has named, WORK and STD
-- aside (13.2). "entity e" names an entity visible here. A design unit's own
-- name, and a library's, hides what a use clause would make visible, and has
-- no value (12.3).
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package pkg is
  type t_one_two is (one, two, rtl);
  function f(x : t_one_two) return integer;
  constant c : integer := 1;
end package;

package lib_names is
  type work is (foo, bar);
end package;

package body pkg is
  function f(x : t_one_two) return integer is
  begin
    return 0;
  end function;
end package body;

entity leaf is
  port (p : in bit);
end entity;

architecture rtl of leaf is
begin
end architecture;

use work.pkg.t_one_two;
use ieee.std_logic_1164.all;  -- Bad: no library clause names IEEE

entity t is
end entity;

architecture a of t is
  signal s : t_one_two := one;
  signal b : bit;
  component leaf
    port (p : in bit);
  end component;
begin
  process
    variable v : integer;
  begin
    if s = two then  -- The literals and "=" come with the type
      v := f(s);  -- Bad: f was not named
      v := c;  -- Bad: c was not named
    end if;
    wait;
  end process;
  u1 : entity leaf port map (p => b);  -- Bad: leaf here is the component
  u2 : entity work.leaf port map (p => b);
end architecture;

entity t2 is
end entity;

architecture a of t2 is
  signal b : bit;
begin
  u1 : entity leaf port map (p => b);  -- Bad: no entity leaf is visible
end architecture;

use work.pkg.all;

entity t3 is
end entity;

architecture rtl of t3 is
  constant c1 : t_one_two := two;
  constant c2 : t_one_two := rtl;  -- Bad: rtl here is the architecture
begin
end architecture;

use work.lib_names.all;

entity t4 is
end entity;

architecture a of t4 is
begin
  process
    variable v : work;  -- Bad: WORK here is the library
  begin
    wait;
  end process;
end architecture;
