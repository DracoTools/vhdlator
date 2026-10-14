-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, one element of a memory word written through an
-- index only running settles, itself a conversion of a vector:
-- "ram(to_integer(unsigned(adr)))(i) <= d(i)". The checks on that index
-- come out of the target ahead of the assignment, and metavalues are
-- written as they are

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_targetidx is
end entity;

architecture a of t_vhdl_nine_targetidx is
  type ram_t is array (0 to 3) of std_logic_vector(7 downto 0);
  signal ram : ram_t := (others => (others => 'U'));
  signal clk : std_logic := '0';
  signal adr : std_logic_vector(1 downto 0) := "01";
  signal d : std_logic_vector(7 downto 0) := "1010XZ0H";
begin
  process (clk)
  begin
    if rising_edge(clk) then
      for i in d'range loop
        ram(to_integer(unsigned(adr)))(i) <= d(i);
      end loop;
      ram((to_integer(unsigned(adr)) + 1) mod 4)(0) <= d(7);
    end if;
  end process;

  process
  begin
    clk <= '1';
    wait for 1 ns;
    report to_string(ram(1)) & " " & to_string(ram(2)) & " " & to_string(ram(0));
    assert ram(1) = "1010XZ0H" severity failure;
    assert ram(2) = "UUUUUUU1" severity failure;
    adr <= "11";
    d <= "0101LW-1";
    clk <= '0';
    wait for 1 ns;
    clk <= '1';
    wait for 1 ns;
    report to_string(ram(3)) & " " & to_string(ram(0)) & " " & to_string(ram(1));
    assert ram(3) = "0101LW-1" severity failure;
    assert ram(0) = "UUUUUUU0" severity failure;
    assert ram(1) = "1010XZ0H" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
