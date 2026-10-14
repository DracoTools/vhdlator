-- DESCRIPTION: Verilator: clocked processes over a clock of the nine values, and their resets
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_clock is
end entity;

architecture a of t_vhdl_nine_clock is
  signal clk : std_logic := '0';
  signal rst : std_logic := '0';
  signal n_rise, n_fall, n_event, n_reset, n_conc : natural := 0;
  signal q : std_logic_vector(3 downto 0) := "0000";
  signal d : std_logic_vector(3 downto 0) := "1010";
begin
  process (clk) is
  begin
    if rising_edge(clk) then
      n_rise <= n_rise + 1;
    end if;
  end process;

  process (clk) is
  begin
    if falling_edge(clk) then
      n_fall <= n_fall + 1;
    end if;
  end process;

  process (clk) is
  begin
    if clk'event and clk = '1' then
      n_event <= n_event + 1;
    end if;
  end process;

  process (clk, rst) is
  begin
    if rst = '1' then
      n_reset <= 0;
    elsif rising_edge(clk) then
      n_reset <= n_reset + 1;
    end if;
  end process;

  q <= d when rising_edge(clk);
  n_conc <= n_conc + 1 when rising_edge(clk);

  process is
    procedure step(v : std_logic) is
    begin
      clk <= v;
      wait for 1 ns;
    end procedure;
  begin
    wait for 1 ns;
    step('1');   -- 0 -> 1: rise, event
    step('X');   -- 1 -> X
    step('1');   -- X -> 1: event only
    step('U');
    step('1');   -- U -> 1: event only
    step('0');   -- 1 -> 0: fall
    step('L');   -- 0 -> L
    step('H');   -- L -> H: rise
    step('L');   -- H -> L: fall
    rst <= 'H'; wait for 1 ns;  -- 'H' is not '1': no reset
    step('1');   -- L -> 1: rise
    rst <= '1'; wait for 1 ns;  -- reset
    report "rise=" & integer'image(n_rise) & " fall=" & integer'image(n_fall)
      & " event=" & integer'image(n_event) & " reset=" & integer'image(n_reset)
      & " conc=" & integer'image(n_conc) & " q=" & to_string(q);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
