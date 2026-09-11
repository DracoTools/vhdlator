-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Before VHDL-2008 a case statement or a selected assignment over an array
-- chooses on an object whose subtype is locally static (VHDL-93 8.8, 9.5.2):
-- not a parameter left unconstrained, nor one constrained by a constant a
-- function call gives its value. References: nvc 1.22.1, GHDL 7.0.0-dev,
-- Questa 2025.2.

entity t is
end entity;

architecture a of t is
  function func(a : natural) return natural is
  begin
    return a / 16;
  end function;
  constant slices : natural := func(32);
  signal x, y : bit;
  signal s2 : bit_vector(1 downto 0);
  signal sf : bit_vector(slices - 1 downto 0);
  procedure p(v : bit_vector) is
  begin
    case v is when "00" => null; when others => null; end case;  -- Bad
  end procedure;
begin
  with s2 select x <= '1' when "00", '0' when others;
  with sf select y <= '1' when "00", '0' when others;  -- Bad
end architecture;
