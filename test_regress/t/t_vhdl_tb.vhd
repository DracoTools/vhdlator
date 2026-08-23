-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A VHDL testbench that drives and checks a VHDL design, with no C++ at all:
-- processes that wait, a clock generator, assertions, reports, and
-- std.env.stop to end the run.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity counter is
  port (clk : in  std_logic;
        rst : in  std_logic;
        en  : in  std_logic;
        q   : out std_logic_vector(7 downto 0));
end entity;

architecture rtl of counter is
  signal c : unsigned(7 downto 0);
begin
  process (clk, rst) is
  begin
    if rst = '1' then
      c <= (others => '0');
    elsif rising_edge(clk) then
      if en = '1' then c <= c + 1; end if;
    end if;
  end process;
  q <= std_logic_vector(c);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.env.all;

entity t_vhdl_tb is
end entity;

architecture sim of t_vhdl_tb is
  signal clk : std_logic := '0';
  signal rst : std_logic := '1';
  signal en  : std_logic := '1';
  signal q   : std_logic_vector(7 downto 0);
  signal ticks : integer := 0;
begin

  dut : entity work.counter port map (clk => clk, rst => rst, en => en, q => q);

  -- A clock generator: the process runs for ever, suspending on each wait
  clkgen : process is
  begin
    clk <= '0';
    wait for 5 ns;
    clk <= '1';
    wait for 5 ns;
  end process;

  -- A monitor, which resumes on an edge rather than after a delay
  monitor : process is
  begin
    wait until clk = '1';
    ticks <= ticks + 1;
  end process;

  stim : process is
  begin
    wait for 12 ns;
    rst <= '0';

    -- Count ten cycles and check
    wait for 100 ns;
    assert q = std_logic_vector(to_unsigned(10, 8))
      report "after ten clocks the counter is " & integer'image(to_integer(unsigned(q)))
      severity failure;

    -- Hold the enable low and check that counting stops
    en <= '0';
    wait for 50 ns;
    assert q = std_logic_vector(to_unsigned(10, 8))
      report "the counter moved while disabled" severity failure;
    en <= '1';

    wait for 30 ns;
    assert q = std_logic_vector(to_unsigned(13, 8))
      report "counting did not resume" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
