-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The condition operator applies to one primary, and that is the whole
-- expression, so what follows it cannot go on to a binary operator.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_condop_bad is
end entity;

architecture a of t_vhdl_condop_bad is
  signal a, b : std_ulogic;
begin
  process
  begin
    if ?? (a and b) then
      null;
    end if;
    if ?? a and b then
      null;
    end if;
    wait;
  end process;
end architecture;
