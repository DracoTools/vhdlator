-- DESCRIPTION: Verilator: VHDL numeric_std "*" over lengths a function of generics gives
--
-- A length that calls a function (GHDL gna issue2065's PIPELINE_BITS) is not
-- one the native product can be as wide as: its body is compiled, and the
-- operation around it must not expect the native one.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nine_mulgenfunc is
  generic (BITS : natural := 6);
end entity;
architecture a of t_vhdl_nine_mulgenfunc is
  function W return integer is
  begin
    return BITS + 2;
  end function;
  type pair_t is record
    i : signed(W - 1 downto 0);
    q : signed(W - 1 downto 0);
  end record;
  signal p : pair_t;
  signal m : signed(W - 1 downto 0);
begin
  m <= resize(shift_right(p.i * p.q - p.q * p.q, 1), W);
  process
  begin
    p.i <= to_signed(-3, W);
    p.q <= to_signed(5, W);
    wait for 1 ns;
    assert to_integer(m) = -20 report "m=" & integer'image(to_integer(m)) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
