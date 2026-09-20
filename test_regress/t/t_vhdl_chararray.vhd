-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A design that writes "type t is array (1 to 8) of character" means text as
-- much as STRING does, and a string literal given to one is its characters
-- rather than a vector of bits (5.3.2, 16.3). And a port of an unresolved
-- type has exactly one driver however its mode is written (6.4.2.3), so an
-- INOUT of one is a port the design drives and reads -- and it keeps the
-- default it was declared with (6.5.6.3).

use std.env.all;

entity t_vhdl_chararray is
  port (pio : inout integer := -1;
        piot : inout time := 1 ns;
        piob : inout bit := '1');
end entity;

architecture tb of t_vhdl_chararray is
  type astr is array (integer range 1 to 8) of character;
  type cstr is array (character range 'a' to 'h') of character;
  signal sg : astr := "abcdefgh";
  constant kc : astr := "abcdefgh";
  constant kx : cstr := "BCDEFGHA";
  signal st : string(1 to 8) := "abcdefgh";
begin

  main : process is
  begin
    -- An array of CHARACTER holds the characters, whatever its type is called
    assert sg(2) = 'b' report "signal " & integer'image(character'pos(sg(2))) severity failure;
    assert kc(2) = 'b' report "constant" severity failure;
    assert st(2) = 'b' report "string" severity failure;
    assert sg(8) = 'h' report "last element" severity failure;
    -- Indexed by a character, which is an ordinary discrete index
    assert kx('a') = 'B' report "character index low" severity failure;
    assert kx('h') = 'A' report "character index high" severity failure;

    -- A port of an unresolved type starts at its own default
    assert pio = -1 report "inout integer is " & integer'image(pio) severity failure;
    assert piot = 1 ns report "inout time" severity failure;
    assert piob = '1' report "inout bit" severity failure;
    pio <= pio + pio;
    wait on pio;
    assert pio = -2 report "inout after drive is " & integer'image(pio) severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
