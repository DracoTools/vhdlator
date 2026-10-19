-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A clocked process whose clock is an alias of a port: the alias has the
-- history of what it denotes (6.6.2; GHDL gna issue301, which crashed)

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aliasclock_ff is
  port (aclk : in std_logic; d : in std_logic; q : out std_logic);
end entity;

architecture rtl of t_vhdl_aliasclock_ff is
  alias clk is aclk;
begin
  process (clk)
  begin
    if rising_edge(clk) then
      q <= d;
    end if;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aliasclock is
end entity;

architecture tb of t_vhdl_aliasclock is
  signal clk, d, q : std_logic := '0';
begin
  ff : entity work.t_vhdl_aliasclock_ff port map (aclk => clk, d => d, q => q);
  process is
  begin
    d <= '1';
    wait for 1 ns;
    assert q /= '1' severity failure;  -- 'U', or '0' under two states
    clk <= '1';
    wait for 1 ns;
    assert q = '1' severity failure;
    d <= '0';
    clk <= '0';
    wait for 1 ns;
    assert q = '1' severity failure;
    clk <= '1';
    wait for 1 ns;
    assert q = '0' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
