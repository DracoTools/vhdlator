-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A package's "maximum(integer, integer)" hides STANDARD's predefined
-- MAXIMUM of INTEGER, its homograph (12.3), and leaves the one of
-- universal_integer, which is no homograph of it. A nested call over
-- literals, whose inner call has no context type of its own, then has two
-- interpretations and not three. Shaped after surf's RoCEv2 mkQP,
-- "resize(..., maximum(maximum((9) - (0) + 1, 10), (9) - (0) + 1))".

package t_vhdl_maxhomograph_pkg is
  function maximum (left, right : integer) return integer;
end package;
package body t_vhdl_maxhomograph_pkg is
  function maximum (left, right : integer) return integer is
  begin
    if left > right then
      return left;
    end if;
    return right;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_maxhomograph_pkg.all;

entity t_vhdl_maxhomograph is
end entity;

architecture rtl of t_vhdl_maxhomograph is
  signal a : unsigned(9 downto 0) := (others => '0');
begin
  process
    variable n : natural;
    variable u : unsigned(10 downto 0);
  begin
    n := maximum(3, 7);
    n := maximum(maximum((9) - (0) + 1, 10), (9) - (0) + 1);
    u := resize(a, maximum(maximum((9) - (0) + 1, 10), (9) - (0) + 1) + 1);
    report integer'image(n) & " " & integer'image(u'length);
    assert n = 10 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
