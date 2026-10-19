-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A function called with no arguments whose result is an access to an array:
-- a following parenthesis indexes or slices what the result designates, which
-- is dereferenced without saying so (8.1, 8.4; GHDL gna bug08's paren10 to 13).
-- GHDL runs it; nvc and Questa refuse it.

entity t_vhdl_callaccindex is
end entity;

architecture behav of t_vhdl_callaccindex is
begin
  proc : process
    type string_acc is access string;
    variable hel : string_acc := new string'("hello");
    impure function a return string_acc is
    begin
      return hel;
    end function;
    function a return natural is
    begin
      return 5;
    end function;
    impure function g return string_acc is
    begin
      return new string'("world");
    end function;
    constant b : natural := 2;
  begin
    assert a(b) = 'e' report "a(b) /= 'e'" severity failure;
    assert proc.a(b) = 'e' report "proc.a(b) /= 'e'" severity failure;
    assert a(2 to 4) = "ell" report "a(2 to 4) /= ell" severity failure;
    assert g(5) = 'd' report "g(5) /= 'd'" severity failure;
    hel(1) := 'j';
    assert a(1) = 'j' report "a(1) /= 'j'" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
