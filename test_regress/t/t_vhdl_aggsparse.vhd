-- DESCRIPTION: Verilator: VHDL aggregate with "others" of an unpacked array
-- longer than 65536 elements
--
-- "(0 => 5, 3 => 7, 70000 => 9, others => 1)" for an array of 2**17
-- integers names three elements and gives every other one the same value.
-- What is written out is the named elements and "others" once, however
-- long the array is (9.3.3.3; GHDL gna issue50 has such a table of 2**20
-- words).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_aggsparse is
end entity;

architecture a of t_vhdl_aggsparse is
  type table_t is array (0 to 2**17 - 1) of integer;
  constant table : table_t := (0 => 5, 3 => 7, 70000 => 9, others => 1);
  signal idx : natural := 0;
begin
  process is
  begin
    assert table(0) = 5 severity failure;
    assert table(1) = 1 severity failure;
    assert table(3) = 7 severity failure;
    assert table(69999) = 1 severity failure;
    assert table(70000) = 9 severity failure;
    assert table(2**17 - 1) = 1 severity failure;
    idx <= 70000;
    wait for 1 ns;
    assert table(idx) = 9 severity failure;
    idx <= 100000;
    wait for 1 ns;
    assert table(idx) = 1 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
