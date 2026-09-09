-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The condition operator arrived in VHDL-2008. Before it, a condition is of
-- type BOOLEAN and nothing else, so a logic value is not one.

library ieee;
use ieee.std_logic_1164.all;

entity t is
end entity;

architecture rtl of t is
  signal s : std_logic := '1';
  signal b : boolean := true;
begin
  process
  begin
    if b then                           -- BOOLEAN, which is a condition
      null;
    end if;
    if s then                           -- not one before VHDL-2008
      null;
    end if;
    wait;
  end process;
end architecture;
