-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A subprogram body is a named construct, so from inside it one of its own
-- declarations may be named in full: "f.v" is the v declared in f (8.3).
-- The prefix may be an operator symbol, which is how the subprogram is
-- named when that is what it is called.

entity t_vhdl_subpname is
end entity;

architecture rtl of t_vhdl_subpname is
begin
  check : process
    -- Takes bit operands so that the integer addition in its own body is
    -- the predefined one rather than a recursive call
    function "+" (a, b : bit) return integer is
      variable bias : integer := 100;
    begin
      return bit'pos(a) + bit'pos(b) + "+".bias;
    end function;
    function scale (v : integer) return integer is
      constant factor : integer := 3;
    begin
      return v * scale.factor;
    end function;
  begin
    assert ('1' + '1') = 102 report "bad operator body" severity error;
    assert scale(4) = 12 report "bad function body" severity error;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
