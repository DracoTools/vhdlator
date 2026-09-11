-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Each element of an array aggregate that is itself an array is as long as
-- the element subtype (9.3.3.3). References: nvc 1.22.1, Questa 2025.2 (GHDL
-- 7.0.0-dev warns).

entity t is
end entity;

architecture a of t is
  type cx_t is array (1 to 2) of bit_vector(15 downto 0);
  constant a16 : bit_vector(15 downto 0) := (others => '0');
  constant a17 : bit_vector(16 downto 0) := (others => '0');
  constant x : cx_t := (1 => a16, 2 => a16);
  constant y : cx_t := (1 => a16, 2 => a17);  -- Bad
begin
end architecture;
