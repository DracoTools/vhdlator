-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "=" of two arrays of different lengths is false, whatever they hold
-- (9.2.3). Where a generic gives one of the lengths, which instances those
-- are is only known once each is elaborated. The comparison was made as
-- Verilog makes one, padding the shorter operand: "w = x"0f"" of a
-- four-element w of ones was true.
-- A null array is declared here with the ends it was written with, and so
-- has places of its own; two of them are equal whatever those hold.
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_eqgenlen_sub is
  generic (G : natural; H : natural);
end entity;

architecture a of t_vhdl_eqgenlen_sub is
begin
  process
    variable w : std_logic_vector(G - 1 downto 0) := (others => '1');
    variable v : std_logic_vector(H - 1 downto 0) := (others => '1');
    variable z : std_logic_vector(G - 1 downto 0) := (others => '0');
    variable b : bit_vector(0 to G - 1) := (others => '0');
  begin
    assert (w = x"0f") = false severity failure;
    assert (w /= "1111") = (G /= 4) severity failure;
    assert (w = x"ff") = (G = 8) severity failure;
    assert (w = v) = (G = H) severity failure;
    -- Two null arrays are equal whatever they were given
    assert (w = z) = (G = 0) severity failure;
    assert (b = "0000") = (G = 4) severity failure;
    assert (b /= "") = (G /= 0) severity failure;
    wait;
  end process;
end architecture;

entity t_vhdl_eqgenlen is
end entity;

architecture a of t_vhdl_eqgenlen is
begin
  u4 : entity work.t_vhdl_eqgenlen_sub generic map (G => 4, H => 4);
  u8 : entity work.t_vhdl_eqgenlen_sub generic map (G => 8, H => 4);
  u0 : entity work.t_vhdl_eqgenlen_sub generic map (G => 0, H => 0);

  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
