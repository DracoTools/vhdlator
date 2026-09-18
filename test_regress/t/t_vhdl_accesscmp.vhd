-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- What an access value points at carries its own bounds, so comparing it with
-- an array value is the same length and then element by element (9.2.3). And
-- an object an allocator makes starts at its subtype's own default (5.4.1),
-- which for a TIME or a constrained INTEGER subtype is its leftmost value
-- rather than zero.

use std.env.all;

entity t_vhdl_accesscmp is
end entity;

architecture tb of t_vhdl_accesscmp is
  subtype idx is integer range 0 to 3;
  subtype pos4 is integer range 5 to 9;
  type bvp is access bit_vector;
  type tarr is array (idx range <>) of time;
  type iarr is array (idx range <>) of pos4;
  type tarrp is access tarr;
  type iarrp is access iarr;
begin

  main : process is
    variable p : bvp;
    variable q : bit_vector(1 to 4) := "1111";
    variable ta : tarrp;
    variable ia : iarrp;
  begin
    p := new bit_vector'("1111");
    -- Against a literal, which is as long as its characters
    assert p.all = "1111" report "literal" severity failure;
    assert p.all /= "1110" report "literal ne" severity failure;
    assert not (p.all = "11111") report "literal longer" severity failure;
    assert not (p.all = "111") report "literal shorter" severity failure;
    -- Against an object of a constrained subtype
    assert p.all = q report "object" severity failure;
    q(4) := '0';
    assert p.all /= q report "object ne" severity failure;

    -- The elements an allocator leaves behind
    ta := new tarr (idx);
    assert ta(0) = time'left report "time default" severity failure;
    assert ta(3) = time'left report "time default last" severity failure;
    ia := new iarr (idx);
    assert ia(0) = 5 report "subtype default" severity failure;
    assert ia(3) = 5 report "subtype default last" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
