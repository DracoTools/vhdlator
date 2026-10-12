-- DESCRIPTION: Verilator: VHDL record signal read field by field over VPI
--
-- A record is held as a packed structure, but VHDL has no packed records:
-- over VPI it is a structure whose fields are members, each read, written
-- and watched on its own.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vpi_vhdl_record is
end entity;

architecture rtl of t_vpi_vhdl_record is
  type inner_t is record
    flag : std_logic;
    code : std_logic_vector(3 downto 0);
  end record;
  type ints_t is record
    a : integer;
    b : integer;
    v : std_logic_vector(7 downto 0);
    n : inner_t;
  end record;
  signal s_ints : ints_t := (1, 2, x"0f", ('0', x"3"));
begin
  process
  begin
    wait for 10 ns;
    s_ints.a <= s_ints.a + 1;
    wait for 10 ns;
    s_ints.b <= 5;
    s_ints.v <= x"a5";
    wait for 10 ns;
    s_ints.n.code <= x"c";
    wait for 10 ns;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
