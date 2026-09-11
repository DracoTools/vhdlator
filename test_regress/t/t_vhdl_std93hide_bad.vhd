-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- What VHDL-2008 added to STD.STANDARD is not there for a unit written in
-- VHDL-93 (16.3): BOOLEAN_VECTOR and the other vector types, MINIMUM and
-- MAXIMUM, TO_STRING, the edge functions of BIT and the reduction operators.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t is
end entity;

architecture a of t is
  signal s1 : boolean := true;
  signal s2 : boolean := false;
  signal b : bit;
  signal bv : bit_vector(1 downto 0);
begin
  process
    variable i : integer;
  begin
    case s1 & s2 is when others => null; end case;  -- Bad: no BOOLEAN_VECTOR
    i := minimum(1, 2);  -- Bad
    report to_string(i);  -- Bad
    if rising_edge(b) then null; end if;  -- Bad
    b <= and bv;  -- Bad
    b <= bv(0) and bv(1);
    report integer'image(i);
    wait;
  end process;
end architecture;
