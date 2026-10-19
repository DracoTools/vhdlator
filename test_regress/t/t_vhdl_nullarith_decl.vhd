-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A constant declared without bounds takes them from its value (5.3.2.2).
-- Inside a procedure's copy, "l + r" with r null is numeric_std's NAU, not as
-- long as l.

library ieee;
use ieee.numeric_std.all;

entity t_vhdl_nullarith_decl is
end entity;

architecture a of t_vhdl_nullarith_decl is
  procedure p (l, r : unsigned; expect : natural) is
    constant s : unsigned := l + r;
    constant d : unsigned := r - l;
  begin
    assert s'length = expect severity failure;
    assert d'length = expect severity failure;
  end procedure;
begin
  process is
    variable n : unsigned(0 downto 1);
  begin
    p(to_unsigned(5, 4), n, 0);
    p(to_unsigned(5, 4), to_unsigned(1, 4), 4);
    p(to_unsigned(5, 4), to_unsigned(1, 6), 6);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
