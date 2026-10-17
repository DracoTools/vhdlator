-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A postponed process that schedules a transaction for the next delta cycle
-- is an error (14.7.5.3): nvc and GHDL stop the run ("postponed process
-- cannot cause a delta cycle"), Questa reports a suppressible error

entity t_vhdl_postponed_delta_bad is
end entity;

architecture a of t_vhdl_postponed_delta_bad is
  signal x : natural := 0;
  signal runs : natural := 0;
  signal later : natural := 0;
begin
  x <= 1 after 1 ns;

  -- With a delay it is legal
  ok : postponed process (x) is
  begin
    later <= x after 1 ns;
  end process;

  log : postponed process (x) is
  begin
    runs <= runs + 1;
  end process;

  process
  begin
    wait for 5 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
