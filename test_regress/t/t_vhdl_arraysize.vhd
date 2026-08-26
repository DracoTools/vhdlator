-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Three lengths that are not a small literal. An array type whose index
-- range names a generic is an ordinary constrained array whose bounds are
-- evaluated when the type declaration is elaborated (14.4.2), not an array of
-- the index subtype's whole range. A null range has no elements at all
-- (5.3.2.1), so an aggregate for one supplies none. And "(others => v)" over
-- a very long range is one value repeated, not a hundred thousand of them.

use std.env.all;

entity t_vhdl_arraysize is
  generic (WIDTH : integer := 6;
           DEPTH : integer := 4);
end entity;

architecture tb of t_vhdl_arraysize is

  -- The bounds are not numbers here, but they are by the time the type is
  -- elaborated
  subtype word_t is bit_vector(WIDTH - 1 downto 0);
  type mem_t is array (0 to DEPTH - 1) of word_t;
  type wide_t is array (0 to 2 * DEPTH) of integer;

  -- A null range: the right bound is below the left of an ascending range
  subtype null_t is bit_vector(1 to 0);

  -- Long enough that writing out every element is not on
  constant BIG : integer := 200000;
  type big_t is array (0 to BIG - 1) of bit;

  signal m : mem_t := (others => (others => '1'));
  signal w : wide_t := (others => 7);
  signal n : null_t := (others => '0');
  signal b : big_t := (others => '1');

begin

  main : process is
  begin
    assert m'length = 4 report "m'length=" & integer'image(m'length) severity failure;
    assert m(0)'length = 6 report "m(0)'length" severity failure;
    assert m(3) = "111111" report "m(3)" severity failure;
    assert w'length = 9 report "w'length=" & integer'image(w'length) severity failure;
    assert w(8) = 7 report "w(8)" severity failure;

    -- A null array is zero long, and comparing two of them is true
    assert n'length = 0 report "n'length=" & integer'image(n'length) severity failure;
    assert n = null_t'(others => '0') report "null compare" severity failure;

    assert b'length = BIG report "b'length" severity failure;
    assert b(0) = '1' and b(BIG - 1) = '1' report "b ends" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
