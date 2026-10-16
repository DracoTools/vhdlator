-- DESCRIPTION: Verilator: VHDL clocked process with a delayed assignment
--
-- A clocked process that also assigns with an 'after' delay keeps its plain
-- assignments (they were lost: the transaction's delivery, a forked process,
-- re-armed the plain target's copy before it was committed), and an 'after'
-- delay that is zero only when running takes effect in the same delta as a
-- plain assignment.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_after_clocked is end entity;
architecture a of t_vhdl_after_clocked is
  signal clk : std_logic := '0';
  signal a, b, c : integer := 0;
  signal d : time := 0 ns;
begin
  clk <= not clk after 5 ns when now < 100 ns;
  p1 : process (clk)
  begin
    if rising_edge(clk) then
      a <= a + 1;
      b <= a after d;
      c <= a after 1 ns;
    end if;
  end process;
  -- b changes in the same delta as a
  check_b : process (a)
  begin
    if a > 0 then
      assert b = a - 1 report "b=" & integer'image(b) & " a=" & integer'image(a)
        severity failure;
    end if;
  end process;
  check_c : process
  begin
    for i in 1 to 9 loop
      wait until rising_edge(clk);
      wait for 0 ns;
      assert c = i - 2 or (i = 1 and c = 0)
        report "before: c=" & integer'image(c) & " i=" & integer'image(i) severity failure;
      wait for 1 ns;
      assert c = i - 1 report "after: c=" & integer'image(c) & " i=" & integer'image(i)
        severity failure;
    end loop;
    wait for 20 ns;
    assert a = 10 and b = 9 and c = 9 report "end: a=" & integer'image(a) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
