-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- S'DELAYED is a signal in its own right (16.2), so an attribute may be asked
-- of it: "clk'DELAYED(t)'LAST_EVENT" is about the delayed signal's own past,
-- which is kept the same way the signal's own is.

use std.env.all;

entity t_vhdl_implsig is
end entity;

architecture tb of t_vhdl_implsig is
  signal clk : bit := '0';
begin

  gen : process is
  begin
    for i in 1 to 6 loop
      wait for 10 ns;
      clk <= not clk;
    end loop;
    wait;
  end process;

  main : process is
  begin
    -- clk last changed at 40 ns, so its delayed-by-5 copy changed at 45 ns
    wait for 45 ns;
    assert clk'last_event = 5 ns
      report "clk " & time'image(clk'last_event) severity failure;
    assert clk'delayed(5 ns)'last_event = 0 ns
      report "delayed " & time'image(clk'delayed(5 ns)'last_event) severity failure;
    wait for 3 ns;
    assert clk'last_event = 8 ns
      report "clk later" severity failure;
    assert clk'delayed(5 ns)'last_event = 3 ns
      report "delayed later " & time'image(clk'delayed(5 ns)'last_event) severity failure;
    -- The delayed signal carries the value the signal had that long ago
    assert clk'delayed(5 ns) = clk'delayed(5 ns) report "stable read" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
