-- DESCRIPTION: Verilator: VHDL concurrent statements over the nine values wait on signals
--
-- A native operation in a concurrent statement holds an operand in a net of
-- its own; the statement still waits on the signals it reads, a trigger each
-- for all the statements, and not on a net per statement (neorv32's PMP).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nine_concsens is end entity;
architecture a of t_vhdl_nine_concsens is
  type addr_t is array (0 to 1) of std_ulogic_vector(31 downto 0);
  signal pmpaddr : addr_t := (others => (others => '0'));
  signal m : addr_t;
begin
  g : for r in 0 to 1 generate
    m(r)(2) <= not pmpaddr(r)(0);
    gi : for i in 3 to 31 generate
      m(r)(i) <= m(r)(i-1) or (not pmpaddr(r)(i-3));
    end generate;
  end generate;
  process
  begin
    wait for 1 ns;
    assert to_hstring(m(0)(31 downto 2)) = "3FFFFFFF" report "0: " & to_hstring(m(0)(31 downto 2))
      severity failure;
    pmpaddr(0) <= x"0000000F";
    pmpaddr(1) <= x"FFFFFFF0";
    wait for 1 ns;
    assert to_hstring(m(0)(31 downto 2)) = "3FFFFFE0" report "1: " & to_hstring(m(0)(31 downto 2))
      severity failure;
    assert to_hstring(m(1)(31 downto 2)) = "3FFFFFFF" report "2: " & to_hstring(m(1)(31 downto 2))
      severity failure;
    pmpaddr(1)(0) <= 'X';
    wait for 1 ns;
    assert to_hstring(m(1)(31 downto 2)) = "3FFFFFFX" report "3: " & to_hstring(m(1)(31 downto 2))
      severity failure;
    pmpaddr(1)(0) <= '1';
    wait for 1 ns;
    assert to_hstring(m(1)(31 downto 2)) = "3FFFFFFC" report "4: " & to_hstring(m(1)(31 downto 2))
      severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
