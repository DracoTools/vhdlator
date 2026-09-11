-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- The character literals a string or bit string literal is made of are
-- visible where it is written (9.3.2): NUMERIC_STD alone does not make
-- STD_LOGIC_1164's visible. References: nvc 1.22.1, GHDL 7.0.0-dev, Questa
-- 2025.2.

library ieee;
use ieee.numeric_std.all;

entity t is
end entity;

architecture a of t is
begin
  process
    variable a : unsigned(3 downto 0) := to_unsigned(1, 4);
  begin
    a := "0101";  -- Bad
    a := x"a";  -- Bad
    a := a + 1;
    wait;
  end process;
end architecture;
