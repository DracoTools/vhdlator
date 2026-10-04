-- DESCRIPTION: Verilator: VHDL signal parameter given a slice whose range
-- is a subtype's name
--
-- "split(outcomb => valid(ret_split_t), ...)": a slice written with a
-- discrete subtype for its range is that subtype's range (8.5), and a
-- procedure called concurrently drives exactly those elements of the
-- caller's signal -- GHDL gna issue156.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_slicesubtype_pkg is
  type bits_t is array (natural range <>) of bit;
  procedure split (signal outcomb : out bits_t; signal outalone : out bit;
                   signal incomb : in bits_t);
end package;

package body t_vhdl_slicesubtype_pkg is
  procedure split (signal outcomb : out bits_t; signal outalone : out bit;
                   signal incomb : in bits_t) is
  begin
    outcomb <= (outcomb'range => '1');
    outalone <= incomb(incomb'left);
  end procedure;
end package body;

use work.t_vhdl_slicesubtype_pkg.all;

entity t_vhdl_slicesubtype is
end entity;

architecture rtl of t_vhdl_slicesubtype is
  type ctrl_t is (MANT_IN, EXP_IN, DIV_IN, MANT_OUT, EXP_OUT, DIV_OUT, MULT);
  subtype ctrl_range_t is integer range ctrl_t'pos(ctrl_t'left) to ctrl_t'pos(ctrl_t'right);
  subtype ret_split_t is integer range ctrl_t'pos(MANT_IN) to ctrl_t'pos(DIV_IN);
  subtype tail_t is integer range 4 to 5;
  signal valid : bits_t(ctrl_range_t) := (others => '0');
  signal ready : bits_t(ctrl_range_t) := (0 => '1', 4 => '1', others => '0');
  signal alone, alone2 : bit;
begin
  split(outcomb => valid(ret_split_t), outalone => alone, incomb => ready(ret_split_t));
  split(outcomb => valid(tail_t), outalone => alone2, incomb => ready(tail_t));

  process is
  begin
    wait for 1 ns;
    assert valid = "1110110" report "valid" severity failure;
    assert alone = '1' and alone2 = '1' report "alone" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
