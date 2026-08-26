-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- VHDL has no $dumpfile, so a design cannot ask for a waveform from inside
-- itself. Asked to trace, the tool asks on the design's behalf, and what it
-- writes shows each signal under its own name, with the range it was
-- declared with -- including one that runs upward.

use std.env.all;

entity t_vhdl_trace is
end entity;

architecture tb of t_vhdl_trace is

  type state_t is (idle, run, done);

  signal clk  : bit := '0';
  signal cnt  : integer := 0;
  signal down : bit_vector(7 downto 0) := "00000000";
  signal up   : bit_vector(0 to 3) := "0000";
  signal st   : state_t := idle;
  signal re   : real := 0.0;

begin

  clk <= not clk after 5 ns;

  tick : process (clk) is
  begin
    if clk = '1' then
      cnt <= cnt + 1;
      down <= down(6 downto 0) & '1';
      up <= up(1 to 3) & '1';
      re <= re + 0.5;
      if cnt = 1 then st <= run; end if;
      if cnt = 3 then st <= done; end if;
    end if;
  end process;

  main : process is
  begin
    wait for 45 ns;
    assert cnt = 4 report "cnt" severity failure;
    assert down = "00001111" report "down" severity failure;
    assert up = "1111" report "up" severity failure;
    assert st = done report "st" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
