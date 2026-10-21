-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Ports sized by a generic of the instance, each associated in parts: a
-- slice and an element (6.5.7.1). Every part reaches its own actual, the
-- element as much as the slice. open-logic's flow control handler testbench
-- wires a delay "In_Data(15 downto 0) => d, In_Data(16) => valid".

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_partportgen_m is
  generic (Width_g : positive := 5);
  port (i : in std_logic_vector(Width_g - 1 downto 0);
        o : out std_logic_vector(Width_g - 1 downto 0));
end entity;
architecture a of t_vhdl_partportgen_m is
begin
  o <= i;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_partportgen is
end entity;
architecture a of t_vhdl_partportgen is
  signal d : std_logic_vector(5 downto 0) := "000010";
  signal v : std_logic := '1';
  signal od : std_logic_vector(3 downto 0);
  signal ov : std_logic;
  signal whole : std_logic_vector(4 downto 0);
begin
  u1 : entity work.t_vhdl_partportgen_m generic map (Width_g => 5)
    port map (i(3 downto 0) => d(3 downto 0), i(4) => v, o(3 downto 0) => od, o(4) => ov);
  u2 : entity work.t_vhdl_partportgen_m generic map (Width_g => 5)
    port map (i(4) => v, i(3 downto 0) => d(3 downto 0), o => whole);
  process
  begin
    wait for 1 ns;
    assert to_string(od) = "0010" and ov = '1'
      report "u1 " & to_string(od) & std_logic'image(ov) severity failure;
    assert to_string(whole) = "10010" report "u2 " & to_string(whole) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
