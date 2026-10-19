-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A conversion written on the formal, "integer(t) => n", converts what comes
-- back out of the parameter on its way to the actual (6.5.7.1; GHDL gna
-- issue369): an integer of another type, or a REAL rounded to an integer.

entity t_vhdl_formalconv_scalar is
end entity;

architecture behav of t_vhdl_formalconv_scalar is
  type type_t is range 0 to integer'high;
  procedure p2 (variable t : out type_t) is
  begin
    t := 1;
  end procedure;
  procedure pr (variable r : out real) is
  begin
    r := 2.5;
  end procedure;
begin
  process
    variable n : natural := 12;
    variable i : integer := 0;
  begin
    p2(integer(t) => n);
    assert n = 1 report "out " & integer'image(n) severity failure;
    pr(integer(r) => i);
    assert i = 3 report "real " & integer'image(i) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
