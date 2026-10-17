-- DESCRIPTION: Verilator: VHDL record signal given an aggregate of to_signed by a function of generics
--
-- The elements are as long as a function of the generics says, and so are
-- the TO_SIGNED results of the initial aggregate: no width warning, and
-- the values as given.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_recaggfuncinit is
  generic (BITS : natural := 6);
end entity;

architecture a of t_vhdl_recaggfuncinit is
  function W return integer is
  begin
    return BITS + 2;
  end function;
  type pair_t is record
    i : signed(W - 1 downto 0);
    q : signed(W - 1 downto 0);
  end record;
  signal p : pair_t := (to_signed(-3, W), to_signed(5, W));
  signal u : unsigned(W - 1 downto 0) := to_unsigned(200, W);
begin
  process
  begin
    wait for 1 ns;
    assert to_integer(p.i) = -3 and to_integer(p.q) = 5 severity failure;
    assert p.i'length = 8 and to_integer(u) = 200 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
