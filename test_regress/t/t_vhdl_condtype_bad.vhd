-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A condition is of type BOOLEAN, or of a type the condition operator is
-- defined for. An integer is neither, however readily it would convert to
-- one in another language.

library ieee;
use ieee.std_logic_1164.all;

entity t is
end entity;

architecture rtl of t is
  type colour is (red, green, blue);
  signal n : integer := 1;
  signal c : colour := red;
  signal s : std_logic := '1';
  signal b : boolean := true;
begin
  -- These two are conditions
  process
  begin
    if b then
      report "boolean";
    end if;
    if s then                           -- std_logic has a condition operator
      report "logic";
    end if;
    wait;
  end process;

  -- These are not
  process
  begin
    assert n report "integer condition" severity note;
    wait;
  end process;

  process
  begin
    if c then
      report "enumeration condition";
    end if;
    wait;
  end process;

  process
  begin
    while n loop
      report "loop condition";
    end loop;
    wait;
  end process;
end architecture;
