-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The matching ordering operators of STD_ULOGIC by the LRM's table for ?<
-- (9.2.3), and ?<=, ?> and ?>= made of it, of ?= and of "or" and "not"
-- (GHDL gna issue1872). '-' operands, which report an error, are left out.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_matchorder is
end entity;

architecture a of t_vhdl_nine_matchorder is
  type table_t is array (std_ulogic, std_ulogic) of std_ulogic;
  constant lt_table : table_t := (
    --  U    X    0    1    Z    W    L    H    -
    ('U', 'U', 'U', 'U', 'U', 'U', 'U', 'U', 'X'),  -- U
    ('U', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'),  -- X
    ('U', 'X', '0', '1', 'X', 'X', '0', '1', 'X'),  -- 0
    ('U', 'X', '0', '0', 'X', 'X', '0', '0', 'X'),  -- 1
    ('U', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'),  -- Z
    ('U', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'),  -- W
    ('U', 'X', '0', '1', 'X', 'X', '0', '1', 'X'),  -- L
    ('U', 'X', '0', '0', 'X', 'X', '0', '0', 'X'),  -- H
    ('X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X')); -- -
begin
  process is
    variable l, r : std_ulogic;
    variable bad : natural := 0;
  begin
    for i in std_ulogic'('U') to std_ulogic'('H') loop
      for j in std_ulogic'('U') to std_ulogic'('H') loop
        l := i;
        r := j;
        if (l ?< r) /= lt_table(i, j) then
          report "?< of " & std_ulogic'image(i) & " and " & std_ulogic'image(j) severity error;
          bad := bad + 1;
        end if;
        if (l ?<= r) /= (lt_table(i, j) or (i ?= j)) then
          report "?<= of " & std_ulogic'image(i) & " and " & std_ulogic'image(j) severity error;
          bad := bad + 1;
        end if;
        if (l ?> r) /= not (lt_table(i, j) or (i ?= j)) then
          report "?> of " & std_ulogic'image(i) & " and " & std_ulogic'image(j) severity error;
          bad := bad + 1;
        end if;
        if (l ?>= r) /= not lt_table(i, j) then
          report "?>= of " & std_ulogic'image(i) & " and " & std_ulogic'image(j) severity error;
          bad := bad + 1;
        end if;
      end loop;
    end loop;
    assert bad = 0 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
