-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An allocator of an array type whose bounds are the entity's generics, compared with
-- constants of that type element by element (GHDL gna bug21497).

entity t_vhdl_accgenbounds is
  generic (low : integer := 1; high : integer := 4);
end entity;

architecture a of t_vhdl_accgenbounds is
begin
  process is
    type arr1_t is array (low to high) of integer;
    type arr2_t is array (low to high) of arr1_t;
    constant c : arr2_t := (others => (others => 1));
    constant d : arr2_t := (others => (others => 2));
    type acc_t is access arr2_t;
    variable v : acc_t := new arr2_t'(c);
  begin
    assert v.all = c report "v.all /= c" severity failure;
    assert v.all /= d report "v.all = d" severity failure;
    assert not (v.all = d) report "v.all = d" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
