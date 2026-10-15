-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, RESIZE of a sum of two vectors whose lengths two
-- generics give: the sum is as long as the longer, a length elaboration
-- settles, and is held for RESIZE at that length (GHDL gna bug24065)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_genericsum is
  generic (output_bits : natural := 8; num_bits : natural := 9);
end entity;

architecture a of t_vhdl_nine_genericsum is
  signal comb_sum : std_logic_vector(num_bits downto 0) := "0000000011";
  signal int_sum : signed(output_bits - 1 downto 0) := x"05";
  signal clk : std_logic := '0';
begin
  process (clk)
  begin
    if rising_edge(clk) then
      int_sum <= resize(int_sum + signed(comb_sum), int_sum'length);
    end if;
  end process;

  process
  begin
    clk <= '1';
    wait for 1 ns;
    assert int_sum = x"08" report to_string(int_sum) severity failure;
    comb_sum <= "00000000X1";
    clk <= '0';
    wait for 1 ns;
    clk <= '1';
    wait for 1 ns;
    assert to_string(int_sum) = "XXXXXXXX" report to_string(int_sum) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
