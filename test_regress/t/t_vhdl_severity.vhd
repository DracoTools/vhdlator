-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Which severity ends the run is the tool's to choose (10.3), and both
-- reference simulators choose FAILURE: nvc 1.22.1 and GHDL 7.0.0-dev each
-- print an assertion of severity ERROR and carry on. A testbench reports a
-- recoverable mismatch at ERROR and expects to reach its own summary.

use std.env.all;

entity t_vhdl_severity is
end entity;

architecture tb of t_vhdl_severity is
  signal reached : integer := 0;
begin

  main : process is
  begin
    assert false report "note is not fatal" severity note;
    reached <= 1;
    wait for 1 ns;
    assert false report "warning is not fatal" severity warning;
    reached <= 2;
    wait for 1 ns;
    assert false report "error is not fatal" severity error;
    reached <= 3;
    wait for 1 ns;
    -- Every one of them was passed, which is the whole point
    assert reached = 3 report "run was cut short" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
