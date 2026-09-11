-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Without "others" the choices of an array selector name every value of it,
-- all of them values of one subtype and so of one length, and two choices of
-- a matching case written alike match the same values (10.9). A subtype with a null range has no value to give an object (5.2.1),
-- and a static division by zero has none either (9.4). References: nvc
-- 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

library ieee;
use ieee.std_logic_1164.all;

entity t is
end entity;

architecture a of t is
  procedure p(s : string) is
  begin
    case s is when "abc" => null; when "de" => null; when others => null; end case;  -- Bad
  end procedure;
begin
  process
    type t_null is range 0 to -1;
    variable x : t_null := 0;  -- Bad
    variable r : real;
    variable v : string(1 to 3);
    variable w : bit_vector(1 downto 0);
    variable u : std_logic_vector(3 downto 0);
  begin
    case w is when "00" | "01" | "10" | "11" => null; end case;
    case v is when "abc" => null; end case;  -- Bad
    case w is when "00" | "01" => null; end case;  -- Bad
    case? u is when "1---" => null; when "01--" => null; when others => null; end case?;
    case? u is when "1---" => null; when "1---" => null; when others => null; end case?;  -- Bad
    r := 1.0 / 2.0;
    r := 1.0 / 0.0;  -- Bad
    wait;
  end process;
end architecture;
