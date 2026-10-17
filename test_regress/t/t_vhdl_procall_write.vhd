-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A "process (all)" that writes a line is sensitive to the signals it reads,
-- not to the table of file modes the run keeps (GHDL gna issue2561: "Cannot
-- detect changes on expression of complex type")

use std.textio.all;

entity t_vhdl_procall_write is
end entity;

architecture a of t_vhdl_procall_write is
  signal x : natural := 0;
begin
  x <= 1 after 1 ns, 2 after 2 ns;

  log : postponed process (all) is
    variable l : line;
  begin
    write(l, string'("x: ") & integer'image(x));
    writeline(output, l);
  end process;

  process
  begin
    wait for 5 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
