-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- 'SUBTYPE names the subtype an object was declared with, which may be
-- written wherever a type mark may: as the prefix of another attribute, and
-- as a conversion's type mark (16.2.5). A protected object's methods are
-- reached through an alias of it as readily as through the object (6.6.1).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package counter_pkg is

  type t_counter is protected
    procedure bump (n : integer);
    impure function value return integer;
  end protected;

end package;

package body counter_pkg is

  type t_counter is protected body
    variable priv_count : integer := 0;
    procedure bump (n : integer) is
    begin
      priv_count := priv_count + n;
    end procedure;
    impure function value return integer is
    begin
      return priv_count;
    end function;
  end protected body;

end package body;

use work.counter_pkg.all;

package shared_pkg is
  shared variable the_counter : t_counter;
end package;

use work.counter_pkg.all;
use work.shared_pkg.all;

package alias_pkg is
  -- Another name for the same object, which is how a library hands one out
  alias counter is work.shared_pkg.the_counter;
end package;

entity t_vhdl_subtypeattr is
end entity;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.alias_pkg.all;
use std.env.all;

architecture tb of t_vhdl_subtypeattr is
  subtype byte_t is std_logic_vector(7 downto 0);
  signal b : byte_t := x"2a";
  signal u : unsigned(7 downto 0) := x"03";
begin

  process is
    variable n : integer range 1 to 42;
    variable m : n'subtype;
  begin
    -- An attribute of the subtype another attribute names
    assert n'subtype'right = 42 report "n'subtype'right" severity failure;
    assert m'subtype'left = 1 report "m'subtype'left" severity failure;
    m := 7;
    assert m = 7 report "m" severity failure;

    -- A conversion whose type mark is written as an attribute
    assert b'subtype(std_logic_vector(u)) = x"03"
      report "conversion" severity failure;
    assert unsigned(b'subtype(std_logic_vector(u))) = 3
      report "conversion value" severity failure;

    -- Methods through the alias of a shared variable of a protected type
    counter.bump(5);
    counter.bump(6);
    assert counter.value = 11
      report "counter is " & integer'image(counter.value) severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
