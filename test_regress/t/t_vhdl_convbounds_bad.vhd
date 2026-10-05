-- DESCRIPTION: Verilator: VHDL conversion to an unconstrained array type of
-- a value whose bounds the target's index subtype cannot hold
--
-- Converted to BIT_VECTOR, a value indexed "31 downto -32" keeps its bounds
-- (9.3.6), and -32 is no NATURAL: the conversion is an error. nvc and
-- Questa say so before it runs, GHDL when it runs; it is stopped here when
-- it runs, so that one in a branch never taken is no error. One within
-- range, and a null one, are not errors (nvc bounds issue617; GHDL also
-- stops at the null one).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_convbounds_bad is
end entity;

architecture rtl of t_vhdl_convbounds_bad is
  type sfixed is array (integer range <>) of bit;
  function width(y : bit_vector) return natural is
  begin
    return y'length;
  end function;
begin
  process is
    variable fine : sfixed(7 downto 0) := (others => '1');
    variable null_range : sfixed(-33 downto -32);
    variable example : sfixed(31 downto -32);
  begin
    assert width(bit_vector(fine)) = 8 severity failure;
    assert width(bit_vector(null_range)) = 0 severity failure;
    report "within range";
    assert width(bit_vector(example)) = 64 severity failure;
    report "not reached";
    wait;
  end process;
end architecture;
