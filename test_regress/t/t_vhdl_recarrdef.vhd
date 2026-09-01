-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A record with an array of its own, declared without a value. Its default
-- is every element's default (6.4.2.2), and the initializer that says so
-- names the array's type rather than holding it.

use std.env.all;

entity t_vhdl_recarrdef is
end entity;

architecture rtl of t_vhdl_recarrdef is
  type t_words is array (0 to 3) of integer;
  type t_grid is array (0 to 1) of t_words;
  type t_rec is record
    w : t_words;
    g : t_grid;
    n : integer;
  end record;
begin
  main : process
    variable v : t_rec;
    variable r : t_rec := (w => (others => 5), g => (others => (others => 6)), n => 7);
  begin
    assert v.w(0) = integer'low report "FAILED: default word" severity failure;
    assert v.g(1)(3) = integer'low report "FAILED: default grid" severity failure;
    v.w(2) := 11;
    v.g(0)(1) := 12;
    assert v.w(2) = 11 report "FAILED: assigned word" severity failure;
    assert v.g(0)(1) = 12 report "FAILED: assigned grid" severity failure;
    assert v.w(3) = integer'low report "FAILED: word untouched" severity failure;
    assert r.w(1) = 5 and r.g(1)(2) = 6 and r.n = 7
      report "FAILED: written default" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;
end architecture;
