-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A design written the way real VHDL is written: against the IEEE packages,
-- naming nothing on the command line. Verilator supplies ieee.std_logic_1164
-- and ieee.numeric_std, and whatever those need in turn.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.env.all;

entity t_vhdl_ieee is
  generic (WIDTH : integer := 8);
  port (clk  : in  std_logic;
        rst  : in  std_logic;
        en   : in  std_logic;
        q    : out std_logic_vector(7 downto 0);
        odd  : out std_logic);
end entity;

architecture rtl of t_vhdl_ieee is
  signal c : unsigned(7 downto 0);
begin

  process (clk, rst) is
  begin
    if rst = '1' then
      c <= (others => '0');
    elsif rising_edge(clk) then
      if en = '1' then
        c <= c + 1;
      end if;
    end if;
  end process;

  q   <= std_logic_vector(c);
  odd <= c(0);

end architecture;
