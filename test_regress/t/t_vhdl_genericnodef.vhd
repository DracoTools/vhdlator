-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- A generic with no default is how VHDL says the instantiation must
-- choose, so it is the ordinary shape of a reusable component. GHDL
-- 7.0.0-dev and nvc 1.22.1 analyze such an entity without a value for it.
entity t_vhdl_genericnodef is
    generic (WIDTH : positive;
             STEP  : integer;
             NAME  : character);
    port (clk : in std_logic;
          q   : out std_logic_vector(WIDTH - 1 downto 0));
end entity;

architecture rtl of t_vhdl_genericnodef is
    signal cnt : unsigned(WIDTH - 1 downto 0) := (others => '0');
begin
    process (clk) is
    begin
        if rising_edge(clk) then
            cnt <= cnt + STEP;
        end if;
    end process;

    q <= std_logic_vector(cnt);
end architecture;
