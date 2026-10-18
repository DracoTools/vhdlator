-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A function whose result has no length of its own returns a null array from
-- a guard that the call's lengths make true: numeric_std's "+" with a null
-- operand ("if L'LENGTH < 1 or R'LENGTH < 1 then return NAU", NVC ieee13).
-- The copy made for the call is as long as that guard's value, not as the
-- value the other path returns.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nullarith_len is
end entity;

architecture t of t_vhdl_nullarith_len is
  function grow (v : bit_vector) return bit_vector is
    variable r : bit_vector(v'length downto 0) := (others => '1');
  begin
    if v'length < 1 then
      return "";
    end if;
    return r;
  end function;
begin
  process
    constant e : bit_vector := grow("");
    constant g : bit_vector := grow("01");
    variable u : unsigned(3 downto 0) := "0101";
  begin
    assert e'length = 0 severity failure;
    assert g'length = 3 severity failure;
    assert u + "0010" = "0111" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
