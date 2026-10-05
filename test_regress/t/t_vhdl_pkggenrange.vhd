-- DESCRIPTION: Verilator: VHDL generic constant of an unconstrained array
-- type of a package instance, bounded by its actual
--
-- "package counter is generic (MAX : bit_vector); subtype counter_t is
-- bit_vector(MAX'range);" instantiated with MAX => 10D"999": the generic
-- takes its bounds from the actual (6.5.6.2), so counter_t is ten bits
-- long, indexed as the bit string literal is (nvc regress vhpi10).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_pkggenrange_counter is
  generic (MAX : bit_vector; NAME : string);
  subtype counter_t is bit_vector(MAX'range);
  constant NAME_LEN : natural := NAME'length;
end package;

entity t_vhdl_pkggenrange is
end entity;

architecture test of t_vhdl_pkggenrange is
  package pkg is new work.t_vhdl_pkggenrange_counter
    generic map (MAX => 10D"999", NAME => "counter");
  package pkg4 is new work.t_vhdl_pkggenrange_counter
    generic map (MAX => "1010", NAME => "c4");
  signal s0 : pkg.counter_t;
  signal s4 : pkg4.counter_t;
begin
  process is
  begin
    assert s0'length = 10 severity failure;
    assert s0'left = 0 and s0'right = 9 severity failure;
    assert s4'length = 4 severity failure;
    assert pkg.MAX = "1111100111" severity failure;
    assert pkg.NAME_LEN = 7 and pkg4.NAME_LEN = 2 severity failure;
    s0 <= pkg.MAX;
    s4 <= pkg4.MAX;
    wait for 1 ns;
    assert s0 = "1111100111" and s4 = "1010" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
