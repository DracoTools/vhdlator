-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- VHDL objects looked up over VPI in a spelling other than the one stored.
-- A basic identifier is case insensitive (15.4.2), and cocotb asks for a
-- name as the source writes it. A Verilog module inside keeps its own,
-- case-sensitive names.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_vpicase_leaf is
  generic (DEPTH : integer := 3);
  port (CLK : in  std_logic;
        Q   : out std_logic_vector(3 downto 0));
end entity;

architecture rtl of t_vhdl_vpicase_leaf is
  signal ShortName : std_logic_vector(3 downto 0) := x"5";
begin
  process (CLK) is
  begin
    if rising_edge(CLK) then
      ShortName <= not ShortName;
    end if;
  end process;
  Q <= ShortName;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_vpicase is
  port (Clk : in  std_logic;
        Q   : out std_logic_vector(3 downto 0);
        R   : out std_logic);
end entity;

architecture rtl of t_vhdl_vpicase is
  component t_vhdl_vpicase_v is
    port (o : out std_logic);
  end component;
begin
  U_LEAF : entity work.t_vhdl_vpicase_leaf
    generic map (DEPTH => 3)
    port map (CLK => Clk, Q => Q);

  U_V : component t_vhdl_vpicase_v
    port map (o => R);
end architecture;
