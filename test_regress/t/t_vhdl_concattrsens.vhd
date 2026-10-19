-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A concurrent assertion is a process waiting on the signals its condition
-- names (11.5, 10.2): "clk'delayed'last_event" names clk'delayed, and
-- 'LAST_EVENT of it is not a signal. It runs once per event of clk'delayed,
-- and not again a delta later when the time of that event is recorded
-- (GHDL gna issue256). Each report is counted.

entity t_vhdl_concattrsens_sub is
  port (clk : in bit; fires : out natural);
end entity;

architecture behav of t_vhdl_concattrsens_sub is
  signal count : natural := 0;
  function check (ok : boolean) return boolean is
  begin
    return ok;
  end function;
begin
  -- The same condition, counted where a process can count it
  counter : process (clk'delayed) begin
    if not (clk'delayed'last_event >= 10 ns) then
      count <= count + 1;
    end if;
  end process;
  check_it : assert clk'delayed'last_event >= 10 ns or now = 0 ns
    report "short pulse" severity note;
  fires <= count;
end architecture;

entity t_vhdl_concattrsens is
end entity;

architecture behav of t_vhdl_concattrsens is
  signal clk : bit;
  signal fires : natural;
begin
  dut : entity work.t_vhdl_concattrsens_sub port map (clk => clk, fires => fires);

  process begin
    clk <= '0';
    wait for 10 ns;
    clk <= '1';
    wait for 10 ns;
    clk <= '1';  -- A transaction, not an event
    wait for 5 ns;
    clk <= '0';
    wait for 5 ns;
    clk <= '1';
    wait for 1 ns;
    -- An event at 10, 25 and 30 ns, each seen once
    assert fires = 3 report "fired " & integer'image(fires) & " times" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
