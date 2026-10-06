-- DESCRIPTION: Verilator: VHDL shift of a vector sized by a generic
--
-- "shift_left(to_signed(1, dword_t'length), w-2)" gives a constant whose
-- subtype is sized by a generic. The shift is emitted over its operand's
-- bits, which are of a fixed length here, so the result is an ordinary
-- value and not one carrying its own bounds (GHDL gna bug24064). A rotate
-- of such a vector is the package's own body, which takes any length (GHDL
-- gna issue2065).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_shiftgeneric is
  generic (w : natural := 16);
end entity;

architecture arch of t_vhdl_shiftgeneric is
  subtype dword_t is signed(2*w-1 downto 0);
  constant round_val : dword_t := shift_left(to_signed(1, dword_t'length), w-2);
  constant down_val : dword_t := shift_right(to_signed(-64, dword_t'length), 2);
  constant rot_val : dword_t := rotate_right(to_signed(1, dword_t'length), 1);
  constant ADDR_BITS : natural := w / 2;
begin
  process
    variable a : unsigned(ADDR_BITS-1 downto 0);
    variable n : natural := 3;
  begin
    assert round_val = to_signed(16384, 32) severity failure;
    assert down_val = to_signed(-16, 32) severity failure;
    assert rot_val = x"80000000" severity failure;
    a := rotate_left(to_unsigned(5*2, ADDR_BITS), n);
    assert a = x"50" severity failure;
    a := rotate_right(to_unsigned(3, ADDR_BITS), n);
    assert a = x"60" severity failure;
    report "shiftgeneric done";
    std.env.finish;
  end process;
end architecture;
