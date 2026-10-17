-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The value a concurrent signal assignment gives its target is checked
-- against the target's subtype, as the process the statement is checks it
-- (10.5.2.1): "i <= i + 1 after 10 ns" with i of a type "range 0 to 100"
-- stops at 101 (NVC bounds11; nvc, GHDL and Questa stop)

entity t_vhdl_concrange_bad is
end entity;

architecture test of t_vhdl_concrange_bad is
  type my_int is range 0 to 100;
  signal i : my_int;
  signal j : my_int;
  signal k : integer := 0;
begin
  -- Undelayed, and in range while k is
  j <= my_int(k);
  -- Delayed, counting up to the bound and past it
  i <= i + 1 after 10 ns;

  process
  begin
    k <= 50;
    wait for 1 ns;
    assert j = 50 severity failure;
    wait;
  end process;
end architecture;
