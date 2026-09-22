-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An alias is overloadable only where what it denotes is (6.6.1, 12.3).
-- An alias of an object is a homograph of anything of that name and hides
-- it, which is what lets a design write "alias ff is f" although STANDARD
-- declares FF, the form-feed character literal. An alias of a subprogram
-- stays overloadable, so two of them may share one designator.

package aliashidepkg is
  function bump (x : integer) return integer;
  function bump (x : real) return real;
end package;

package body aliashidepkg is
  function bump (x : integer) return integer is
  begin
    return x + 1;
  end function;
  function bump (x : real) return real is
  begin
    return x + 0.5;
  end function;
end package body;

use work.aliashidepkg.all;

entity t_vhdl_aliashide is
end entity;

architecture rtl of t_vhdl_aliashide is
begin
  check : process
    variable f : bit_vector (47 downto 0);
    -- Hides STANDARD's FF, the form-feed character literal
    alias ff : bit_vector (47 downto 0) is f;
    -- A slice alias, which denotes no single declaration by name
    alias fh : bit_vector (0 to 31) is f (47 downto 16);
    -- Two aliases of subprograms under one designator: still overloadable
    alias sc is bump [integer return integer];
    alias sc is bump [real return real];
  begin
    ff := x"000fff000fff";
    assert f = x"000fff000fff" report "bad write through an object alias" severity failure;
    assert fh = x"000fff00" report "bad read through a slice alias" severity failure;
    assert sc(2) = 3 report "bad integer overload of an aliased subprogram" severity failure;
    assert sc(2.0) = 2.5 report "bad real overload of an aliased subprogram" severity failure;
    -- The character literal is still reachable where nothing hides it
    assert character'pos(character'val(12)) = 12 report "bad character" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
