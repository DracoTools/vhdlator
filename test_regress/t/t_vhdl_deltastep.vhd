-- DESCRIPTION: Verilator: VHDL "wait for 0 ns" steps one delta at a time
--
-- Each "wait for 0 ns" resumes after one delta (14.7.5), so a testbench can
-- watch a value move down a chain one stage per step: concurrent
-- assignments, sensitivity-list processes, "process (all)", an instance's
-- ports, a port given an expression (an implicit signal, one delta more),
-- and a clocked process.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_deltastep_sub is
  port (i : in integer; o : out integer);
end entity;

architecture a of t_vhdl_deltastep_sub is
begin
  o <= i + 1;
end architecture;

entity t_vhdl_deltastep is
end entity;

architecture a of t_vhdl_deltastep is
  signal s, t, u, p, q, r, x, a2, c : integer := 0;
  signal clk : bit := '0';
begin
  t <= s;
  u <= t;
  process (s) begin p <= s; end process;
  process (p) begin q <= p; end process;
  inst : entity work.t_vhdl_deltastep_sub port map (i => s, o => r);
  expr : entity work.t_vhdl_deltastep_sub port map (i => s + 1, o => x);
  process (all) begin a2 <= s * 2; end process;
  process (clk) begin if clk'event and clk = '1' then c <= s; end if; end process;

  process
  begin
    s <= 7;
    assert s = 0 severity failure;
    wait for 0 ns;
    -- One delta: s itself, and what reads it through ports with no delay
    assert s = 7 and t = 0 and u = 0 and p = 0 and q = 0 severity failure;
    assert r = 1 and a2 = 0 severity failure;
    wait for 0 ns;
    assert t = 7 and u = 0 and p = 7 and q = 0 severity failure;
    assert r = 8 and a2 = 14 severity failure;
    assert x = 2 severity failure;  -- The implicit signal took its delta
    wait for 0 ns;
    assert u = 7 and q = 7 severity failure;
    assert x = 9 severity failure;
    clk <= '1';
    wait for 0 ns;
    assert c = 0 severity failure;
    wait for 0 ns;
    assert c = 7 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
