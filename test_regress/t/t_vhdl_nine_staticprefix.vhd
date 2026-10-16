-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A concurrent statement is sensitive to the longest static prefix of each
-- name it reads (11.6): one element of an array, by a constant or by a
-- generate parameter, or one element of a record. It runs once at the
-- start and not again as the other parts change, nor as its own target
-- does, so each comparison of a metavalue warns once, as in nvc 1.22.1,
-- GHDL 7.0.0-dev and Questa 2025.2 (neorv32's PMP and CLINT)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nine_staticprefix is
end entity;
architecture a of t_vhdl_nine_staticprefix is
  type arr_t is array (0 to 2) of unsigned(3 downto 0);
  type rec_t is record
    addr : unsigned(3 downto 0);
    data : unsigned(3 downto 0);
  end record;
  signal a : arr_t := ("XXXX", "0000", "0000");
  signal r : rec_t := ("XXXX", "0000");
  signal lt, eq : std_ulogic;
  signal ge : std_ulogic_vector(1 to 2);
begin
  lt <= '1' when a(0) < 3 else '0';
  eq <= '1' when r.addr = 3 else '0';
  g : for i in 1 to 2 generate
    ge(i) <= '1' when a(i - 1) >= 2 else '0';
  end generate;
  process
  begin
    for k in 1 to 3 loop
      a(2) <= to_unsigned(k, 4);
      r.data <= to_unsigned(k, 4);
      wait for 0 ns;
    end loop;
    wait for 1 ns;
    assert lt = '0' and eq = '0' and ge = "00" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
