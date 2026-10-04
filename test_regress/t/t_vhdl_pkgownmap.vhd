-- DESCRIPTION: Verilator: VHDL package design unit with a generic map of
-- its own
--
-- A package that declares generics and maps them in its own header is an
-- instance of itself (4.7): its generics hold what it gives them, and its
-- subprograms and declarations read them -- GHDL gna bug057.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_pkgownmap_pkg is
  generic (c : natural; w : positive);
  generic map (c => 5, w => 3);

  subtype word_t is bit_vector(w - 1 downto 0);
  function f return natural;
  function scaled (x : natural) return natural;
end package;

package body t_vhdl_pkgownmap_pkg is
  function f return natural is
  begin
    return c;
  end function;

  function scaled (x : natural) return natural is
  begin
    return x * c + w;
  end function;
end package body;

use work.t_vhdl_pkgownmap_pkg.all;

entity t_vhdl_pkgownmap is
end entity;

architecture behav of t_vhdl_pkgownmap is
  signal word : word_t := "101";
begin
  process is
  begin
    assert work.t_vhdl_pkgownmap_pkg.f = 5 severity failure;
    assert scaled(2) = 13 severity failure;
    assert word'length = 3 and word = "101" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
