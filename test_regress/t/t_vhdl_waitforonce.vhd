-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- The timeout of "wait for T" is evaluated once (10.2). UVVM's AXI VVC
-- test holds a forced value for "v_rand.rand(ONLY, (1 ns, 5 ns, 10 ns))",
-- and evaluating that twice drew two numbers and waited the second.

entity t_vhdl_waitforonce is
end entity;

architecture a of t_vhdl_waitforonce is
  type t_counter is protected
    impure function next_delay return time;
    impure function calls return natural;
  end protected;
  type t_counter is protected body
    variable n : natural := 0;
    impure function next_delay return time is
    begin
      n := n + 1;
      return n * 1 ns;
    end function;
    impure function calls return natural is
    begin
      return n;
    end function;
  end protected body;
  shared variable counter : t_counter;
begin
  process
  begin
    wait for counter.next_delay;
    assert counter.calls = 1 report "calls " & integer'image(counter.calls) severity failure;
    assert now = 1 ns report "now " & time'image(now) severity failure;
    wait for counter.next_delay;
    assert counter.calls = 2 report "calls " & integer'image(counter.calls) severity failure;
    assert now = 3 ns report "now " & time'image(now) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
