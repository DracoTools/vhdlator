-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- What a case statement or a selected assignment chooses on is discrete or a
-- one-dimensional array of characters. An aggregate target names each
-- object once, by a locally static name, and its signals are all guarded or
-- none is. Outside a process a guarded signal is assigned only by a guarded
-- assignment.

entity t_vhdl_aggtarget_bad is
  generic (g : natural := 1);
end entity;

architecture a of t_vhdl_aggtarget_bad is
  type bits4 is array (1 to 4) of bit;
  function resolve (v : bit_vector) return bit is
  begin
    return v(v'left);
  end function;
  signal r, q : real := 1.0;
  signal v4 : bits4;
  signal x, y : bit;
  signal gb : resolve bit bus;
  signal plain1, plain2 : bit;
begin
  -- Legal
  (plain1, plain2) <= bit_vector'("01");
  -- Illegal
  with r select q <= 1.0 when 1.0, 0.0 when others;
  (v4(g), v4(2), v4(3), v4(4)) <= bits4'("0101");
  (x, x) <= bit_vector'("10");
  (gb, y) <= bit_vector'("11");
  gb <= '1';
end architecture;
