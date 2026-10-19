-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A function whose parameter is a signal, called in a concurrent statement:
-- the signal given is read where the call is written (10.2, 11.6), so the
-- statement waits on it though the copy made for that signal names it
-- directly. PoC's "q <= d when registered(clk, true)" never updated
-- (GHDL gna issue317).

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_sigfuncsens is
end entity;

architecture a of t_vhdl_sigfuncsens is
  function ite(c : boolean; t, f : boolean) return boolean is
  begin
    if c then
      return t;
    end if;
    return f;
  end function;
  function registered(signal clock : std_logic; constant isreg : boolean) return boolean is
  begin
    return ite(isreg, rising_edge(clock), true);
  end function;
  function edge(signal clock : std_logic) return boolean is
  begin
    return rising_edge(clock);
  end function;
  function level(signal s : std_logic) return std_logic is
  begin
    return s;
  end function;
  signal clk : std_logic := '1';
  signal v : std_logic := '0';
  signal q1, q2, q3, q4 : std_logic := '0';
begin
  clk <= not clk after 5 ns;
  q1 <= v when registered(clk, true);
  q2 <= v when edge(clk);
  q3 <= v when registered(clk, false);
  -- Reads a signal through the call alone
  q4 <= level(v);
  process
  begin
    wait until rising_edge(clk);
    v <= '1';
    wait until rising_edge(clk);
    assert q1 = '0' and q2 = '0' report "registered too early" severity failure;
    assert q3 = '1' and q4 = '1' report "pass-through late" severity failure;
    wait until rising_edge(clk);
    assert q1 = '1' report "q1 /= 1" severity failure;
    assert q2 = '1' report "q2 /= 1" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
